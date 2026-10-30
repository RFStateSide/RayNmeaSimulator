@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-m239-route-acceptance.ps1" %*
if errorlevel 1 pause
