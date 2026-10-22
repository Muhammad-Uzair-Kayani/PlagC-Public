@echo off
setlocal
cd /d "%~dp0"
rem Use Git for Windows Bash, never the Windows WSL bash launcher.
set "GIT_BASH="
for /f "delims=" %%G in ('where git.exe 2^>nul') do (
    if exist "%%~dpGbash.exe" set "GIT_BASH=%%~dpGbash.exe"
    if exist "%%~dpG..\bin\bash.exe" set "GIT_BASH=%%~dpG..\bin\bash.exe"
)
if not defined GIT_BASH (
    echo Install Git for Windows with Git on PATH; its bundled Bash is required.
    exit /b 1
)
"%GIT_BASH%" --noprofile --norc ./FetchDependencies.sh
exit /b %errorlevel%
