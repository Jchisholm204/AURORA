# AURORA Block Test
add_executable(block_test_aurora block_test/block_test_aurora.c)

target_link_libraries(block_test_aurora PRIVATE 
    ${CMAKE_PROJECT_NAME}::aul
    ${CMAKE_PROJECT_NAME}::mpi
    # gtest # (If you decide to use GoogleTest later)
)

if(veloc_FOUND AND MPI_FOUND)

    # VeLOC Block Test
    add_executable(block_test_veloc block_test/block_test_veloc.c)

    target_link_libraries(block_test_veloc PRIVATE 
        # veloc::client
        ${CMAKE_PROJECT_NAME}::mpi
        ${CMAKE_PROJECT_NAME}::veloc
        # gtest # (If you decide to use GoogleTest later)
    )
else()
    message(WARNING "VELOC Missing - Cannot Build VeLOC Block Test")
endif()

