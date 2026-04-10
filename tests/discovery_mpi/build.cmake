# Example test executable
add_executable(test_discovery_mpi discovery_mpi/test_discovery.c)

# Link against the objects directly for "white-box" testing 
# OR link against barf_client for "black-box" API testing
target_link_libraries(test_discovery_mpi PRIVATE 
    aurora_user
    mpi
    # gtest # (If you decide to use GoogleTest later)
)

