# Specification — CrispRustGUI v1

> Version 1 functional specification. Status: **approved for implementation**. Revised on 2026-10-11 with the owner's directions (a pluggable parameter panel with one parameter set per model, transcription windows with retry, the API key, the `C:\` rule) and with corrections from a source review of the server. Items marked *proposed* wait for the owner's decision in [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md).
> Sources: user requirements (2026-10-11), audit reports in `reference/audits/audit-reports/`, server source in `reference/crispasr/`, the server contract in [API-CRISPASR.md](API-CRISPASR.md).

## 1. Problem statement

The CrispASR engine is driven today through a PowerShell launcher and manual CLI or server commands. The user needs a desktop application that makes the engine's daily operation — starting it with the right model, tuning parameters, and batch-transcribing files to SRT — safe, visual, and queue-based, while remaining a thin client that never breaks the separation between GUI and engine.

## 2. System context

```
┌────────────────────────┐        HTTP (127.0.0.1:5114)         ┌──────────────────────────┐
│     CrispRustGUI       │ ── multipart POST /v1/audio/... ──▶ │   CrispASR server        │
│  (C++17 + EUI-NEO)     │ ── GET /health, /progress, ... ───▶ │   (spawned or adopted;   │
│                        │                                      │    one model at a time)  │
│  manages the server    │                                      │   reads models from      │
│  process + profiles    │                                      │   CrispASR\models\       │
└────────────────────────┘                                      └──────────────────────────┘
        │  spawns / stops                                         ▲
        ▼                                                         │ reads
  CrispASR\profile\<folder>\*.conf  ──────────────────────────────┘  (JSONC profiles = single source of truth)
```

- The GUI **owns the server process lifecycle** when it starts the server: it runs `crispasr.exe --server ...` with arguments derived from the selected profile, monitors it, and stops it. A server that is already running is adopted instead.
- The GUI **never modifies CrispASR files**. Profiles are read-only inputs in v1.
- The GUI **never embeds the engine**. An in-process integration is a possible future evolution, out of scope for v1.

## 3. Functional requirements

### FR-1 Server management

