@echo off
REM Windows launcher for UCX Avalonia App

setlocal enabledelayedexpansion

cd /d "%~dp0"

echo UCX Avalonia App Launcher
echo =========================
echo.

REM Check if .NET is installed
where dotnet >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: .NET SDK not found!
    echo Please install .NET 9.0 SDK or later from:
    echo https://dotnet.microsoft.com/download
    exit /b 1
)

for /f "delims=" %%i in ('dotnet --version') do set DOTNET_VERSION=%%i
echo [OK] .NET SDK: %DOTNET_VERSION%

REM Check if native library exists
set LIB_SRC=..\build-wrapper\bin\Release\ucxclient_wrapper.dll
if not exist "%LIB_SRC%" (
    echo.
    echo WARNING: Native library not found: %LIB_SRC%
    echo You need to build the native wrapper first.
    echo.
    echo Build instructions:
    echo   cd ..\ucxclient-wrapper
    echo   mkdir build ^&^& cd build
    echo   cmake ..
    echo   cmake --build . --config Release
    echo.
    choice /C YN /M "Continue anyway"
    if errorlevel 2 exit /b 1
) else (
    echo [OK] Native library: ucxclient_wrapper.dll
    
    REM Copy native library to output directory
    set TARGET_DIR=bin\Debug\net9.0
    if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"
    copy /Y "%LIB_SRC%" "%TARGET_DIR%\" >nul
    echo [OK] Copied ucxclient_wrapper.dll to %TARGET_DIR%
)

echo.
echo Building and running application...
echo.

REM Build and run
dotnet build
if %errorlevel% neq 0 (
    echo.
    echo Build failed!
    exit /b %errorlevel%
)

dotnet run
