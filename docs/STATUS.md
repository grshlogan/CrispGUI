# Current Status

Updated 2026-10-12 (round 7) by A003. The owner (the user) is the acceptance authority.

## Baseline

The repository holds the specification, the architecture plan, the server contract, the phased plan, the agent entry points, a read-only reference library, the MinGW-w64 toolchain file, the Phase 0 smoke scripts, the Phase 1 application skeleton, and the Phase 2 profile reading. Phases 0 (build environment), 1 (application skeleton), and 2 (ProfileStore) are complete; Phase 3 (ServerManager) is in progress.

## Present facts

| Area | State | Evidence or detail |
| --- | --- | --- |
| Specification and design | SPEC approved; revised on 2026-10-11 with the owner's directions and source-review corrections. The parameter model and window loop details are proposed | [SPEC.md](SPEC.md), [ARCHITECTURE.md](ARCHITECTURE.md), [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) |
| Server contract | Checked against the v0.8.42 server source, which is byte-identical to the same file in the CrispASR v0.8.42 snapshot in the Learn library; deployed-build behavior `not verified` | [API-CRISPASR.md](API-CRISPASR.md), [reference/README.md](../reference/README.md) |
| Implementation | Phase 1 skeleton accepted on 2026-10-11. Phase 2 accepted on 2026-10-12: JSON and JSONC parsing, SHA-256, the CrispASR 0.8.42 CLI flag table, profile scanning and readiness, the launcher's tuning conditions, and a dashboard that lists the profiles. Phase 3 in progress | [Phase 1 report](devlog/phase-1.md), [Phase 2 report](devlog/phase-2.md) |
| Phase 0 | Accepted on 2026-10-11. The EUI-NEO smoke builds from commit `7e4e89e` pass: under WSLg, 26 of 26 unit tests and 7 of 8 probes (`frame_presentation_probe` fails only under WSLg); on the owner's Windows 11 machine, 0 failures of 34 and no visual problems | [Phase 0 report](devlog/phase-0.md) |
| Repository | Git, branch `main`; the remote `origin` is the owner's private GitHub repository `grshlogan/CrispGUI` | [agents/README.md](agents/README.md#checkouts) |
| Profiles | The four Japanese reference profiles parse with the expected fields and launcher arguments in the project's own reader; on the owner's machine the dashboard lists the deployed profiles under `E:\AI\ASR\CrispASR\profile` with the launcher's readiness (`observed` by the owner, 2026-10-12) | `profile_store_test`, `launch_command_test` (`ctest`, exit 0, 2026-10-12); [Phase 2 report](devlog/phase-2.md#windows-results-owners-machine) |
| UI fonts | HarmonyOS Sans SC, provided by the owner and kept out of Git; license `not verified` | [ENVIRONMENT.md §4.3](ENVIRONMENT.md#43-ui-fonts) |
| WSL2 toolchain | Complete: MinGW-w64 GCC 12 (POSIX threads), GCC 12, CMake 3.25.1, Ninja 1.11.1, and EUI-NEO's Linux development packages | [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) |
| EUI-NEO source | v0.6.1 in the shared Learn library, verified identical to its tag archive; not copied into this repository | [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| Windows side | Runtime and acceptance only. All three deliveries so far, `crisprustgui-eui-smoke-7e4e89e.zip`, `crisprustgui-1817b8e.zip`, and `crisprustgui-b397dcd.zip`, passed | [agents/README.md](agents/README.md#delivery-to-windows) |
| Identities | A003 established; A002 authored the audit reports | [agents/README.md](agents/README.md) |
| Workflow | None selected | [OQ-9](OPEN-QUESTIONS.md#oq-9-formal-workflow-and-review-policy) |

## Blockers

None for Phase 3. OQ-8 (missing aligner at start) affects Phase 3 and has a recommendation that mirrors the launcher; OQ-4 item 2 (capability names), OQ-5 to OQ-7, OQ-9, and OQ-10 affect later phases.

## Verification boundary

Evidence comes from reading the documents and sources, environment probes in WSL2, the Phase 0 to 2 builds and tests in WSL2, and the owner's Phase 0 to 2 runs on Windows. The GUI has read the deployed profiles on the owner's machine; the real server under the GUI's control is `not run`.

## Next event

A003 builds Phase 3 (ServerManager) and sends a delivery; the owner starts and stops the server from the Japanese cohere profile with it.
