# Test Discovery MPI (build.cmake):
# Compiles a sample program to verify lib-server connection and proper runtime (using MPI)

add_executable(test_discovery_mpi discovery_mpi/test_discovery.c)
target_link_libraries(test_discovery_mpi PRIVATE 
    ${CMAKE_PROJECT_NAME}::aul
    # Link against project MPI
    ${CMAKE_PROJECT_NAME}::mpi
    # gtest # (If you decide to use GoogleTest later)
)

