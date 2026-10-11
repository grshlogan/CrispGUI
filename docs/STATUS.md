# Current Status

Updated 2026-10-11 (round 4) by A003. The owner (the user) is the acceptance authority.

## Baseline

The repository holds the specification, the architecture plan, the server contract, the phased plan, the agent entry points, a read-only reference library, the MinGW-w64 toolchain file, and the Phase 0 smoke scripts. It has no application source yet. Phase 0 (build environment) is in progress.

## Present facts

| Area | State | Evidence or detail |
| --- | --- | --- |
| Specification and design | SPEC approved; revised on 2026-10-11 with the owner's directions and source-review corrections. The parameter model and window loop details are proposed | [SPEC.md](SPEC.md), [ARCHITECTURE.md](ARCHITECTURE.md), [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) |
| Server contract | Checked against the v0.8.42 server source, which is byte-identical to the same file in the CrispASR v0.8.42 snapshot in the Learn library; deployed-build behavior `not verified` | [API-CRISPASR.md](API-CRISPASR.md), [reference/README.md](../reference/README.md) |
| Implementation | Not started; no `src/` | Directory listing |
| Phase 0 | EUI-NEO smoke builds pass for Linux and MinGW. Under WSLg, `eui_demo` renders, 26 of 26 unit tests pass, and 7 of 8 probes pass (`frame_presentation_probe` fails, probably because of WSLg's window manager). The Windows run of the first delivery is pending | [Phase 0 report](devlog/phase-0.md) |
| Repository | Git, branch `main`; the remote `origin` is the owner's private GitHub repository `grshlogan/CrispGUI` | [agents/README.md](agents/README.md#checkouts) |
| Profiles | The four Japanese reference profiles parse as JSON after string-aware comment stripping | Python check in WSL2, exit 0 (2026-10-11) |
| WSL2 toolchain | Complete: MinGW-w64 GCC 12 (POSIX threads), GCC 12, CMake 3.25.1, Ninja 1.11.1, and EUI-NEO's Linux development packages | [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) |
| EUI-NEO source | v0.6.1 in the shared Learn library, verified identical to its tag archive; not copied into this repository | [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| Windows side | Runtime and acceptance only; the first delivery is prepared for the owner | [agents/README.md](agents/README.md#delivery-to-windows) |
| Identities | A003 established; A002 authored the audit reports | [agents/README.md](agents/README.md) |
| Workflow | None selected | [OQ-9](OPEN-QUESTIONS.md#oq-9-formal-workflow-and-review-policy) |

## Blockers

Phase 0 acceptance waits for the owner's Windows run of the first delivery. OQ-4 item 2 and OQ-5 to OQ-10 affect later phases.

## Verification boundary

Evidence comes from reading the documents and sources, environment probes in WSL2, the profile parse check, and the Phase 0 smoke builds and tests in WSL2. Building this project (Phase 1), the real server, and every Windows-native behavior are `not run`.

## Next event

The owner runs the first delivery on Windows and reports the results; A003 then closes Phase 0 and starts Phase 1.
