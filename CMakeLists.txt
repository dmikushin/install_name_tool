cmake_minimum_required(VERSION 3.0)

project(install_name_tool)

file(GLOB SRCS "src/*.c" "src/*.cpp")

add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE include)

