cmake_minimum_required(VERSION 3.21)
project(champions_level_texture_preview LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(GODOT_CPP_DIR "" CACHE PATH "Pinned godot-cpp checkout")
set(GODOTCPP_TARGET "template_debug" CACHE STRING "Godot target")
if(NOT EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set GODOT_CPP_DIR to the pinned godot-cpp checkout")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GODOT_CPP_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE GODOT_CPP_REV OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE PIN_RESULT)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies.json" DEPENDENCY_LOCK)
string(JSON GODOT_CPP_PIN GET "${DEPENDENCY_LOCK}" godot_cpp_commit)
if(NOT PIN_RESULT EQUAL 0 OR NOT GODOT_CPP_REV STREQUAL GODOT_CPP_PIN)
    message(FATAL_ERROR "godot-cpp must be checked out at ${GODOT_CPP_PIN}")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../source_path_map.cmake")
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "Keep adapter exception boundary" FORCE)
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/../build_profile.json" CACHE PATH "Pinned binding profile" FORCE)
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "Upstream headers" FORCE)
set(GODOTCPP_PRECISION "single" CACHE STRING "Pinned Godot precision" FORCE)
add_subdirectory("${GODOT_CPP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp")
add_library(champions-level-texture-preview SHARED
    level_texture_preview.cpp
    level_texture_stream.cpp
    real_world_binding.cpp
    ../../native/src/level_texture/level_texture.cpp
    ../../native/src/level_texture/material.cpp
    ../../native/src/level_texture/level_draw.cpp
    ../../native/src/world_geometry.cpp
    ../../native/src/real_world.cpp
    ../../native/src/water_draw.cpp
    ../../native/src/terrain_file.cpp
    ../../native/src/world_terrain.cpp
    ../../native/src/world_movement.cpp
    ../../native/src/core_arithmetic.cpp
    ../../native/src/core_math.cpp
    ../../native/src/core_state.cpp
    ../../native/src/level_flow.cpp
    ../../native/src/arena_modes.cpp
    ../../native/src/objects_lifetime.cpp
    ../../native/src/objects_scheduler.cpp
    ../../native/src/objects_queries.cpp
    ../../native/src/creature_hp.cpp
    ../../native/src/creature_movement.cpp
    ../../native/src/creature_water.cpp
    ../../native/src/archive.cpp
    ../../native/src/model.cpp
    ../../native/src/texture.cpp
    ../../native/src/model_texture_bind.cpp)
target_include_directories(champions-level-texture-preview PRIVATE ../../native/include)
target_link_libraries(champions-level-texture-preview PRIVATE godot::cpp)
if(MSVC)
    target_compile_options(champions-level-texture-preview PRIVATE /W4 /utf-8 /EHsc /fp:strict)
else()
    target_compile_options(champions-level-texture-preview PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
if(APPLE)
    set(CHAMPIONS_LIBRARY_NAME "champions_level_texture.macos.${GODOTCPP_TARGET}")
elseif(WIN32)
    get_target_property(CHAMPIONS_ARCH godot-cpp GODOTCPP_ARCH)
    set(CHAMPIONS_LIBRARY_NAME "champions_level_texture.windows.${GODOTCPP_TARGET}.${CHAMPIONS_ARCH}")
else()
    get_target_property(CHAMPIONS_ARCH godot-cpp GODOTCPP_ARCH)
    set(CHAMPIONS_LIBRARY_NAME "champions_level_texture.linux.${GODOTCPP_TARGET}.${CHAMPIONS_ARCH}")
endif()
set_target_properties(champions-level-texture-preview PROPERTIES PREFIX ""
    OUTPUT_NAME "${CHAMPIONS_LIBRARY_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../godot/bin/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../godot/bin/$<0:>")
