cmake_minimum_required(VERSION 3.0)

project(install_name_tool)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

file(GLOB SRCS "src/*.cpp")

add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE include)

add_executable(patchelf "src/patchelf.cpp")
target_include_directories(patchelf PRIVATE include)
target_compile_definitions(patchelf PRIVATE PATCHELF)

