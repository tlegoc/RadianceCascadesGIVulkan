cmake_minimum_required(VERSION 3.29)
project(RadianceCascadeGIVulkan)

set(CMAKE_CXX_STANDARD 23)

add_executable(ComputeApp main.cpp)

option(SHADERS_OUTPUT_GLSL "Make slangc also output glsl shader code for debugging" ON)

# Set output
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders_headers/Shaders)
set(SHADER_INCLUDE_DIR ${CMAKE_BINARY_DIR}/shaders_headers)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
target_include_directories(ComputeApp PRIVATE ${SHADER_INCLUDE_DIR})

find_package(Vulkan REQUIRED)

target_link_libraries(ComputeApp Vulkan::Vulkan)
target_include_directories(ComputeApp PRIVATE include)

include(FetchContent)

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

# vma
FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        GIT_TAG        v3.1.0
)

FetchContent_MakeAvailable(vk_bootstrap glfw VulkanMemoryAllocator)

target_link_libraries(ComputeApp vk-bootstrap glfw GPUOpen::VulkanMemoryAllocator)

add_subdirectory(imgui)
add_subdirectory(src)

# Find slangc if SLANGC is not set
if (NOT SLANGC)
    message(STATUS "SLANGC not set, trying to find it")
    find_program(SLANGC slangc REQUIRED)
endif ()

# Compile shaders
file(GLOB_RECURSE shader_SOURCES CONFIGURE_DEPENDS shaders/*.slang)

# Get exe directory
get_target_property(EXE_DIR ComputeApp RUNTIME_OUTPUT_DIRECTORY)

foreach(shader ${shader_SOURCES})
    set(shader_SOURCE "${shader}")
    # get file name without extension
    get_filename_component(shader ${shader} NAME_WE)
    set(shader_OUTPUT "${shader}.h")

    add_custom_command(
            OUTPUT ${shader_OUTPUT}
            COMMAND ${SLANGC} ${shader_SOURCE} -entry main -target spirv -o ${SHADER_OUTPUT_DIR}/${shader_OUTPUT} -source-embed-style u32 -source-embed-name ${shader} -fvk-use-gl-layout
            DEPENDS ${shader_SOURCE}
    )

    list(APPEND shader_OUTPUTS ${shader_OUTPUT})

    if (SHADERS_OUTPUT_GLSL)
        set(shader_OUTPUT_GLSL "${shader}.glsl")
        add_custom_command(
                OUTPUT ${shader_OUTPUT_GLSL}
                COMMAND ${SLANGC} ${shader_SOURCE} -entry main -target glsl -o ${SHADER_OUTPUT_DIR}/${shader_OUTPUT_GLSL} -fvk-use-gl-layout
                DEPENDS ${shader_SOURCE}
        )
        list(APPEND shader_OUTPUTS ${shader_OUTPUT_GLSL})
    endif()
endforeach()
add_custom_target(Shaders DEPENDS ${shader_OUTPUTS})

add_dependencies(ComputeApp Shaders)
