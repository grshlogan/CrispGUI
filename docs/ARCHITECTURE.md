# Architecture — CrispRustGUI

> Design decisions with rationale. Companion documents: [SPEC.md](SPEC.md) (what), [API-CRISPASR.md](API-CRISPASR.md) (the server contract), [TASKS.md](TASKS.md) (build order). Revised on 2026-10-11: owner invariants (§1.1), transcription windows (§2.4, §2.5), the pluggable parameter model (§2.7), the SRT writer (§2.8), the platform split, and corrections from a source review; revised on 2026-10-12 with the Phase 2 implementation (§2.1, §2.3, §2.6, §3, §4). Entries marked (P1) and (P2) in §2 are implemented; items marked *proposed* wait for the owner's decisions in [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md).

## 1. Architectural style: thick client over a local service

CrispRustGUI is a **desktop client for a local HTTP service**. The engine (the CrispASR server) is a separate process that the GUI manages; all AI work happens there. This was chosen over in-process engine embedding (what CrisperWeaver does through FFI) for v1 because:

1. **Batch efficiency**: the server keeps a model resident across requests, so there is no per-file model load. FFI embedding achieves the same at a much higher implementation cost.
2. **Crash containment**: engine faults kill the server process, not the GUI.
3. **It matches the user's mental model**: "the GUI starts and stops the service".
4. **Evolution path**: the C API (`reference/crispasr/crispasr_session.h`, crispasr-audit §7) documents how to move the engine in-process later if ever needed; nothing in the v1 design blocks that.

Trade-off accepted: switching models requires a server restart (seconds of model load). This matches the existing deployment (one model per server instance) and the profile-as-truth convention.

### 1.1 Owner invariants

Set by the owner on the dates shown; changing one needs the owner.

