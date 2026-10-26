@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

echo [PlagC] Fetching high-fidelity CC0 world assets...
python Scripts\FetchAAAWorld.py
if errorlevel 1 exit /b %errorlevel%

set "COOKER=Bin\windows-x86_64\Release\AssetCooker.exe"
if not exist "%COOKER%" (
    echo [PlagC] Release AssetCooker not found. Building it first...
    if not exist "Build\Generated\PlagC.sln" call GenerateProjects.bat
    if errorlevel 1 exit /b %errorlevel%
    where msbuild >nul 2>nul
    if errorlevel 1 (
        echo ERROR: MSBuild is not on PATH. Run this from a VS 2022 Developer Command Prompt,
        echo        or build AssetCooker Release once in Visual Studio.
        exit /b 2
    )
    msbuild Build\Generated\PlagC.sln /m /t:AssetCooker /p:Configuration=Release /p:Platform=x64
    if errorlevel 1 exit /b %errorlevel%
)

for /f "usebackq tokens=1,2 delims=|" %%A in ("Assets\Source\AAA\cook_manifest.txt") do (
    echo [PlagC] Cooking %%~nxA
    for %%D in ("%%B") do if not exist "%%~dpD" mkdir "%%~dpD"
    "%COOKER%" "%%A" "%%B"
    if errorlevel 1 exit /b !errorlevel!
)

echo [PlagC] AAA CC0 asset pack is ready.
endlocal
