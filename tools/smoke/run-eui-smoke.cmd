@echo off
rem Runs the EUI-NEO smoke tests in this folder and writes smoke-results.txt.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-eui-smoke.ps1"
echo.
echo Send smoke-results.txt back.
pause
