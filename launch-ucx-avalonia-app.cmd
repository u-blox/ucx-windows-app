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

REM Check if CMake is installed
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found!
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

REM Always rebuild the native DLL to ensure latest changes
echo.
echo ===================================
echo Building Native Wrapper DLL
echo ===================================
call :build_native_dll
if errorlevel 1 (
    echo [ERROR] Failed to build native DLL
    pause
    exit /b 1
)

REM Copy DLL to output directories
set DLL_SOURCE=ucxclient-wrapper\build\bin\Release\ucxclient_wrapper.dll

echo.
echo Copying DLL to output directories...
if not exist "ucx-avalonia-app\bin\Debug\net9.0" mkdir "ucx-avalonia-app\bin\Debug\net9.0"
copy /Y "%DLL_SOURCE%" "ucx-avalonia-app\bin\Debug\net9.0\ucxclient_wrapper.dll" >nul 2>&1

if not exist "ucx-avalonia-app\bin\Release\net9.0" mkdir "ucx-avalonia-app\bin\Release\net9.0"
copy /Y "%DLL_SOURCE%" "ucx-avalonia-app\bin\Release\net9.0\ucxclient_wrapper.dll" >nul 2>&1

REM Build the Avalonia app
echo.
echo ===================================
echo Building UCX Avalonia App
echo ===================================
cd ucx-avalonia-app
dotnet build -c Release
if errorlevel 1 (
    echo [ERROR] Failed to build Avalonia app
    cd ..
    pause
    exit /b 1
)

REM Run the Avalonia app
echo.
echo ===================================
echo Starting UCX Avalonia App
echo ===================================
echo.
dotnet run -c Release --no-build

cd ..
exit /b 0

REM ===================================
REM Build Native DLL
REM ===================================
:build_native_dll
echo.
echo [Step 1/2] Configuring CMake...
cd ucxclient-wrapper

REM Clean and reconfigure if needed
if not exist "build" (
    mkdir build
)

cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    cd ..\..
    exit /b 1
)

echo.
echo [Step 2/2] Building native DLL (Release)...
cmake --build . --config Release
if errorlevel 1 (
    cd ..\..
    exit /b 1
)

cd ..\..
echo.
echo [OK] Native DLL built successfully
exit /b 0
