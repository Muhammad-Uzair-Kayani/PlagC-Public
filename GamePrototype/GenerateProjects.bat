@echo off
setlocal
pushd "%~dp0" || exit /b 1
if "%~1"=="" (
    premake5.exe vs2022
) else (
    premake5.exe %*
)
set "result=%errorlevel%"
popd
exit /b %result%
