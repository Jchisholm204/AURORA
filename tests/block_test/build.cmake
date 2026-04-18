# Example test executable
add_executable(block_test block_test/block_test.c)

# Link against the objects directly for "white-box" testing 
# OR link against barf_client for "black-box" API testing
target_link_libraries(block_test PRIVATE 
    aul
    ${CMAKE_PROJECT_NAME}::mpi
    # gtest # (If you decide to use GoogleTest later)
)


