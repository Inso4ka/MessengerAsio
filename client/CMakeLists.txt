cmake_minimum_required(VERSION 3.5)

project(client)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(client client.h  client.cpp main.cpp)

target_link_libraries(client ${Boost_LIBRARIES})

