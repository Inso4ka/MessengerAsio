cmake_minimum_required(VERSION 3.5)

project(server)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})

add_executable(server server.h database.h server.cpp database.cpp main.cpp)

target_link_libraries(server ${Boost_LIBRARIES})
target_link_libraries(server ${SQLite3_LIBRARIES})

