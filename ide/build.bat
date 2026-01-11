@echo off
REM =============================================================================
REM Build Script for Multi-Core MESI Simulator
REM =============================================================================
REM Use with Visual Studio Developer Command Prompt
REM =============================================================================

echo Building Multi-Core MESI Simulator...
echo.

REM Check if cl.exe is available
where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: cl.exe not found!
    echo Please run this from Visual Studio Developer Command Prompt
    echo Or install Visual Studio Build Tools
    exit /b 1
)

REM Clean old files
if exist sim.exe del sim.exe
if exist *.obj del *.obj

REM Compile all source files
cl /nologo /W3 /O2 /I include /Fe:sim.exe ^
    src\main.c ^
    src\pipeline.c ^
    src\cache.c ^
    src\bus.c ^
    /link /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! Created sim.exe
    echo.
    echo Usage: sim.exe imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt ...
    echo   (27 command line arguments total)
    del *.obj 2>nul
) else (
    echo.
    echo Build FAILED!
    exit /b 1
)
