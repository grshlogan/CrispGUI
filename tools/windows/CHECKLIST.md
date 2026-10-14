# Windows checks for this delivery

Phase 5: transcribing one file on the Transcribe page and saving its SRT. The owner, or the Windows-side agent at the owner's direction, runs these checks on the owner's Windows machine and reports each one as `W5-n PASS`, `FAIL`, or `SKIP`, together with what the check asks to send back. `BUILD-INFO.txt` names the commit of this delivery.

Rules for every check:

- Run everything from this folder. The checks write only into this folder, apart from what the CrispASR server itself writes.
- Do not edit or move anything in the CrispASR folder (`E:\AI\ASR\CrispASR`). The GUI writes the SRT next to the audio file, so never transcribe a file inside that folder: W5-3 copies the sample out first.
- Keep the machine's usual real-time virus protection on, and say so in the results if it was off.
- On a FAIL, stop and send back what the check names, plus any error text on the screen.

## W5-1 Automated tests

1. Double-click `run-tests.cmd`, or run `powershell -NoProfile -ExecutionPolicy Bypass -File .\run-tests.ps1` in this folder.
2. Expected: the last line of `test-results.txt` reads `Failures: 0 of N`, where N is the number of tests that `BUILD-INFO.txt` names. The tests include the Windows-only cases of writing a file atomically and `transcription_test`, the longest test.

Send back: `test-results.txt`, the whole file.

## W5-2 Start the server from the GUI

1. Start `crispgui.exe`. If the Server line already shows Running, a server started elsewhere holds the port: press Stop next to it and confirm, then wait for Stopped.
2. On the Dashboard, in the Profiles list after the Server panel, press Start in the row of a ready Japanese profile, for example `parakeet-ctc-1.1b-ja-q8` or `cohere-asr-ja-q8`. Note whether that Start button was in view at the default window size without scrolling.
3. Expected: the Server line turns to Running.

Keep the GUI and the server running for W5-3 to W5-6.

Send back: PASS or FAIL, the profile, and whether the Start button was in view; a screenshot for a FAIL or if it was not in view.

## W5-3 Transcribe the sample on the Transcribe page

1. In this folder, create a folder `w5` and copy `E:\AI\ASR\CrispASR\samples\ja-demo.mp3` into it. Copy; never move.
2. On the Transcribe tab, press "Choose file…" and choose `w5\ja-demo.mp3`. The page names the running server, the file, and the destination `w5\ja-demo.srt`.
3. Press Transcribe and watch the progress and the elapsed time.
4. Expected: the page shows the result, the number of cues, and a preview, and `w5\ja-demo.srt` exists.

Send back: PASS or FAIL, `w5\ja-demo.srt`, and a screenshot of the result.

## W5-4 The GUI's SRT equals the server's own

1. With the same server still running, run in PowerShell in this folder:

   ```powershell
   .\api_probe.exe "w5\ja-demo.mp3"
   $LASTEXITCODE
   ```

   The probe writes `probe-results\<UTC time>\`, among others `transcript.srt`, the server's own SRT for the same audio.
2. Compare the two files byte for byte (in PowerShell `fc` means something else, so call `fc.exe`):

   ```powershell
   fc.exe /b "w5\ja-demo.srt" "probe-results\<UTC time>\transcript.srt"
   ```

3. Expected: the probe ends with `Failures: 0` and exit code 0, and `fc.exe` reports no differences.

Send back: the `fc.exe` output and the whole `probe-results\<UTC time>\` folder.

## W5-5 An existing SRT, a failed write, and its rescue

1. Transcribe `w5\ja-demo.mp3` again. Expected: the page says that `w5\ja-demo.srt` exists and asks before replacing it; confirm, and the result follows.
2. Make the SRT read-only: `Set-ItemProperty "w5\ja-demo.srt" -Name IsReadOnly -Value $true`. Transcribe again and confirm the replacement. Expected: the transcription finishes, the write fails with a readable message, and the page keeps the SRT and offers Retry, "Save to the app data folder", and Discard, with Transcribe disabled. If the write does not fail, report that and skip step 3.
3. Press "Save to the app data folder". Expected: the page names the file it wrote under this folder's `appdata\output\`, and that file exists.
4. Make the SRT writable again: `Set-ItemProperty "w5\ja-demo.srt" -Name IsReadOnly -Value $false`.

Send back: PASS or FAIL, screenshots of steps 1, 2, and 3, and the name of the file under `appdata\output\`.

## W5-6 A destination path of about 250 characters

1. In PowerShell in this folder, make a folder whose SRT path will have about 250 characters, and copy the sample into it:

   ```powershell
   $base = (Resolve-Path .\w5).Path
   $long = Join-Path $base ('x' * (237 - $base.Length))
   New-Item -ItemType Directory $long | Out-Null
   Copy-Item "w5\ja-demo.mp3" $long
   (Join-Path $long 'ja-demo.srt').Length
   ```

2. Transcribe that copy on the Transcribe page.
3. Report what happens: the result with the SRT written, or the failure and the step it names. The temporary file of the write adds up to 21 characters, and Windows limits paths to 260 characters, so a failure here is a finding to report, not a reason to stop.

Send back: the length the last command printed, the outcome, and a screenshot of it.

## W5-7 Stop the server

1. In the GUI, press Stop next to the Server line and confirm, then close the GUI.
2. Expected: the server panel shows Stopped before the GUI closes.

Send back: PASS or FAIL.
