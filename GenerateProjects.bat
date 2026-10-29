@echo off
setlocal
cd /d "%~dp0"
call FetchDependencies.bat
if errorlevel 1 exit /b 1

set "PREMAKE=Tools\premake5.exe"
if not exist "%PREMAKE%" (
    where premake5 >nul 2>nul
    if errorlevel 1 (
        echo Install Premake 5 in Tools\premake5.exe or on PATH. See README.md.
        exit /b 1
    )
    set "PREMAKE=premake5"
)
"%PREMAKE%" vs2022 %*
exit /b %errorlevel%
