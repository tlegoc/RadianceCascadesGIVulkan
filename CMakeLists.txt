cmake_minimum_required(VERSION 3.29)
project(RadianceCascadeGIVulkan)

set(CMAKE_CXX_STANDARD 20)

add_executable(RadianceCascadeGIVulkan main.cpp)

find_package(Vulkan REQUIRED)

target_link_libraries(RadianceCascadeGIVulkan Vulkan::Vulkan)

# FetchContent vk-bootstrap, glfw

include(FetchContent)

# Fetch content verbose
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
        vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG        v1.3.295
)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG        3.4
)

FetchContent_MakeAvailable(vk_bootstrap glfw)

target_link_libraries(RadianceCascadeGIVulkan vk-bootstrap glfw)