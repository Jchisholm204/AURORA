# LULESH Checkpoint/Restart Benchmarks
#
# Builds three variants of LULESH:
#   lulesh        - no checkpoint backend (checkpoint macros are no-ops)
#   lulesh_aurora - AURORA checkpoint backend
#   lulesh_veloc  - VELOC checkpoint backend (only when VELOC is available)

if(MPI_FOUND)
    set(LULESH_WORKING_DIR LULESH)
    set(LULESH_SOURCES
        ${LULESH_WORKING_DIR}/lulesh.cc
        ${LULESH_WORKING_DIR}/lulesh-comm.cc
        ${LULESH_WORKING_DIR}/lulesh-viz.cc
        ${LULESH_WORKING_DIR}/lulesh-util.cc
        ${LULESH_WORKING_DIR}/lulesh-init.cc
    )

    # Baseline LULESH - checkpoint calls expand to no-ops
    add_executable(lulesh ${LULESH_SOURCES})

    target_compile_definitions(lulesh PRIVATE USE_MPI=1)
    target_compile_features(lulesh PRIVATE cxx_std_11)
    target_link_libraries(lulesh PRIVATE
        ${CMAKE_PROJECT_NAME}::mpi
        m
    )

    # AURORA LULESH
    add_executable(lulesh_aurora ${LULESH_SOURCES})

    target_compile_definitions(lulesh_aurora PRIVATE
        USE_MPI=1
        LULESH_CKPT_AURORA
    )
    target_compile_features(lulesh_aurora PRIVATE cxx_std_11)
    target_link_libraries(lulesh_aurora PRIVATE
        ${CMAKE_PROJECT_NAME}::aul
        ${CMAKE_PROJECT_NAME}::mpi
        m
    )

    # VELOC LULESH
    if(veloc_FOUND)
        add_executable(lulesh_veloc ${LULESH_SOURCES})

        target_compile_definitions(lulesh_veloc PRIVATE
            USE_MPI=1
            LULESH_CKPT_VELOC
        )
        target_compile_features(lulesh_veloc PRIVATE cxx_std_11)
        target_link_libraries(lulesh_veloc PRIVATE
            ${CMAKE_PROJECT_NAME}::veloc
            ${CMAKE_PROJECT_NAME}::mpi
            m
        )
    else()
        message(WARNING "VELOC Missing - Cannot Build VeLOC LULESH Test")
    endif()
else()
    message(WARNING "MPI Missing - Cannot Build LULESH Tests")
endif()
