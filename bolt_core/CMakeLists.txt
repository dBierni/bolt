cmake_minimum_required(VERSION 3.0.2)
project(bolt_core)

# C++ 11
add_compile_options(-std=c++11)

# Warnings
# add_definitions(-W -Wall -Wextra
#   -Wcast-qual -Wwrite-strings -Wunreachable-code -Wpointer-arith
#   -Winit-self -Wredundant-decls
#   -Wno-unused-parameter -Wno-unused-function)

find_package(catkin REQUIRED COMPONENTS
        geometry_msgs
)

#find_package(ompl REQUIRED)
find_package(Boost REQUIRED)
find_package(Eigen3 REQUIRED)
include_directories(${Eigen3_INCLUDE_DIRS})

catkin_package(
  CATKIN_DEPENDS
    ompl
  LIBRARIES ${PROJECT_NAME}
  INCLUDE_DIRS include

)

# If enabled, show debug message output
add_definitions(-DENABLE_DEBUG_MACRO)
# If enabled, use quality criteria data structures
#add_definitions(-DENABLE_QUALITY)
# Note: NDEBUG is enabled when the CMake build type is Debug

###########
## Build ##
###########

include_directories(SYSTEM
  ${Boost_INCLUDE_DIR}
  ${OMPL_INCLUDE_DIRS}
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${OMPL_INCLUDE_DIRS}
)

# Library
add_library(${PROJECT_NAME}
  src/Bolt.cpp
  src/BoltPlanner.cpp
  src/SparseGraph.cpp
  src/SparseGenerator.cpp
  src/SparseMirror.cpp
  src/SparseCriteria.cpp
  src/SparseStorage.cpp
  src/TaskGraph.cpp
  src/VertexDiscretizer.cpp
  src/CandidateQueue.cpp
  src/SparseSmoother.cpp
  src/SPARS2.cpp
 )

# Specify libraries to link a library or executable target against
target_link_libraries(${PROJECT_NAME}
  ${catkin_LIBRARIES}
  ${OMPL_LIBRARIES}
  ${Boost_LIBRARIES}
)

#############
## Install ##
#############

## Mark executables and/or libraries for installation
install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

# Mark cpp header files for installation
install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)
