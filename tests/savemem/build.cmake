# Example test executable
add_executable(savemem savemem/savemem.cpp)

# Link against the objects directly for "white-box" testing 
# OR link against barf_client for "black-box" API testing
target_link_libraries(savemem PRIVATE 
    barf
    barf_common
    gtest # (If you decide to use GoogleTest later)
)

