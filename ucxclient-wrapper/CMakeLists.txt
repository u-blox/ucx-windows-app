cmake_minimum_required(VERSION 3.20)
project(ucxclient_wrapper VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Use NORA-W36X module
set(UCX_MODULE "NORA-W36X" CACHE STRING "UCX module to use")

# Include ucxclient
include(../ucxclient/ucxclient.cmake)

# Windows UART port and OS port
set(UCXCLIENT_PORT_SRC 
    ${UCXCLIENT_PORT_DIR}/uart/u_port_uart_windows.c
    ${UCXCLIENT_PORT_DIR}/os/u_port_windows.c
)

# Wrapper library (shared DLL)
add_library(ucxclient_wrapper SHARED
    ucxclient_wrapper.c
    ucxclient_wrapper.h
    ucx_wrapper_config.h
    ${UCXCLIENT_UCX_API_SRC}
    ${UCXCLIENT_AT_API_SRC}
    ${UCXCLIENT_PORT_SRC}
)

target_include_directories(ucxclient_wrapper
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${UCXCLIENT_INC}
    PRIVATE ${UCXCLIENT_PORT_DIR}/os
)

# Add preprocessor definitions for Windows port
target_compile_definitions(ucxclient_wrapper PRIVATE
    U_CX_USE_URC_QUEUE=1
)

# Force include our config header first, then the Windows port header
if(MSVC)
    target_compile_options(ucxclient_wrapper PRIVATE 
        /FI"${CMAKE_CURRENT_SOURCE_DIR}/ucx_wrapper_config.h"
        /FI"${UCXCLIENT_PORT_DIR}/os/u_port_windows.h"
    )
else()
    target_compile_options(ucxclient_wrapper PRIVATE 
        -include "${CMAKE_CURRENT_SOURCE_DIR}/ucx_wrapper_config.h"
        -include "${UCXCLIENT_PORT_DIR}/os/u_port_windows.h"
    )
endif()

# Export all symbols for DLL
set_target_properties(ucxclient_wrapper PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install
install(TARGETS ucxclient_wrapper
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ucxclient_wrapper.h DESTINATION include)
