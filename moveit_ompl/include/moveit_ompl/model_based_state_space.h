/*********************************************************************
 * Software License Agreement (BSD License)
 *
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
 *   * Neither the name of Willow Garage nor the names of its
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

/* Author: Dave Coleman, Ioan Sucan */

#ifndef MOVEIT_OMPL_MODEL_BASED_STATE_SPACE_
#define MOVEIT_OMPL_MODEL_BASED_STATE_SPACE_

// OMPL
#include <ompl/base/StateSpace.h>
#include <ompl/geometric/PathGeometric.h>

// MoveIt
#include <moveit/robot_model/robot_model.h>
#include <moveit/robot_state/robot_state.h>
#include <moveit/kinematic_constraints/kinematic_constraint.h>
#include <moveit/constraint_samplers/constraint_sampler.h>
#include <moveit_visual_tools/moveit_visual_tools.h>

namespace moveit_ompl
{
typedef std::function<bool(const ompl::base::State *from, const ompl::base::State *to, const double t,
                           ompl::base::State *state)> InterpolationFunction;
typedef std::function<double(const ompl::base::State *state1, const ompl::base::State *state2)> DistanceFunction;

struct ModelBasedStateSpaceSpecification
{
  ModelBasedStateSpaceSpecification(
      const robot_model::RobotModelConstPtr &robot_model, const robot_model::JointModelGroup *jmg,
      moveit_visual_tools::MoveItVisualToolsPtr visual_tools = moveit_visual_tools::MoveItVisualToolsPtr())
    : robot_model_(robot_model), joint_model_group_(jmg), visual_tools_(visual_tools)
  {
  }

  ModelBasedStateSpaceSpecification(
      const robot_model::RobotModelConstPtr &robot_model, const std::string &group_name,
      moveit_visual_tools::MoveItVisualToolsPtr visual_tools = moveit_visual_tools::MoveItVisualToolsPtr())
    : robot_model_(robot_model)
    , joint_model_group_(robot_model_->getJointModelGroup(group_name))
    , visual_tools_(visual_tools)
  {
    if (!joint_model_group_)
      throw std::runtime_error("Group '" + group_name + "'  was not found");
  }

  robot_model::RobotModelConstPtr robot_model_;
  const robot_model::JointModelGroup *joint_model_group_;
  robot_model::JointBoundsVector joint_bounds_;

  // For visualizing things in rviz
  moveit_visual_tools::MoveItVisualToolsPtr visual_tools_;
};

class ModelBasedStateSpace : public ompl::base::StateSpace
{
public:
  class StateType : public ompl::base::State
  {
  public:
    StateType() : ompl::base::State(), values(NULL)
    {
    }

    double *values;
  };

  ModelBasedStateSpace(const ModelBasedStateSpaceSpecification &spec);
  virtual ~ModelBasedStateSpace();

  void setInterpolationFunction(const InterpolationFunction &fun)
  {
    interpolation_function_ = fun;
  }

  void setDistanceFunction(const DistanceFunction &fun)
  {
    distance_function_ = fun;
  }

  virtual ompl::base::State *allocState() const;
  virtual void freeState(ompl::base::State *state) const;

  /** \brief Allocate an array of states */
  virtual void allocStates(std::size_t numStates, ompl::base::State *) const
  {
    ROS_ERROR_STREAM("allocStates() Not implemented!");
    exit(-1);
  }

  /** \brief Free an array of states */
  virtual void freeStates(ompl::base::State *states) const
  {
    ROS_ERROR_STREAM("freeStates() Not implemented!");
    exit(-1);
  }

  virtual void copyFromReals(ompl::base::State *destination, const std::vector<double> &reals) const;
  virtual unsigned int getDimension() const;
  virtual void enforceBounds(ompl::base::State *state) const;
  virtual bool satisfiesBounds(const ompl::base::State *state) const;

  virtual void copyState(ompl::base::State *destination, const ompl::base::State *source) const;
  virtual void interpolate(const ompl::base::State *from, const ompl::base::State *to, const double t,
                           ompl::base::State *state) const;
  virtual double distance(const ompl::base::State *state1, const ompl::base::State *state2) const;
  virtual bool equalStates(const ompl::base::State *state1, const ompl::base::State *state2) const;
  virtual double getMaximumExtent() const;
  virtual double getMeasure() const;

  virtual unsigned int getSerializationLength() const;
  virtual void serialize(void *serialization, const ompl::base::State *state) const;
  virtual void deserialize(ompl::base::State *state, const void *serialization) const;
  virtual double *getValueAddressAtIndex(ompl::base::State *state, const unsigned int index) const;

  virtual ompl::base::StateSamplerPtr allocDefaultStateSampler() const;

  const robot_model::RobotModelConstPtr &getRobotModel() const
  {
    return spec_.robot_model_;
  }

  const robot_model::JointModelGroup *getJointModelGroup() const
  {
    return spec_.joint_model_group_;
  }

  const std::string &getJointModelGroupName() const
  {
    return getJointModelGroup()->getName();
  }

  const ModelBasedStateSpaceSpecification &getSpecification() const
  {
    return spec_;
  }

  virtual void printState(const ompl::base::State *state, std::ostream &out = std::cout) const;
  virtual void printSettings(std::ostream &out = std::cout) const;

  /// Set the planning volume for the possible SE2 and/or SE3 components of the state space
  virtual void setPlanningVolume(double minX, double maxX, double minY, double maxY, double minZ, double maxZ);

  const robot_model::JointBoundsVector &getJointsBounds() const
  {
    return spec_.joint_bounds_;
  }

  /// Copy the data from an OMPL state to a set of joint states.
  // The joint states \b must be specified in the same order as the joint models in the constructor
  virtual void copyToRobotState(robot_state::RobotState &rstate, const ompl::base::State *state) const;

  /// Copy the data from a set of joint states to an OMPL state.
  //  The joint states \b must be specified in the same order as the joint models in the constructor
  virtual void copyToOMPLState(ompl::base::State *state, const robot_state::RobotState &rstate) const;

  /**
   * \brief Copy a single joint's values (which might have multiple variables) from a MoveIt! robot_state to an OMPL
   * state.
   * \param state - output OMPL state with single joint modified
   * \param robot_state - input MoveIt! state to get the joint value from
   * \param joint_model - the joint to copy values of
   * \param ompl_state_joint_index - the index of the joint in the ompl state (passed in for efficiency, you should
   * cache this index)
   *        e.g. ompl_state_joint_index = joint_model_group_->getVariableGroupIndex("virtual_joint");
   */
  virtual void copyJointToOMPLState(ompl::base::State *state, const robot_state::RobotState &robot_state,
                                    const moveit::core::JointModel *joint_model, int ompl_state_joint_index) const;

  /** \brief Convert from ompl path to moveit path */
  bool convertPathToRobotState(const ompl::geometric::PathGeometric& path, const robot_model::JointModelGroup* jmg,
                           robot_trajectory::RobotTrajectoryPtr& traj, double speed);


protected:
  ModelBasedStateSpaceSpecification spec_;
  std::vector<robot_model::JointModel::Bounds> joint_bounds_storage_;
  std::vector<const robot_model::JointModel *> joint_model_vector_;
  unsigned int variable_count_;
  size_t state_values_size_;

  InterpolationFunction interpolation_function_;
  DistanceFunction distance_function_;
};

typedef std::shared_ptr<ModelBasedStateSpace> ModelBasedStateSpacePtr;
}

#endif
