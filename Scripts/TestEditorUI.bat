@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "BACKEND=%~2"
if "%BACKEND%"=="" set "BACKEND=vulkan"

if /I not "%BACKEND%"=="vulkan" if /I not "%BACKEND%"=="opengl" (
    echo Usage: TestEditorUI.bat [Debug^|Release^|Dist] [vulkan^|opengl]
    exit /b 2
)

set "RUNDIR=%ROOT%\Bin\windows-x86_64\%CONFIG%"
set "EDITOR=%RUNDIR%\Editor.exe"
if not exist "%EDITOR%" (
    echo Editor binary not found: "%EDITOR%"
    echo Build the Editor target for %CONFIG% first.
    exit /b 2
)

pushd "%RUNDIR%" || exit /b 2
if exist "Logs\Ogre.log" del /q "Logs\Ogre.log"
if exist "Logs\PlagC.log" del /q "Logs\PlagC.log"

echo [Editor UI] Running %BACKEND% presentation smoke test...
"%EDITOR%" --renderer %BACKEND% --frames 12 --workers 1 --no-audio
set "RUN_EXIT=%ERRORLEVEL%"
if not "%RUN_EXIT%"=="0" (
    echo [FAIL] Editor exited with code %RUN_EXIT%.
    popd
    exit /b 1
)

if not exist "Logs\Ogre.log" (
    echo [FAIL] Logs\Ogre.log was not produced.
    popd
    exit /b 1
)

findstr /C:"[PlagC UI] ERROR: submitted UI was not presented" "Logs\Ogre.log" >nul
if not errorlevel 1 (
    echo [FAIL] ImGui commands were submitted but never presented.
    echo See "%RUNDIR%\Logs\Ogre.log".
    popd
    exit /b 1
)

findstr /C:"[PlagC UI] explicit output=" "Logs\Ogre.log" >nul
if errorlevel 1 (
    echo [FAIL] No editor UI presentation event was recorded.
    echo The workspace listener or UI render path did not execute.
    echo See "%RUNDIR%\Logs\Ogre.log".
    popd
    exit /b 1
)

findstr /C:" vertices=0 " "Logs\Ogre.log" >nul
if not errorlevel 1 (
    echo [FAIL] Editor UI presentation contained zero vertices.
    popd
    exit /b 1
)

findstr /C:" commands=0" "Logs\Ogre.log" >nul
if not errorlevel 1 (
    echo [FAIL] Editor UI presentation contained zero draw commands.
    popd
    exit /b 1
)

echo [PASS] %BACKEND% editor UI commands reached the main Ogre output.
popd
exit /b 0
