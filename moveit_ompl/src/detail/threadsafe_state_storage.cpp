/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
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

/* Author: Ioan Sucan */

#include <moveit_ompl/detail/threadsafe_state_storage.h>

moveit_ompl::TSStateStorage::TSStateStorage(const robot_model::RobotModelPtr &robot_model) : start_state_(robot_model)
{
  start_state_.setToDefaultValues();
}

moveit_ompl::TSStateStorage::TSStateStorage(const robot_state::RobotState &start_state) : start_state_(start_state)
{
}

moveit_ompl::TSStateStorage::~TSStateStorage()
{
  for (std::map<boost::thread::id, robot_state::RobotState *>::iterator it = thread_states_.begin();
       it != thread_states_.end(); ++it)
    delete it->second;
}

robot_state::RobotState *moveit_ompl::TSStateStorage::getStateStorage() const
{
  robot_state::RobotState *st = NULL;
  boost::mutex::scoped_lock slock(lock_);  /// \todo use Thread Local Storage?
  std::map<boost::thread::id, robot_state::RobotState *>::const_iterator it =
      thread_states_.find(boost::this_thread::get_id());
  if (it == thread_states_.end())
  {
    st = new robot_state::RobotState(start_state_);
    thread_states_[boost::this_thread::get_id()] = st;
  }
  else
    st = it->second;
  return st;
}
