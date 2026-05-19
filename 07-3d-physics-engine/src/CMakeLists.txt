cmake_minimum_required(VERSION 3.16)
project(physics_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(physics_core STATIC)
target_include_directories(physics_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(raylib QUIET)
if(raylib_FOUND)
    add_library(physics_render STATIC)
    target_link_libraries(physics_render PUBLIC physics_core raylib)
else()
    message(STATUS "Raylib not found - rendering disabled")
endif()

add_executable(physics_sim app/main.cpp)
target_link_libraries(physics_sim PRIVATE physics_core)
if(raylib_FOUND)
    target_link_libraries(physics_sim PRIVATE physics_render)
endif()

enable_testing()
add_subdirectory(tests)
