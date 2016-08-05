/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014, University of Colorado, Boulder
 *  Copyright (c) 2012, Willow Garage, Inc.
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
 *   * Neither the name of the Willow Garage nor the names of its
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

/*
  Author: Dave Coleman <dave@dav.ee>
  Desc:   Visualize planning with OMPL in Rviz
*/

// ROS
#include <ros/ros.h>
#include <ros/package.h>  // for getting file path for loading images

// Display in Rviz tool
#include <ompl_visual_tools/ompl_visual_tools.h>
#include <ompl_visual_tools/costs/two_dimensional_validity_checker.h>

// OMPL
#include <ompl/tools/lightning/Lightning.h>
#include <ompl/geometric/planners/rrt/RRT.h>
#include <ompl/geometric/planners/rrt/TRRT.h>
#include <ompl/geometric/planners/rrt/RRTstar.h>
#include <ompl/geometric/planners/rrt/RRTConnect.h>
#include <ompl/base/PlannerTerminationCondition.h>

namespace ob = ompl::base;
namespace ot = ompl::tools;
namespace og = ompl::geometric;

namespace bolt_2d
{
static const std::string BASE_FRAME = "/world";

/**
 * \brief Lightning Planning Class
 */
class LightningDemo
{
private:
  // Save the experience setup until the program ends so that the planner data is not lost
  ot::LightningPtr lightning_;

  // Cost in 2D
  ompl::base::CostMap2DOptimizationObjectivePtr cost_map_;

  // The visual tools for interfacing with Rviz
  ompl_visual_tools::OmplVisualToolsPtr visual_tools_;

  // The number of dimensions - always 2 for images
  static const unsigned int DIMENSIONS = 2;

  // The state space we plan in
  ob::StateSpacePtr space_;

  // Remember what space we are working in
  ompl::base::SpaceInformationPtr si_;

  // Flag for determining amount of debug output to show
  bool verbose_;

  // Display graphics in Rviz
  bool use_visuals_;

public:
  /**
   * \brief Constructor
   */
  LightningDemo(bool verbose, bool use_visuals) : verbose_(verbose), use_visuals_(use_visuals)
  {
    // Construct the state space we are planning in
    space_.reset(new ob::RealVectorStateSpace(DIMENSIONS));

    // Define an experience setup class
    lightning_ = ot::LightningPtr(new ot::Lightning(space_));
    lightning_->load("two_dimension_world");
    si_ = lightning_->getSpaceInformation();

    // Load the tool for displaying in Rviz
    visual_tools_.reset(new ompl_visual_tools::OmplVisualTools(BASE_FRAME));
    visual_tools_->setSpaceInformation(si_);
    visual_tools_->setGlobalScale(100);

    // Set the planning from scratch planner
    lightning_->setPlanner(ob::PlannerPtr(new og::RRTstar(si_)));

    // Set the repair planner
    std::shared_ptr<og::RRTConnect> repair_planner(new og::RRTConnect(si_));
    // repair_planner->setGoalBias(0.2);
    lightning_->setRepairPlanner(ob::PlannerPtr(repair_planner));

    // Load the cost map
    cost_map_.reset(new ompl::base::CostMap2DOptimizationObjective(si_));
  }

  /**
   * \brief Deconstructor
   */
  ~LightningDemo()
  {
  }

  /**
   * \brief Clear all markers displayed in Rviz
   */
  void resetMarkers()
  {
    // Reset rviz markers cause we can
    visual_tools_->deleteAllMarkers();
  }

  /**
   * \brief Load cost map from file
   * \param file path
   * \param how much of the peaks of the mountains are considered obstacles
   */
  void loadCostMapImage(std::string image_path, double max_cost_threshold_percent = 0.4)
  {
    cost_map_->max_cost_threshold_percent_ = max_cost_threshold_percent;
    cost_map_->loadImage(image_path);

    // Set the bounds for the R^2
    ob::RealVectorBounds bounds(DIMENSIONS);
    bounds.setLow(0);                             // both dimensions start at 0
    bounds.setHigh(0, cost_map_->image_->x - 1);  // allow for non-square images
    bounds.setHigh(1, cost_map_->image_->y - 1);  // allow for non-square images
    space_->as<ob::RealVectorStateSpace>()->setBounds(bounds);
    space_->setup();

    // Pass cost to visualizer
    visual_tools_->setCostMap(cost_map_->cost_);
  }

  void publishCostMapImage()
  {
    if (use_visuals_)
      visual_tools_->publishCostMap(cost_map_->image_);
  }

