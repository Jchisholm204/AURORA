# Host GCC Toolchain
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
# STATIC_LIBRARY try-compiles skip linking, so CMake can't detect the multiarch
# lib dir (e.g. /usr/lib/x86_64-linux-gnu) on Debian/Ubuntu - set it explicitly
set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

set(CMAKE_EXECUTABLE_SUFFIX_C "")
set(CMAKE_EXECUTABLE_SUFFIX_CXX "")

if(NOT "$ENV{UCX_HOME}" STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH $ENV{UCX_HOME})
    message(STATUS "Toolchain: Using UCX_HOME from environment: $ENV{UCX_HOME}")
endif()

if(NOT "$ENV{HPC_SDK_PATH}" STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH $ENV{HPC_SDK_PATH})
endif()
