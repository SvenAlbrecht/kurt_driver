/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2008, Willow Garage, Inc.
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

//#include <cstdio>
//#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <ros/ros.h>
#include <sensor_msgs/JointState.h>
#include <nav_msgs/Odometry.h>

// Services
#include "laser_assembler/AssembleScans.h"

// Messages
#include "sensor_msgs/PointCloud.h"

/***
 * This a simple test app that requests a point cloud from the
 * point_cloud_assembler every 4 seconds, and then publishes the
 * resulting data
 */
namespace laser_assembler
{

  class PeriodicSnapshotter
  {

    public:

      PeriodicSnapshotter()
      {
        // Create a publisher for the clouds that we assemble
        pub_ = n_.advertise<sensor_msgs::PointCloud> ("assembled_cloud", 1);

        subStates_ = n_.subscribe("joint_states", 1000, &PeriodicSnapshotter::rotCallback, this);
        subOdom_ = n_.subscribe("odom", 1000, &PeriodicSnapshotter::odomCallback, this);

        // Create the service client for calling the assembler
        client_ = n_.serviceClient<AssembleScans>("assemble_scans");

        first_time_ = true;
        arm_ = false;

        dump = false;
        counter = 0;

        if(ros::param::has("~file")) {
          ROS_INFO("dumping data to files!");
          ros::param::get("~file", path);
          std::ofstream file(path.c_str());
          if(!file.is_open()) ROS_WARN("unable to open file for writing '%s'", path.c_str());
          else {
            file << "TODO timestamp etc";
            dump = true;
          }
          file.close();
        }
      }

      void rotCallback(const sensor_msgs::JointState::ConstPtr& e)
      {

        if(first_time_) {
          last_time_ = e->header.stamp;
          first_time_ = false;
          return;
        }

        if(!arm_ && e->position[0] > 3) {
          arm_ = true;
          return;
        }

        if(arm_ && e->position[0] > 0 && e->position[0] < 1) {

          // Populate our service request based on our timer callback times
          AssembleScans srv;
          srv.request.begin = last_time_;
          srv.request.end   = e->header.stamp;

          // Make the service call
          if (client_.call(srv))
          {
            ROS_INFO("Published Cloud with %zu points", srv.response.cloud.points.size()) ;
            pub_.publish(srv.response.cloud);
            if(dump) {
              char actpath[1024];
              sprintf(actpath, "%s%010d", path.c_str(), counter);
              std::ofstream file (actpath);
              if(!file.is_open()) ROS_WARN("unable to open file for writing '%s'", actpath);
              else {
                file << srv.response.cloud.points.size();
                counter++;
              }
              file.close();
            }
          }
          else
          {
            ROS_ERROR("Error making service call\n") ;
          }

          arm_ = false;
          last_time_ = e->header.stamp;
        }
      }
      void odomCallback(const nav_msgs::Odometry::ConstPtr& e) {

      }

    private:
      ros::NodeHandle n_;
      ros::Publisher pub_;
      ros::Subscriber subStates_;
      ros::Subscriber subOdom_;
      ros::ServiceClient client_;
      bool first_time_;
      bool arm_;
      ros::Time last_time_;
      bool dump;
      int counter;
      std::string path; //path where clouds get dumped into files in SLAM format

      // remember the last known pose
      float posx, posy, posz;
      float orix, oriy, oriz, oriw;
  } ;

}

using namespace laser_assembler ;

int main(int argc, char **argv)
{
  ros::init(argc, argv, "rotunit_snapshotter");
  ros::NodeHandle n;
  ROS_INFO("Waiting for [build_cloud] to be advertised");
  ros::service::waitForService("build_cloud");
  ROS_INFO("Found build_cloud! Starting the snapshotter");
  PeriodicSnapshotter snapshotter;
  ros::spin();
  return 0;
}
