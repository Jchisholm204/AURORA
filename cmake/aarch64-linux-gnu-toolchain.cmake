# ARM GCC Toolchain
# Modified From https://github.com/Jchisholm204/Scout/blob/main/src/control_board/firmware/gcc-arm-none-eabi.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOOLCHAIN_PREFIX aarch64-none-linux-gnu-)
## CFLAGS
set(CMAKE_CFLAGS "")

set(CMAKE_LDFLAGS "-rpath")

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc ${CMAKE_CFLAGS} ${CMAKE_LDFLAGS})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++ ${CMAKE_CFLAGS} ${CMAKE_LDFLAGS})
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump)

set(CMAKE_EXECUTABLE_SUFFIX_C "")
set(CMAKE_EXECUTABLE_SUFFIX_CXX "")

# Do not link anything from the host isa
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

if(NOT "$ENV{UCX_HOME}" STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH $ENV{UCX_HOME})
    message(STATUS "Toolchain: Using UCX_HOME from environment: $ENV{UCX_HOME}")
endif()

if(NOT "$ENV{HPC_SDK_PATH}" STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH $ENV{HPC_SDK_PATH})
endif()

# Don't let CMake try to check if the compiler works
# set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
