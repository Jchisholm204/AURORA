# miniVite Checkpoint/Restart Benchmarks
#
# Builds three variants of miniVite:
#   miniVite        - no checkpoint backend (checkpoint macros are no-ops)
#   miniVite_aurora - AURORA checkpoint backend
#   miniVite_veloc  - VELOC checkpoint backend (only when VELOC is available)

if(MPI_FOUND)
    set(MINIVITE_WORKING_DIR miniVite)
    set(MINIVITE_SOURCES
        ${MINIVITE_WORKING_DIR}/main.cpp
    )

    # Baseline miniVite - checkpoint calls expand to no-ops
    add_executable(miniVite ${MINIVITE_SOURCES})

    target_compile_definitions(miniVite PRIVATE
        TIMER
    )
    target_compile_options(miniVite PRIVATE -fopenmp -O3)
    target_compile_features(miniVite PRIVATE cxx_std_11)
    target_link_libraries(miniVite PRIVATE
        ${CMAKE_PROJECT_NAME}::mpi
        gomp
    )

    # AURORA miniVite
    add_executable(miniVite_aurora ${MINIVITE_SOURCES})

    target_compile_definitions(miniVite_aurora PRIVATE
        TIMER
        MINIVITE_CKPT_AURORA
    )
    target_compile_options(miniVite_aurora PRIVATE -fopenmp -O3)
    target_compile_features(miniVite_aurora PRIVATE cxx_std_11)
    target_link_libraries(miniVite_aurora PRIVATE
        ${CMAKE_PROJECT_NAME}::aul
        ${CMAKE_PROJECT_NAME}::mpi
        gomp
    )

    # VELOC miniVite
    if(veloc_FOUND)
        add_executable(miniVite_veloc ${MINIVITE_SOURCES})

        target_compile_definitions(miniVite_veloc PRIVATE
            TIMER
            MINIVITE_CKPT_VELOC
        )
        target_compile_options(miniVite_veloc PRIVATE -fopenmp -O3)
        target_compile_features(miniVite_veloc PRIVATE cxx_std_11)
        target_link_libraries(miniVite_veloc PRIVATE
            ${CMAKE_PROJECT_NAME}::veloc
            ${CMAKE_PROJECT_NAME}::mpi
            gomp
        )
    else()
        message(WARNING "VELOC Missing - Cannot Build VeLOC miniVite Test")
    endif()
else()
    message(WARNING "MPI Missing - Cannot Build miniVite Tests")
endif()
