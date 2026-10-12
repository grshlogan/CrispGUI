# Phase 2 Report — ProfileStore

Status: built and tested on Linux; the owner's Windows run of the delivery is pending. Written by A003 on 2026-10-11 and 2026-10-12 (round 6; dates are UTC). The plan is [TASKS.md Phase 2](../TASKS.md#phase-2--profilestore); the design is [ARCHITECTURE.md §2.1](../ARCHITECTURE.md#21-profilestore).

## What was built

- `src/util/json.{h,cpp}`: a strict JSON parser (RFC 8259) whose values keep object members in order, duplicates included, and each number's text next to its value. Errors give the line and the column in characters; strings must be valid UTF-8, `\u` escapes decode surrogate pairs, and nesting is limited to 64 levels. `FormatJsonNumber` writes the shortest text that reads back as the same double.
- `src/core/jsonc.{h,cpp}`: the profile comment syntax: `//` and `#` outside strings, string-aware with working escapes, line breaks kept so that error lines match the file.
- `src/util/sha256.{h,cpp}`: SHA-256 after FIPS 180-4, for each profile's content hash.
- `src/core/cli_flags.{h,cpp}` and the generated `cli_flags_table.inc`: all 246 flags (320 spellings) of the CrispASR 0.8.42 CLI, extracted from `examples/cli/cli.cpp` in the Learn library by `tools/extract-cli-flags.py`. The generated file carries CrispASR's MIT notice. `ParseCliArguments` reads an argument list with the CLI's rules and reports unknown flags, missing values, unreadable or out-of-range numbers, values outside the accepted choices (`--lcs-dedup`, `--sensitivity`, `--stream-punc`, `--stream-final-mode`), `--lcs-min-length` below 1, flags that make the CLI exit, and stray arguments.
- `src/core/launch_command.{h,cpp}`: the launcher's ten tuning keys with their apply conditions (`start-crispasr.ps1:203-212`) and `ReadProfileStartup`, which emits the tuning flags whose condition holds, appends `args[]`, and reads the result with the CLI rules.
- `src/core/profile_store.{h,cpp}`: the `Profile` model, parsing with the rules of ARCHITECTURE §2.1, readiness of the server executable, model, and aligner, and the folder scan.
- `src/platform/files.{h,cpp}` with `files_win32.cpp` (`_wfopen`, hidden and system attributes) and `files_posix.cpp`.
- `src/app/crispasr_location.{h,cpp}`: the CrispASR root is `E:\AI\ASR\CrispASR` on Windows until the Settings page stores one; `CRISPGUI_CRISPASR_ROOT` overrides it, which Linux development runs need.
- The dashboard scans the profiles on EUI-NEO's async pool at start and on "Rescan", and lists each folder's profiles like the launcher's menu: `[OK]`, `[MISS]`, or `[ERROR]`, the backend, what is missing with the download URL, the startup flags, and warnings. The page scrolls. 21 new string table entries, each in English and Chinese.
- UI fonts: the owner's HarmonyOS Sans SC ([ENVIRONMENT.md §4.3](../ENVIRONMENT.md#43-ui-fonts)); `src/ui/fonts.{h,cpp}` selects Regular for text, Medium for headings, and Bold for titles; the build copies the three files next to the executable; `fonts/` is ignored by Git at the owner's choice.
- Tests: `json_test`, `jsonc_test`, `sha256_test`, `cli_flags_test`, `profile_store_test`, and `launch_command_test`. The build copies the four reference profiles byte for byte to `testdata/` next to the tests, and `tools/package-windows.sh` adds that folder and the fonts to deliveries.

## Linux results (WSL2, WSLg)

