cmake_minimum_required(VERSION 3.5)
project(boost)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

set(CMAKE_CACHEFILE_DIR ${CMAKE_BINARY_DIR}/cache)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS system thread)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# Определить исходные файлы проекта
set(SOURCE_FILES server.cpp)

# Добавить исполняемый файл в проект
add_executable(boost ${SOURCE_FILES})

# Линковка с библиотеками Boost
target_link_libraries(boost ${Boost_LIBRARIES})

# Изменить имя выходного файла на "server"
set_target_properties(boost PROPERTIES OUTPUT_NAME "server")
