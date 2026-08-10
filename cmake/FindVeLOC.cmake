# FindVeLOC.cmake

find_package(veloc QUIET)

if(NOT veloc_FOUND)
    if(VELOC_INSTALL_DIR)
        add_library(${CMAKE_PROJECT_NAME}::veloc INTERFACE IMPORTED GLOBAL)
        target_include_directories(${CMAKE_PROJECT_NAME}::veloc
            INTERFACE ${VELOC_INSTALL_DIR}/include)
        target_link_libraries(${CMAKE_PROJECT_NAME}::veloc
            INTERFACE ${VELOC_INSTALL_DIR}/lib64)

        message(STATUS "Found VeLOC using Install Dir" ${VELOC_INSTALL_DIR})
        set(veloc_FOUND True)
    else()
        message(WARNING "Failed to find VELOC" ${VELOC_INSTALL_DIR})
    endif()
else()
    message(STATUS "VELOC Found")
    add_library(${CMAKE_PROJECT_NAME}::veloc INTERFACE IMPORTED GLOBAL)
    target_link_libraries(${CMAKE_PROJECT_NAME}::veloc INTERFACE veloc::client)
endif()
