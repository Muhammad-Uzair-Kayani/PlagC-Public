@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" if /I not "%CONFIG%"=="Dist" (
    echo Usage: ValidateEditor.bat [Debug^|Release^|Dist]
    exit /b 2
)

for %%T in (
    EngineBoundaryTests.py
    EditorArchitectureTests.py
    EditorPlayRuntimeTests.py
    EditorAssetInteractionTests.py
    EditorMilestone3Tests.py
    EditorMilestone4InspectorTests.py
    EditorMilestone5Tests.py
    EditorMilestone6Tests.py
    EditorMilestone9Tests.py
    EditorMilestone12Tests.py
    EditorMilestone15Tests.py
    EditorSceneHierarchyMilestoneTests.py
    EditorGizmoTests.py
    EditorUiTextureTests.py
) do (
    echo [Editor Validation] Running %%T...
    python "Scripts\Tests\%%T"
    if errorlevel 1 exit /b 1
)

where msbuild >nul 2>nul
if errorlevel 1 (
    echo [FAIL] MSBuild is not on PATH.
    echo Run this script from a Visual Studio 2022 Developer Command Prompt.
    exit /b 2
)

if not exist "Build\Generated\PlagC.sln" (
    echo [Editor Validation] Generated solution missing; generating VS2022 projects with Vulkan enabled...
    call GenerateProjects.bat --vulkan
    if errorlevel 1 exit /b 1
)

set "LOG=Build\ValidationLogs\Editor-%CONFIG%.log"
if not exist "Build\ValidationLogs" mkdir "Build\ValidationLogs"

echo [Editor Validation] Building Editor %CONFIG%...
msbuild Build\Generated\PlagC.sln /m /nologo /v:minimal /t:Editor /p:Configuration=%CONFIG% /p:Platform=x64 /fl /flp:logfile=%LOG%;verbosity=normal
if errorlevel 1 (
    echo [FAIL] Editor build failed. Compiler errors:
    findstr /I /C:" fatal error C" /C:": error C" "%LOG%"
    echo Full log: %LOG%
    exit /b 1
)

echo [Editor Validation] Vulkan UI presentation...
call Scripts\TestEditorUI.bat %CONFIG% vulkan
if errorlevel 1 exit /b 1

echo [Editor Validation] OpenGL UI presentation...
call Scripts\TestEditorUI.bat %CONFIG% opengl
if errorlevel 1 exit /b 1

echo [PASS] Editor source regressions, native build, Vulkan UI and OpenGL UI all passed.
exit /b 0
