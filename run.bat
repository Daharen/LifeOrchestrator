@echo off
setlocal
set REPO_ROOT=%~dp0

if "%~1"=="" (
    start "" powershell -WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%run.ps1" assistant-shell
    exit /b 0
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%run.ps1" %*
exit /b %ERRORLEVEL%
