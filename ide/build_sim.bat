@echo off
REM =============================================================================
REM Build Script for Multi-Core MESI Simulator
REM =============================================================================
REM Use with Visual Studio Developer Command Prompt
REM =============================================================================

setlocal enabledelayedexpansion
set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..") do set REPO_ROOT=%%~fI
set SRC_DIR=%REPO_ROOT%\src
set BUILD_DIR=%REPO_ROOT%\build
set OUTPUT_EXE=%BUILD_DIR%\sim.exe

echo Building Multi-Core MESI Simulator...
echo Repository: %REPO_ROOT%
echo Output:     %OUTPUT_EXE%
echo.

REM Check if cl.exe is available
where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: cl.exe not found!
    echo Please run this from a Visual Studio Developer Command Prompt.
    exit /b 1
)

REM Prepare build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Clean old artifacts
if exist "%OUTPUT_EXE%" del "%OUTPUT_EXE%"
del /q "%BUILD_DIR%\*.obj" 2>nul
del /q "%BUILD_DIR%\*.pdb" 2>nul

REM Compile all source files
cl /nologo /W4 /O2 /I "%SRC_DIR%" ^
    /Fo"%BUILD_DIR%\\" ^
    /Fe"%OUTPUT_EXE%" ^
    "%SRC_DIR%\main.c" ^
    "%SRC_DIR%\pipeline.c" ^
    "%SRC_DIR%\cache.c" ^
    "%SRC_DIR%\bus.c" ^
    /link /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! Created %OUTPUT_EXE%
    echo.
    echo Usage: sim.exe imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt ...
    echo   (27 command line arguments total)
    exit /b 0
) else (
    echo.
    echo Build FAILED!
    exit /b 1
)
