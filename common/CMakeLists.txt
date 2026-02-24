set(LIB_TARG barf_common)

file(GLOB_RECURSE BARF_COMMON_SOURCES FOLLOW_SYMLINKS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/log.c/src/*.c
)

# add_library(${LIB_TARG} OBJECT ${BARF_COMMON_SOURCES})
add_library(${LIB_TARG} STATIC ${BARF_COMMON_SOURCES})

# Required so this .a can be linked into a .so
set_target_properties(${LIB_TARG} PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(${LIB_TARG} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/log.c/src
)

target_compile_options(${LIB_TARG} PRIVATE ${GENERIC_COMPILE_OPTIONS})

target_link_libraries(${LIB_TARG} INTERFACE ${COMMON_LIBRARIES})
