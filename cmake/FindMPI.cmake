# FindMPI.cmake

find_package(MPI COMPONENTS C)

if(NOT MPI_FOUND OR DEFINED ENV{MPI_HOME})
    message(STATUS "find_package(MPI) Failed")
    find_path(MPI_INCLUDE_DIR NAMES mpi.h
        HINTS $ENV{MPI_HOME} $ENV{MPI_INCLUDE} $ENV{HPC_SDK_PATH}
        PATH_SUFFIXES include
    )
    find_library(MPI_LIB NAMES mpi 
        HINTS $ENV{MPI_HOME} $ENV{MPI_LIB} $ENV{MPICC}
        PATH_SUFFIXES lib lib64
    )

    if(MPI_INCLUDE_DIR AND MPI_LIB)
        add_library(${CMAKE_PROJECT_NAME}::mpi INTERFACE IMPORTED GLOBAL)
        target_include_directories(${CMAKE_PROJECT_NAME}::mpi
            INTERFACE ${MPI_INCLUDE_DIR})
        target_link_libraries(${CMAKE_PROJECT_NAME}::mpi
            INTERFACE ${MPI_LIB})

        # set(MPI_FOUND TRUE CACHE INTERNAL "Bypassed" FORCE)
        # set(MPI_C_FOUND TRUE CACHE INTERNAL "Bypassed" FORCE)
        # set(MPI_C_WORKS TRUE CACHE INTERNAL "Bypassed" FORCE)
        # set(MPI_VERSION "3.1" CACHE INTERNAL "Bypassed" FORCE)
        # set(MPI_C_INCLUDE_DIRS "${MPI_INCLUDE_DIR}" CACHE INTERNAL "Bypassed" FORCE)
        # set(MPI_C_LIBRARIES "${MPI_LIB}" CACHE INTERNAL "Bypassed" FORCE)


        message(STATUS "Found MPI")
        message(STATUS ${MPI_INCLUDE_DIR})
        message(STATUS ${MPI_LIB})
    else()
        message(WARNING "Failed to find MPI" ${MPI_INCLUDE_DIR} ${MPI_LIB})
    endif()
else()
    message(STATUS "Found MPI with CMAKE")
    # add_library(${CMAKE_PROJECT_NAME}::mpi ALIAS MPI::MPI_C)
    add_library(${CMAKE_PROJECT_NAME}::mpi INTERFACE IMPORTED GLOBAL)
    target_link_libraries(${CMAKE_PROJECT_NAME}::mpi INTERFACE MPI::MPI_C)
endif()

