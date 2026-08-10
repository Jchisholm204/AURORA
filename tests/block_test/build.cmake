# AURORA Block Test
add_executable(block_test_aurora block_test/block_test_aurora.c)

target_link_libraries(block_test_aurora PRIVATE 
    ${CMAKE_PROJECT_NAME}::aul
    ${CMAKE_PROJECT_NAME}::mpi
)

if(veloc_FOUND AND MPI_FOUND)

    # VeLOC Block Test
    add_executable(block_test_veloc block_test/block_test_veloc.c)

    target_link_libraries(block_test_veloc PRIVATE 
        ${CMAKE_PROJECT_NAME}::mpi
        # veloc::client - described in FindVeLOC.cmake
        ${CMAKE_PROJECT_NAME}::veloc
    )
else()
    message(WARNING "VELOC Missing - Cannot Build VeLOC Block Test")
endif()

