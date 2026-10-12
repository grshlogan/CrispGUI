# Current Status

Updated 2026-10-12 (round 10) by A003. The owner (the user) is the acceptance authority.

## Baseline

The repository holds the specification, the architecture plan, the server contract, the phased plan, the agent entry points, a read-only reference library, the MinGW-w64 toolchain file, the Phase 0 smoke scripts, the Phase 1 application skeleton, the Phase 2 profile reading, the Phase 3 server management, and the Phase 4 ApiClient. Phases 0 (build environment), 1 (application skeleton), 2 (ProfileStore), and 3 (ServerManager) are complete; Phase 4 (ApiClient) is implemented and waits for its Windows checks.

## Present facts

| Area | State | Evidence or detail |
| --- | --- | --- |
| Specification and design | SPEC approved; revised on 2026-10-11 with the owner's directions and source-review corrections. The parameter model and window loop details are proposed | [SPEC.md](SPEC.md), [ARCHITECTURE.md](ARCHITECTURE.md), [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) |
| Server contract | Checked against the v0.8.42 server source, which is byte-identical to the same file in the CrispASR v0.8.42 snapshot in the Learn library; deployed-build behavior `not verified` | [API-CRISPASR.md](API-CRISPASR.md), [reference/README.md](../reference/README.md) |
| Implementation | Phase 1 skeleton accepted on 2026-10-11. Phase 2 accepted on 2026-10-12: JSON and JSONC parsing, SHA-256, the CrispASR 0.8.42 CLI flag table, profile scanning and readiness, the launcher's tuning conditions, and a dashboard that lists the profiles. Phase 3 accepted on 2026-10-12: the launcher's command line and environment, process start and stop, `/health` polling, pre-flight, adoption, the stop of adopted servers, the server panel, and the log pane. Windows (owner, 2026-10-12): the GUI works, and the corrected Phase 3 delivery passes 14 of 14 tests. Phase 4 implemented on 2026-10-12: the streamed multipart upload over WinHTTP and libcurl, typed errors, `verbose_json` parsing, a JSON writer, and `api_probe`, which checks a running server and keeps its answers. Linux: clean builds without warnings, 20 of 20 tests pass (15 unit, 5 integration against a stand-in server) | [Phase 1 report](devlog/phase-1.md), [Phase 2 report](devlog/phase-2.md), [Phase 3 report](devlog/phase-3.md), [Phase 4 report](devlog/phase-4.md) |
| Phase 0 | Accepted on 2026-10-11. The EUI-NEO smoke builds from commit `7e4e89e` pass: under WSLg, 26 of 26 unit tests and 7 of 8 probes (`frame_presentation_probe` fails only under WSLg); on the owner's Windows 11 machine, 0 failures of 34 and no visual problems | [Phase 0 report](devlog/phase-0.md) |
| Repository | Git, branch `main`; the remote `origin` is the owner's private GitHub repository `grshlogan/CrispGUI` | [agents/README.md](agents/README.md#checkouts) |
| Profiles | The four Japanese reference profiles parse with the expected fields and launcher arguments in the project's own reader; on the owner's machine the dashboard lists the deployed profiles under `E:\AI\ASR\CrispASR\profile` with the launcher's readiness (`observed` by the owner, 2026-10-12) | `profile_store_test`, `launch_command_test` (`ctest`, exit 0, 2026-10-12); [Phase 2 report](devlog/phase-2.md#windows-results-owners-machine) |
| UI fonts | HarmonyOS Sans SC, provided by the owner and kept out of Git; license `not verified` | [ENVIRONMENT.md §4.3](ENVIRONMENT.md#43-ui-fonts) |
| WSL2 toolchain | Complete: MinGW-w64 GCC 12 (POSIX threads), GCC 12, CMake 3.25.1, Ninja 1.11.1, and EUI-NEO's Linux development packages | [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) |
| EUI-NEO source | v0.6.1 in the shared Learn library, verified identical to its tag archive; not copied into this repository | [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| Windows side | Runtime and acceptance only. The five deliveries up to Phase 3 are accepted. From Phase 4 each delivery carries `CHECKLIST.md`, numbered checks with what to send back, run by the owner or the Windows-side agent | [agents/README.md](agents/README.md#delivery-to-windows) |
| Identities | A003 established; A002 authored the audit reports; the Windows-side agent is registered, its stable ID and display name to be assigned by the owner | [agents/README.md](agents/README.md) |
| Workflow | None selected | [OQ-9](OPEN-QUESTIONS.md#oq-9-formal-workflow-and-review-policy) |

## Blockers

None. Phase 4's acceptance needs the owner's Windows run of its delivery. OQ-7 (local SRT generation) is the decision Phase 5 needs; OQ-8 (missing aligner at start) is implemented as its recommendation, which mirrors the launcher, and stays open for the owner; OQ-4 item 2 (capability names), OQ-5, OQ-6, OQ-9, and OQ-10 affect later phases.

## Verification boundary

Evidence comes from reading the documents and sources, environment probes in WSL2, the Phase 0 to 4 builds and tests in WSL2, and the owner's Phase 0 to 3 runs on Windows. The GUI has read the deployed profiles on the owner's machine; the Windows code paths of Phase 3 (process, pipe, WinHTTP GET, port owner) pass the integration tests there; the owner reports that the GUI works normally with the deployed server, without itemized results. The Phase 4 upload has run only against the stand-in server on Linux; its WinHTTP path and the deployed server's answers are `not verified` until the checks W4-1 to W4-4.

## Next event

The owner, or the Windows-side agent, runs `CHECKLIST.md` of the Phase 4 delivery and sends back `test-results.txt` and the `probe-results` folder; A003 then records the results, turns the recorded answers into test data, and closes Phase 4. The owner assigns the Windows-side agent's ID and display name.
