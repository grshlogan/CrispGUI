# Current Status

Updated 2026-10-14 (round 26) by A003 (`A003[ClaudeCode]DocArch Planner`). The owner (the user) is the acceptance authority.

## Baseline

The repository holds the specification, the architecture plan, the server contract, the phased plan, the agent entry points, a read-only reference library, the MinGW-w64 toolchain file, the Phase 0 smoke scripts, the Phase 1 application skeleton, the Phase 2 profile reading, the Phase 3 server management, and the Phase 4 ApiClient. Phases 0 (build environment), 1 (application skeleton), 2 (ProfileStore), and 3 (ServerManager) are complete; Phase 4 (ApiClient) is complete (accepted on 2026-10-14), and Phase 5 (single-file transcription) is in progress.

## Present facts

| Area | State | Evidence or detail |
| --- | --- | --- |
| Specification and design | SPEC approved; revised on 2026-10-11 with the owner's directions and source-review corrections. The parameter model and window loop details are proposed | [SPEC.md](SPEC.md), [ARCHITECTURE.md](ARCHITECTURE.md), [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) |
| Server contract | Checked against the v0.8.42 server source, which is byte-identical to the same file in the CrispASR v0.8.42 snapshot in the Learn library; the deployed build reports 0.8.41 and answered the ten probe checks of W4-3 as the contract describes | [API-CRISPASR.md](API-CRISPASR.md), [reference/README.md](../reference/README.md) |
| Implementation | Phase 1 skeleton accepted on 2026-10-11. Phase 2 accepted on 2026-10-12: JSON and JSONC parsing, SHA-256, the CrispASR 0.8.42 CLI flag table, profile scanning and readiness, the launcher's tuning conditions, and a dashboard that lists the profiles. Phase 3 accepted on 2026-10-12: the launcher's command line and environment, process start and stop, `/health` polling, pre-flight, adoption, the stop of adopted servers, the server panel, and the log pane. Windows (owner, 2026-10-12): the GUI works, and the corrected Phase 3 delivery passes 14 of 14 tests. Phase 4 implemented on 2026-10-12: the streamed multipart upload over WinHTTP and libcurl, typed errors, `verbose_json` parsing, a JSON writer, and `api_probe`, which checks a running server and keeps its answers. Linux: clean builds without warnings, 20 of 20 tests pass (15 unit, 5 integration against a stand-in server). Phase 4 accepted on 2026-10-14. Phase 5 so far: the SRT writer, atomic writes, the transcription controller, and the Transcribe page (P5-T1 to P5-T4; 17 unit and 6 integration tests) | [Phase 1 report](devlog/phase-1.md), [Phase 2 report](devlog/phase-2.md), [Phase 3 report](devlog/phase-3.md), [Phase 4 report](devlog/phase-4.md) |
| Phase 0 | Accepted on 2026-10-11. The EUI-NEO smoke builds from commit `7e4e89e` pass: under WSLg, 26 of 26 unit tests and 7 of 8 probes (`frame_presentation_probe` fails only under WSLg); on the owner's Windows 11 machine, 0 failures of 34 and no visual problems | [Phase 0 report](devlog/phase-0.md) |
| Repository | Git, branch `main`; the remote `origin` is the owner's private GitHub repository `grshlogan/CrispGUI` | [agents/README.md](agents/README.md#checkouts) |
| Profiles | The four Japanese reference profiles parse with the expected fields and launcher arguments in the project's own reader; on the owner's machine the dashboard lists the deployed profiles under `E:\AI\ASR\CrispASR\profile` with the launcher's readiness (`observed` by the owner, 2026-10-12) | `profile_store_test`, `launch_command_test` (`ctest`, exit 0, 2026-10-12); [Phase 2 report](devlog/phase-2.md#windows-results-owners-machine) |
| UI fonts | HarmonyOS Sans SC, provided by the owner and kept out of Git; license `not verified` | [ENVIRONMENT.md §4.3](ENVIRONMENT.md#43-ui-fonts) |
| WSL2 toolchain | Complete: MinGW-w64 GCC 12 (POSIX threads), GCC 12, CMake 3.25.1, Ninja 1.11.1, and EUI-NEO's Linux development packages | [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) |
| EUI-NEO source | v0.6.1 in the shared Learn library, verified identical to its tag archive; not copied into this repository | [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| Windows side | Runtime and acceptance only. The five deliveries up to Phase 3 are accepted; their GUI check did not start a server (correction of round 11). From Phase 4 each delivery carries `CHECKLIST.md`, numbered checks with what to send back, run by the owner or the Windows-side agent. Phase 4 accepted: `crisprustgui-c296f23` passed W4-2 to W4-4 and `crispgui-621df7d` passed W4-1 with 0 failures of 20 (2026-10-14). The WinHTTP diagnostic is complete ([ENVIRONMENT.md §4.6](ENVIRONMENT.md#46-http-transport-facts)). `crispgui-47054b9` (the Transcribe page) is with the owner for an informal test; the Phase 5 delivery with W5-1 to W5-7 follows P5-T5 |
| Identities | A003 plans and writes the documents (`A003[ClaudeCode]DocArch Planner`); A004 does all coding (`A004[ClaudeCode]Master Coding`, in `<CODING_ROOT>`, started on 2026-10-13; all five Phase 4 packets merged; Phase 5: P5-T1 to P5-T4 merged, P5-T5 next); A005 runs the Windows checks (started the same day); A001 and A002 are the owner's Windows-side agents outside this project | [agents/README.md](agents/README.md#identities) |
| Workflow | A lightweight `agent-dev-loop` (round 13): A003 writes task packets, A004 codes, a fresh Sonnet 5.5 session reviews each candidate commit (a top-level session the owner starts since 2026-10-14), and A004 merges after a PASS | [agents/README.md](agents/README.md#workflow), [task packets](agents/tasks/README.md) |

## Blockers

None. Phase 4 was accepted on 2026-10-14 when `crispgui-621df7d` passed W4-1 with 0 failures of 20. OQ-7 (local SRT generation) is decided (round 20); OQ-8 (missing aligner at start) is implemented as its recommendation, which mirrors the launcher; OQ-4 item 2 (capability names), OQ-5, OQ-6, and OQ-10 affect later phases.

## Verification boundary

Evidence comes from reading the documents and sources, environment probes in WSL2, the Phase 0 to 4 builds and tests in WSL2, and the owner's Phase 0 to 4 runs on Windows. The GUI has read the deployed profiles on the owner's machine; the Windows code paths of Phase 3 (process, pipe, WinHTTP GET, port owner) pass the integration tests there; the owner reports that the GUI started the deployed server (round 12) and stopped it (W4-4, round 15). The Phase 4 upload passes against the stand-in server on Windows; WinHTTP applies the transport's limits on a coarse timer, and long answers arrive within them (diagnostic, to 95 s); the deployed server answered the ApiClient's requests as the contract describes (W4-3: ten checks, parakeet profile).

## Next event

A004 takes [P5-T5](agents/tasks/P5-T5-kept-srt/packet.md) (a kept SRT is never overwritten or dropped by accident) and then builds the Phase 5 delivery in [P5-T6](agents/tasks/P5-T6-delivery/packet.md), whose `CHECKLIST.md` holds W5-1 to W5-7. A005 runs those checks with the owner, with the prompt A003 gave in round 26.
