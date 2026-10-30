@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."

python Scripts\Tests\EngineBoundaryTests.py
if errorlevel 1 exit /b 1
python Scripts\Tests\RuntimeStagingTests.py
if errorlevel 1 exit /b 1
python Scripts\Tests\EditorArchitectureTests.py
if errorlevel 1 exit /b 1
python Scripts\Tests\EditorPlayRuntimeTests.py
if errorlevel 1 exit /b 1

where msbuild >nul 2>nul
if errorlevel 1 (
    echo Run this script from a Visual Studio 2022 Developer Command Prompt.
    exit /b 1
)

call GenerateProjects.bat
if errorlevel 1 exit /b 1

set "VALIDATION_LOG_DIR=Build\ValidationLogs"
if not exist "%VALIDATION_LOG_DIR%" mkdir "%VALIDATION_LOG_DIR%"

for %%C in (Debug Release Dist) do (
    set "VALIDATION_LOG=%VALIDATION_LOG_DIR%\%%C.log"
    echo Building %%C. Full MSBuild log: !VALIDATION_LOG!
    msbuild Build\Generated\PlagC.sln /m /nologo /v:minimal /p:Configuration=%%C /p:Platform=x64 /fl /flp:logfile=!VALIDATION_LOG!;verbosity=normal
    if errorlevel 1 (
        echo.
        echo Native build failed for %%C.
        echo Compiler errors from !VALIDATION_LOG!:
        findstr /I /C:" fatal error C" /C:": error C" "!VALIDATION_LOG!"
        echo.
        echo Full log: !VALIDATION_LOG!
        exit /b 1
    )

    Bin\windows-x86_64\%%C\Sandbox.exe --headless --frames 120 --workers 1
    if errorlevel 1 exit /b 1
    Bin\windows-x86_64\%%C\Sandbox.exe --null --frames 120 --workers 4
    if errorlevel 1 exit /b 1

    echo Running %%C editor offscreen OpenGL smoke test.
    Bin\windows-x86_64\%%C\Editor.exe --offscreen --renderer opengl --frames 3 --workers 1 --no-audio
    if errorlevel 1 (
        echo Editor smoke test failed for %%C.
        exit /b 1
    )
)

echo Automated engine and editor validation passed.
exit /b 0
