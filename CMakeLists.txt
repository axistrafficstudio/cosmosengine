cmake_minimum_required(VERSION 3.21)

project(CosmosEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Options
option(COSMOS_ENABLE_WARNINGS "Enable extra compiler warnings" ON)
option(COSMOS_ENABLE_LTO "Enable link-time optimization if available" OFF)

# Dependencies via vcpkg (recommended)
# Required ports:
#   vcpkg install glfw3 glm glad imgui[glfw-binding,opengl3-binding]

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE COSMOS_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

add_executable(cosmosengine ${COSMOS_SOURCES})

target_include_directories(cosmosengine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(cosmosengine PRIVATE
    glfw
    glm::glm
    glad::glad
    imgui::imgui
)

target_compile_definitions(cosmosengine PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

if(MSVC AND COSMOS_ENABLE_WARNINGS)
    target_compile_options(cosmosengine PRIVATE /W4 /permissive- /Zc:preprocessor)
else()
    if(COSMOS_ENABLE_WARNINGS)
        target_compile_options(cosmosengine PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(COSMOS_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported OUTPUT out)
    if(lto_supported)
        set_property(TARGET cosmosengine PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# Copy shaders to build/bin directory on build
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders
    BYPRODUCTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
    COMMENT "Copying shaders to runtime directories"
)
add_dependencies(cosmosengine copy_shaders)
