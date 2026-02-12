@echo off
REM ===================================
REM UCX Web App Launcher
REM ===================================

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM Check if Python is installed
where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found!
    echo Please install Python from https://www.python.org/downloads/
    pause
    exit /b 1
)

REM Check if emsdk folder exists and set up environment
if exist "emsdk" (
    echo.
    echo ===================================
    echo Setting up Emscripten environment
    echo ===================================
    echo.
    cd emsdk
    call emsdk_env.bat
    cd ..
) else (
    REM Check if emcc is in PATH
    where emcc >nul 2>&1
    if errorlevel 1 (
        echo.
        echo ===================================
        echo WARNING: Emscripten not found
        echo ===================================
        echo.
        echo The full WASM build requires Emscripten SDK.
        echo.
        echo Would you like to install Emscripten now? (Y/N)
        set /p INSTALL_EMSDK=
        
        if /i "!INSTALL_EMSDK!"=="Y" (
            echo.
            echo Installing Emscripten SDK...
            echo.
            
            REM Check if git is installed
            where git >nul 2>&1
            if errorlevel 1 (
                echo [ERROR] Git not found! Please install Git first.
                echo Download from: https://git-scm.com/download/win
                pause
                exit /b 1
            )
            
            REM Clone emsdk if not already present
            if not exist "emsdk" (
                echo Cloning Emscripten SDK repository...
            git clone https://github.com/emscripten-core/emsdk.git
            if errorlevel 1 (
                echo [ERROR] Failed to clone emsdk
                pause
                exit /b 1
            )
        )
        
        REM Install and activate latest Emscripten
        cd emsdk
        echo Installing latest Emscripten version...
        call emsdk install latest
        if errorlevel 1 (
            echo [ERROR] Failed to install Emscripten
            cd ..
            pause
            exit /b 1
        )
        
        echo Activating Emscripten...
        call emsdk activate latest
        if errorlevel 1 (
            echo [ERROR] Failed to activate Emscripten
            cd ..
            pause
            exit /b 1
        )
        
        echo Setting up environment...
        call emsdk_env.bat
        
        cd ..
        
        echo.
        echo [OK] Emscripten installed successfully!
        echo.
        ) else (
            echo.
            echo Using stub version for serial port testing only.
            echo To install manually later, see:
            echo https://emscripten.org/docs/getting_started/downloads.html
            echo.
            timeout /t 3 >nul
            goto :start_server
        )
    )
)
REM Check and initialize ucxclient submodule (without nested submodules)
echo.
echo ===================================
echo Checking ucxclient submodule
echo ===================================
echo.

if not exist "ucxclient\.git" (
    echo Initializing ucxclient submodule...
    git submodule update --init ucxclient
    if errorlevel 1 (
        echo [ERROR] Failed to initialize ucxclient submodule
        pause
        exit /b 1
    )
    echo [OK] ucxclient submodule initialized
) else (
    echo [OK] ucxclient submodule already initialized
)

REM Build WASM module with Emscripten
echo.
echo ===================================
echo Building WASM Module
echo ===================================
echo.

cd ucx-web-app

REM Create build directory if it doesn't exist
if not exist "build" mkdir build

REM Configure with Emscripten
echo [Step 1/2] Configuring CMake with Emscripten...
call emcmake cmake -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build the WASM module
echo.
echo [Step 2/2] Building WASM module...

REM First build object files with emmake
call emmake cmake --build build --target ucxclient --config Release 2>nul
REM Ignore error from emmake (linking fails), but object files are built

REM Now link directly with Python to avoid .bat/.ps1 wrapper issues
echo Linking with emcc (Python)...
cd build
%EMSDK_PYTHON% "%EMSDK%/upstream/emscripten/emcc.py" -O3 -DNDEBUG -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME=createUCXModule -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8 -s EXPORTED_FUNCTIONS=_ucx_init,_ucx_deinit,_ucx_set_log_level,_ucx_read_int32,_ucx_copy_bytes,_ucx_wifi_connect,_ucx_wifi_disconnect,_ucx_wifi_scan_begin,_ucx_wifi_scan_get_next,_ucx_wifi_scan_end,_ucx_wifi_get_ip,_ucx_send_at_command,_ucx_get_version,_ucx_bt_discovery_begin,_ucx_bt_discovery_get_next,_ucx_bt_discovery_end,_ucx_bt_connect,_ucx_bt_disconnect,_ucx_bt_set_local_name,_ucx_bt_get_local_name,_ucx_bt_set_pairing_mode,_ucx_bt_set_io_capabilities,_ucx_bt_user_confirmation,_ucx_gatt_discover_services_begin,_ucx_gatt_discover_services_get_next,_ucx_gatt_discover_services_end,_ucx_gatt_discover_chars_begin,_ucx_gatt_discover_chars_get_next,_ucx_gatt_discover_chars_end,_ucx_gatt_discover_descriptors_begin,_ucx_gatt_discover_descriptors_get_next,_ucx_gatt_discover_descriptors_end,_ucx_gatt_read,_ucx_gatt_write,_ucx_gatt_config_write,_ucx_gatt_server_service_define,_ucx_gatt_server_char_define,_ucx_gatt_server_activate,_ucx_gatt_server_set_value,_ucx_gatt_server_send_notification,_ucx_bt_advertise_start,_ucx_bt_advertise_stop,_malloc,_free -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=16777216 -s ASYNCIFY=1 -s ASYNCIFY_IMPORTS=js_serial_write,js_serial_read --js-library ../library.js -O2 --no-entry CMakeFiles/ucxclient.dir/u_port_web.c.o CMakeFiles/ucxclient.dir/ucx_wasm_wrapper.c.o "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/src/u_cx_at_client.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/src/u_cx_at_params.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/src/u_cx_at_urc_queue.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/src/u_cx_at_util.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/src/u_cx_log.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/src/u_cx_xmodem.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_bluetooth.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_diagnostics.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_error_codes.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_gatt_client.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_gatt_server.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_general.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_http.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_mqtt.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_network_time.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_power.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_security.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_socket.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_sps.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_system.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_urc.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/generated/NORA-W36X/u_cx_wifi.c.o" "CMakeFiles/ucxclient.dir/C_/u-blox/ucx-web-app/ucxclient/ucx_api/u_cx.c.o" -o ucxclient.js
cd ..

if not exist "build\ucxclient.js" (
    echo [ERROR] Build failed
    pause
    exit /b 1
)

echo.
echo [OK] WASM module built successfully
echo Output: ucx-web-app\build\ucxclient.js and ucxclient.wasm
echo.

REM Copy build outputs to root for easier serving
if exist "build\ucxclient.js" (
    copy /Y "build\ucxclient.js" "ucxclient.js" >nul
    echo Copied ucxclient.js
)
if exist "build\ucxclient.wasm" (
    copy /Y "build\ucxclient.wasm" "ucxclient.wasm" >nul
    echo Copied ucxclient.wasm
)

cd ..

:start_server
REM Start web server
echo.
echo ===================================
echo Starting UCX Web App
echo ===================================
echo.
echo Server will start on http://localhost:8000
echo Open your browser to: http://localhost:8000/ucx-web-app/index.html
echo.
echo Press Ctrl+C to stop the server
echo.

python -m http.server 8000

exit /b 0