1. **One model, one parameter set** (2026-10-11). The parameter panel is pluggable and generated from the selected model's authoritative description. No fixed parameter page may limit which parameters a model exposes (§2.7).
2. **Small units with retry** (2026-10-11). A job runs as transcription windows. A failed window is retried instead of building elaborate resume logic, and resume after a restart happens at window granularity (§2.5).
3. **Profiles are the truth, and the GUI is a client** (SPEC). The GUI reads profiles, never writes CrispASR files, and drives a single-model server over HTTP.
4. **Keep data off `C:\`** (2026-10-11). A temporary write that a third-party component cannot redirect stays inside the system TEMP directory ([AGENTS.md](../AGENTS.md#hard-rules) rule 4).

## 2. Component model

Status: Phase 1 created the entries marked `(P1)` and Phase 2 those marked `(P2)`; the rest is `planned`.

```
crisprustgui/
├── src/
│   ├── main.cpp                      // (P1) EUI-NEO entry points; owns the single Application
│   ├── app/                          // (P1) application.{h,cpp}: state and page composition; app_data.{h,cpp}: app data folder and TMP/TEMP; (P2) crispasr_location.{h,cpp}: the CrispASR root; later the UI inbox
│   ├── core/
│   │   ├── profile_store.{h,cpp}     // (P2) profile model, scanning, readiness
│   │   ├── jsonc.{h,cpp}             // (P2) JSONC comment stripping
│   │   ├── cli_flags.{h,cpp}         // (P2) the CrispASR CLI flags (cli_flags_table.inc, generated) and the args[] reader
│   │   ├── launch_command.{h,cpp}    // (P2) the launcher's tuning conditions; the full command line in Phase 3
│   │   ├── server_manager.{h,cpp}    // pre-flight, process lifecycle, health polling, log buffer
│   │   ├── api_client.{h,cpp}        // typed CrispASR calls over an HttpTransport
│   │   ├── params/                   // parameter catalog, sources, resolver (§2.7)
│   │   ├── job_model.{h,cpp}         // Job / StepSpec / states
│   │   ├── job_queue.{h,cpp}         // queue execution, ordering, pause and resume
│   │   ├── job_store.{h,cpp}         // persistence: per-job JSON + checkpoint lines
│   │   ├── srt_writer.{h,cpp}        // segments -> SRT (§2.8)
│   │   └── step_runners/
│   │       ├── step_runner.h         // interface
│   │       └── asr_step.cpp          // v1's only runner: the window loop (§2.5)
│   ├── platform/                     // (P1) paths.h with paths_win32.cpp and paths_posix.cpp; (P2) files.h; later HttpTransport (WinHTTP | libcurl), Process (Win32 | POSIX)
│   ├── ui/
│   │   ├── pages: dashboard / transcribe / batch / settings / log  (P1: pages.{h,cpp}; P2: the dashboard lists the profiles); (P2) fonts.{h,cpp}
│   │   └── widgets: generated parameter panel, profile picker, job table
│   └── util/                         // (P1) string_table.{h,cpp}; (P2) json.{h,cpp}: JSON values and the parser, sha256.{h,cpp}; later the JSON writer and logging
├── params/                           // data: catalog/*.json, models/<folder>/<name>.json (§2.7)
├── cmake/toolchains/                 // MinGW-w64 cross-compilation toolchain file
├── tools/                            // smoke builds, Windows packaging, the Windows test runner, the CLI flag extractor
└── tests/                            // (P1) unit tests with check.h; later a stub server for HTTP tests
```

EUI-NEO is not part of the tree; the build adds it from the shared Learn library ([ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot)).

### 2.1 ProfileStore

Implemented in Phase 2 (`src/core/profile_store.h`, `jsonc.h`, `cli_flags.h`, `launch_command.h`; [Phase 2 report](devlog/phase-2.md)).

- JSONC: iterate over characters; inside a string, `"` ends the string and a backslash escapes the next character; outside strings, `//` and `#` start a comment that runs to the end of the line. Never strip inside string literals; profile URLs contain `//`. Like the launcher, which works line by line, a string ends at the end of its line. The launcher's `Remove-JsonComments` compares a character with the PowerShell literal `'\\'`, which is two characters long (`start-crispasr.ps1:34`), so its escape branch probably never fires; it works today only because no profile string contains `\"`. The GUI implements escapes correctly instead. The stripped text then goes to a strict JSON parser (`src/util/json.h`) that reports the line and column of an error.
- Model: `Profile` holds the file's path, folder, name, and SHA-256 (for job snapshots); the known keys (`comment`, `build`, `backend`, `model`, `aligner`, `language`, `download`, `host`, `port`, `device`, `api_keys`, `env`, `args`); the whole parsed object as `raw`, tuning keys and unknown keys included; an `error` that makes it unusable and a list of `warnings`; and the readiness paths and flags.
- Reading rules, chosen so that the GUI never silently differs from the launcher:
  - Keys are compared without regard to case, as PowerShell's property access is. Keys that differ only in case make the profile unusable, as `ConvertFrom-Json` rejects them; an identical key twice keeps the last value with a warning.
  - `build`, `backend`, and `model` are required. A known key with the wrong type makes the profile unusable instead of being coerced the PowerShell way (`"vad": "false"` would be true there): strings for the string keys, a string or whole number for `device`, a whole number up to 65535 for `port` (0 means 5114, as in the launcher), an object of strings or numbers for `env`, an array of strings or numbers for `args`, booleans and numbers for the tuning keys. `null` counts as absent.
  - A UTF-8 byte order mark is skipped; UTF-16 files and files over 1 MiB are rejected with a message.
  - Folders and files are listed as the launcher lists them: subfolders of `profile\` sorted by name without regard to case, `.json` and `.conf` files (any case) inside them, hidden and system entries skipped, files directly inside `profile\` ignored.
- Readiness mirrors the launcher: paths join below the CrispASR root (`build`) or the models folder (`model`, `aligner`) as `Join-Path` joins them, with backslashes and slashes as separators and leading separators dropped. A path with a drive letter cannot lie below the folder and is reported. `[OK]` and `[MISS]` follow the model file, as in the launcher's menu; the dashboard also reports a missing server executable or aligner and shows the `download` URL for a missing model.
- `args[]` is read with the CrispASR 0.8.42 CLI's rules from a flag table extracted from its source (`tools/extract-cli-flags.py`; OQ-4 item 2): a flag that takes a value takes the next argument, and a later flag overrides an earlier one for the same parameter. Unknown flags, missing values, unparsable numbers, values outside the accepted choices, and flags that make the CLI exit become warnings, because a newer engine may accept what 0.8.42 rejects.
- The launcher's apply conditions decide which tuning keys reach the server (for example `temperature` only when > 0 and `max_new_tokens` only when non-zero; [API-CRISPASR.md §3.2](API-CRISPASR.md#32-tier-b-startup-only-parameters)). `ReadProfileStartup` emits the tuning flags whose condition holds in the launcher's order, then `args[]`, and reads the result with the CLI rules, so each parameter's effective startup value and its source (tuning key or `args[]`) are known. A value the launcher does not pass is the server default. The parameter panel (§2.7) builds on this.
- Unknown keys stay in `raw` (forward compatibility; the CrisperWeaver preset lesson: tolerate unknown and missing fields, and version the GUI's own files).
- The scan runs on EUI-NEO's async pool, and the result replaces the previous one on the UI thread; a newer scan supersedes a running one.

### 2.2 ServerManager

- Builds the command line and environment exactly as [API-CRISPASR.md §3.2](API-CRISPASR.md#32-tier-b-startup-only-parameters) describes, and adds `TMP` and `TEMP` pointing at `<appdata>/tmp/server`, because on Windows the server writes each upload to the system temp directory ([API-CRISPASR.md §6](API-CRISPASR.md#6-lifecycle-and-resource-behavior), item 8).
- Pre-flight through `/health` before spawning (SPEC FR-1).
- Spawns the child: on Windows, `CreateProcessW` with an explicit environment block, redirected stdout and stderr, and no console window; on Linux development builds, `posix_spawn`. The child is not placed in a kill-on-close job object, so it survives a GUI crash and can be adopted afterwards.
- A reader thread drains stdout and stderr continuously into a ring buffer for the log pane and a log file. The server logs to stderr, and an undrained pipe would block it.
- State machine `stopped → starting → running → stopping`, plus `failed`. `starting` lasts while the process is alive and `/health` refuses connections, because the server loads its model before it listens. `failed` is entered when the process exits; an exit code of 0 is not success, since a failed `listen()` also returns 0 ([API-CRISPASR.md §6](API-CRISPASR.md#6-lifecycle-and-resource-behavior), item 3).
- Polls `/health` every 1–2 s on a background thread and reads `/v1/models` after reaching `running`.
- Adoption: when `/health` answers `200` at GUI start, the server shows as adopted; Stop warns that it may belong to someone else.
- Stop: `TerminateProcess` on Windows, SIGTERM then SIGKILL on Linux; reap the process; state `stopped`.

### 2.3 ApiClient and HTTP transport

- `ApiClient` offers typed operations: `health()`, `progress()`, `models()`, and `transcribe(file, fields, window, cancel)`, which returns the parsed `verbose_json` transcript or an `ApiError`.
- `HttpTransport` is the platform seam: `get(url, headers, timeout)` and `postMultipart(url, headers, fields, filePath, timeout, cancel)`, which streams the file from disk instead of loading it into memory. Implementations: WinHTTP on Windows, a system library that needs no third-party import, and libcurl on Linux development builds (*proposed*, depends on OQ-1 and OQ-3; the original plan used libcurl everywhere with WinHTTP as a fallback, eui-neo-audit §3.1).
- Every request carries `Authorization: Bearer <key>` from the settings ([API-CRISPASR.md §2](API-CRISPASR.md#2-authentication)).
- Errors are typed after [API-CRISPASR.md §5](API-CRISPASR.md#5-errors): `Unreachable`, `Timeout`, `Cancelled`, `BadRequest(message)`, `Unauthorized`, `PayloadTooLarge`, `ServerBusy` (503), `ServerError` (500), `Protocol` (unexpected body). The ASR runner decides which ones to retry (§2.5).
- Responses are parsed with the project's JSON parser (`src/util/json.h`, Phase 2), because EUI-NEO's `eui/json.h` cannot list an object's members ([ENVIRONMENT.md §4.4](ENVIRONMENT.md#44-eui-neo-runtime-facts)); a small JSON writer in `util/` will produce the GUI's own files.
- The GUI does not use `eui::network`: it is GET-only, starts once per key, sends no headers, and on Windows without libcurl downloads through files in the system temp directory.
- Timeouts: short for connecting (about 2 s); generous and configurable for responses, since one window on a slow backend can take minutes.

### 2.4 Job model (the step chain)

```cpp
enum class StepType { Asr /*, Separate, Diarize, Events, Translate, Merge*/ };

