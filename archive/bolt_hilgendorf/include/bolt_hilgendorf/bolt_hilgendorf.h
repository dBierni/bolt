/*********************************************************************
 * Software License Agreement (BSD License)
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
 *   * Neither the name of PickNik LLC nor the names of its
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

/* Author: Dave Coleman
   Desc:   Demo dual arm manipulation
*/

#ifndef BOLT_HILGENDORF_BOLT_HILGENDORF_H
#define BOLT_HILGENDORF_BOLT_HILGENDORF_H

// ROS
#include <ros/ros.h>

// MoveIt
#include <moveit/planning_interface/planning_interface.h>
#include <moveit/robot_state/conversions.h>
#include <moveit/kinematic_constraints/utils.h>
#include <bolt_hilgendorf/moveit_base.h>

// bolt_moveit
#include <bolt_moveit/model_based_state_space.h>
#include <bolt_moveit/remote_control.h>

// OMPL
#include <ompl/tools/thunder/Thunder.h>
#include <bolt_core/Bolt.h>
#include <bolt_hilgendorf/moveit_viz_window.h>

// this package
//#include <bolt_hilgendorf/process_mem_usage.h>
#include <bolt_hilgendorf/state_validity_checker.h>
#include <bolt_hilgendorf/cart_path_planner.h>
#include <moveit_visual_tools/imarker_robot_state.h>

namespace mo = bolt_moveit;

namespace bolt_hilgendorf
{
class BoltHilgendorf : public bolt_hilgendorf::MoveItBase
{
public:
  /** \brief Constructor */
  BoltHilgendorf(const std::string& hostname, const std::string& package_path);

  /** \brief Destructor */
  ~BoltHilgendorf();

  /** \brief Load the basic planning context components */
  bool loadOMPL();

  /** \brief Generate states for testing */
  void testConnectionToGraphOfRandStates();

  void loadCollisionChecker();

  bool loadData();

  void run();

  bool runProblems();

  bool plan();

  /** \brief Create multiple dummy cartesian paths */
  bool generateCartGraph();

  bool checkOMPLPathSolution(og::PathGeometric& path);
  bool checkMoveItPathSolution(robot_trajectory::RobotTrajectoryPtr traj);

  bool getRandomState(moveit::core::RobotStatePtr& robot_state);

  /**
   * \brief Clear all markers displayed in Rviz
   */
  void deleteAllMarkers(bool clearDatabase = true);

  void loadVisualTools();

  void visualizeStartGoal();

  void visualizeRawTrajectory(og::PathGeometric& path);

  void displayWaitingState(bool waiting);

  void waitForNextStep(const std::string& msg);

  void testMotionValidator();

  // --------------------------------------------------------

  // A shared node handle
  ros::NodeHandle nh_;

  // The short name of this class
  std::string name_ = "bolt_hilgendorf";

  // Recieve input from Rviz
  bolt_moveit::RemoteControl remote_control_;

  // File location of this package
  std::string package_path_;

  // Save the experience setup until the program ends so that the planner data is not lost
  ompl::tools::ExperienceSetupPtr experience_setup_;
  ompl::tools::bolt::BoltPtr bolt_;

  // Configuration space
  bolt_moveit::ModelBasedStateSpacePtr space_;
  ompl::base::SpaceInformationPtr si_;

  // The visual tools for interfacing with Rviz
  std::vector<bolt_hilgendorf::MoveItVizWindowPtr> vizs_;
  bolt_hilgendorf::MoveItVizWindowPtr viz1_;
  bolt_hilgendorf::MoveItVizWindowPtr viz2_;
  bolt_hilgendorf::MoveItVizWindowPtr viz3_;
  bolt_hilgendorf::MoveItVizWindowPtr viz4_;
  bolt_hilgendorf::MoveItVizWindowPtr viz5_;
  bolt_hilgendorf::MoveItVizWindowPtr viz6_;
  moveit_visual_tools::MoveItVisualToolsPtr visual_moveit_start_;  // Clone of ompl1
  moveit_visual_tools::MoveItVisualToolsPtr visual_moveit_goal_;   // Clone of ompl2

  // Robot states
  moveit::core::RobotStatePtr moveit_start_;
  moveit::core::RobotStatePtr moveit_goal_;
  ob::State* ompl_start_;
  ob::State* ompl_goal_;

  // Planning groups
  std::string planning_group_name_;
  std::string ee_tip_link_;
  moveit::core::JointModelGroup* jmg_;
  moveit::core::LinkModel* ee_link_;

  // Modes
  bool run_problems_;
  bool create_spars_;
  bool load_spars_;
  bool continue_spars_;
  bool eliminate_dense_disjoint_sets_;
  bool check_valid_vertices_;
  bool display_disjoint_sets_;
  bool benchmark_performance_;
  bool post_processing_;
  int post_processing_interval_;

  // Type of planner
  std::string experience_planner_;
  bool is_bolt_ = false;
  bool is_thunder_ = false;

  // Operation settings
  std::size_t planning_runs_;
  int problem_type_;
  bool use_task_planning_;
  bool headless_;
  bool auto_run_;
  bool track_memory_consumption_ = false;
  bool use_logging_ = false;
  bool collision_checking_enabled_ = true;

  // Verbosity levels
  bool debug_print_trajectory_;

  // Display preferences
  bool visualize_display_database_;
  bool visualize_interpolated_traj_;
  bool visualize_grid_generation_;
  bool visualize_start_goal_states_;
  bool visualize_cart_neighbors_;
  bool visualize_cart_path_;
  bool visualize_wait_between_plans_;
  double visualize_time_between_plans_;
  bool visualize_database_every_plan_;

  // Average planning time
  double total_duration_ = 0;
  std::size_t total_runs_ = 0;
  std::size_t total_failures_ = 0;

  // Create constrained paths
  CartPathPlannerPtr cart_path_planner_;

  // Interactive markers
  moveit_visual_tools::IMarkerRobotStatePtr imarker_start_;
  moveit_visual_tools::IMarkerRobotStatePtr imarker_goal_;

  // Validity checker
  bolt_moveit::StateValidityChecker* validity_checker_;
};  // end class

// Create boost pointers for this class
typedef boost::shared_ptr<BoltHilgendorf> BoltHilgendorfPtr;
typedef boost::shared_ptr<const BoltHilgendorf> BoltHilgendorfConstPtr;

}  // namespace bolt_hilgendorf

#endif  // BOLT_HILGENDORF_BOLT_HILGENDORF_H