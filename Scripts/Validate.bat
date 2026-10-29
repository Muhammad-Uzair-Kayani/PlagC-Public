@echo off
setlocal
cd /d "%~dp0.."

python Scripts\Tests\EngineBoundaryTests.py
if errorlevel 1 exit /b 1
python Scripts\Tests\RuntimeStagingTests.py
if errorlevel 1 exit /b 1

where msbuild >nul 2>nul
if errorlevel 1 (
    echo Run this script from a Visual Studio 2022 Developer Command Prompt.
    exit /b 1
)

call GenerateProjects.bat
if errorlevel 1 exit /b 1

for %%C in (Debug Release Dist) do (
    msbuild Build\Generated\PlagC.sln /m /p:Configuration=%%C /p:Platform=x64
    if errorlevel 1 exit /b 1
    Bin\windows-x86_64\%%C\Sandbox.exe --headless --frames 120 --workers 1
    if errorlevel 1 exit /b 1
    Bin\windows-x86_64\%%C\Sandbox.exe --null --frames 120 --workers 4
    if errorlevel 1 exit /b 1
)

echo Automated engine validation passed.
exit /b 0