struct StepSpec {
  StepType type;
  Json params;                       // frozen parameter snapshot for this step
  Json state;                        // runner-owned checkpoint summary, opaque to the queue
  std::vector<std::string> outputs;  // file paths produced
};

enum class JobState { Queued, Running, Done, Error, Cancelled };

struct ProfileRef { std::string folder, name, contentHash; };

struct Job {
  std::string id;                    // uuid
  std::string inputPath;             // v1: audio/video file
  ProfileRef profile;                // the profile the job was enqueued with
  std::vector<StepSpec> steps;       // v1: exactly one Asr step
  JobState state;
  std::string error;                 // last error, readable
  std::string createdAt;
};
```

- **Snapshot at enqueue** (CrisperWeaver pattern P2, made per job): the effective request-tier values of the resolved parameter set, the GUI-tier values (window length, retries, boundary guard, subtitle shaping), and the catalog and manifest versions used. Later panel edits never change queued jobs.
- **Persistence** (`job_store`): `jobs/<id>.json` is rewritten atomically (temporary file plus rename in the same directory) on every state change, and `jobs/<id>.ckpt.jsonl` is appended and flushed at every runner checkpoint, one line per completed ASR window. Writes run on a background thread, ordered per job. Loading tolerates a torn last line (CrisperWeaver's `loadCheckpoint`).
- **Startup repair**: a job found `running` returns to `queued`; its runner rebuilds its position from the checkpoint lines.
- **Execution**: a single worker drains the queue in order; a job's steps run sequentially, each step's output feeding the next. v1 has one step, but the loop is written against `steps[]`.

### 2.5 StepRunner interface and the ASR window loop

```cpp
struct StepContext {
  ProgressFn progress;               // fraction plus text, marshalled to the UI
  const CancelToken& cancel;
  CheckpointStore& checkpoints;      // append(Json) / readAll() for this job and step
};

