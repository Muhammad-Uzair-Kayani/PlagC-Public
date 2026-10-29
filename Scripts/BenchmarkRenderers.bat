@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0.."
set "BIN=%ROOT%\Bin\windows-x86_64\Release"

if not exist "%BIN%\Sandbox.exe" (
    echo Release Sandbox.exe was not found at:
    echo   %BIN%\Sandbox.exe
    echo Build Release after running GenerateProjects.bat --vulkan.
    exit /b 1
)

if not defined PC_RENDER_BENCH_RUNS set "PC_RENDER_BENCH_RUNS=2"
if not defined PC_RENDER_BENCH_COOLDOWN set "PC_RENDER_BENCH_COOLDOWN=3"
set "PC_FPS_OVERLAY=0"

pushd "%BIN%"
if not exist Logs mkdir Logs

echo ============================================================
echo PlagC renderer backend benchmark
echo Pairs    : %PC_RENDER_BENCH_RUNS%
echo Cooldown : %PC_RENDER_BENCH_COOLDOWN% seconds between runs
echo FPS OSD  : disabled for measurement
echo Order    : GL/VK, then VK/GL, alternating each pair
echo ============================================================

for /L %%R in (1,1,%PC_RENDER_BENCH_RUNS%) do (
    set /a ODD=%%R %% 2
    if !ODD! EQU 1 (
        call :RunBackend opengl OpenGL %%R
        if errorlevel 1 goto :Failed
        call :Cooldown
        call :RunBackend vulkan Vulkan %%R
        if errorlevel 1 goto :Failed
    ) else (
        call :RunBackend vulkan Vulkan %%R
        if errorlevel 1 goto :Failed
        call :Cooldown
        call :RunBackend opengl OpenGL %%R
        if errorlevel 1 goto :Failed
    )
    if not %%R==%PC_RENDER_BENCH_RUNS% call :Cooldown
)

echo.
echo Complete. Compare per-run CSV/log pairs under:
echo   %BIN%\Logs\RendererBenchmark-OpenGL-R*.csv
echo   %BIN%\Logs\RendererBenchmark-Vulkan-R*.csv
echo.
echo Use median plus P95/P99/hitch counts across runs. Alternating order reduces

echo warm-up, clock and thermal bias between APIs.
popd
exit /b 0

:RunBackend
set "BACKEND=%~1"
set "DISPLAY=%~2"
set "RUN=%~3"
echo.
echo Running %DISPLAY% pair %RUN%...
Sandbox.exe --renderer %BACKEND% --no-audio --performance-csv "Logs\RendererBenchmark-%DISPLAY%-R%RUN%.csv" --performance-warmup 240 --performance-seconds 20 > "Logs\RendererBenchmark-%DISPLAY%-R%RUN%.log" 2>&1
if errorlevel 1 (
    echo %DISPLAY% benchmark pair %RUN% failed. See Logs\RendererBenchmark-%DISPLAY%-R%RUN%.log
    exit /b 1
)
exit /b 0

:Cooldown
if %PC_RENDER_BENCH_COOLDOWN% GTR 0 timeout /t %PC_RENDER_BENCH_COOLDOWN% /nobreak >nul
exit /b 0

:Failed
popd
exit /b 1
