# Reference Library — Provenance and Reading Guide

> Curated source files from three projects, selected by the audit phase (reports in `audits/audit-reports/`, Chinese). These are **read-only learning materials** — study them, do not compile them as part of this repo, do not modify them.

## Provenance and licenses

| Directory | Source project | Upstream location | License |
|---|---|---|---|
| `crispasr/` | CrispASR (the engine) | `E:\AI\ASR\CrispASR\Source` (v0.8.42 snapshot) | MIT |
| `crisperweaver/` | CrisperWeaver (official GUI) | `E:\AI\ASR\CrisperWeaver\Source` (v0.11.0) | AGPL-3.0 (as recorded by the audit phase; not re-verified) |
| `eui-neo/` | EUI-NEO (UI framework) | `E:\project\Learn\sources\github.com\sudoevolve\EUI-NEO-v0.6.1` (pinned); the full snapshot is at `/home/claude/projects/Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1` in WSL2 | Apache-2.0 |
| `audits/` | This project's audit reports | Written by the audit phase | Internal |

`crispasr/start-crispasr.ps1` and `crispasr/profiles-japanese/` come from the owner's CrispASR deployment, not from the upstream source.

Verification (2026-10-11, A003): the other six files in `crispasr/` (`crispasr_server.cpp`, `crispasr_output.cpp`, `crispasr_backend.h`, `crispasr_session.h`, `concurrency.md`, `diarization-speakers.md`) have the same SHA-256 as the files of the same name in the CrispASR 0.8.42 source archive in the Learn library (now `/home/claude/projects/Learn/.downloads/CrispASR-v0.8.42.zip`, extracted at `sources/github.com/CrispStrobe/CrispASR/v0.8.42`; SHA-256 `2a6ba392a2d253c76a857e264e5564e7a47b4202a08a48e3863800fab1104664`, zip comment `3be8bcb4390b8080dc73f807b7abb34dc0aca9c3`). In the archive they sit under `examples/cli/`, `include/`, and `docs/`. The archive's `LICENSE` is MIT, which corrects the AGPL-3.0 this table used to show for CrispASR.

Study freely; when code is adapted into this repository, keep this in mind: CrisperWeaver is AGPL-3.0, and substantial adaptation of its code would make this repository AGPL-3.0 as well. CrispASR (MIT) and EUI-NEO (Apache-2.0) allow adaptation with their notices kept. Prefer reading for concepts and writing our own implementation in every case (which is also the intent — the audits extract patterns, not code to copy verbatim).

## Reading order

### Before writing any code (P0)

1. `audits/audit-reports/crispasr-audit.md` — the server contract, especially §3 (request parameters) and §5 (lifecycle). *This is the spec for our API client.*
2. `crispasr/crispasr_server.cpp` — the authoritative implementation behind that spec. Focus: request parsing (`form_` handling) and the transcription endpoint. 4000+ lines; use the audit's line references.
3. `crispasr/crispasr_output.cpp` — how each response format is generated; our parsers must match these field names exactly.
4. `crisperweaver/batch_queue_service.dart` — the batch queue blueprint: job model, states, grouping, crash recovery. Our `job_model`/`job_queue` are a C++ translation of this.
5. `crispasr/start-crispasr.ps1` — profile → server command line mapping (our ServerManager mirrors this logic).
6. `crispasr/profiles-japanese/*.conf` — the four tested profiles; the parameter panel's seed values.
7. `eui-neo/examples/eui_demo.cpp` + `eui-neo/examples/multipage_table.cpp` — the app skeleton and the closest UI shape (tabs + table).

### While implementing specific modules (P1)

- Job persistence: `crisperweaver/batch_persistence_service.dart`
- Batch loop behavior (snapshot timing, one model load): `crisperweaver/transcription_screen.dart` (search `_startBatchRun`)
- Parameter panel field list and mapping: `crisperweaver/transcription_service.dart` (AdvancedTranscribeOptions) and the mapping table in the audit report
- Async job model: `eui-neo/docs/异步.md` + `eui-neo/include/async.h`
- Network layer limits (why we need our own POST client): `eui-neo/docs/网络.md` + `eui-neo/core-platform/network.cpp`
- Queue table UI: `eui-neo/components/datatable.h`
- Server concurrency model: `crispasr/concurrency.md`

### When a specific problem appears (P2)

- Segment structure and backend capability flags: `crispasr/crispasr_backend.h`
- Diarization semantics and privacy boundaries (future step 2): `crispasr/diarization-speakers.md`
- Profile schema versioning strategy: `crisperweaver/preset_service.dart`
- Future in-process option (not v1): `crispasr/crispasr_session.h` (full C ABI header) + `audits/audit-reports/crispasr-audit.md` §7
- Keyboard handling / background-thread + UI update patterns: `eui-neo/examples/keyboard.cpp`, `eui-neo/examples/ffmpeg_video_player.cpp` (note: only in the upstream snapshot, not copied here)
- App config fields: `eui-neo/include/dsl_app.h`

## File map

```
crispasr/
├── crispasr_server.cpp          # HTTP server: all endpoints, request parsing (4000+ lines)
├── crispasr_output.cpp          # response format generation (srt/vtt/json/verbose_json)
├── crispasr_backend.h           # segment struct + backend capability flags
├── concurrency.md               # server concurrency model
├── diarization-speakers.md      # speaker labeling semantics (future step 2)
├── crispasr_session.h           # full C ABI header (future in-process path, not used in v1)
├── start-crispasr.ps1           # profile → server command line (proven reference)
└── profiles-japanese/           # 4 tested profiles (cohere/kotoba/parakeet/qwen3)

crisperweaver/
├── batch_queue_service.dart     # job model, state machine, grouping, recovery
├── batch_persistence_service.dart # per-job JSON + checkpoint persistence
├── transcription_service.dart   # AdvancedTranscribeOptions (parameter field list)
├── transcription_worker.dart    # "reset all sticky params per dispatch" pattern
├── transcription_screen.dart    # batch loop (_startBatchRun) — large file, see audit for line range
├── preset_service.dart          # schema versioning + tolerant deserialization
├── aligner_service.dart         # second-pass alignment pattern (absorbed by server in our design)
└── crispasr_engine.dart         # engine wrapper: model load order, failure cleanup (see lines 520–700 per audit)

eui-neo/
├── docs/集成指南.md              # official embedding guide (Chinese)
├── docs/组件.md                  # all components + state model (Chinese)
├── docs/异步.md                  # async task model (Chinese)
├── docs/网络.md                  # network layer limits (Chinese)
├── include/dsl_app.h            # app config fields
├── include/async.h              # async API
├── components/datatable.h       # table component
├── core-platform/network.h/.cpp # existing GET-only network layer (reference for our POST client)
└── examples/eui_demo.cpp, multipage_table.cpp, keyboard.cpp

audits/
└── audit-reports/               # the four audit reports (Chinese) — the foundation of this project's design
```

Note: some EUI-NEO documentation files keep their original Chinese filenames; their content is reference material and is not translated here. The `ffmpeg_video_player.cpp` example mentioned in the audit exists in the upstream snapshot but was not copied into this reference library.
