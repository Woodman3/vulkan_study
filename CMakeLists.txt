cmake_minimum_required(VERSION 3.20)

project(hellovulkan)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main src/main.cpp)

target_compile_options(main PRIVATE -g)

find_package(Stb REQUIRED)
target_include_directories(main PRIVATE ${Stb_INCLUDE_DIR})

find_package(glm CONFIG REQUIRED)
# target_link_libraries(main PRIVATE glm::glm)
target_link_libraries(main PRIVATE glm::glm-header-only)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(main PRIVATE glfw)

find_package(Vulkan REQUIRED)
target_link_libraries(main PRIVATE Vulkan::Vulkan)