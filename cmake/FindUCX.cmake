# FindUCX.cmake

find_package(ucx)

if(NOT ucx_FOUND)
    find_path(UCX_INCLUDE_DIR NAMES ucp/api/ucp.h
        HINTS $ENV{UCX_HOME} $ENV{HPC_SDK_PATH}
        PATH_SUFFIXES include
    )
    find_library(UCX_UCP_LIB NAMES ucp HINTS $ENV{UCX_HOME} PATH_SUFFIXES lib lib64)
    find_library(UCX_UCT_LIB NAMES uct HINTS $ENV{UCX_HOME} PATH_SUFFIXES lib lib64)
    find_library(UCX_UCM_LIB NAMES ucm HINTS $ENV{UCX_HOME} PATH_SUFFIXES lib lib64)
    find_library(UCX_UCS_LIB NAMES ucs HINTS $ENV{UCX_HOME} PATH_SUFFIXES lib lib64)

    find_library(Z_LIB NAMES z)

    if(UCX_INCLUDE_DIR AND UCX_UCP_LIB)
        add_library(${CMAKE_PROJECT_NAME}::ucx INTERFACE IMPORTED GLOBAL)
        target_include_directories(${CMAKE_PROJECT_NAME}::ucx INTERFACE 
            ${UCX_INCLUDE_DIR}
        )
        target_link_libraries(${CMAKE_PROJECT_NAME}::ucx INTERFACE 
            ${UCX_UCP_LIB} 
            ${UCX_UCT_LIB} 
            ${UCX_UCS_LIB} 
            ${UCX_UCM_LIB} 
            ${Z_LIB}
            pthread
            dl
            m
        )
        set(UCX_FOUND TRUE)
    else()
        message(WARNING "Failed to find UCX")
    endif()
else()
    message(STATUS "Found UCX")
    add_library(${CMAKE_PROJECT_NAME}::ucx INTERFACE IMPORTED GLOBAL)
    target_link_libraries(${CMAKE_PROJECT_NAME}::ucx INTERFACE 
        ucx::uct
        ucx::ucp
        ucx::ucs
        ucx::ucm
    )

    set(UCX_FOUND TRUE CACHE INTERNAL TRUE)
endif()
