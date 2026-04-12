# x86_64 GCC Toolchain
# Modified From https://github.com/Jchisholm204/Scout/blob/main/src/control_board/firmware/gcc-arm-none-eabi.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

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
