# AMG (HYPRE BoomerAMG) Checkpoint/Restart Benchmarks
#
# Builds three variants of the AMG benchmark driver:
#   amg        - no checkpoint backend (checkpoint calls are no-ops)
#   amg_aurora - AURORA checkpoint backend
#   amg_veloc  - VELOC checkpoint backend (only when VELOC is available)

if(MPI_FOUND)
    set(AMG_WORKING_DIR AMG)

    # Vendored HYPRE sources (mirrors the original per-directory Makefile lists)
    file(GLOB AMG_UTILITIES_SOURCES CONFIGURE_DEPENDS
        ${AMG_WORKING_DIR}/utilities/*.c
    )
    file(GLOB AMG_IJ_SOURCES CONFIGURE_DEPENDS
        ${AMG_WORKING_DIR}/IJ_mv/*.c
    )
    file(GLOB AMG_PARCSR_MV_SOURCES CONFIGURE_DEPENDS
        ${AMG_WORKING_DIR}/parcsr_mv/*.c
    )
    # parcsr_ls: explicit list - par_difconv.c is excluded because it calls
    # hypre_map/hypre_map2/hypre_map3, which are declared in
    # _hypre_parcsr_ls.h but have no definition in this benchmark tree (the
    # driver never uses them)
    file(GLOB AMG_PARCSR_LS_SOURCES CONFIGURE_DEPENDS
        ${AMG_WORKING_DIR}/parcsr_ls/*.c
    )
    list(REMOVE_ITEM AMG_PARCSR_LS_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/${AMG_WORKING_DIR}/parcsr_ls/par_difconv.c
    )
    file(GLOB AMG_SEQ_MV_SOURCES CONFIGURE_DEPENDS
        ${AMG_WORKING_DIR}/seq_mv/*.c
    )
    # krylov: explicit list - old_gmres.c is kept only for reference
    set(AMG_KRYLOV_SOURCES
        ${AMG_WORKING_DIR}/krylov/gmres.c
        ${AMG_WORKING_DIR}/krylov/HYPRE_gmres.c
        ${AMG_WORKING_DIR}/krylov/HYPRE_pcg.c
        ${AMG_WORKING_DIR}/krylov/pcg.c
    )

    set(AMG_SOURCES
        ${AMG_UTILITIES_SOURCES}
        ${AMG_IJ_SOURCES}
        ${AMG_PARCSR_MV_SOURCES}
        ${AMG_PARCSR_LS_SOURCES}
        ${AMG_SEQ_MV_SOURCES}
        ${AMG_KRYLOV_SOURCES}
        ${AMG_WORKING_DIR}/test/amg.c
    )

    set(AMG_INCLUDE_DIRS
        ${AMG_WORKING_DIR}
        ${AMG_WORKING_DIR}/utilities
        ${AMG_WORKING_DIR}/IJ_mv
        ${AMG_WORKING_DIR}/seq_mv
        ${AMG_WORKING_DIR}/parcsr_mv
        ${AMG_WORKING_DIR}/parcsr_ls
        ${AMG_WORKING_DIR}/krylov
    )

    set(AMG_COMPILE_DEFINITIONS
        TIMER
        TIMER_USE_MPI
        HYPRE_TIMING
    )

    # Baseline AMG - checkpoint calls expand to no-ops
    add_executable(amg ${AMG_SOURCES})

    target_compile_definitions(amg PRIVATE ${AMG_COMPILE_DEFINITIONS})
    target_include_directories(amg PRIVATE ${AMG_INCLUDE_DIRS})
    target_link_libraries(amg PRIVATE
        ${CMAKE_PROJECT_NAME}::mpi
        m
    )

    # AURORA AMG
    add_executable(amg_aurora ${AMG_SOURCES})

    target_compile_definitions(amg_aurora PRIVATE
        ${AMG_COMPILE_DEFINITIONS}
        AMG_CKPT_AURORA
    )
    target_include_directories(amg_aurora PRIVATE ${AMG_INCLUDE_DIRS})
    target_link_libraries(amg_aurora PRIVATE
        ${CMAKE_PROJECT_NAME}::aul
        ${CMAKE_PROJECT_NAME}::mpi
        m
    )

    # VELOC AMG
    if(veloc_FOUND)
        add_executable(amg_veloc ${AMG_SOURCES})

        target_compile_definitions(amg_veloc PRIVATE
            ${AMG_COMPILE_DEFINITIONS}
            AMG_CKPT_VELOC
        )
        target_include_directories(amg_veloc PRIVATE ${AMG_INCLUDE_DIRS})
        target_link_libraries(amg_veloc PRIVATE
            ${CMAKE_PROJECT_NAME}::veloc
            ${CMAKE_PROJECT_NAME}::mpi
            m
        )
    else()
        message(WARNING "VELOC Missing - Cannot Build VeLOC AMG Test")
    endif()
else()
    message(WARNING "MPI Missing - Cannot Build AMG Tests")
endif()
