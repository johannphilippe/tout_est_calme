cmake_minimum_required(VERSION 3.5)

project(csound_opcodes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")
find_package(Csound REQUIRED)

add_library(csound_opcodes SHARED sirenes.cpp)
target_include_directories(csound_opcodes PRIVATE 
    ${CSOUND_INCLUDE_DIRS} 
    libs/q/q_lib/include/q)


option(Q_BUILD_EXAMPLES "build Q library examples" OFF)
option(Q_BUILD_TEST "build Q library tests" OFF)
option(Q_BUILD_IO "build Q IO library" OFF)
add_subdirectory(libs/q)

target_link_libraries(csound_opcodes PUBLIC libq)