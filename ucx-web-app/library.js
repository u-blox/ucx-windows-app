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
 * @file library.js
 * @brief Emscripten JS library stub
 *
 * This file is referenced by the Emscripten --js-library flag at link time.
 * All serial I/O is now handled by EM_JS functions in u_port_web.c which
 * call Module.serialWrite / serialRead / serialAvailable (wired in
 * wasm-bridge.js at runtime). This file is kept as an empty stub so the
 * build command does not need to be changed.
 */

mergeInto(LibraryManager.library, {
    // Intentionally empty - all bindings are handled by EM_JS + wasm-bridge.js
});
