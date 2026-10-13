# Implementation Tasks — Phased Plan

> The build order for this repository. Each phase has an acceptance test. Work top to bottom; do not skip phases. Revised on 2026-10-11 for the WSL2-only development environment and the owner's directions. Owner: the user; maintained by A003.

## Ground rules for every phase

- Read [AGENTS.md](../AGENTS.md) and [CONVENTIONS.md](CONVENTIONS.md) before the first change. English in the repository, C++17, no third-party additions beyond [ENVIRONMENT.md](ENVIRONMENT.md).
- The hard rules in AGENTS.md apply throughout.
- If a task blocks on a question, record it in [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) and continue with the next independent task; surface all blockers at the end of the phase.
- After each phase: update the checklist status in this file and write a short phase report in `docs/devlog/phase-N.md`, created with the first report (what was built, what deviates from the plan, open questions).
- Platform tags: **[Linux]** can be verified in WSL2; **[Windows]** needs the owner's Windows machine and a delivery bound to an exact commit.

## Phase 0 — Build environment (no application code)

Complete: accepted on 2026-10-11 after the owner's Windows run ([Phase 0 report](devlog/phase-0.md)).

- [x] EUI-NEO v0.6.1 source available: the owner placed it in the shared Learn library; location and provenance are in [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot).
- [x] Windows build path decided: MinGW-w64 cross-compilation in WSL2. The owner installed the toolchain, and a probe of EUI-NEO's static link mode passes ([ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain)).
- [x] Git initialized with `.gitignore` and `.gitattributes`; the remote `origin` is the owner's private GitHub repository ([agents/README.md](agents/README.md#checkouts)).
- [x] The owner installed the Linux packages; the versions are in [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain).
- [x] [Linux] Smoke build: `tools/smoke/build-eui-smoke.sh` configures EUI-NEO from the Learn library into `build/eui-smoke-linux` with its examples and tests and `EUI_DEPS_MODE=bundled`; `eui_demo` opens and renders under WSLg, and nothing is left inside the Learn library.
- [x] `cmake/toolchains/mingw-w64-x86_64.cmake` added; the same script cross-builds EUI-NEO into `build/eui-smoke-mingw`.
- [x] [Windows] First delivery `crisprustgui-eui-smoke-7e4e89e.zip`: the owner ran `eui_demo.exe`, `multipage_table.exe`, and `run-eui-smoke.cmd`; no visual problems, 0 failures of 34 ([agents/README.md](agents/README.md#delivery-to-windows)).
- [x] Acceptance: the Linux and Windows builds come from commit `7e4e89e`, and both results are recorded with platform and SHA.

## Phase 1 — Application skeleton

Complete: accepted on 2026-10-11 after the owner's Windows run of `crisprustgui-1817b8e.zip` ([Phase 1 report](devlog/phase-1.md)).

- [x] Top-level `CMakeLists.txt` that adds EUI-NEO from `CRISPGUI_EUI_NEO_DIR` with the options of [ENVIRONMENT.md §4.2](ENVIRONMENT.md#42-eui-neo-build-facts), for both the Linux and the MinGW build.
- [x] The directory layout of [ARCHITECTURE.md §2](ARCHITECTURE.md#2-component-model) for the modules Phase 1 needs, with Windows and Linux implementations under `src/platform/`; later modules get their directories when they are written.
- [x] `src/main.cpp`: a minimal EUI-NEO app using `DslAppConfig` (titled with the project's name, window 1100x720, `onStart`), with a string table from the start.
- [x] Tabs shell with four pages: Dashboard, Transcribe, Batch, Settings, built on EUI-NEO's `tabs` component; Settings already switches the UI language between English and Chinese, without saving it.
- [x] Create the app data directory (`<folder of the executable>/appdata`) and point the GUI process's own `TMP`/`TEMP` at `appdata/tmp` in `onStart`; the dashboard shows both paths and warns when they are on the system drive.
- [x] Unit tests for the string table and the app data preparation, run by CTest on Linux and by `run-tests.cmd` on Windows.
- [x] Acceptance: [Linux] the app opens with four tabs, stays responsive, and closes cleanly; [Windows] the same on the owner's machine.

## Phase 2 — ProfileStore

Complete: accepted on 2026-10-12 after the owner's Windows run of `crisprustgui-b397dcd.zip` ([Phase 2 report](devlog/phase-2.md)).

- [x] Implement the JSONC parser with string-aware comment stripping and correct escape handling ([ARCHITECTURE.md §2.1](ARCHITECTURE.md#21-profilestore)). Unit-test it with the four profiles in `reference/crispasr/profiles-japanese/`, a string containing an escaped quote, and a corrupted file.
- [x] Implement `ProfileStore`: scan `<CrispASR root>/profile/*` subfolders, compute readiness (`model`/`aligner` existence under the models root), and hash each file.
- [x] Parse known `args[]` flags and apply the launcher's conditions, using a flag table extracted from the CrispASR 0.8.42 CLI source in the Learn library (`examples/cli/cli.cpp`; [OQ-4](OPEN-QUESTIONS.md#oq-4-source-material-needed) item 2).
- [x] Added: the dashboard lists the profiles of the CrispASR folder with readiness, errors, and startup flags; the UI uses the owner's HarmonyOS Sans SC fonts.
- [x] Acceptance: [Linux] unit tests pass; the four Japanese profiles parse with the expected fields; [Windows] the owner's run of the delivery: the unit tests pass, and the dashboard lists the real profiles under `E:\AI\ASR\CrispASR\profile` with the readiness the launcher shows.

## Phase 3 — ServerManager

Complete: accepted on 2026-10-12 after the owner's Windows runs of `crisprustgui-5046d3e.zip` (the GUI) and `crisprustgui-eda8e33.zip` (14 of 14 tests, with `app_data_test` corrected) ([Phase 3 report](devlog/phase-3.md)). Correction of round 11: the GUI check did not start a server, so starting, checking, and stopping the deployed server from the GUI moved to the Phase 4 checks W4-2 to W4-4.

- [x] Build the command line and environment per [API-CRISPASR.md §3.2](API-CRISPASR.md#32-tier-b-startup-only-parameters), including `CRISPASR_API_KEYS`, `CRISPASR_SCRATCH_DIR`, the profile's `env`, and `TMP`/`TEMP`.
- [x] Implement process start and stop through `src/platform/` (Win32 and POSIX), with stdout and stderr drained into a ring buffer.
- [x] Implement the pre-flight, the state machine, health polling, and adoption ([ARCHITECTURE.md §2.2](ARCHITECTURE.md#22-servermanager)).
- [x] Added: the dashboard's server panel with Start buttons in the profile list, Stop with a confirmation for adopted servers, and the log pane; HTTP GET through WinHTTP and libcurl; a stand-in server and integration tests that also run in Windows deliveries.
- [x] Acceptance: [Linux] against a stub server executable: start, health, stop, a busy port, and a process that exits with code 0; [Windows] the delivery's unit and integration tests pass, and with the app, the Japanese cohere profile starts the server, `/health` turns ok, Stop terminates the process, and the log pane shows the server's output.

## Phase 4 — ApiClient

Implemented on 2026-10-12; the Linux acceptance passes. On Windows, W4-1 passed 19 of 20 tests: `api_client_test` allows too little time for WinHTTP's coarse timer, which the diagnostic measured; W4-2 passed by the owner's statement; W4-3 passed, ten of ten probe checks against the deployed server; W4-4 passed by the owner's statement ([Phase 4 report](devlog/phase-4.md)). The corrections, the rename to CrispGUI, the server's answers as test data, and the next delivery are task packets for A004 (all merged: `c483964`, `5649b4f`, `403bde9`, `017b5b5`, and `bd925e8`; the delivery `crispgui-621df7d` waits for its Windows checks): [P4-T1](agents/tasks/P4-T1-timeout-test/packet.md), [P4-T2](agents/tasks/P4-T2-dashboard-start/packet.md), [P4-T4](agents/tasks/P4-T4-rename/packet.md), [P4-T5](agents/tasks/P4-T5-server-answers/packet.md), [P4-T3](agents/tasks/P4-T3-delivery/packet-v2.md).

- [x] Implement the transport with libcurl (Linux) and WinHTTP (Windows): streaming multipart upload, timeouts, cancellation, and the `Authorization` header (`HttpPost` and `MultipartBody` in `src/platform/`).
- [x] Map errors to the typed set of [ARCHITECTURE.md §2.3](ARCHITECTURE.md#23-apiclient-and-http-transport); parse `verbose_json`; add a small JSON writer.
- [x] Write a local stub server for tests (no network beyond localhost): `tests/stub_crispasr.cpp` gained the transcription route, `/progress`, and an echo route.
- [x] Added (owner's agreement of 2026-10-12): `api_probe`, which sends the ApiClient's requests to a running server and keeps its answers as test data; `CHECKLIST.md`, the numbered Windows checks that each delivery carries; the Windows-side agent's entry in the identity register, with its ID still to be assigned by the owner.
- [ ] Acceptance: [Linux] stub tests cover success, `400`, `401`, `413`, `500`, `503`, and refused connections (done); [Windows] a hand-rolled request to `/v1/audio/transcriptions` with a sample file returns `verbose_json`: checks W4-1 to W4-4 of the delivery's `CHECKLIST.md`.

## Phase 5 — Single-file transcription and SRT writer

Started on 2026-10-13 with the core tasks that do not depend on Phase 4's last Windows check: [P5-T1](agents/tasks/P5-T1-srt-writer/packet.md) (the SRT writer; OQ-7 is decided) and [P5-T2](agents/tasks/P5-T2-atomic-write/packet.md) (writing a file atomically). The Transcribe page and the Phase 5 delivery follow as packets once Phase 4 closes.

- [x] Implement the SRT writer ([ARCHITECTURE.md §2.8](ARCHITECTURE.md#28-srt-writer)) with equivalence fixtures: [P5-T1](agents/tasks/P5-T1-srt-writer/packet.md), merged as `3a11fef`; byte-equal to the recorded server SRT.
- [ ] Wire the Transcribe page: file picker with size pre-check, profile indicator (the server must be running), Transcribe button, progress from `/progress`, result preview, and save path.
- [ ] Save the SRT next to the source file by default; honor the output-directory setting. The atomic write exists: [P5-T2](agents/tasks/P5-T2-atomic-write/packet.md), merged as `d841b27` and `2575782`.
- [ ] Windows checks of the Phase 5 delivery (W5) include, besides the acceptance below: the Windows-only cases of `file_write_test` (replacing, a destination held open, wide paths), which `run-tests.cmd` runs; saving to a destination path of about 250 characters, since the temporary name adds up to 21 characters and `MAX_PATH` is 260 (`hypothesis`, P5-T2 Review R02 F1); and saving with real-time virus scanning on, since `MoveFileExW` runs once without a retry (`hypothesis`, R01 F6).
- [ ] Acceptance: [Windows] transcribing `E:\AI\ASR\CrispASR\samples\ja-demo.mp3` through the GUI yields an SRT identical to the server's `srt` output for the same audio and parameters.

## Phase 6 — Parameter model and panel (pluggable)

- [ ] Define the catalog format and an initial catalog from [API-CRISPASR.md §3.1](API-CRISPASR.md#31-tier-a-per-request-parameters) and the profiles; complete CLI flags and capability names from the CrispASR 0.8.42 source (OQ-4 item 2).
- [ ] Implement the profile, manifest, and capability sources, the resolver, and validation ([ARCHITECTURE.md §2.7](ARCHITECTURE.md#27-parameter-model-pluggable)); the manifest location follows [OQ-5](OPEN-QUESTIONS.md#oq-5-location-of-the-models-authoritative-parameter-description).
- [ ] Implement the widget factory and the generated panel, with tier and value-source display and "Restart with changes".
- [ ] Acceptance: [Linux] resolver tests produce different parameter sets for the four profiles with no model-specific code, and adding a catalog or manifest entry changes the panel without recompiling; [Windows] a `vad_threshold` override changes the next transcription without a restart, and backend fields show as "requires restart".

## Phase 7 — Batch queue, windows, persistence

- [ ] Implement the job model, the snapshot at enqueue, and the single worker ([ARCHITECTURE.md §2.4](ARCHITECTURE.md#24-job-model-the-step-chain)).
- [ ] Implement the `AsrStepRunner` window loop with the boundary guard and retry classification ([ARCHITECTURE.md §2.5](ARCHITECTURE.md#25-steprunner-interface-and-the-asr-window-loop)); defaults per [OQ-6](OPEN-QUESTIONS.md#oq-6-transcription-window-defaults).
- [ ] Implement persistence: atomic `jobs/<id>.json`, append-only `jobs/<id>.ckpt.jsonl`, and startup repair (layout after `reference/crisperweaver/batch_persistence_service.dart`).
- [ ] Batch page: queue table (add files, reorder, remove, clear finished), status column, per-job output path, start/stop queue, resume on startup, and profile-switch handling per [OQ-10](OPEN-QUESTIONS.md#oq-10-queue-behavior-when-the-next-job-needs-another-profile).
- [ ] Acceptance per SPEC FR-5: [Linux] stub-server tests for retry, boundary handling, and resume after a kill; [Windows] a 3-file batch where killing the app mid-job and restarting continues from the first incomplete window, and stopping and restarting the server mid-window leads to that window being retried.

## Phase 8 — Polish and hardening

- [ ] Log viewer page (ring buffer plus a file sink in the app data directory). Move the recognition of the GUI's own notes into `core` beside `kNotePrefix` (for example `IsNoteLine`) with a unit test, since no test links the UI library (P4-T4 Review, finding F3).
- [ ] Error UX: every failure path produces a readable message and a suggested action.
- [ ] Settings page: CrispASR root path, port, API key, default output directory, window length, request timeout, UI language, theme.
- [ ] A full pass against the SPEC acceptance items; write the phase report.

## Post-v1 (not in this plan, for context only)

Pipeline runners (steps 1, 2, 4, 5, 6), a chain composition UI, an in-process engine option, and additional service management. The architecture allows adding these as new step runners and service registry entries.
