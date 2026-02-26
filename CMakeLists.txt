cmake_minimum_required(VERSION 3.21)

project(AURORA)
enable_language(C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Better portability

option(BUILD_TESTING "Enable Compilation of Test Files" ON)

# Add additional include directories for standard libraries
include_directories(SYSTEM ${CMAKE_C_COMPILER_INCLUDE_DIRS})

# Set additional compiler flags (e.g., include paths for standard libraries)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${CMAKE_C_COMPILER_INCLUDE_DIRS}")

# Enable export of compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GENERIC_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -pedantic
    # Include Debug Symbols
    -g
    -fopenmp
    -march=native
    # Color Logs
    -DLOG_USE_COLOR
)

# Append to this list
include_directories(
    # Needed for linking protobuf-c on alliancecan systems
    ~/.local/include
    # Include Directories for MPI on RHEL Systems
    /usr/include/openmpi-x86_64
)

# Append to this list
link_directories(
    # Needed for linking protobuf-c on alliancecan systems
    ~/.local/lib
    # Library Directories for MPI on RHEL Systems
    /usr/lib/
    /usr/lib64/openmpi/lib
)

set(COMMON_LIBRARIES
    m
    pthread
    gfortran
    ucp
    uct
    ucs
)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(lib)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
