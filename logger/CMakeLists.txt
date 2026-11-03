set(TARG aurora_external_logger)

file(GLOB_RECURSE LOGGER_SOURCES FOLLOW_SYMLINKS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_executable(${TARG} 
    ${LOGGER_SOURCES}
)

target_compile_options(${TARG} PRIVATE ${GENERIC_COMPILE_OPTIONS})

set_target_properties(${TARG} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
)

target_link_libraries(${TARG} PRIVATE
    ${COMMON_LIBRARIES}
    ${CMAKE_PROJECT_NAME}::aroc
)

install(TARGETS ${TARG}
    EXPORT ${TARG}Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
