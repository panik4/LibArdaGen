cmake_minimum_required(VERSION 3.15)

project(LibArdaGen
    VERSION 1.0
    LANGUAGES CXX
)

# ------------------------------------------------------------
# C++ standard
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------
file(GLOB_RECURSE LIBARDAGEN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
)

file(GLOB_RECURSE LIBARDAGEN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

# ------------------------------------------------------------
# Library target
# ------------------------------------------------------------
add_library(LibArdaGen
    ${LIBARDAGEN_SOURCES}
    ${LIBARDAGEN_HEADERS}
)

add_library(LibArdaGen::LibArdaGen ALIAS LibArdaGen)

# ------------------------------------------------------------
# Include directories
# ------------------------------------------------------------
target_include_directories(LibArdaGen
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/gli
        ${CMAKE_CURRENT_SOURCE_DIR}/external/gli/external
        ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
)

# ------------------------------------------------------------
# Compiler warnings & optimizations
# ------------------------------------------------------------
if (MSVC)
    target_compile_options(LibArdaGen PRIVATE
        /W4
        /MP
        $<$<CONFIG:Release>:/O2>
    )
else()
    target_compile_options(LibArdaGen PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
if (NOT TARGET FastWorldGenLib)
    message(STATUS "FastWorldGenLib not found — adding as subdirectory")
    add_subdirectory(
        ../FastWorldGen
        ${CMAKE_BINARY_DIR}/FastWorldGen_build
    )
endif()

target_link_libraries(LibArdaGen
    PUBLIC
        FastWorldGenLib
)

# ------------------------------------------------------------
# Diagnostics
# ------------------------------------------------------------
get_target_property(LibArdaGen_inc_dirs LibArdaGen INCLUDE_DIRECTORIES)
message(STATUS "LibArdaGen include directories: ${LibArdaGen_inc_dirs}")