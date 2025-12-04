@echo off
REM ===================================
REM UCX Avalonia App Launcher
REM ===================================

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM Check if .NET is installed
where dotnet >nul 2>&1
if errorlevel 1 (
    echo [ERROR] .NET SDK not found!
    echo Please install .NET 9.0 SDK from https://dotnet.microsoft.com/download
    pause
    exit /b 1
)

REM Check if native DLL exists
set DLL_SOURCE=build-wrapper\bin\Release\ucxclient_wrapper.dll
set DLL_DEST_DEBUG=ucx-avalonia-app\bin\Debug\net9.0\ucxclient_wrapper.dll
set DLL_DEST_RELEASE=ucx-avalonia-app\bin\Release\net9.0\ucxclient_wrapper.dll

if not exist "%DLL_SOURCE%" (
    echo [WARNING] Native DLL not found: %DLL_SOURCE%
    echo.
    echo Building native wrapper DLL...
    call :build_native_dll
    if errorlevel 1 (
        echo [ERROR] Failed to build native DLL
        pause
        exit /b 1
    )
)

REM Copy DLL to output directories if they exist
if exist "ucx-avalonia-app\bin\Debug\net9.0" (
    copy /Y "%DLL_SOURCE%" "%DLL_DEST_DEBUG%" >nul 2>&1
)
if exist "ucx-avalonia-app\bin\Release\net9.0" (
    copy /Y "%DLL_SOURCE%" "%DLL_DEST_RELEASE%" >nul 2>&1
)

REM Run the Avalonia app
echo.
echo ===================================
echo Starting UCX Avalonia App
echo ===================================
echo.
cd ucx-avalonia-app
dotnet run

exit /b 0

REM ===================================
REM Build Native DLL
REM ===================================
:build_native_dll
echo.
echo [Step 1/2] Configuring CMake...
if not exist "build-wrapper" mkdir build-wrapper
cd build-wrapper
cmake -G "Visual Studio 17 2022" -A x64 ..\ucxclient-wrapper
if errorlevel 1 (
    cd ..
    exit /b 1
)

echo.
echo [Step 2/2] Building native DLL...
cmake --build . --config Release
if errorlevel 1 (
    cd ..
    exit /b 1
)

cd ..
echo.
echo [OK] Native DLL built successfully
exit /b 0