  /**
   * \brief Solve a planning proble that we randomly make up
   * \param use_recall - use an expereince database or not
   * \param use_scratch - plan from scratch or not
   * \param run_id - which run this is
   * \param runs - how many total runs we will do
   * \return true on success
   */
  bool plan(bool use_recall, bool use_scratch, const int& run_id, const int& runs)
  {
    // Start and Goal State ---------------------------------------------
    ob::PlannerStatus solved;

    // Clear all planning data. This only includes data generated by motion plan computation.
    // Planner settings, start & goal states are not affected.
    if (run_id)  // skip first run
      lightning_->clear();

    // Set state validity checking for this space
    lightning_->setStateValidityChecker(ob::StateValidityCheckerPtr(
        new ob::TwoDimensionalValidityChecker(si_, cost_map_->cost_, cost_map_->max_cost_threshold_)));

    // Setup the optimization objective to use the 2d cost map
    lightning_->setOptimizationObjective(cost_map_);

    // Create the termination condition
    double seconds = 1;
    ob::PlannerTerminationCondition ptc = ob::timedPlannerTerminationCondition(seconds, 0.1);

    // Create start and goal space
    ob::ScopedState<> start(space_);
    ob::ScopedState<> goal(space_);
    chooseStartGoal(start, goal);

    // Show start and goal
    if (use_visuals_)
    {
      visual_tools_->publishState(start, moveit_visual_tools::GREEN, moveit_visual_tools::LARGE, "plan_start_goal");
      visual_tools_->publishState(goal, moveit_visual_tools::RED, moveit_visual_tools::LARGE, "plan_start_goal");
    }

    // set the start and goal states
    lightning_->setStartAndGoalStates(start, goal);

    // Setup -----------------------------------------------------------

    // Auto setup parameters (optional actually)
    lightning_->setup();
    lightning_->enablePlanningFromRecall(use_recall);
    lightning_->enablePlanningFromScratch(use_scratch);

    // ROS_ERROR_STREAM_NAMED("temp","out of curiosity: coll check resolution: "
    //   << si_->getStateValidityCheckingResolution());

    // The interval in which obstacles are checked for between states
    // seems that it default to 0.01 but doesn't do a good job at that level
    si_->setStateValidityCheckingResolution(0.005);

    // Debug - this call is optional, but we put it in to get more output information
    // lightning_->print();

    // Solve -----------------------------------------------------------

    // attempt to solve the problem within x seconds of planning time
    solved = lightning_->solve(ptc);

    geometry_msgs::Pose text_pose;
    text_pose.position.x = cost_map_->cost_->size1() / 2.0;
    text_pose.position.y = cost_map_->cost_->size1() / -20.0;
    text_pose.position.z = cost_map_->cost_->size1() / 10.0;

    if (solved)
    {
      if (!lightning_->haveExactSolutionPath())
      {
        ROS_WARN_STREAM_NAMED("plan", "APPROXIMATE solution found from planner "
                                          << lightning_->getSolutionPlannerName());
        if (use_visuals_)
          visual_tools_->publishText("APPROXIMATE solution found from planner " + lightning_->getSolutionPlannerName(),
                                     text_pose);
      }
      else
      {
        ROS_DEBUG_STREAM_NAMED("plan", "Exact solution found from planner " << lightning_->getSolutionPlannerName());
        if (use_visuals_)
          visual_tools_->publishText("Exact solution found from planner " + lightning_->getSolutionPlannerName(),
                                     text_pose);

        // Display states on available solutions
        lightning_->printResultsInfo();
      }

      if (use_visuals_)
      {
        if (runs == 1)
        {
          // display all the aspects of the solution
          publishPlannerData(false);
        }
        else
        {
          // only display the paths
          publishPlannerData(false);
        }
      }
    }
    else
    {
      ROS_ERROR("No Solution Found");
      if (use_visuals_)
        visual_tools_->publishText("No Solution Found", text_pose);
    }

    return solved;
  }

  bool save()
  {
    return lightning_->saveIfChanged();
  }

