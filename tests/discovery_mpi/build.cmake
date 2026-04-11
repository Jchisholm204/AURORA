# Example test executable
add_executable(test_discovery_mpi discovery_mpi/test_discovery.c)

find_package(MPI)
if(NOT MPI_FOUND)
    find_path(MPI_INCLUDE_DIR NAMES mpi.h
        HINTS $ENV{MPI_HOME} $ENV{HPC_SDK_PATH}
        PATH_SUFFIXES include
    )
    find_library(MPI_LIB NAMES mpi
        HINTS $ENV{MPI_HOME} $ENV{HPC_SDK_PATH}
        PATH_SUFFIXES lib lib64
    )
endif()

# Link against the objects directly for "white-box" testing 
# OR link against barf_client for "black-box" API testing
target_link_libraries(test_discovery_mpi PRIVATE 
    aurora_user
    mpi
    # gtest # (If you decide to use GoogleTest later)
)

