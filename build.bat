@echo off
title Generate VS2022 Solution
echo Running Premake5 for Visual Studio 2022...
echo.

:: Ensure the script runs from the directory it is located in
cd /d "%~dp0"

:: Run the premake command
vendor\premake\premake5.exe vs2022

:: Check if the command succeeded
if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] Visual Studio 2022 project files generated.
) else (
    echo.
    echo [ERROR] Premake failed to generate project files.
)

echo.
pause
