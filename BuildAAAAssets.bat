@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where python >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python 3 is required.
    exit /b 1
)
where msbuild >nul 2>nul
if errorlevel 1 call :FindVisualStudio
where msbuild >nul 2>nul
if errorlevel 1 (
    echo ERROR: MSBuild not found. Install the VS2022 Desktop development with C++ workload.
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

:FindVisualStudio
set "PLAGC_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PLAGC_VSWHERE%" exit /b 1
for /f "usebackq tokens=*" %%I in (`"%PLAGC_VSWHERE%" -latest -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PLAGC_VSINSTALL=%%I"
if not defined PLAGC_VSINSTALL exit /b 1
call "%PLAGC_VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64
exit /b %errorlevel%
