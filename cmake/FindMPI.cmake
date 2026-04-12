# FindMPI.cmake

find_package(MPI)

if(NOT MPI_FOUND)
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
        set(MPI_FOUND TRUE)
        set(MPI_C_FOUND TRUE)
        message(STATUS "Found MPI")
    else()
        message(WARNING "Failed to find MPI" ${MPI_INCLUDE_DIR} ${MPI_LIB})
    endif()
else()
    message(STATUS "Found MPI")
    add_library(${CMAKE_PROJECT_NAME}::mpi INTERFACE IMPORTED GLOBAL)
    target_link_libraries(${CMAKE_PROJECT_NAME}::mpi INTERFACE mpi::mpi)

    set(MPI_FOUND TRUE CACHE INTERNAL TRUE)
    set(MPI_C_FOUND TRUE)
endif()

