/*********************************************************************
 * Software License Agreement (SBD License)
 *
 *  Copyright (c) 2016, University of Colorado, Boulder
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Univ of CO, Boulder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Dave Coleman */

// OMPL
#include <ompl/base/PlannerStatus.h>
#include <ompl/util/Console.h>

// Bolt
#include <bolt_core/Bolt.h>
#include <bolt_core/SparseGenerator.h>
#include <bolt_core/SparseCriteria.h>
#include <bolt_core/SparseMirror.h>

namespace og = ompl::geometric;
namespace ob = ompl::base;
namespace ot = ompl::tools;

namespace ompl
{
namespace tools
{
namespace bolt
{
Bolt::Bolt(const base::SpaceInformationPtr &si) : geometric::SimpleSetup(si)
{
  initialize();
}

Bolt::Bolt(const base::StateSpacePtr &space) : geometric::SimpleSetup(space)
{
  initialize();
}

void Bolt::initialize(std::size_t indent)
{
  BOLT_INFO(indent, true, "Initializing Bolt Framework");

  // Initalize visualizer class
  BOLT_INFO(indent, verbose_, "Loading visualizer");
  visual_.reset(new Visualizer());

  filePath_ = std::move("unloaded");

  // Load the sparse graph datastructure
  BOLT_INFO(indent, verbose_, "Loading SparseGraph");
  sparseGraph_.reset(new SparseGraph(si_, visual_));

  // Load criteria used to determine if samples are saved or rejected
  BOLT_INFO(indent, verbose_, "Loading SparseCriteria");
  sparseCriteria_.reset(new SparseCriteria(sparseGraph_));

  // Give the sparse graph reference to the criteria, because sometimes it needs data from there
  sparseGraph_->setSparseCriteria(sparseCriteria_);

  // Load the generator of sparse vertices and edges, and give it reference to the criteria
  BOLT_INFO(indent, verbose_, "Loading SparseGenerator");
  sparseGenerator_.reset(new SparseGenerator(sparseGraph_));
  sparseGenerator_->setSparseCriteria(sparseCriteria_);

  // Load mirror for duplicating arm
  BOLT_INFO(indent, verbose_, "Loading SparseMirror");
  sparseMirror_.reset(new SparseMirror(sparseGraph_));

  // ----------------------------------------------------------------------------
  // CompoundState settings for task planning

  // Create discrete state space
  const int NUM_LEVELS = 2;  // TODO do not hardcode?
  const int lowerBound = 0;
  const int upperBound = NUM_LEVELS;
  base::StateSpacePtr discreteSpace = std::make_shared<base::DiscreteStateSpace>(lowerBound, upperBound);

  // Create compound state
  base::CompoundStateSpacePtr compoundSpace = std::make_shared<base::CompoundStateSpace>();
  compoundSpace->addSubspace(si_->getStateSpace(), 1.0);  // 100% weight
  compoundSpace->addSubspace(discreteSpace, 0.0);         // 0% weight

  // Create space information
  compoundSI_ = std::make_shared<base::SpaceInformation>(compoundSpace);

  // Load the task graph used for combining multiple layers of sparse graph
  BOLT_INFO(indent, verbose_, "Loading TaskGraph");
  taskGraph_.reset(new TaskGraph(si_, compoundSI_, sparseGraph_));

  // Load the Retrieve repair database. We do it here so that setRepairPlanner() works
  BOLT_INFO(indent, verbose_, "Loading BoltPlanner");
  boltPlanner_ = BoltPlannerPtr(new BoltPlanner(si_, compoundSI_, taskGraph_, visual_));

  std::size_t numThreads = boost::thread::hardware_concurrency();
  OMPL_INFORM("Bolt Framework initialized using %u threads", numThreads);
}

void Bolt::setup()
{
  std::size_t indent = 0;
  if (!configured_ || !si_->isSetup() || !boltPlanner_->isSetup())
  {
    // Setup Space Information if we haven't already done so
    if (!si_->isSetup())
      si_->setup();

    if (!compoundSI_->isSetup())
      compoundSI_->setup();

    // Setup planning from experience planner
    boltPlanner_->setProblemDefinition(pdef_);

    if (!boltPlanner_->isSetup())
      boltPlanner_->setup();

    // Setup SPARS
    sparseGraph_->setup();
    sparseCriteria_->setup(indent);
    sparseGenerator_->setup(indent);
    taskGraph_->setup();

    // Set the configured flag
    configured_ = true;
  }
}

void Bolt::clearForNextPlan()
{
  boltPlanner_->clear();
  pdef_->clearSolutionPaths();
}

void Bolt::clear()
{
  sparseGraph_->clear();
  sparseCriteria_->clear();
  sparseGenerator_->clear();
  boltPlanner_->clear();
  pdef_->clearSolutionPaths();
}

void Bolt::setPlannerAllocator(const base::PlannerAllocator &pa)
{
  pa_ = pa;
  // note: the boltPlanner_ never uses the allocator so does not need to be reset
  configured_ = false;
}

base::PlannerStatus Bolt::solve(const base::PlannerTerminationCondition &ptc)
{
  std::size_t indent = 0;

  // Setup again in case it has not been done yet
  setup();

  lastStatus_ = base::PlannerStatus::UNKNOWN;
  time::point start = time::now();

  // Warn if there are queued paths that have not been added to the experience database
  // OMPL_INFORM("Num solved paths uninserted into the experience database in the post-proccessing queue: %u",
  //             queuedSolutionPaths_.size());

  // SOLVE
  lastStatus_ = boltPlanner_->solve(ptc);

  // Task time
  planTime_ = time::seconds(time::now() - start);

  // Do logging
  processResults(indent);

  return lastStatus_;
}

void Bolt::visualize(std::size_t indent)
{
  BOLT_FUNC(indent, true, "visualizing()");

  // Optionally visualize raw trajectory
  if (visualizeRawTrajectory_)
  {
    geometric::PathGeometricPtr originalPath = boltPlanner_->getOriginalSolutionPath();

    // Make the chosen path a different color and thickness
    visual_->viz5()->path(originalPath.get(), tools::MEDIUM, tools::BLUE, tools::BLACK);
    visual_->viz5()->trigger();

    // Don't show raw trajectory twice in larger dimensions
    if (si_->getStateSpace()->getDimension() == 3)
    {
      visual_->viz6()->path(originalPath.get(), tools::MEDIUM, tools::BLUE, tools::BLACK);
      visual_->viz6()->trigger();
    }
  }

  //geometric::PathGeometric *solutionPath = static_cast<geometric::PathGeometric *>(pdef_->getSolutionPath().get());

  // Show smoothed & interpolated path
  if (visualizeSmoothTrajectory_)
  {
    std::vector<geometric::PathGeometricPtr> modelSolutionSegments = boltPlanner_->getModelSolutionSegments();
    for (std::size_t i = 0; i < modelSolutionSegments.size(); ++i)
    {
      geometric::PathGeometricPtr modelSolutionSegment = modelSolutionSegments[i];
      if (i == 1)
        visual_->viz6()->path(modelSolutionSegment.get(), tools::LARGE, tools::BLACK, tools::PURPLE);
      else
        visual_->viz6()->path(modelSolutionSegment.get(), tools::LARGE, tools::BLACK, tools::BLUE);
    }
    visual_->viz6()->trigger();
  }
  visual_->waitForUserFeedback("review visual from Bolt.cpp");

  // Show robot animated
  if (visualizeRobotTrajectory_)
  {
    BOLT_DEBUG(indent, true, "Blocking while visualizing solution path TODO");
    //visual_->viz6()->path(&solutionPathCopy, tools::ROBOT, tools::DEFAULT, tools::DEFAULT);
  }
}

bool Bolt::checkBoltPlannerOptimality(std::size_t indent)
{
  geometric::PathGeometric *rawPath = boltPlanner_->getOriginalSolutionPath().get();
  geometric::PathGeometric *smoothedPath = static_cast<geometric::PathGeometric *>(pdef_->getSolutionPath().get());

  double optimalLength = smoothedPath->length();
  double sparseLength = rawPath->length();
  double theoryLength = sparseCriteria_->getStretchFactor() * optimalLength + 4 * sparseCriteria_->getSparseDelta();
  double percentOfMaxAllows = sparseLength / theoryLength * 100.0;

  BOLT_DEBUG(indent, 1, "-----------------------------------------");
  BOLT_DEBUG(indent, 1, "Checking Asymptotic Optimality Guarantees");
  BOLT_DEBUG(indent + 2, 1, "Raw Path Length:         " << sparseLength);
  BOLT_DEBUG(indent + 2, 1, "Smoothed Path Length:    " << optimalLength);
  BOLT_DEBUG(indent + 2, 1, "Theoretical Path Length: " << theoryLength);
  BOLT_DEBUG(indent + 2, 1, "Stretch Factor t:        " << sparseCriteria_->getStretchFactor());
  BOLT_DEBUG(indent + 2, 1, "Sparse Delta:            " << sparseCriteria_->getSparseDelta());

  if (sparseLength >= theoryLength)
  {
    BOLT_ERROR(indent + 2, "Asymptotic optimality guarantee VIOLATED");
    return false;
  }
  else
    BOLT_GREEN(indent + 2, 1, "Asymptotic optimality guarantee maintained");
  BOLT_WARN(indent + 2, 1, "Percent of max allowed:  " << percentOfMaxAllows << " %");
  BOLT_DEBUG(indent, 1, "-----------------------------------------");

  // visual_->waitForUserFeedback("review results");

  return true;
}

void Bolt::processResults(std::size_t indent)
{
  // Record stats
  stats_.totalPlanningTime_ += planTime_;  // used for averaging
  stats_.numProblems_++;                   // used for averaging

  switch (static_cast<ompl::base::PlannerStatus::StatusType>(lastStatus_))
  {
    case base::PlannerStatus::TIMEOUT:
      stats_.numSolutionsTimedout_++;
      BOLT_ERROR(indent, "Bolt::solve(): TIMEOUT - No solution found after " << planTime_);
      break;
    case base::PlannerStatus::ABORT:
      stats_.numSolutionsTimedout_++;
      BOLT_ERROR(indent, "Bolt::solve(): ABORT - No solution found after " << planTime_);
      break;
    case base::PlannerStatus::APPROXIMATE_SOLUTION:
      BOLT_ERROR(indent, "Bolt::solve(): Approximate - should not happen!");
      exit(-1);
      break;
    case base::PlannerStatus::EXACT_SOLUTION:
    {
      og::PathGeometric solutionPath = og::SimpleSetup::getSolutionPath();  // copied so that it is non-const
      BOLT_BLUE(indent, true, "Bolt Finished - solution found in " << planTime_ << " seconds with "
                                                                        << solutionPath.getStateCount() << " states");

      // Show in Rviz
      visualize(indent);

      std::cout << "after visualize " << std::endl;

      // Error check for repeated states
      if (!checkRepeatedStates(solutionPath, indent))
        exit(-1);

      // Check optimality
      // if (!checkBoltPlannerOptimality())
      // exit(-1);

      // Stats
      stats_.numSolutionsFromRecall_++;

      // Make sure solution has at least 2 states
      if (solutionPath.getStateCount() < 2)
      {
        OMPL_INFORM("NOT saving to database because solution is less than 2 states long");
        stats_.numSolutionsTooShort_++;
      }
      else
      {
        // Queue the solution path for future insertion into experience database (post-processing)
        queuedSolutionPaths_.push_back(solutionPath);
      }
    }
    break;
    default:
      BOLT_ERROR(indent, "Unknown status type: " << lastStatus_);
      stats_.numSolutionsFailed_++;
  }
}

bool Bolt::checkRepeatedStates(const og::PathGeometric &path, std::size_t indent)
{
  for (std::size_t i = 1; i < path.getStateCount(); ++i)
  {
    if (si_->getStateSpace()->equalStates(path.getState(i - 1), path.getState(i)))
    {
      BOLT_ERROR(indent, "Duplicate state found between " << i - 1 << " and " << i << " on trajectory, out of "
                                                          << path.getStateCount());

      visual_->viz6()->state(path.getState(i), tools::ROBOT, tools::RED, 0);
      visual_->waitForUserFeedback("duplicate");

      return false;
    }
  }
  return true;
}

base::PlannerStatus Bolt::solve(double time)
{
  ob::PlannerTerminationCondition ptc = ob::timedPlannerTerminationCondition(time);
  return solve(ptc);
}

bool Bolt::setFilePath(const std::string &filePath)
{
  sparseGraph_->setFilePath(filePath + ".ompl");
  sparseGraph_->getSparseStorage()->setLoggingPath(filePath + ".logging");
  benchmarkFilePath_ = filePath + ".benchmark";
  return true;
}

bool Bolt::save()
{
  // setup(); // ensure the db has been loaded to the Experience DB
  return sparseGraph_->save();
}

bool Bolt::saveIfChanged()
{
  // setup(); // ensure the db has been loaded to the Experience DB
  return sparseGraph_->saveIfChanged();
}

void Bolt::printResultsInfo(std::ostream &out) const
{
  for (std::size_t i = 0; i < pdef_->getSolutionCount(); ++i)
  {
    out << "Solution " << i << "\t | Length: " << pdef_->getSolutions()[i].length_
        << "\t | Approximate: " << (pdef_->getSolutions()[i].approximate_ ? "true" : "false")
        << "\t | Planner: " << pdef_->getSolutions()[i].plannerName_ << std::endl;
  }
}

bool Bolt::load(std::size_t indent)
{
  // Load from file
  if (!sparseGraph_->isEmpty())
  {
    BOLT_WARN(indent, 1, "Database already loaded, vertices: " << sparseGraph_->getNumVertices()
                                                               << ", edges: " << sparseGraph_->getNumEdges()
                                                               << ", queryV: " << sparseGraph_->getNumQueryVertices());
    return false;
  }

  if (!sparseGraph_->load())  // load from file
  {
    return false;
  }

  return true;
}

void Bolt::print(std::ostream &out) const
{
  if (si_)
  {
    si_->printProperties(out);
    si_->printSettings(out);
  }
  if (boltPlanner_)
  {
    boltPlanner_->printProperties(out);
    boltPlanner_->printSettings(out);
  }
  if (pdef_)
    pdef_->print(out);
}

void Bolt::printLogs(std::ostream &out) const
{
  double vertPercent = sparseGraph_->getNumVertices() / double(sparseGraph_->getNumVertices()) * 100.0;
  double edgePercent = sparseGraph_->getNumEdges() / double(sparseGraph_->getNumEdges()) * 100.0;
  double solvedPercent = stats_.numSolutionsFromRecall_ / static_cast<double>(stats_.numProblems_) * 100.0;
  out << "Bolt Framework Logging Results" << std::endl;
  out << "  Solutions Attempted:           " << stats_.numProblems_ << std::endl;
  out << "    Solved:                      " << stats_.numSolutionsFromRecall_ << " (" << solvedPercent << "%)\n";
  out << "    Failed:                      " << stats_.numSolutionsFailed_ << std::endl;
  out << "    Timedout:                    " << stats_.numSolutionsTimedout_ << std::endl;
  out << "    Approximate:                 " << stats_.numSolutionsApproximate_ << std::endl;
  out << "  SparseGraph                       " << std::endl;
  out << "    Vertices:                    " << sparseGraph_->getNumVertices() << " (" << vertPercent << "%)"
      << std::endl;
  out << "    Edges:                       " << sparseGraph_->getNumEdges() << " (" << edgePercent << "%)" << std::endl;
  out << "    Disjoint Samples Added:      " << sparseGenerator_->getNumRandSamplesAdded() << std::endl;
  out << "    Sparse Delta:                " << sparseCriteria_->getSparseDelta() << std::endl;
  out << "  Average planning time:         " << stats_.getAveragePlanningTime() << " seconds" << std::endl;
  out << "  Average insertion time:        " << stats_.getAverageInsertionTime() << " seconds" << std::endl;
  out << std::endl;
}

}  // namespace bolt
}  // namespace tools
}  // namespace ompl
