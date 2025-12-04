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

#ifndef U_PORT_WINDOWS_H
#define U_PORT_WINDOWS_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: MISC
 * -------------------------------------------------------------- */

#ifndef U_CX_PORT_INT32_T_IS_NOT_INT
# define U_CX_PORT_INT32_T_IS_NOT_INT
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: OS STUFF
 * -------------------------------------------------------------- */

#define U_CX_MUTEX_HANDLE                     CRITICAL_SECTION
#define U_CX_MUTEX_CREATE(mutex)              InitializeCriticalSection(&mutex)
#define U_CX_MUTEX_DELETE(mutex)              DeleteCriticalSection(&mutex)
#define U_CX_MUTEX_LOCK(mutex)                EnterCriticalSection(&mutex)
#define U_CX_MUTEX_TRY_LOCK(mutex, timeoutMs) uPortMutexTryLock(&mutex, timeoutMs)
#define U_CX_MUTEX_UNLOCK(mutex)              LeaveCriticalSection(&mutex)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: OS GENERIC
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Windows implementation of U_CX_MUTEX_TRY_LOCK()
 *
 * @param pMutex      Pointer to mutex
 * @param timeoutMs   Timeout in milliseconds
 * @return            true if mutex was locked, false on timeout
 */
bool uPortMutexTryLock(U_CX_MUTEX_HANDLE *pMutex, int32_t timeoutMs);

/**
 * @brief Get current time in milliseconds
 *
 * @return Current time in milliseconds
 */
int32_t uPortGetTimeMs(void);

#endif /* U_PORT_WINDOWS_H */
