@echo off
setlocal

set "ROOT=%~dp0.."
set "BIN=%ROOT%\Bin\windows-x86_64\Release"

if not exist "%BIN%\Sandbox.exe" (
    echo Release Sandbox.exe was not found at:
    echo   %BIN%\Sandbox.exe
    echo Build Release after running GenerateProjects.bat --vulkan.
    exit /b 1
)

if not defined PC_RENDER_BENCH_OBJECTS set "PC_RENDER_BENCH_OBJECTS=2500"
if not defined PC_RENDER_BENCH_MATERIALS set "PC_RENDER_BENCH_MATERIALS=%PC_RENDER_BENCH_OBJECTS%"
rem Keep official captures free of the extra overlay draw/upload. Manual runs show FPS by default.
set "PC_FPS_OVERLAY=0"

pushd "%BIN%"
if not exist Logs mkdir Logs

echo ============================================================
echo PlagC renderer benchmark
echo Objects  : %PC_RENDER_BENCH_OBJECTS%
echo Materials: %PC_RENDER_BENCH_MATERIALS%
echo FPS OSD  : disabled for measurement
echo ============================================================

echo Running OpenGL...
echo RendererBenchmark|Sandbox.exe --renderer opengl --no-audio --performance-csv Logs\RendererBenchmark-OpenGL.csv --performance-warmup 240 --performance-seconds 20 > Logs\RendererBenchmark-OpenGL.log 2>&1
if errorlevel 1 (
    echo OpenGL benchmark failed. See Logs\RendererBenchmark-OpenGL.log
    popd
    exit /b 1
)

echo Running Vulkan...
echo RendererBenchmark|Sandbox.exe --renderer vulkan --no-audio --performance-csv Logs\RendererBenchmark-Vulkan.csv --performance-warmup 240 --performance-seconds 20 > Logs\RendererBenchmark-Vulkan.log 2>&1
if errorlevel 1 (
    echo Vulkan benchmark failed. See Logs\RendererBenchmark-Vulkan.log
    popd
    exit /b 1
)

echo.
echo Complete. Compare:
echo   %BIN%\Logs\RendererBenchmark-OpenGL.log
echo   %BIN%\Logs\RendererBenchmark-Vulkan.log
echo   %BIN%\Logs\RendererBenchmark-OpenGL.csv
echo   %BIN%\Logs\RendererBenchmark-Vulkan.csv
popd
exit /b 0
