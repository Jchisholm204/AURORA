# Test Discovery (build.cmake):
# Compiles a sample program to verify lib-server connection and proper runtime

add_executable(test_afv ${CMAKE_CURRENT_SOURCE_DIR}/afv/test_afv.c)

target_link_libraries(test_afv PRIVATE 
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::ucx
    ${CMAKE_PROJECT_NAME}::aroc
)

