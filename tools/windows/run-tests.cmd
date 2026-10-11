@echo off
rem Runs the tests in this folder and writes test-results.txt.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-tests.ps1"
echo.
echo Send test-results.txt back.
pause
