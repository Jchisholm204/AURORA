# Example test executable
add_executable(block_test_aurora block_test/block_test_aurora.c)

# Link against the objects directly for "white-box" testing 
# OR link against barf_client for "black-box" API testing
target_link_libraries(block_test_aurora PRIVATE 
    aul
    ${CMAKE_PROJECT_NAME}::mpi
    # gtest # (If you decide to use GoogleTest later)
)


# Example test executable
add_executable(block_test_veloc block_test/block_test_veloc.c)

# Link against the objects directly for "white-box" testing 
# OR link against barf_client for "black-box" API testing
target_link_libraries(block_test_veloc PRIVATE 
    veloc::client
    ${CMAKE_PROJECT_NAME}::mpi
    # gtest # (If you decide to use GoogleTest later)
)