  void chooseStartGoal(ob::ScopedState<>& start, ob::ScopedState<>& goal)
  {
    if (true)  // choose completely random state
    {
      findValidState(start);
      findValidState(goal);
    }
    else if (false)  // Manually set the start location
    {
      // Plan from scrach location
      start[0] = 5;
      start[1] = 5;
      goal[0] = 5;
      goal[1] = 45;
      // Recall location
      start[0] = 45;
      start[1] = 5;
      goal[0] = 45;
      goal[1] = 45;
    }
    else  // Randomly sample around two states
    {
      ROS_INFO_STREAM_NAMED("temp", "Sampling start and goal around two center points");

      ob::ScopedState<> start_area(space_);
      start_area[0] = 100;
      start_area[1] = 80;

      ob::ScopedState<> goal_area(space_);
      goal_area[0] = 330;
      goal_area[1] = 350;

      // Check these hard coded values against varying image sizes
      if (!space_->satisfiesBounds(start_area.get()) || !space_->satisfiesBounds(goal_area.get()))
      {
        ROS_ERROR_STREAM_NAMED("chooseStartGoal:", "State does not satisfy bounds");
        exit(-1);
        return;
      }

      // Choose the distance to sample around
      double maxExtent = si_->getMaximumExtent();
      double distance = maxExtent * 0.1;
      ROS_INFO_STREAM_NAMED("temp", "Distance is " << distance << " from max extent " << maxExtent);

      // Publish the same points
      if (true)
      {
        findValidState(start.get(), start_area.get(), distance);
        findValidState(goal.get(), goal_area.get(), distance);
      }
      else
      {
        //                start[0] = 277.33;  start[1] = 168.491;
        //                 goal[0] = 843.296;  goal[1] = 854.184;
      }

      // Show the sample regions
      if (use_visuals_ && false)
      {
        visual_tools_->publishSampleRegion(start_area, distance);
        visual_tools_->publishSampleRegion(goal_area, distance);
      }
    }

    // Print the start and goal
    // ROS_DEBUG_STREAM_NAMED("chooseStartGoal","Start: " << start);
    // ROS_DEBUG_STREAM_NAMED("chooseStartGoal","Goal: " << goal);
  }

  void findValidState(ob::ScopedState<>& state)
  {
    std::size_t rounds = 0;
    while (rounds < 100)
    {
      state.random();

      // Check if the sampled points are valid
      if (si_->isValid(state.get()))
      {
        return;
      }
      ++rounds;
    }
    ROS_ERROR_STREAM_NAMED("findValidState", "Unable to find valid start/goal state after " << rounds << " rounds");
  }

  void findValidState(ob::State* state, const ob::State* near, const double distance)
  {
    // Create sampler
    ob::StateSamplerPtr sampler = si_->allocStateSampler();

    while (true)
    {
      sampler->sampleUniformNear(state, near, distance);  // samples (near + distance, near - distance)

      // Check if the sampled points are valid
      if (si_->isValid(state))
      {
        return;
      }
      else
        ROS_INFO_STREAM_NAMED("temp", "Searching for valid start/goal state");
    }
  }

  /**
   * \brief Show the planner data in Rviz
   * \param just_path - if true, do not display the search tree/graph or the samples
   */
  void publishPlannerData(bool just_path)
  {
    // Final Solution  ----------------------------------------------------------------

    if (true)
    {
      // Show basic solution
      // visual_tools_->publishPath( lightning_->getSolutionPath(), RED);

      // Simplify solution
      // lightning_->simplifySolution();

      // Interpolate solution
      lightning_->getSolutionPath().interpolate();
      visual_tools_->publishPath(lightning_->getSolutionPath(), moveit_visual_tools::GREEN, 1.0, "final_solution");
    }

    // Print the states to screen -----------------------------------------------------
    if (false)
    {
      ROS_DEBUG_STREAM_NAMED("temp", "showing path");
      lightning_->getSolutionPath().print(std::cout);
    }

    // Planning From Scratch -----------------------------------------------------------
    if (true)
    {
      // Get information about the exploration data structure the motion planner used in planning from scratch
      const ob::PlannerDataPtr planner_data(new ob::PlannerData(si_));
      lightning_->getPlannerData(*planner_data);

      // Optionally display the search tree/graph or the samples
      if (!just_path)
      {
        // Visualize the explored space
        visual_tools_->publishGraph(planner_data, moveit_visual_tools::ORANGE, 0.2, "plan_from_scratch");

        // Visualize the sample locations
        // visual_tools_->publishSamples(planner_data);
      }
    }

    // Retrieve Planner - show filtered paths ------------------------------------------------
    if (true)
    {
      const std::vector<ob::PlannerDataPtr>& recallPlannerDatas =
          lightning_->getRetrieveRepairPlanner().getLastRecalledNearestPaths();
      const std::size_t& chosenID = lightning_->getRetrieveRepairPlanner().getLastRecalledNearestPathChosen();
      // lightning_->getRetrieveRepairPlanner().getRecalledPlannerDatas( recallPlannerDatas, chosenID);

      for (std::size_t i = 0; i < recallPlannerDatas.size(); ++i)
      {
        // ROS_DEBUG_STREAM_NAMED("temp","Displaying planner data " << i << " and chosen ID was " << chosenID);

        // Make the chosen path a different color and thickness
        moveit_visual_tools::rviz_colors color = moveit_visual_tools::BLACK;
        double thickness = 0.2;
        std::string ns = "repair_filtered_paths";
        if (chosenID == i)
        {
          color = moveit_visual_tools::RED;
          thickness = 0.6;
          ns = "repair_chosen_path";
        }

        visual_tools_->publishPath(recallPlannerDatas[i], color, thickness, ns);
      }
    }

    // Repair Planner - search trees of repair solvers ------------------------------------------------
    if (true)
    {
      std::vector<ob::PlannerDataPtr> repairPlannerDatas;
      lightning_->getRetrieveRepairPlanner().getRepairPlannerDatas(repairPlannerDatas);
      for (std::size_t i = 0; i < repairPlannerDatas.size(); ++i)
      {
        // ROS_DEBUG_STREAM_NAMED("temp","Displaying planner data " << i);
        visual_tools_->publishGraph(repairPlannerDatas[i], moveit_visual_tools::RAND, 0.2,
                                    std::string("repair_tree_" + boost::lexical_cast<std::string>(i)));

        visual_tools_->publishStartGoalSpheres(repairPlannerDatas[i],
                                               std::string("repair_tree_" + boost::lexical_cast<std::string>(i)));
      }
    }
  }

