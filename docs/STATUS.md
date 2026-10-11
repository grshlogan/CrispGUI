# Current Status

Updated 2026-10-11 (round 6) by A003. The owner (the user) is the acceptance authority.

## Baseline

The repository holds the specification, the architecture plan, the server contract, the phased plan, the agent entry points, a read-only reference library, the MinGW-w64 toolchain file, the Phase 0 smoke scripts, and the Phase 1 application skeleton. Phases 0 (build environment) and 1 (application skeleton) are complete; Phase 2 (ProfileStore) is in progress.

## Present facts

| Area | State | Evidence or detail |
| --- | --- | --- |
| Specification and design | SPEC approved; revised on 2026-10-11 with the owner's directions and source-review corrections. The parameter model and window loop details are proposed | [SPEC.md](SPEC.md), [ARCHITECTURE.md](ARCHITECTURE.md), [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) |
| Server contract | Checked against the v0.8.42 server source, which is byte-identical to the same file in the CrispASR v0.8.42 snapshot in the Learn library; deployed-build behavior `not verified` | [API-CRISPASR.md](API-CRISPASR.md), [reference/README.md](../reference/README.md) |
| Implementation | Phase 1 skeleton: four tabs, the string table, and the app data folder with `TMP`/`TEMP`; accepted on 2026-10-11 after the owner's Windows run of `crisprustgui-1817b8e.zip`. Phase 2 in progress | [Phase 1 report](devlog/phase-1.md), [Phase 2 report](devlog/phase-2.md) |
| Phase 0 | Accepted on 2026-10-11. The EUI-NEO smoke builds from commit `7e4e89e` pass: under WSLg, 26 of 26 unit tests and 7 of 8 probes (`frame_presentation_probe` fails only under WSLg); on the owner's Windows 11 machine, 0 failures of 34 and no visual problems | [Phase 0 report](devlog/phase-0.md) |
| Repository | Git, branch `main`; the remote `origin` is the owner's private GitHub repository `grshlogan/CrispGUI` | [agents/README.md](agents/README.md#checkouts) |
| Profiles | The four Japanese reference profiles parse as JSON after string-aware comment stripping | Python check in WSL2, exit 0 (2026-10-11) |
| WSL2 toolchain | Complete: MinGW-w64 GCC 12 (POSIX threads), GCC 12, CMake 3.25.1, Ninja 1.11.1, and EUI-NEO's Linux development packages | [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) |
| EUI-NEO source | v0.6.1 in the shared Learn library, verified identical to its tag archive; not copied into this repository | [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| Windows side | Runtime and acceptance only. Both deliveries so far, `crisprustgui-eui-smoke-7e4e89e.zip` and `crisprustgui-1817b8e.zip`, passed | [agents/README.md](agents/README.md#delivery-to-windows) |
| Identities | A003 established; A002 authored the audit reports | [agents/README.md](agents/README.md) |
| Workflow | None selected | [OQ-9](OPEN-QUESTIONS.md#oq-9-formal-workflow-and-review-policy) |

## Blockers

None for Phase 2. OQ-5 to OQ-10 affect later phases.

## Verification boundary

Evidence comes from reading the documents and sources, environment probes in WSL2, the profile parse check, the Phase 0 smoke builds and tests in WSL2, and the owner's Phase 0 and Phase 1 runs on Windows. The real server and the GUI's handling of the real CrispASR deployment are `not run`.

## Next event

A003 completes Phase 2 (ProfileStore) and sends a Windows delivery for the owner's check against the real profiles.
