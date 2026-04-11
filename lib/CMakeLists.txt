set(LIB_TARG aurora_user)

file(GLOB_RECURSE LIB_SOURCES FOLLOW_SYMLINKS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(${LIB_TARG} SHARED
    ${LIB_SOURCES}
)

target_include_directories(${LIB_TARG} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(${LIB_TARG} PRIVATE ${GENERIC_COMPILE_OPTIONS})

target_link_libraries(${LIB_TARG} PRIVATE 
    aurora_common 
    ${COMMON_LIBRARIES}
)
