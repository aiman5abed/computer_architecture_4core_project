@echo off
REM =============================================================================
REM Build Script for Multi-Core MESI Simulator (wrapper)
REM =============================================================================
REM Use with Visual Studio Developer Command Prompt
REM =============================================================================

setlocal
set SCRIPT_DIR=%~dp0
call "%SCRIPT_DIR%build_sim.bat" %*
exit /b %ERRORLEVEL%
