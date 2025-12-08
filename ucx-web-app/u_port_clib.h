/*
 * Copyright 2025 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file u_port_clib.h
 * @brief WebAssembly/Browser platform configuration for ucxclient
 * 
 * WebAssembly runs single-threaded, so mutexes are no-ops
 */

#ifndef U_PORT_CLIB_H
#define U_PORT_CLIB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ----------------------------------------------------------------
 * MUTEX CONFIGURATION (WebAssembly is single-threaded)
 * -------------------------------------------------------------- */

// Mutex handle is just a dummy int since WebAssembly is single-threaded
typedef int uCxMutexHandle_t;

#define U_CX_MUTEX_HANDLE uCxMutexHandle_t

// Mutex create/destroy/lock/unlock are no-ops in single-threaded environment
#define U_CX_MUTEX_CREATE(name) ((uCxMutexHandle_t)0)
#define U_CX_MUTEX_DELETE(mutex) do {} while(0)
#define U_CX_MUTEX_LOCK(mutex) do {} while(0)
#define U_CX_MUTEX_UNLOCK(mutex) do {} while(0)
#define U_CX_MUTEX_TRY_LOCK(mutex, timeoutMs) (0)  // Always succeeds immediately

#endif // U_PORT_CLIB_H
