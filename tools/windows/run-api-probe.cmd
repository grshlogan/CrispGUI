@echo off
rem Probes the CrispASR server at 127.0.0.1:5114 with the audio file dropped on this script, or
rem given as its argument, and keeps the answers in probe-results\ (CHECKLIST.md, W4-3).
"%~dp0api_probe.exe" --window 5000,10000 %*
echo.
echo Send the newest folder in probe-results back.
pause
