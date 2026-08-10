# Test Discovery (build.cmake):
# Compiles a sample program to verify lib-server connection and proper runtime

add_executable(test_discovery discovery/test_discovery.c)
target_link_libraries(test_discovery PRIVATE 
    ${CMAKE_PROJECT_NAME}::aul
)

