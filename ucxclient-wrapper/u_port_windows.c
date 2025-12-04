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

#include "u_port_windows.h"
#include <windows.h>

bool uPortMutexTryLock(U_CX_MUTEX_HANDLE *pMutex, int32_t timeoutMs)
{
    // Windows CRITICAL_SECTION doesn't support timeout directly
    // For simplicity, just try to lock (this could be improved with a timed approach)
    if (TryEnterCriticalSection(pMutex)) {
        return true;
    }
    
    // Simple timeout implementation
    int32_t startTime = uPortGetTimeMs();
    while ((uPortGetTimeMs() - startTime) < timeoutMs) {
        if (TryEnterCriticalSection(pMutex)) {
            return true;
        }
        Sleep(1); // Sleep 1ms between attempts
    }
    
    return false;
}

int32_t uPortGetTimeMs(void)
{
    return (int32_t)GetTickCount();
}
