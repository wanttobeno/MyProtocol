cmake_minimum_required(VERSION 2.8)

project(bigCheck)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})        


add_executable(bigCheck ${SRC_LIST})




