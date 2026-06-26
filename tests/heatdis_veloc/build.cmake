# Example test executable

set(HEATDIS_WORKING_DIR heatdis_veloc)

find_package(veloc REQUIRED)

# VeloC Heat Distribution Test (memory based)
add_executable(heatdis_veloc_mem ${HEATDIS_WORKING_DIR}/heatdis_mem.c)

target_link_libraries(heatdis_veloc_mem PRIVATE 
    veloc::client
    ${CMAKE_PROJECT_NAME}::mpi 
    m
)

# VeloC Heat Distribution Test (memory based)
add_executable(heatdis_veloc_file ${HEATDIS_WORKING_DIR}/heatdis_file.c)

target_link_libraries(heatdis_veloc_file PRIVATE 
    veloc::client
    ${CMAKE_PROJECT_NAME}::mpi 
    m
)
