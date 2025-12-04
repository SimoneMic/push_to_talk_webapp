cmake_minimum_required(VERSION 3.12)

project(webserver_push_to_talk)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED)
find_package(YARP 3.12 REQUIRED COMPONENTS sig os)

add_executable(webserver src/webserver.cpp)
target_link_libraries(webserver Boost::boost YARP::YARP_sig YARP::YARP_os ${YARP_LIBRARIES})
