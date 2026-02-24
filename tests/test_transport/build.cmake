# Example test executable
add_executable(test_transport test_transport/test_transport.cpp)

# Link against the objects directly for "white-box" testing 
# OR link against barf_client for "black-box" API testing
target_link_libraries(test_transport PRIVATE 
    barf
    barf_common
    gtest # (If you decide to use GoogleTest later)
)
