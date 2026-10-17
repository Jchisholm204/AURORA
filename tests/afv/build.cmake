# Test Discovery (build.cmake):
# Compiles a sample program to verify lib-server connection and proper runtime

add_executable(test_afv_metadata ${CMAKE_CURRENT_SOURCE_DIR}/afv/test_afv_metadata.c)
add_executable(test_afv_checkpoint ${CMAKE_CURRENT_SOURCE_DIR}/afv/test_afv_checkpoint.c)
add_executable(test_afv_restore ${CMAKE_CURRENT_SOURCE_DIR}/afv/test_afv_restore.c)

target_link_libraries(test_afv_metadata PRIVATE 
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::ucx
    ${CMAKE_PROJECT_NAME}::aroc
)

target_link_libraries(test_afv_checkpoint PRIVATE 
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::ucx
    ${CMAKE_PROJECT_NAME}::aroc
)

target_link_libraries(test_afv_restore PRIVATE 
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::ucx
    ${CMAKE_PROJECT_NAME}::aroc
)

