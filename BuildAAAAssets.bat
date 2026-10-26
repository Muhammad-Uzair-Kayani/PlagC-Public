@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where python >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python 3 is required.
    exit /b 1
)
where msbuild >nul 2>nul
if errorlevel 1 (
    echo ERROR: Run from a VS2022 Developer Command Prompt with MSBuild on PATH.
    exit /b 1
)
if not exist "Build\Generated\PlagC.sln" (
    call GenerateProjects.bat
    if errorlevel 1 exit /b 1
)
rem Always request the incremental cooker build; an old EXE may lack current format validation.
msbuild Build\Generated\PlagC.sln /m /t:AssetCooker /p:Configuration=Release /p:Platform=x64
if errorlevel 1 exit /b 1
python Scripts\BuildAAAAssets.py --cooker Bin\windows-x86_64\Release\AssetCooker.exe %*
if errorlevel 1 exit /b 1
echo [PlagC] Source packages, cooked models and PBR textures validated.
echo [PlagC] Build Sandbox to verify and stage the complete runtime into its configuration folder.
exit /b 0
