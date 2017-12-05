PROJECT( strava_segments2gpx )
cmake_minimum_required(VERSION 2.6)
# Debug, Release, RelWithDebInfo and MinSizeRe
set(CMAKE_BUILD_TYPE RelWithDebInfo)
SET(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra") # add extra warnings

add_executable( strava_segments2gpx strava_segments2gpx.cpp )
target_link_libraries( strava_segments2gpx ${OpenCV_LIBS} curl)
