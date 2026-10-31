@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where msbuild >nul 2>nul
if errorlevel 1 (
    echo [FAIL] MSBuild is not on PATH.
    echo Run this script from a Visual Studio 2022 Developer Command Prompt.
    exit /b 2
)

if not exist "Build\Generated\PlagC.sln" (
    echo [PlagC] Generated solution not found. Generating projects first...
    call GenerateProjects.bat
    if errorlevel 1 exit /b 1
)

echo [PlagC] Building complete Dist solution before pruning shared linker artifacts...
msbuild "Build\Generated\PlagC.sln" /m /nologo /v:minimal /p:Configuration=Dist /p:Platform=x64
if errorlevel 1 (
    echo.
    echo [FAIL] Dist build failed. Package output was not pruned.
    exit /b 1
)

echo [PlagC] Dist build complete. Pruning development-only artifacts...
python "Scripts\PruneDistOutput.py" --output "Bin\windows-x86_64\Dist"
if errorlevel 1 (
    echo.
    echo [FAIL] Dist pruning failed.
    exit /b 1
)

echo [PASS] Dist package built and pruned safely.
exit /b 0
