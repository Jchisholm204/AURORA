# Example test executable

set(HEATDIS_WORKING_DIR heatdis)

add_executable(heatdis_original ${HEATDIS_WORKING_DIR}/heatdis_original.c)
add_executable(heatdis_aurora ${HEATDIS_WORKING_DIR}/heatdis_aurora.c)

target_link_libraries(heatdis_original PRIVATE ${CMAKE_PROJECT_NAME}::mpi m)
target_link_libraries(heatdis_aurora PRIVATE aul ${CMAKE_PROJECT_NAME}::mpi m)
