cmake_minimum_required(VERSION 3.20)

project(hellovulkan)

add_executable(hellovulkan main.cpp)

find_package(Stb REQUIRED)
target_include_directories(hellovulkan PRIVATE ${Stb_INCLUDE_DIR})

