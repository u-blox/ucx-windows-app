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

# Disable response files for linking (Emscripten doesn't handle them well on Windows)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS OFF)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES OFF)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
set(CMAKE_C_RESPONSE_FILE_LINK_FLAG "@")

# Set Emscripten compiler
set(CMAKE_C_COMPILER "emcc")
set(CMAKE_CXX_COMPILER "em++")

# Set UCX module for NORA-W36
set(UCX_MODULE "NORA-W36X")

# Include ucxclient configuration
set(UCX_CLIENT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../ucxclient")
include(${UCX_CLIENT_PATH}/ucxclient.cmake)

# Include directories
include_directories(
    ${UCXCLIENT_INC}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${UCX_CLIENT_PATH}/ports
)

# Source files
set(SOURCES
    # Web-specific port layer
    u_port_web.c
    
    # WASM wrapper functions
    ucx_wasm_wrapper.c
    
    # ucxclient core sources from ucxclient.cmake
    ${UCXCLIENT_AT_API_SRC}
    ${UCXCLIENT_UCX_API_SRC}
)

# Create WASM module
add_executable(ucxclient ${SOURCES})

# Add compile definitions for WebAssembly port
target_compile_definitions(ucxclient PRIVATE
    U_CX_PORT_HEADER_FILE="u_port_clib.h"
)

# Emscripten link options
target_link_options(ucxclient PRIVATE
    "SHELL:-s WASM=1"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_NAME=createUCXModule"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s INITIAL_MEMORY=16777216"
    "-O2"
    "--no-entry"
)

# Output files
set_target_properties(ucxclient PROPERTIES
    SUFFIX ".js"
    OUTPUT_NAME "ucxclient"
)

# Installation (copy to output directory)
install(TARGETS ucxclient DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dist)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ucxclient.wasm DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dist)
