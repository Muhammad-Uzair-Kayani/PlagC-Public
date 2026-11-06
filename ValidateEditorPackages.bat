@echo off
setlocal
pushd "%~dp0" || exit /b 1
where msbuild >nul 2>nul || (echo Run from a VS2022 Developer Command Prompt. & exit /b 2)
call GenerateProjects.bat --vulkan
if errorlevel 1 exit /b 1
for %%C in (Release Dist) do (
    msbuild Build\Generated\PlagC.sln /m /nologo /v:minimal /t:Editor /p:Configuration=%%C /p:Platform=x64
    if errorlevel 1 exit /b 1
)
python Scripts\ValidateEditorPackages.py
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
