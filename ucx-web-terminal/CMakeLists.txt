# Copyright 2025 u-blox
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.10)
project(ucx-web-terminal)

# Set Emscripten compiler
set(CMAKE_C_COMPILER "emcc")
set(CMAKE_CXX_COMPILER "em++")

# Include ucxclient source paths
set(UCX_CLIENT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../ucxclient")
set(UCX_CLIENT_SRC "${UCX_CLIENT_PATH}/ucxclient")
set(UCX_CLIENT_INC "${UCX_CLIENT_PATH}/inc")

# Include directories
include_directories(
    ${UCX_CLIENT_INC}
    ${UCX_CLIENT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Source files
set(SOURCES
    # Web-specific port layer
    u_port_web.c
    
    # WASM wrapper functions
    ucx_wasm_wrapper.c
    
    # ucxclient core sources
    ${UCX_CLIENT_SRC}/ucx_client.c
    ${UCX_CLIENT_SRC}/ucx_wifi.c
    ${UCX_CLIENT_SRC}/ucx_bt.c
    ${UCX_CLIENT_SRC}/ucx_data.c
    ${UCX_CLIENT_SRC}/ucx_mqtt.c
    ${UCX_CLIENT_SRC}/ucx_http.c
    ${UCX_CLIENT_SRC}/ucx_socket.c
    ${UCX_CLIENT_SRC}/ucx_handle.c
    ${UCX_CLIENT_SRC}/ucx_at.c
    ${UCX_CLIENT_SRC}/ucx_parser.c
    ${UCX_CLIENT_SRC}/ucx_buffer.c
    ${UCX_CLIENT_SRC}/ucx_urc.c
)

# Create WASM module
add_executable(ucxclient ${SOURCES})

# Emscripten link flags
set_target_properties(ucxclient PROPERTIES
    LINK_FLAGS "\
        -s WASM=1 \
        -s MODULARIZE=1 \
        -s EXPORT_NAME='createUCXModule' \
        -s ASYNCIFY=1 \
        -s ASYNCIFY_IMPORTS=['js_serial_write','js_serial_read'] \
        -s EXPORTED_FUNCTIONS='[\
            \"_ucx_create\",\
            \"_ucx_destroy\",\
            \"_ucx_wifi_connect\",\
            \"_ucx_wifi_disconnect\",\
            \"_ucx_wifi_scan_begin\",\
            \"_ucx_wifi_scan_get_next\",\
            \"_ucx_wifi_scan_end\",\
            \"_ucx_wifi_get_connection_info\",\
            \"_malloc\",\
            \"_free\"\
        ]' \
        -s EXPORTED_RUNTIME_METHODS='[\
            \"ccall\",\
            \"cwrap\",\
            \"UTF8ToString\",\
            \"stringToUTF8\",\
            \"lengthBytesUTF8\"\
        ]' \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s INITIAL_MEMORY=16MB \
        -s STACK_SIZE=2MB \
        --js-library ${CMAKE_CURRENT_SOURCE_DIR}/library.js \
        -O2 \
        --no-entry"
)

# Output files
set_target_properties(ucxclient PROPERTIES
    SUFFIX ".js"
    OUTPUT_NAME "ucxclient"
)

# Installation (copy to output directory)
install(TARGETS ucxclient DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dist)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ucxclient.wasm DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dist)
