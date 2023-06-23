cmake_minimum_required(VERSION 3.16)

# project name and enable C++ support
project(safeconfig LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# we define the executable
add_executable(${PROJECT_NAME} "example.cpp")