| Check | Command (in `<WSL_ROOT>`) | Result |
|---|---|---|
| Clean Debug build | `rm -rf build/linux && cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/linux` | exit 0; 184 steps; no warnings |
| Unit tests | `ctest --test-dir build/linux --output-on-failure` | exit 0; 8 of 8 passed |
| The tests catch changes | one byte appended to the copied `cohere-asr-ja-q8.conf`, then `build/linux/tests/profile_store_test` | exit 1 at the content-hash check; exit 0 after restoring the file |
| Expected hashes | `sha256sum` on the FIPS 180-4 messages, the padding-boundary lengths, and the reference profiles | the same values as in `sha256_test` and `profile_store_test` |
| Dashboard, fake deployment | `CRISPGUI_CRISPASR_ROOT=<scratch>/crispasr-root build/linux/crisprustgui`; the four reference profiles, a broken one, the server stub, the cohere and qwen3 models, and the aligner | the capture shows `[ERROR]` with "line 4, column 3: A ',' is missing before this member", `[OK]` for cohere with `--vad --force-aligner --chunk-seconds 30 -bs 1 -n 512 --vad-threshold 0.3`, and `[MISS]` for kotoba and parakeet with the model path and download URL; "Profiles: 5, OK: 2" |
| Dashboard, no root | the same without `CRISPGUI_CRISPASR_ROOT` | "No CrispASR folder is set …" |
| Dashboard, missing root | `CRISPGUI_CRISPASR_ROOT=<scratch>/no-such-root` | "The profiles could not be read: There is no profile folder at …" |
| Clean close | `WM_DELETE_WINDOW` through Xlib, after each of the three runs | exit 0, no output |
| Clean MinGW release build | the Windows commands of [ENVIRONMENT.md §2.2](../ENVIRONMENT.md#22-commands) after `rm -rf build/mingw` | exit 0; 184 steps; no warnings; `crisprustgui.exe` (3,722,240 bytes) imports only Windows system DLLs; all 8 tests import only `KERNEL32.dll` and `msvcrt.dll` |
| Learn snapshot untouched | `find <Learn> -newer <marker>` and `-cnewer <marker>` around the clean builds | no entries |

Not checked on Linux: scrolling, the Rescan button, and the Chinese texts of the new section, because synthetic X11 input does not reach EUI-NEO ([Phase 1 report](phase-1.md#linux-results-wsl2-wslg)). The Windows run covers them.

## Decisions and deviations

- JSON: an in-repo parser instead of EUI-NEO's `eui/json.h`, which cannot list an object's members ([ARCHITECTURE.md §3](../ARCHITECTURE.md#3-key-decisions-and-rejected-alternatives)).
- Strict reading: a known key with the wrong type makes the profile unusable instead of being coerced as PowerShell would; keys ignore case; duplicates follow `ConvertFrom-Json` (ARCHITECTURE §2.1). `args[]` problems are warnings, because a newer engine may accept flags that 0.8.42 rejects.
- `[OK]` and `[MISS]` follow the model file, as in the launcher's menu; a missing executable or aligner shows as an extra line.
- Scope: the dashboard list and a Windows check were added to Phase 2, so that the real profiles are read on the owner's machine before Phase 3 starts servers from them.
- Text wrapping: EUI-NEO measures text with `maxWidth`, so wrapped lines now set it; this also fixes Phase 1's line helper, which had no long text then ([ENVIRONMENT.md §4.4](../ENVIRONMENT.md#44-eui-neo-runtime-facts)).
- The CLI source corrected three statements of [API-CRISPASR.md](../API-CRISPASR.md): the API-key flag is `--api-keys`, `-falign` is an alias of `--force-aligner`, and in server mode the process returns the server's result, so a failed `listen()` exits with code 0.

## Open items

- [Windows] The owner runs the delivery: `run-tests.cmd` (8 unit tests; `test-results.txt` back to A003), then `crisprustgui.exe`: the dashboard lists the profiles under `E:\AI\ASR\CrispASR\profile` with the same `[OK]`/`[MISS]` as `start-crispasr.ps1 -ListOnly`; scrolling, Rescan, and the Chinese texts work; the HarmonyOS Sans font shows.
- The license document of HarmonyOS Sans, if the owner has it.
- The engine's capability names, for Phase 6 (OQ-4 item 2).
