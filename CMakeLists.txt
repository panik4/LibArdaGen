# Minimum CMake version
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(LibArdaGen VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)

# For Release build (force optimizations)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2")
# Enable Link Time Optimization (LTO)
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Enable MSVC-specific optimizations (if using MSVC compiler)
if(MSVC)
    add_compile_options(/MP)  # Enables multi-threaded compilation
    add_compile_options(/GL-)  # Disable whole program optimization
    add_compile_options(/Gy)  # Enable function-level linking
endif()

add_compile_options(/std:c++latest)

# Core library (LibArdaGen) ------------------------------------
# Find all core source files (excluding main.cpp) and header files
file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB_RECURSE HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h LibArdaGen/include/*.hpp)

# Add the library target
add_library(LibArdaGen ${SOURCE_FILES} ${HEADER_FILES})

# Add the FastWorldGenLib subdirectory first
if(NOT TARGET FastWorldGenLib)
    # Allow standalone development
    message(STATUS "Building standalone: adding FastWorldGen manually")
    add_subdirectory(../FastWorldGen ${CMAKE_BINARY_DIR}/FastWorldGen_build)
endif()
# Query the include directories for LibArdaGen to ensure they are properly set
get_target_property(inc_dirs LibArdaGen INCLUDE_DIRECTORIES)


# Ensure that LibArdaGen depends on FastWorldGenLib so that the libraries are built first
add_dependencies(LibArdaGen FastWorldGenLib)

# Link the LibArdaGen library to FastWorldGenLib
target_link_libraries(LibArdaGen PUBLIC FastWorldGenLib)
target_include_directories(LibArdaGen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

get_target_property(LibArdaGen_inc_dirs LibArdaGen INCLUDE_DIRECTORIES)
message(STATUS "LibArdaGen include directories: ${LibArdaGen_inc_dirs}")

# Executable target (LibArdaGenExe) ------------------------------------
#add_executable(${PROJECT_NAME} main.cpp)
# Link executable with the library
#target_link_libraries(${PROJECT_NAME} PUBLIC LibArdaGen)
