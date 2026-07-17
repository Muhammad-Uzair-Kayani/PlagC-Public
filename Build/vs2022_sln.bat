@echo off
title Generate VS2022 Solution

echo Running Premake5 for Visual Studio 2022...
echo.

:: Move to the project root (parent of Build)
cd /d "%~dp0.."

:: Run Premake
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