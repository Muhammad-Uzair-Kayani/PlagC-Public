@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" if /I not "%CONFIG%"=="Dist" (
    echo Usage: BuildEditorFast.bat [Debug^|Release^|Dist]
    exit /b 2
)

set "PROJECT=Build\Generated\Editor.vcxproj"
if not exist "%PROJECT%" (
    echo [FAIL] %PROJECT% does not exist.
    echo Run GenerateProjects.bat once, then perform one normal full build first.
    exit /b 2
)

where msbuild >nul 2>nul
if errorlevel 1 (
    echo [FAIL] MSBuild is not on PATH.
    echo Run this script from a Visual Studio 2022 Developer Command Prompt.
    exit /b 2
)

set "OUT=Bin\windows-x86_64\%CONFIG%"
if not exist "%OUT%\PlagC.lib" (
    echo [FAIL] Engine/dependency libraries have not been built for %CONFIG% yet.
    echo Perform one normal Editor build first. After that this script skips Ogre/Jolt/etc.
    exit /b 2
)

echo [PlagC] Fast Editor incremental build: %CONFIG%
echo [PlagC] Project references are disabled; existing engine/vendor libraries are reused.
msbuild "%PROJECT%" /m /nologo /v:minimal /p:Configuration=%CONFIG% /p:Platform=x64 /p:BuildProjectReferences=false
if errorlevel 1 (
    echo.
    echo [FAIL] Fast Editor build failed.
    echo If a dependency or public engine ABI changed, perform one normal full build first.
    exit /b 1
)

echo [PASS] Editor incremental build completed without rebuilding project references.
exit /b 0
