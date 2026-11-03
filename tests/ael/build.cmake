# Test AEL (build.cmake):
# Emits one of each AEL event to verify the relay and subscribers

add_executable(test_ael_emit ael/test_ael_emit.c)
target_link_libraries(test_ael_emit PRIVATE 
    ${CMAKE_PROJECT_NAME}::aroc
)
