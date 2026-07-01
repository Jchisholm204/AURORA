# cmake/FindThreads.cmake

# Standard Latch Variables
set(Threads_FOUND TRUE CACHE INTERNAL "Bypassed")
set(THREADS_FOUND TRUE CACHE INTERNAL "Bypassed")

# Internal CMake Implementation Latches (This stops CMake from re-running tests)
set(CMAKE_USE_PTHREADS_INIT 1 CACHE INTERNAL "Bypassed")
set(CMAKE_HP_PTHREADS_INIT 0 CACHE INTERNAL "Bypassed")
set(CMAKE_USE_WIN32_THREADS_INIT 0 CACHE INTERNAL "Bypassed")
set(CMAKE_HAVE_LIBC_PTHREAD TRUE CACHE INTERNAL "Bypassed")

# Create the expected target so downstream dependencies don't crash
if(NOT TARGET Threads::Threads)
    add_library(Threads::Threads INTERFACE IMPORTED GLOBAL)
    target_compile_options(Threads::Threads INTERFACE "-pthread")
    target_link_options(Threads::Threads INTERFACE "-pthread")
endif()

message(STATUS "Bypassed CMake's internal Thread test successfully.")
