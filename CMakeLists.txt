cmake_minimum_required(VERSION 3.21)

project(AURORA VERSION 0.0.3)

# Install Configuration
include(GNUInstallDirs REQUIRED)
include(CMakePackageConfigHelpers REQUIRED)

# Language Options
enable_language(C)
set(CMAKE_C_STANDARD 11)
# Ensure ISO C
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

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
    # Color Logs
    -DLOG_USE_COLOR
    # Optimization
    -O2
)

option(BUILD_TESTING "Enable Compilation of Test Files" OFF)

# Libraries
include(${CMAKE_SOURCE_DIR}/cmake/FindUCX.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindMPI.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindThreads.cmake)
# Requires - Threads, MPI
include(${CMAKE_SOURCE_DIR}/cmake/FindVeLOC.cmake)


set(COMMON_LIBRARIES
    m
    pthread
)

# Add All Subprojects
add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(lib)
if(BUILD_TESTING)
    # Adds all test Subprojects
    add_subdirectory(tests)
endif()


# Install

write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/AURORAConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/AURORAConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/AURORAConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AURORA
)

install(FILES
    "${CMAKE_BINARY_DIR}/AURORAConfig.cmake"
    "${CMAKE_BINARY_DIR}/AURORAConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AURORA
)