class StepRunner {
public:
  virtual ~StepRunner() = default;
  virtual StepType type() const = 0;
  // inputFiles: outputs of the previous step, or the job input for the first step
  virtual StepResult run(const std::vector<std::string>& inputFiles,
                         const Json& params, StepContext& ctx) = 0;
};
```

Registration is a static map; adding a step touches one file plus the enum. Future runners (separation, translation through the Hy-MT2 node, merge) implement the same interface.

`AsrStepRunner` follows owner invariant 2; its defaults are [OQ-6](OPEN-QUESTIONS.md#oq-6-transcription-window-defaults):

1. Read the completed windows from the checkpoint. `offset` is the last line's `next_offset_ms`, or 0.
2. Request the window `[offset, offset + window)` through `offset_t_ms` and `duration_ms`, both omitted when the window length is 0, with `response_format=verbose_json` and the snapshot's request-tier fields (§2.7, request builder).
3. A response with no segments and `duration` 0 means the offset lies past the end of the audio: finish.
4. Let `end = offset + duration`. If the window was full (`duration` ≥ window − 0.5 s, so more audio may follow) and the last segment ends within the boundary guard of `end` and starts later than `offset + guard`, drop that segment and set `next` to its start; otherwise set `next = end`. A segment cut by the window edge is then transcribed whole in the next window instead of being kept in halves.
5. Append `{window, offset_ms, next_offset_ms, segments}` to the checkpoint and flush.
6. If the window was not full, finish; otherwise set `offset = next` and continue.
7. On finish, merge all checkpointed segments in time order, write the SRT atomically (§2.8), and record the output; the queue marks the job done and deletes the checkpoint.

Failure handling for one window:

- `Timeout`, `ServerError`, `Protocol`, `ServerBusy`: retry the same window up to the snapshot's retry count, with backoff.
- `BadRequest`, `Unauthorized`, `PayloadTooLarge`: the job fails at once; retrying cannot help.
- `Unreachable` (server down): the job fails without spending retries, and the queue pauses until the server runs again.
- A later manual retry starts from the first incomplete window.

Cost: every window request uploads and decodes the full file ([API-CRISPASR.md §6](API-CRISPASR.md#6-lifecycle-and-resource-behavior), item 9). The window length trades retry granularity against that repeated work; very short windows are possible but multiply it.

Progress combines the completed windows and processed time from the checkpoint with `GET /progress` once per second while a window is in flight; the value is global, and v1 sends one request at a time.

Cancellation aborts the transport. The server finishes the window anyway. The checkpoint stays, so a retry continues from the first incomplete window.

### 2.6 Threading model

| Thread | Owns | Notes |
|---|---|---|
| UI thread | EUI-NEO compose loop and all UI state | Never blocks: no HTTP, no process waits, no file writes |
| Queue worker | Job execution, the ASR window loop, the current job's blocking HTTP calls | One dedicated `std::thread`; posts results to the UI inbox |
| Health and progress poller | `/health` and `/progress` | Short calls; a dedicated thread or `app::async` |
| Server log reader | Child stdout and stderr pipes | Ring buffer, then `app::requestUpdate()` |
| Job store writer | Job files and checkpoint lines | Ordered per job |

Rule: shared state is confined to the UI thread except for the explicitly guarded UI inbox and job store. Worker threads hand results to the UI through the mutex-protected inbox plus `app::requestUpdate()`, or through `app::async` completion callbacks (eui-neo-audit §3.3). Long transcription calls do not run on EUI-NEO's shared async pool: it has at most four workers ([ENVIRONMENT.md §4.4](ENVIRONMENT.md#44-eui-neo-runtime-facts)), and a call that lasts minutes could starve other async work. Short tasks such as the profile scan use the pool.

### 2.7 Parameter model (pluggable)

Owner invariant 1. Goals: each model shows exactly its own parameter set; adding a parameter or a model changes data, not code; the panel never assumes a fixed page.

1. **Parameter catalog**: data files `params/catalog/*.json`, loaded at startup, versioned by `schemaVersion`, tolerant of unknown fields. One descriptor per parameter:

   ```json
   {
     "id": "vad_threshold",
     "group": "vad",
     "label": "param.vad_threshold.label",
     "help": "param.vad_threshold.help",
     "type": "float",
     "range": {"min": 0.0, "max": 1.0, "step": 0.05},
     "default": 0.5,
     "tier": "request",
     "request_field": "vad_threshold",
     "profile_key": null,
     "cli": {"flag": "--vad-threshold", "apply": "always"},
     "requires_caps": [],
     "widget": null
   }
   ```

   `type` is `bool`, `int`, `float`, `enum` (with `options`), `string`, or `string_list`. `tier` is `request`, `startup`, or `gui`. `request_field` names the multipart field ([API-CRISPASR.md §3.1](API-CRISPASR.md#31-tier-a-per-request-parameters)); `profile_key` names the top-level profile key, and a descriptor may map an inverted key (`no_punctuation` ↔ `punctuation`); `cli` gives the startup flag and the launcher's apply condition (`always`, `truthy`, `gt0`, `present`) for reading `args[]` and building command lines; `requires_caps` lists engine capability names the backend must report; `widget` optionally names a custom widget. Request field names are confirmed from the server source, and CLI flag names from the CLI source (`src/core/cli_flags_table.inc`, Phase 2). Capability names wait for [OQ-4](OPEN-QUESTIONS.md#oq-4-source-material-needed), item 2.
2. **Sources**, pluggable providers that contribute to a model's parameter set:
   - `ProfileSource`: the profile's tuning keys and recognized `args[]` flags, with values as the launcher applies them.
   - `ManifestSource`: the model's explicit parameter description: which parameters to expose and in what order, with model-specific defaults, ranges, locks, and notes. Its location is [OQ-5](OPEN-QUESTIONS.md#oq-5-location-of-the-models-authoritative-parameter-description): a block inside the profile or `params/models/<folder>/<name>.json` in this repository.
   - `CapabilitySource`: the backend's capabilities from `crispasr.exe --list-backends-json` (declared in `reference/crispasr/crispasr_backend.h`), read once per executable and cached in the app data directory. It removes descriptors whose `requires_caps` the backend lacks. That the option loads no model is a `hypothesis` until observed.
3. **Resolver**: produces the model's `ParameterSet`. A manifest, when present, decides the exposed list. Without one, the exposed list is the parameters the profile sets plus a small baseline: `language` and the GUI-tier window and subtitle options. Capability filtering applies in both cases. Each entry carries its descriptor, profile value, effective value, and the source of the effective value.
4. **Widget factory**: maps descriptor types to EUI-NEO components: `bool` → `toggleSwitch`, `int` → `stepper`, `float` → `slider` with a numeric `input`, `enum` → `dropdown`, `string` → `input`, `string_list` → multiline `input`. Custom widgets, such as a hotword list editor or the `att_context` pair, register by name and are referenced through `widget`. The panel never switches on parameter IDs.
5. **Request builder**: sends the request-tier entries that the job overrides, and always sends `language`. Unchanged values are left to the server, which already has them from the profile; some fields change server behavior merely by being present (sending `chunk_seconds` marks it explicit, which changes VAD slicing; [API-CRISPASR.md §3.1](API-CRISPASR.md#31-tier-a-per-request-parameters)). This relies on the server having been started from the job's profile, which the queue checks through `ProfileRef.contentHash` before each job ([OQ-10](OPEN-QUESTIONS.md#oq-10-queue-behavior-when-the-next-job-needs-another-profile); an adopted server's profile is unknown, so the queue warns). Startup-tier changes go to "Restart with changes", which builds a temporary command line from the profile plus overrides without writing the profile. GUI-tier entries configure the runner and the SRT writer.

Values are validated against descriptor ranges before a job is enqueued, because the server silently ignores malformed numbers ([API-CRISPASR.md §3](API-CRISPASR.md#3-request-parameters-for-v1audiotranscriptions)).

Illustration from the current profiles, without a manifest: the `parakeet-ctc-1.1b-ja-q8` profile sets `vad`, `chunk_seconds`, `frequency_penalty`, `seed`, and `--vad-max-speech-duration-s 15` in `args[]`, but no `temperature` or `beam_size`, so those two never appear; its comments call `frequency_penalty` and `seed` meaningless for CTC, which a manifest would express by hiding them. The `kotoba-whisper-v2.2-q8` profile additionally sets `beam_size`, `temperature`, and `max_new_tokens`, so its panel differs without any model-specific code.

### 2.8 SRT writer

- Input: the merged segments (start and end in seconds with centisecond precision, text, optional words and speaker).
- Default output reproduces the server's `srt` format ([API-CRISPASR.md §4](API-CRISPASR.md#4-response-formats)): one cue per segment with non-empty text, numbered from 1, `HH:MM:SS,mmm --> HH:MM:SS,mmm`, the text, and a blank line, in UTF-8. Equivalence with the server's own output is a test (SPEC FR-3).
- Optional shaping (GUI tier): a maximum number of characters per cue and splitting at sentence-ending punctuation, using word timings when present and proportional timing otherwise. The behavior follows `crispasr_output.cpp`, but the code is written independently (AGPL boundary).

## 3. Key decisions and rejected alternatives

| Decision | Choice | Rejected | Why |
|---|---|---|---|
| Engine integration | HTTP client to a managed server process | In-process FFI (CrisperWeaver-style) | Crash containment, batch model residency, simpler v1, matches the user's "start/stop service" model. The C API path is documented for the future. |
| UI framework | EUI-NEO v0.6.1, built from the pinned snapshot in the shared Learn library | Rust GUIs, Tauri | Same language as the engine (no FFI), Apache-2.0, component coverage verified in the audit; version pinned to control 0.x churn |
| Windows build | MinGW-w64 GCC 12 cross-compilation in WSL2, static runtime | An owner-side MSVC build; a CI build on a Windows runner | Owner decision (2026-10-11): keeps the copy-a-folder workflow and needs no Windows toolchain; EUI-NEO's CMake files support MinGW |
| HTTP stack | In-repo transport behind `HttpTransport`: WinHTTP on Windows, libcurl on Linux (*proposed*) | EUI-NEO `requestText` (GET-only); libcurl everywhere | Multipart POST is required; no third-party HTTP library has to be imported for Windows |
| Profiles | Read the existing JSONC files; no GUI editing in v1 | A new GUI-side preset store | Profiles are already the project's single source of truth with proven content (4 Japanese profiles, tested) |
| Model switching | Restart the server with the new profile | Hot-swap through `/load` | The server holds one model per process, and `/load` keeps the old aligner (crispasr-audit §5.3–5.4) |
| Language | C++17 end to end | Rust glue | User decision: one language across engine and GUI, no glue layer |
| Parameter panel | Data-driven catalog plus the model's description plus a capability filter, rendered by a widget factory | One hardcoded page; per-model pages in code | Owner invariant 1: every model has its own parameter set, and new parameters or models must not need code changes |
| Long jobs | Transcription windows with per-window retry and window-granular resume | One request per file with segment-level resume | Owner direction; the HTTP endpoint returns results only at the end of a request, so segment checkpoints are impossible without windows |
| Response format | `verbose_json` plus a GUI-side SRT writer (*proposed*, OQ-7) | Saving the server's `srt` | The server's `srt` ignores `max_len`/`split_on_punct`, and windows need segment timings |
| JSON parsing | An in-repo parser that keeps member order, duplicates, and number text, with line and column in errors | EUI-NEO's `eui/json.h`; yyjson directly | `eui/json.h` cannot list object members; EUI-NEO compiles yyjson into its own library, so a second copy of the same symbols would risk duplicate definitions at link time |
| UI font | HarmonyOS Sans SC (Regular, Medium, Bold), provided by the owner and kept out of Git | EUI-NEO's bundled bold display font; Microsoft YaHei | Owner decision (2026-10-11, round 6): better legibility; the license is not verified, so the files are not redistributed through Git |
| Development platform | Linux builds in WSL2 for development; Windows for acceptance | Development on Windows | Owner rule: Windows does no development, and WSL2 never mounts Windows drives |

## 4. Risk register

| Risk | Likelihood | Mitigation |
|---|---|---|
| EUI-NEO 0.x API changes | Medium | Version pinned to a snapshot; upgrades are explicit re-evaluation tasks |
| EUI-NEO's MinGW-w64 path breaks | Low | Upstream CI builds Windows only with MSVC. Phase 0: the MinGW build passes after one header shim ([ENVIRONMENT.md §4.2](ENVIRONMENT.md#42-eui-neo-build-facts)), and its tests and probes pass on the owner's Windows machine; fallback: an owner-side MSVC build or a CI build |
| The build depends on the Learn library outside the repository | Low | `CRISPGUI_EUI_NEO_DIR` names the snapshot, and configuration stops with a clear message when it is missing |
| Release builds run without exceptions or RTTI | Low | Convention: application code uses neither ([CONVENTIONS.md](CONVENTIONS.md#c-conventions)); tests run in both Debug and Release |
| The deployed engine differs from the 0.8.42 source | Low | The CLI flags come from the 0.8.42 source (Phase 2); `args[]` flags the table does not know are warnings, not errors. The deployed build's `--help` and `--list-backends-json` output would confirm the version (OQ-4 item 2), and capability names are still to be extracted for Phase 6 |
| Window boundaries cut speech | Medium | Boundary guard re-transcribes cut segments; window length 0 sends short files in one request |
| Repeated upload and decode per window | Medium | Window length setting, default 300 s (OQ-6) |
| Long-file transcription timeouts | Medium | Configurable response timeout; per-window retry |
| EUI-NEO async pool too small for long calls | Medium | Dedicated queue worker thread for blocking calls (§2.6) |
| `/progress` is global | Low | One request at a time; an adopted server may show foreign work |
| Server output floods the UI | Low | Ring buffer, capped log pane |
| Port held by another program at startup | Low | Pre-flight through `/health`; the GUI reports "port busy" instead of starting |
| CJK rendering or IME issues | Low | The EUI-NEO audit verified CJK fonts and IME (msyh fallback chain) |
