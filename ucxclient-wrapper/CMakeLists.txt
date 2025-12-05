cmake_minimum_required(VERSION 3.20)
project(ucxclient_wrapper VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Use NORA-W36X module
set(UCX_MODULE "NORA-W36X" CACHE STRING "UCX module to use")

# Include ucxclient
include(../ucxclient/ucxclient.cmake)

# Platform-specific UART port and OS port
if(WIN32)
    set(UCXCLIENT_PORT_SRC 
        ${UCXCLIENT_PORT_DIR}/uart/u_port_uart_windows.c
        ${UCXCLIENT_PORT_DIR}/os/u_port_windows.c
    )
    set(PORT_HEADER "${UCXCLIENT_PORT_DIR}/os/u_port_windows.h")
elseif(APPLE)
    set(UCXCLIENT_PORT_SRC 
        ${UCXCLIENT_PORT_DIR}/uart/u_port_uart_linux.c
        ${UCXCLIENT_PORT_DIR}/os/u_port_posix.c
    )
    set(PORT_HEADER "${UCXCLIENT_PORT_DIR}/os/u_port_posix.h")
elseif(UNIX)
    set(UCXCLIENT_PORT_SRC 
        ${UCXCLIENT_PORT_DIR}/uart/u_port_uart_linux.c
        ${UCXCLIENT_PORT_DIR}/os/u_port_posix.c
    )
    set(PORT_HEADER "${UCXCLIENT_PORT_DIR}/os/u_port_posix.h")
endif()

# Wrapper library (shared library)
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

# Add preprocessor definitions
target_compile_definitions(ucxclient_wrapper PRIVATE
    U_CX_USE_URC_QUEUE=1
)

# Force include our config header first, then the platform port header
if(MSVC)
    target_compile_options(ucxclient_wrapper PRIVATE 
        /FI"${CMAKE_CURRENT_SOURCE_DIR}/ucx_wrapper_config.h"
        /FI"${PORT_HEADER}"
    )
else()
    target_compile_options(ucxclient_wrapper PRIVATE 
        -include "${CMAKE_CURRENT_SOURCE_DIR}/ucx_wrapper_config.h"
        -include "${PORT_HEADER}"
    )
endif()

# Platform-specific settings
if(WIN32)
    # Export all symbols for DLL
    set_target_properties(ucxclient_wrapper PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
elseif(APPLE)
    # macOS dynamic library settings
    set_target_properties(ucxclient_wrapper PROPERTIES
        MACOSX_RPATH ON
    )
elseif(UNIX)
    # Linux shared library settings
    set_target_properties(ucxclient_wrapper PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )
endif()

# Output directory
set_target_properties(ucxclient_wrapper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install
install(TARGETS ucxclient_wrapper
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ucxclient_wrapper.h DESTINATION include)
