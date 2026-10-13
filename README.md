# CrispGUI

A desktop GUI for driving the [CrispASR](https://github.com/CrispStrobe/CrispASR) speech engine, built with C++17 and the EUI-NEO UI framework.

> **Status: Phases 0 (build environment), 1 (application skeleton), 2 (ProfileStore), and 3 (ServerManager) complete; Phase 4 (ApiClient) implemented; its Windows checks W4-2 and W4-3 passed, and its last corrections are under way.** This repository contains the specification, architecture plan, server contract, reference material, and the application as far as the completed phases go. A003 (Claude Code in WSL2) plans the work and writes the documents; A004 (Claude Code in WSL2) implements it from task packets, following `docs/TASKS.md` and the workflow in `docs/agents/README.md`; `docs/STATUS.md` records the current state.

> The project was called CrispRustGUI until 2026-10-13. It contains no Rust code and none is planned: the code is C++17, built with CMake, with helper scripts in Bash, Python, PowerShell, and Windows batch. Deliveries, files, and records made before the rename keep the old name.

## What this is

CrispGUI is a **client for a CrispASR HTTP server** (the engine listens on `127.0.0.1:5114` in our deployment). The GUI does not embed the engine; it manages the server process and talks to it over HTTP.

Version 1 covers the most valuable slice of a larger pipeline:

- **Server management** — start the CrispASR server with a chosen profile, stop it, watch its health and status, and adopt a server that is already running.
- **Profile-driven model selection** — the existing JSONC profile files (under `CrispASR/profile/<folder>/*.conf`) remain the single source of truth. Switching model = restarting the server with another profile.
- **Transcription to SRT** — send audio/video files to `/v1/audio/transcriptions` and write finished `.srt` files.
- **Per-model parameter panel** — each model has its own parameter set; the panel is generated from the model's authoritative description instead of a fixed page. Request-level parameters apply per job; startup-level parameters are marked as requiring a server restart.
- **Batch processing with a queue** — enqueue many files with per-job parameter snapshots; each job runs as transcription windows, a failed window is retried, and an interrupted job continues from its first incomplete window after a restart.
- **A foundation for a 6-step media pipeline** (separation → speaker ID → ASR → events → translation → merge): the job data model is step-chain-shaped from day one, but v1 implements only the ASR step.

## Repository map

| Path | Contents |
|---|---|
| `AGENTS.md` | Rules for agents: scope, language, hard rules, identities, environment boundaries |
| `docs/START_HERE.md` | Reading order and project map — **start here** |
| `docs/STATUS.md` | Present state, blockers, next event |
| `docs/index.md` | Index of all documents and their responsibility |
| `docs/SPEC.md` | v1 functional specification (requirements, scope, acceptance) |
| `docs/ARCHITECTURE.md` | System design, owner invariants, components, threading, key decisions |
| `docs/API-CRISPASR.md` | The CrispASR server HTTP API (endpoints, parameters, responses, errors, lifecycle) |
| `docs/WORKFLOW.md` | The 6-step pipeline and what v1 does / does not cover |
| `docs/TASKS.md` | Phased implementation plan with acceptance criteria |
| `docs/OPEN-QUESTIONS.md` | Undecided questions waiting for the owner |
| `docs/CONVENTIONS.md` | Coding conventions |
| `docs/ENVIRONMENT.md` | Build environment (WSL2 development, Windows target, dependencies) |
| `docs/agents/README.md` | Identities, delivery to Windows, authorizations |
| `docs/identity/` | One context file per agent identity |
| `CMakeLists.txt`, `src/` | The application (skeleton, profile reading, server management, and the API client so far) and its libraries |
| `tests/` | Unit tests, integration tests that run a stand-in CrispASR server (`tests/stub_crispasr.cpp`), and server answers as test data (`tests/data/`) |
| `cmake/`, `tools/` | The MinGW-w64 toolchain file, smoke builds, Windows packaging, what Windows deliveries carry (test runner, `CHECKLIST.md`), `api_probe` (checks a running CrispASR server), and the CrispASR CLI flag extractor |
| `fonts/` | The UI fonts (HarmonyOS Sans SC), provided by the owner and not in Git (`docs/ENVIRONMENT.md` §4.3) |
| `reference/` | Curated source files from CrispASR / CrisperWeaver / EUI-NEO to learn from, plus the audit reports they came from |

## The three source projects behind this design

1. **CrispASR** (C++ engine, MIT) — provides the HTTP API we drive. Audit: `reference/audits/audit-reports/crispasr-audit.md`.
2. **CrisperWeaver** (Flutter GUI, AGPL-3.0) — the official reference GUI; we replicate its *architecture patterns* (batch queue, parameter snapshots) as an HTTP client. Audit: `.../crisperweaver-audit.md`.
3. **EUI-NEO** (C++17 UI framework, Apache-2.0) — the UI toolkit we build on. Audit: `.../eui-neo-audit.md`.

All three audits were produced by a separate review pass and live in `reference/audits/audit-reports/` (Chinese). The English operating summaries are the `docs/*.md` files above; `docs/API-CRISPASR.md` §10 lists corrections to the audits.

## Rules

The non-negotiable rules for anyone changing this repository are the hard rules in `AGENTS.md`.

## Environment summary

- Target: Windows 10/11 x64, C++17, CMake ≥ 3.25. The Windows executable is cross-compiled in WSL2 with MinGW-w64 GCC 12 (`docs/ENVIRONMENT.md`).
- Development happens only in WSL2, with Linux development builds; Windows-native behavior is accepted on the owner's machine using a build folder bound to an exact commit (`docs/agents/README.md`).
- Repository: Git on `main`; the remote `origin` is the owner's private GitHub repository `grshlogan/CrispGUI`.
- UI framework: EUI-NEO v0.6.1, built through `add_subdirectory` from the pinned snapshot in the shared Learn library; it is not copied into this repository. UI font: HarmonyOS Sans SC from the owner's local `fonts/` folder.
- HTTP: a thin in-repo transport (WinHTTP on Windows, libcurl on Linux).
- The CrispASR server endpoint and profile paths are configured, not hardcoded; defaults match the existing deployment.
