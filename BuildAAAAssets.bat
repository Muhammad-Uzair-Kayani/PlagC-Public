@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where python >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python 3 is required.
    exit /b 1
)
set "PLAGC_MSBUILD="
where msbuild >nul 2>nul
if not errorlevel 1 set "PLAGC_MSBUILD=msbuild"
if not defined PLAGC_MSBUILD call :FindVisualStudio
if not defined PLAGC_MSBUILD (
    echo ERROR: No usable VS2022 MSBuild installation was found.
    echo Run Common7\Tools\VsDevCmd.bat from your Visual Studio installation and retry.
    exit /b 1
)
if not exist "Build\Generated\PlagC.sln" (
    call GenerateProjects.bat
    if errorlevel 1 exit /b 1
)
rem Always request the incremental cooker build; an old EXE may lack current format validation.
echo [PlagC] Building AssetCooker with "%PLAGC_MSBUILD%"
"%PLAGC_MSBUILD%" Build\Generated\PlagC.sln /m /t:AssetCooker /p:Configuration=Release /p:Platform=x64
if errorlevel 1 exit /b 1
python Scripts\BuildAAAAssets.py --cooker Bin\windows-x86_64\Release\AssetCooker.exe %*
if errorlevel 1 exit /b 1
echo [PlagC] Source packages, cooked models and PBR textures validated.
echo [PlagC] Build Sandbox to verify and stage the complete runtime into its configuration folder.
exit /b 0

:FindVisualStudio
rem An existing developer environment may identify a custom install location.
if defined VSINSTALLDIR call :TryVisualStudio "%VSINSTALLDIR%"
if defined PLAGC_MSBUILD exit /b 0
set "PLAGC_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PLAGC_VSWHERE%" goto ProbeDefaults
rem Execute directly rather than through FOR /F command substitution.
rem Do not require a registered workload: repaired installs can have incomplete metadata.
set "PLAGC_VSRESULT=%TEMP%\PlagC-VS-%RANDOM%-%RANDOM%.txt"
"%PLAGC_VSWHERE%" -all -version "[17.0,18.0)" -products * -property installationPath > "%PLAGC_VSRESULT%"
for /f "usebackq delims=" %%I in ("%PLAGC_VSRESULT%") do if not defined PLAGC_MSBUILD call :TryVisualStudio "%%I"
del /q "%PLAGC_VSRESULT%" >nul 2>nul
if defined PLAGC_MSBUILD exit /b 0

:ProbeDefaults
rem Fall back to physical folders when vswhere registration is absent or stale.
for %%E in (Community Professional Enterprise BuildTools) do if not defined PLAGC_MSBUILD call :TryVisualStudio "%ProgramFiles%\Microsoft Visual Studio\2022\%%E"
for %%E in (Community Professional Enterprise BuildTools) do if not defined PLAGC_MSBUILD call :TryVisualStudio "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\%%E"
if defined PLAGC_MSBUILD exit /b 0
exit /b 1

:TryVisualStudio
if not exist "%~1\MSBuild\Current\Bin\MSBuild.exe" exit /b 1
if not exist "%~1\Common7\Tools\VsDevCmd.bat" exit /b 1
echo [PlagC] Initializing Visual Studio at "%~1"
call "%~1\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1
rem Use the verified absolute executable path even if PATH initialization is incomplete.
set "PLAGC_MSBUILD=%~1\MSBuild\Current\Bin\MSBuild.exe"
exit /b 0