  /**
   * \brief Dump the entire database contents to Rviz
   */
  void publishDatabase()
  {
    // Display all of the saved paths
    std::vector<ob::PlannerDataPtr> paths;
    lightning_->getAllPlannerDatas(paths);

    ROS_INFO_STREAM_NAMED("experience_database_test", "Number of paths: " << paths.size());

    // Show all paths
    for (std::size_t i = 0; i < paths.size(); ++i)
    {
      visual_tools_->publishPath(paths[i], moveit_visual_tools::RAND);
    }
  }

  /**
   * \brief Score the paths within the database for similarity
   */
  void scoreDatabase()
  {
    // Display all of the saved paths
    std::vector<ob::PlannerDataPtr> paths;
    lightning_->getAllPlannerDatas(paths);

    ROS_INFO_STREAM_NAMED("experience_database_test", "Number of paths: " << paths.size());

    // Score all paths two at a time
    if (false)
    {
      for (std::size_t i = 0; i < paths.size(); ++i)
      {
        // compare this path against all other unseen paths
        for (std::size_t j = i; j < paths.size(); ++j)
        {
          // detect if needed to exit early
          if (!ros::ok())
          {
            i = paths.size();
            break;
          }

          // Create paths
          og::PathGeometric path1(si_);
          visual_tools_->convertPlannerData(paths[i], path1);
          og::PathGeometric path2(si_);
          visual_tools_->convertPlannerData(paths[j], path2);

          // Reverse path2 if necessary so that it matches path1 better
          lightning_->reversePathIfNecessary(path1, path2);

          double score = lightning_->getDynamicTimeWarp()->getPathsScoreNonConst(path1, path2);

          ROS_DEBUG_STREAM_NAMED("temp", "Score is " << score);
          visual_tools_->publishText("Score " + boost::lexical_cast<std::string>(score));

          visual_tools_->publishPath(path1, moveit_visual_tools::GREEN, 0.8);
          visual_tools_->publishSamples(path1);
          visual_tools_->publishPath(path2, moveit_visual_tools::RAND);
          visual_tools_->publishSamples(path2);

          ros::Duration(1).sleep();
          resetMarkers();
        }
      }
    }
    // Test threshold methods for scoring paths
    else if (true)
    {
      for (std::size_t i = 0; i < paths.size(); ++i)
      {
        i = ompl_visual_tools::OmplVisualTools::dRand(0, paths.size());

        og::PathGeometric path1(si_);
        visual_tools_->convertPlannerData(paths[i], path1);
        bool found = false;

        // compare this path against all other unseen paths
        for (std::size_t j = 0; j < paths.size(); ++j)
        {
          // Don't match to self
          if (j == i)
            continue;

          og::PathGeometric path2(si_);
          visual_tools_->convertPlannerData(paths[j], path2);

          // detect if needed to exit early
          if (!ros::ok())
          {
            i = paths.size();
            break;
          }
          double score = lightning_->getDynamicTimeWarp()->getPathsScoreNonConst(path1, path2);

          // Display both paths
          double maxToDisplay = 20;
          if (score < maxToDisplay / 3)
          {
            visual_tools_->publishPath(path2, moveit_visual_tools::GREEN);
            found = true;
          }
          else if (score < maxToDisplay / 3 * 2)
          {
            visual_tools_->publishPath(path2, moveit_visual_tools::YELLOW);
            found = true;
          }
          else if (score < maxToDisplay)
          {
            visual_tools_->publishPath(path2, moveit_visual_tools::RED);
            found = true;
          }

          // ROS_DEBUG_STREAM_NAMED("temp","Score is " << score);
          // visual_tools_->publishText("Score " + boost::lexical_cast<std::string>(score));
        }
        if (found)
        {
          visual_tools_->publishPath(path1, moveit_visual_tools::GREEN, 0.8);
          ros::Duration(2).sleep();
        }
        else
        {
          OMPL_DEBUG("Skipped path because no other paths were similar enough");
        }
        resetMarkers();
      }
    }
    else
    {
      for (std::size_t i = 0; i < 50; i += 5)
      {
        // Create first line
        og::PathGeometric path1(si_);
        // Create dummy states
        ob::ScopedState<> start(space_);
        ob::ScopedState<> goal(space_);
        start[0] = 5;
        start[1] = 5;
        goal[0] = 5;
        goal[1] = 45;
        path1.append(start.get());
        path1.append(goal.get());

        // Create second line that is slightly moved
        og::PathGeometric path2(path1);
        path2.getState(0)->as<ompl::base::RealVectorStateSpace::StateType>()->values[0] += i;
        path2.getState(1)->as<ompl::base::RealVectorStateSpace::StateType>()->values[0] += i;
        path2.getState(1)->as<ompl::base::RealVectorStateSpace::StateType>()->values[1] += i;
        path1.print(std::cout);
        path2.print(std::cout);
        path2.interpolate();
        path2.print(std::cout);

        // Score
        double score = lightning_->getDynamicTimeWarp()->getPathsScoreNonConst(path1, path2);
        ROS_DEBUG_STREAM_NAMED("temp", "Score is " << score);
        visual_tools_->publishText("Score " + boost::lexical_cast<std::string>(score));

        // Display
        visual_tools_->publishPath(path1, moveit_visual_tools::GREEN, 0.8);
        visual_tools_->publishSamples(path1);
        visual_tools_->publishPath(path2, moveit_visual_tools::RAND);
        visual_tools_->publishSamples(path2);

        ros::Duration(4.0).sleep();
        resetMarkers();

        if (!ros::ok())
          break;
      }
    }
  }

