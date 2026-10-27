# Test Discovery (build.cmake):
# Compiles sample programs to verify lib-server connection and proper runtime

add_executable(test_ael_client ${CMAKE_CURRENT_SOURCE_DIR}/ael/test_ael_client.c)
add_executable(test_ael_server ${CMAKE_CURRENT_SOURCE_DIR}/ael/test_ael_server.c)

target_link_libraries(test_ael_server PRIVATE 
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::ucx
    ${CMAKE_PROJECT_NAME}::aroc
)
target_link_libraries(test_ael_client PRIVATE 
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::ucx
    ${CMAKE_PROJECT_NAME}::aroc
)

# Multi-threaded client (conditionally linked if MPI is available)
add_executable(multi_client ${CMAKE_CURRENT_SOURCE_DIR}/ael/multi_client.c)
target_link_libraries(multi_client PRIVATE 
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::ucx
    ${CMAKE_PROJECT_NAME}::aroc
    pthread
)

