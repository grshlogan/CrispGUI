# Windows checks for this delivery

Phase 4: the ApiClient, which uploads files to the CrispASR server. The owner, or the Windows-side agent at the owner's direction, runs these checks on the owner's Windows machine and reports each one as `W4-n PASS`, `FAIL`, or `SKIP`, together with what the check asks to send back. `BUILD-INFO.txt` names the commit of this delivery.

Rules for every check:

- Run everything from this folder. The checks write only into this folder, apart from what the CrispASR server itself writes.
- Do not edit or move anything in the CrispASR folder (`E:\AI\ASR\CrispASR`); the sample file is only read.
- On a FAIL, stop and send back what the check names, plus any error text on the screen.

## W4-1 Automated tests

1. Double-click `run-tests.cmd`, or run `powershell -NoProfile -ExecutionPolicy Bypass -File .\run-tests.ps1` in this folder.
2. Expected: the last line of `test-results.txt` reads `Failures: 0 of N`, where N is the number of tests that `BUILD-INFO.txt` names.

Send back: `test-results.txt`, the whole file.

## W4-2 Start the server from the GUI

1. Start `crisprustgui.exe`. If the Server line already shows Running, a server started elsewhere (by the launcher) holds the port, and the Start buttons stay dimmed: press Stop next to it and confirm, then wait for Stopped.
2. On the Dashboard, scroll the upper part of the page down with the mouse wheel, or drag the scroll bar at its right edge, past "Profiles" and "Rescan". At the default window size the profile list starts below the visible part.
3. Each profile row reads `[OK] <name> <backend>` followed by a Start button (启动 in the Chinese interface). Press Start in the row of a ready Japanese profile, for example `parakeet-ctc-1.1b-ja-q8`, which W4-3 used on 2026-10-13, or `cohere-asr-ja-q8`.
4. The Server line at the top of the page changes to "Starting: loading the model…" and then to Running; the server log at the bottom shows the server's lines.
5. Expected: Running.

The server must come from the GUI: a server the GUI starts writes each upload into this folder's `appdata\tmp\server`, whereas a server from the launcher writes them into the user's TEMP folder, normally on `C:`. Keep the GUI and the server running for W4-3.

Send back: PASS or FAIL; for a FAIL, a screenshot.

## W4-3 Probe the running server

1. In PowerShell, in this folder, run:

   ```powershell
   .\api_probe.exe --window 5000,10000 "E:\AI\ASR\CrispASR\samples\ja-demo.mp3"
   $LASTEXITCODE
   ```

   Instead, the owner may drag the audio file onto `run-api-probe.cmd`, which runs the same command.
2. Expected: checks `[1]` to `[10]` read PASS (INFO is acceptable for `[4]` and `[10]`), the report ends with `Failures: 0`, and the exit code is 0. The probe sends four transcriptions of the sample; with a long sample this takes a while.

The probe writes a new folder `probe-results\<UTC time>\` next to `api_probe.exe`: `report.txt`, `report.json`, the server's answers as received (`transcript.verbose.json`, `transcript.srt`, `window.verbose.json`, `error-*.json`, `health.json`, `models.json`), and `not-audio.mp3`, the small text file it sends to see the server reject a file that is not audio.

Send back: the whole `probe-results\<UTC time>\` folder; all its files are small text files. These answers become the test data that lets the WSL2 tests replay the real server.

## W4-4 Stop the server

1. In the GUI, press Stop, then close the GUI.
2. Expected: the server panel shows Stopped before the GUI closes.

Send back: PASS or FAIL.