  /** \brief Allow access to lightning framework */
  ot::LightningPtr getLightning()
  {
    return lightning_;
  }

};  // end of class

}  // namespace

// *********************************************************************************************************
// Main
// *********************************************************************************************************
int main(int argc, char** argv)
{
  ros::init(argc, argv, "bolt_2d");
  ROS_INFO("OMPL Visual Tools with Lightning Framework ----------------------------------------- ");

  // Seed random
  srand(time(NULL));

  // Allow the action server to recieve and send ros messages
  ros::AsyncSpinner spinner(1);
  spinner.start();

  // Default argument values
  bool verbose = false;
  bool display_database = false;
  bool score_database = false;
  bool use_recall = true;
  bool use_scratch = true;
  bool use_visuals = true;
  std::string image_path;
  int runs = 1;

  if (argc > 1)
  {
    for (std::size_t i = 0; i < argc; ++i)
    {
      // Help mode
      if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
      {
        ROS_INFO_STREAM_NAMED("main", "Usage: ompl_rviz_lightning"
                                          << " --verbose --noRecall --noScratch --noVisuals --image [image_file] "
                                             "--runs [num plans] "
                                          << "--displayDatabase --scoreDatabase -h");
        return 0;
      }

      // Show all available plans
      if (strcmp(argv[i], "--displayDatabase") == 0)
      {
        ROS_INFO_STREAM_NAMED("main", "Visualizing entire database");
        display_database = true;
      }

      // Show all available plans
      if (strcmp(argv[i], "--scoreDatabase") == 0)
      {
        ROS_INFO_STREAM_NAMED("main", "Visualizing entire database");
        score_database = true;
      }

      // Check for verbose flag
      if (strcmp(argv[i], "--verbose") == 0)
      {
        ROS_INFO_STREAM_NAMED("main", "Running in VERBOSE mode (slower)");
        verbose = true;
      }

      // Check if we should ignore the recall mechanism
      if (strcmp(argv[i], "--noRecall") == 0)
      {
        ROS_INFO_STREAM_NAMED("main", "NOT using recall for planning");
        use_recall = false;
      }

      // Check if we should ignore the plan from scratch mechanism
      if (strcmp(argv[i], "--noScratch") == 0)
      {
        ROS_INFO_STREAM_NAMED("main", "NOT using planning from scratch");
        use_scratch = false;
      }

      // Check if we should publish markers
      if (strcmp(argv[i], "--noVisuals") == 0)
      {
        ROS_INFO_STREAM_NAMED("main", "NOT displaying graphics");
        use_visuals = false;
      }

      // Check if user has passed in an image to read
      if (strcmp(argv[i], "--image") == 0)
      {
        i++;  // get next arg
        image_path = argv[i];
      }

      // Check if user has passed in the number of runs to perform
      if (strcmp(argv[i], "--runs") == 0)
      {
        i++;  // get next arg
        runs = atoi(argv[i]);
      }
    }
  }

  // Provide image if necessary
  if (image_path.empty())  // use default image
  {
    // Get image path based on package name
    image_path = ros::package::getPath("ompl_visual_tools");
    if (image_path.empty())
    {
      ROS_ERROR("Unable to get OMPL Visual Tools package path ");
      return false;
    }

    // Choose random image
    int rand_num = ompl_visual_tools::OmplVisualTools::dRand(0, 2);
    ROS_ERROR_STREAM_NAMED("temp", "random num is " << rand_num);
    switch (rand_num)
    {
      case 0:
        image_path.append("/resources/wilbur_medium/wilbur_medium1.ppm");
        break;
      case 1:
        image_path.append("/resources/wilbur_medium/wilbur_medium2.ppm");
        break;
      default:
        ROS_ERROR_STREAM_NAMED("main", "Random has no case " << rand_num);
        break;
    }
  }

  // Create the planner
  bolt_2d::LightningDemo demo(verbose, use_visuals);
  ROS_DEBUG_STREAM_NAMED("main", "Loaded " << demo.getLightning()->getExperiencesCount() << " experiences from file");

  // Clear Rviz
  demo.resetMarkers();

  // Load an image
  ROS_INFO_STREAM_NAMED("main", "Loading image " << image_path);
  demo.loadCostMapImage(image_path, 0.4);
  demo.publishCostMapImage();

  // Display Contents of database if desires
  if (display_database)
  {
    if (!use_visuals)
    {
      ROS_ERROR_STREAM_NAMED("main", "Visuals disabled, cannot display database.");
      return 0;
    }
    demo.publishDatabase();
    return 0;
  }

  // Score the database
  if (score_database)
  {
    if (!use_visuals)
    {
      ROS_ERROR_STREAM_NAMED("main", "Visuals disabled, cannot display database.");
      return 0;
    }
    demo.scoreDatabase();
    return 0;
  }

  // Run the planner the desired number of times
  for (std::size_t i = 0; i < runs; ++i)
  {
    // Check if user wants to shutdown
    if (!ros::ok())
    {
      ROS_WARN_STREAM_NAMED("plan", "Terminating early");
      break;
    }
    ROS_INFO_STREAM_NAMED("plan", "Planning " << i + 1 << " out of " << runs << " -----------------------------------"
                                                                                "-");

    // Refresh visuals
    if (use_visuals && i > 0)
    {
      demo.publishCostMapImage();
      // demo.loadCostMapImage( image_path, 0 ); //OmplVisualTools::fRand(0.2,0.5) );
      // Display before planning
      ros::spinOnce();
    }

    // Run the planner
    demo.plan(use_recall, use_scratch, i, runs);

    // Create a pause if we are doing more runs and this is not the last run
    if (runs > 1 && i < runs - 1 && use_visuals)
    {
      // Let publisher publish
      ros::spinOnce();
      ros::Duration(1.0).sleep();
    }

    // Reset marker if this is not our last run
    if (i < runs - 1 && use_visuals)
      demo.resetMarkers();
  }

  // Save the database at the end
  if (!demo.save())
    ROS_ERROR("Unable to save experience database");

  // Wait to let anything still being published finish
  ros::Duration(0.1).sleep();

  ROS_INFO_STREAM("Shutting down.");

  return 0;
}
