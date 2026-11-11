@echo off
setlocal
pushd "%~dp0" || exit /b 1
py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 (
    py -3 scripts\run_game.py %*
    goto :finished
)
python -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 (
    python scripts\run_game.py %*
    goto :finished
)
echo Python 3.10 or newer was not found. Install Python or use its full path to run scripts\run_game.py.
popd
exit /b 1
:finished
set "result=%errorlevel%"
popd
exit /b %result%