- Pre-flight: before starting, request `GET /health` on the configured port. `200` means a server is already running: adopt it. A refused connection means the port is free: start. Any other answer means another program holds the port: do not start, and say so.
- Start the server with the command line and environment the owner's launcher derives from a profile ([API-CRISPASR.md §3.2](API-CRISPASR.md#32-tier-b-startup-only-parameters)): the executable `<root>\<build>\crispasr.exe`, `--server --backend <backend> -m <model> [-am <aligner>] [-l <language>]`, the tuning flags under the launcher's conditions, `args[]` verbatim, `--host` and `--port`. The child environment receives `CRISPASR_MODELS_DIR`, `CRISPASR_CACHE_DIR`, `CRISPASR_SCRATCH_DIR`, `CRISPASR_API_KEYS`, `CUDA_VISIBLE_DEVICES`, the profile's `env` entries, and `TMP`/`TEMP` pointing at the GUI's app data directory.
- Readiness checks before starting: the executable and the model file must exist. A missing aligner follows [OQ-8](OPEN-QUESTIONS.md#oq-8-missing-aligner-at-server-start).
- States: `stopped → starting → running → stopping`, plus `failed`. `starting` means the process is alive but the port does not answer yet, because the server loads its model before it listens. `running` means `/health` returns `200`. `failed` means the process exited; an exit code of 0 is no proof of success, because a port conflict also ends with exit code 0 after the model load. Poll `/health` every 1–2 s.
- An adopted server shows as running or unreachable. Stop is still available but warns that the server may have been started by someone else.
- Stop terminates the process; the server has no graceful-shutdown endpoint. In-flight requests fail, and the queue pauses.
- Display the backend and model path from `GET /v1/models`, the profile for a GUI-started server, and the server's log (stderr) in the log pane.

**Acceptance**: from a stopped state, selecting a profile and pressing Start yields a healthy server within the model's load time; the UI shows health, backend, and model; Stop returns to a clean stopped state; a port held by another program ends in `failed` with a readable message; a server already running at GUI start is adopted.

### FR-2 Profile selector

- Two-level selection, mirroring the existing convention: first list the subfolders of `CrispASR\profile\`, then the `*.json`/`*.conf` files in the chosen folder.
- Parse JSONC: strip `//` and `#` comments outside string literals, honoring backslash escapes inside strings (profile URLs contain `//`), then parse strict JSON. Report unparsable files as errors without crashing, and keep listing the others.
- Show per-profile readiness: `[OK]` when the profile's `model` file exists (relative to `CrispASR\models\`), `[MISS]` otherwise, with the profile's `download` URL as a hint. Show the same for `aligner` when present.
- The profile schema is the existing one: `comment, build, backend, model, aligner, language, host, port, device, api_keys, download, env, args[]`, plus optional tuning fields (`vad, force_aligner, chunk_seconds, beam_size, temperature, frequency_penalty, max_new_tokens, seed, no_punctuation, split_on_punct`). Unknown fields are preserved in memory. Known flags inside `args[]` (for example `--vad-threshold 0.3`) are parsed so the parameter panel can show their values (FR-4). Reference: `reference/crispasr/start-crispasr.ps1`.

**Acceptance**: all four profiles under `profile\Japanese\` parse and show `[OK]` on the owner's machine; a profile with a missing model shows `[MISS]` and its download URL. (`verified` in WSL2 on 2026-10-11: the four profiles in `reference/crispasr/profiles-japanese/` parse with an independent implementation of the stripping rule; the Windows check is `not run`.)

### FR-3 Transcription (single file)

- Pick one or more audio or video files. The engine decodes wav/mp3/flac/ogg natively and m4a/aac/opus/webm/wma through its internal ffmpeg (verified in the CLI help, per the original specification).
- Refuse files larger than the server's 512 MB upload limit before sending, with a readable message.
- Send `POST /v1/audio/transcriptions` as `multipart/form-data` with `file`, `model` (informational; the server ignores it), `response_format=verbose_json` (*proposed*, [OQ-7](OPEN-QUESTIONS.md#oq-7-local-srt-generation)), `language`, the job's effective request-tier parameters (FR-4), and `offset_t_ms`/`duration_ms` for each window (FR-5). Every request carries `Authorization: Bearer <key>` (FR-7).
- Write `<name>.srt` from the returned segments, next to the source file or into the output directory from the settings. With no subtitle shaping applied, the file equals the server's own `srt` output for the same segments: cue numbering, timestamps, and text. Write it atomically (a temporary file in the destination directory, then rename).
- Show request status, elapsed time, window progress, `/progress` for the in-flight window, and a text preview.
- Map errors to readable messages per [API-CRISPASR.md §5](API-CRISPASR.md#5-errors): no connection → "server not running" or "server still starting", decided by the process state; `400` → the engine's message; `401` → key mismatch; `413` → file too large; `503` → model loading, retry; `500` → internal error, retry once.

**Acceptance**: a Japanese sample from `CrispASR\samples\` transcribes through the GUI to an SRT file identical to the server's `response_format=srt` output for the same audio and parameters (Windows-side acceptance).

### FR-4 Parameter panel (pluggable, one parameter set per model)

Owner invariant (2026-10-11): every model has its own parameter set, and the panel shows the settings that the model's authoritative description defines. A fixed parameter page must never limit which parameters a model exposes. Design: [ARCHITECTURE.md §2.7](ARCHITECTURE.md#27-parameter-model-pluggable).

- The panel has no hardcoded parameter page. It renders the parameter set resolved for the selected profile from data: a catalog of parameter descriptors (field names, types, ranges, tier, required engine capability) and the model's authoritative description (the profile itself, plus an optional model manifest whose location is [OQ-5](OPEN-QUESTIONS.md#oq-5-location-of-the-models-authoritative-parameter-description)), filtered by the capabilities the engine reports for the backend. Adding a parameter or a model changes data files, not panel code.
- Tiers, shown on every control:
  - **Request**: sent with each request; a change applies to the next job without a restart.
  - **Startup**: comes from the server command line; a change needs a restart. "Restart with changes" starts the server with a temporary command line and never writes the profile.
  - **GUI**: client-side options, such as the transcription window length and subtitle shaping.
- Effective value = the job's override if set, else the profile's value as the launcher applies it (including flags inside `args[]`), else the server default. The panel shows where each effective value comes from.
- A parameter that the engine reports as unsupported for the backend, or that the model's description leaves out, is not shown.

**Acceptance**: the four Japanese profiles render different parameter sets with no model-specific code (for example, no temperature or beam size for the `fastconformer-ctc` profile); adding a descriptor or a manifest entry changes the panel after a GUI restart without recompiling; changing `vad_threshold` at request level affects the next job without a server restart; the backend can change only by switching profile and restarting, and the panel says so.

### FR-5 Batch queue

- A job is one input file, a reference to its profile, and a **parameter snapshot** taken at enqueue time: the effective request-tier values and the GUI-tier values. Later edits in the panel affect only new jobs.
- Job states: `queued → running → done | error | cancelled`, with retry for error and cancelled jobs.
- Sequential execution: one request at a time, because the server serves one model.
- **Transcription windows** (owner direction, 2026-10-11): each job runs as consecutive windows of the snapshot's window length, where 0 means the whole file in one request. Each window is one request with `offset_t_ms`/`duration_ms`. A failed window is retried automatically, 2 times by default ([OQ-6](OPEN-QUESTIONS.md#oq-6-transcription-window-defaults)), before the job turns to `error`; a later manual retry continues from that window. When the server is down, the job turns to `error` without spending its retries, and the queue pauses.
- **Window-granular resume**: completed windows are checkpointed. After a crash or restart, a running job returns to `queued` and continues from its first incomplete window.
- Progress: completed windows and processed audio time, plus `/progress` for the in-flight window; elapsed time and a spinner where nothing finer is available.
- Cancel aborts the GUI's request. The server still finishes the window in progress, because it has no cancel endpoint, and the next request waits for it.
- Persistence: `jobs/<id>.json` is rewritten atomically on every state change, and `jobs/<id>.ckpt.jsonl` gets one appended line per completed window. Both survive app restarts. The layout follows `reference/crisperweaver/batch_persistence_service.dart`.
- When the next job needs a different profile than the running server, the queue follows [OQ-10](OPEN-QUESTIONS.md#oq-10-queue-behavior-when-the-next-job-needs-another-profile).
- Reorder (move up/down), remove, and clear finished jobs. On completion, show the job's SRT path.

**Acceptance**: enqueue 3 files and run them; killing the app mid-job and restarting continues the interrupted job from its first incomplete window and completes it; stopping the server during a window and starting it again leads to that window being retried; finished jobs have SRT files.

### FR-6 Extensibility foundation (the "step chain")

- `Job` contains `steps: StepSpec[]`. In v1 every job has exactly one step, `{type: "asr", params: {...}}`.
- A `StepSpec` is a tagged type `{type, params, state}`, where `type` comes from a closed enum that will grow (`separate`, `diarize`, `asr`, `events`, `translate`, `merge`) and `state` is the step's checkpoint data, owned by its runner.
- A `StepRunner` interface: `run(inputs, params, context) -> outputs`, where the context provides progress, cancellation, and checkpoint storage. v1 implements only `AsrStepRunner` (HTTP windows). Future runners implement the other pipeline steps without touching the queue.
- The queue executes a job by iterating its steps and passing each step's output as the next step's input (trivially one step in v1). It persists each step's `state` without knowing its schema.
- No UI for composing chains in v1; the data model must not need a rewrite when that UI arrives.

**Acceptance**: code review — adding a hypothetical `separate` step requires implementing one class and registering it, not touching the queue or persistence.

### FR-7 Settings (GUI-side)

- Global settings are persisted by the GUI itself as a JSON file in the app data directory, **not** in CrispASR: CrispASR root path, server port, API key, default output directory, default window length, request timeout, UI language (en/zh), theme, and the app data directory location.
- The API key defaults to `default`. The owner runs everything locally and rates the key as low importance; it is a setting, not a secret.
- Defaults match the current deployment: root `E:\AI\ASR\CrispASR`, port 5114, key `default`, app data directory `<app folder>\appdata`.

## 4. Non-functional requirements

- Target Windows 10/11 x64, C++17, CMake ≥ 3.25. The Windows executable is cross-compiled in WSL2 with MinGW-w64 GCC 12 and linked statically, so a delivery is a folder the owner copies ([ENVIRONMENT.md §1](ENVIRONMENT.md#1-target-and-toolchains)).
- All development happens in WSL2, so the application also builds and runs on Linux for development. Platform-specific code (process management, HTTP transport, paths) sits behind interfaces with Windows and Linux implementations. Windows-native behavior is accepted only on Windows.
- UI text in English and Chinese through a string table (EUI-NEO has CJK font support and IME); code comments in English.
- No telemetry and no network access except to the configured local server endpoints.
- The GUI stays responsive during long transcriptions: HTTP calls, process management, and disk writes happen off the UI thread, and results reach the UI through EUI-NEO's async completion callbacks or `app::requestUpdate()` (`reference/eui-neo/docs/异步.md`).
- Crash containment: an engine failure surfaces as a failed window, job, or server state and never takes the GUI down; the engine is a separate process.
- No data on `C:\` ([AGENTS.md](../AGENTS.md#hard-rules) rule 4).
- Startup time < 2 s; memory footprint dominated by the UI framework, since the engine lives in its own process.

## 5. Out of scope for v1

- Pipeline steps 1, 2, 4, 5, 6 (separation, diarization UI, event tagging, translation, merge): the data model supports them; the runners and UI come later.
- Model downloads or management beyond existence checks.
- Profile editing or saving (v1 is read-only toward CrispASR files).
- Multiple simultaneous server instances.
- In-process engine integration (the C API path in the audit, §7, is the future evolution).
- GUI-side audio decoding or pre-splitting of files.
- Non-Windows platforms as a product target (Linux builds serve development only).

## 6. Acceptance summary

The v1 milestone is accepted on Windows when the GUI can start the server from any Japanese profile, render each profile's own parameter set, transcribe a batch of at least three files to correct SRT files with per-job parameter snapshots, retry a failed window, survive an app restart mid-batch by continuing from the first incomplete window, and every requirement above has a demonstrated path in the UI.
