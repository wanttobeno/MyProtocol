cmake_minimum_required(VERSION 2.8)

project(MyProtocol)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp Json_SRC_LIST)

add_executable(MyProtocol ${SRC_LIST} ${Json_SRC_LIST})


if(win32)
	target_link_libraries(${PROJECT_NAME} Ws2_32.lib)
endif()







