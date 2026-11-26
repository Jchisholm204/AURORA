# HPCCG Checkpoint/Restart Benchmarks
#
# Builds three variants of HPCCG:
#   hpccg        - no checkpoint backend (checkpoint macros are no-ops)
#   hpccg_aurora - AURORA checkpoint backend
#   hpccg_veloc  - VELOC checkpoint backend (only when VELOC is available)

if(MPI_FOUND)
    set(HPCCG_WORKING_DIR HPCCG)
    set(HPCCG_SOURCES
        ${HPCCG_WORKING_DIR}/main.cpp
        ${HPCCG_WORKING_DIR}/HPCCG.cpp
        ${HPCCG_WORKING_DIR}/generate_matrix.cpp
        ${HPCCG_WORKING_DIR}/HPC_Sparse_Matrix.cpp
        ${HPCCG_WORKING_DIR}/HPC_sparsemv.cpp
        ${HPCCG_WORKING_DIR}/compute_residual.cpp
        ${HPCCG_WORKING_DIR}/ddot.cpp
        ${HPCCG_WORKING_DIR}/dump_matlab_matrix.cpp
        ${HPCCG_WORKING_DIR}/exchange_externals.cpp
        ${HPCCG_WORKING_DIR}/make_local_matrix.cpp
        ${HPCCG_WORKING_DIR}/mytimer.cpp
        ${HPCCG_WORKING_DIR}/read_HPC_row.cpp
        ${HPCCG_WORKING_DIR}/waxpby.cpp
        ${HPCCG_WORKING_DIR}/YAML_Doc.cpp
        ${HPCCG_WORKING_DIR}/YAML_Element.cpp
    )

    # Baseline HPCCG - checkpoint calls expand to no-ops
    add_executable(hpccg ${HPCCG_SOURCES})

    target_compile_definitions(hpccg PRIVATE
        TIMER
    )
    target_compile_features(hpccg PRIVATE cxx_std_11)
    target_link_libraries(hpccg PRIVATE
        ${CMAKE_PROJECT_NAME}::mpi
        m
    )

    # AURORA HPCCG
    add_executable(hpccg_aurora ${HPCCG_SOURCES})

    target_compile_definitions(hpccg_aurora PRIVATE
        TIMER
        HPCCG_CKPT_AURORA
    )
    target_compile_features(hpccg_aurora PRIVATE cxx_std_11)
    target_link_libraries(hpccg_aurora PRIVATE
        ${CMAKE_PROJECT_NAME}::aul
        ${CMAKE_PROJECT_NAME}::mpi
        m
    )

    # VELOC HPCCG
    if(veloc_FOUND)
        add_executable(hpccg_veloc ${HPCCG_SOURCES})

        target_compile_definitions(hpccg_veloc PRIVATE
            TIMER
            HPCCG_CKPT_VELOC
        )
        target_compile_features(hpccg_veloc PRIVATE cxx_std_11)
        target_link_libraries(hpccg_veloc PRIVATE
            ${CMAKE_PROJECT_NAME}::veloc
            ${CMAKE_PROJECT_NAME}::mpi
            m
        )
    else()
        message(WARNING "VELOC Missing - Cannot Build VeLOC HPCCG Test")
    endif()
else()
    message(WARNING "MPI Missing - Cannot Build HPCCG Tests")
endif()
