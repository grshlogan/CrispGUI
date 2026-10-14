# Open Questions

The single register of undecided questions. Each entry states the context, the options, A003's recommendation, and the status. Questions of requirements, scope, and authorization are the owner's; technical and planning questions within the approved SPEC are decided by A003 as planner since 2026-10-13 (round 13), and the owner can overrule them ([agents/README.md](agents/README.md#identities)). A decided entry moves to "Resolved" with a pointer to the document that now records the decision.

OQ-1 (Windows build path), OQ-2 (Git repository), OQ-3 (WSL2 toolchain), OQ-7 (local SRT generation), and OQ-9 (workflow and review policy) are resolved; see [Resolved](#resolved). The numbers are not reused.

## OQ-4: Source material needed

- Context: The parameter catalog needs engine data that the reference library does not contain.
- Items:
  1. The EUI-NEO v0.6.1 source: resolved; see [Resolved](#resolved).
  2. The engine's CLI flag names and backend capability names. The CLI flag names are resolved: Phase 2 extracted all 246 flags of the CrispASR 0.8.42 CLI from `examples/cli/cli.cpp` in the Learn library into `src/core/cli_flags_table.inc` (`tools/extract-cli-flags.py`). The capability names remain for Phase 6 (`crispasr_backend.h` declares them). On 2026-10-14 the owner provided the deployed build's `--help` text (`cli_help_full.txt`) and a server log that reports version 0.8.41 (git `340d7085`), in `/home/claude/projects/CrispGUI-handoff/scratch`; Phase 6 compares the 0.8.42 flag table with that help text. `--list-backends-json` remains useful for the capability names.
  3. Optional: one short Japanese sample with the SRT the server produces for it, for parser and SRT-writer tests without models. Since Phase 4 the audio itself is not needed in WSL2: `api_probe` records the server's `verbose_json` and `srt` answers for the same file (Windows check W4-3), and the stub server replays them.
- Status: item 1 resolved; item 2 resolved for the CLI flags, open for the capability names; item 3 resolved: the deployed server's answers to W4-3 are test data in `tests/data/deployed-parakeet/` ([P4-T5](agents/tasks/P4-T5-server-answers/packet.md)).

## OQ-5: Location of the model's authoritative parameter description

- Context: Owner invariant: one parameter set per model, rendered from that model's authoritative description ([ARCHITECTURE §2.7](ARCHITECTURE.md#27-parameter-model-pluggable)). Profiles are the per-model source of truth, but they are read-only to the GUI and owned by another workflow, and their comments (such as the "not applicable" lists) are not machine-readable.
- Options:
  - (a) Profile only: expose the parameters the profile sets (top-level keys and recognized `args[]` flags), filtered by engine capabilities. No extra files, but no way to expose a parameter the profile does not mention or to attach ranges and notes.
  - (b) An optional machine-readable block inside the profile (for example a `gui` object), written by the owner's profile workflow. One file per model; the launcher ignores unknown keys; needs the profile owner to adopt the convention.
  - (c) GUI-side model packs in this repository, keyed by profile folder and name (`params/models/<folder>/<name>.json`). Maintained here, but a second file per model has to stay in step with the profile.
- Recommendation: Build the resolver so that all three work, with precedence (b) over (c) over (a). Start with (a), add (c) where a model needs more, and adopt (b) if the profile workflow agrees.
- Status: open.

## OQ-6: Transcription window defaults

- Context: Owner direction: retry failed chunks rather than build elaborate resume. Each window is one request. The server decodes the whole uploaded file for every request and then trims it to the window ([API-CRISPASR §6](API-CRISPASR.md#6-lifecycle-and-resource-behavior)), so very short windows multiply decode and upload work.
- Proposal: window length 300 s, where 0 means the whole file in one request and files shorter than 1.5 windows also go in one request; 2 automatic retries per window with 2 s and 10 s backoff; boundary guard 1.0 s. All three are GUI-tier parameters stored in each job snapshot.
- Status: open; the owner may prefer shorter windows.

## OQ-8: Missing aligner at server start

- Context: When the aligner file is missing, the launcher warns and starts without it (`reference/crispasr/start-crispasr.ps1:191-199`). SPEC only asks for a readiness display. With `force_aligner`, the aligner also refines segment start and end times (`reference/crispasr/crispasr_server.cpp:838-851`), so SRT timing may degrade without it.
- Options: (a) behave like the launcher, with a visible warning; (b) refuse to start until the aligner exists.
- Recommendation: (a), mirroring the proven launcher, with the warning shown on the dashboard and stored in each job's log.
- Status: open. Phase 3 implements the recommendation: the server starts without `-am`, the dashboard and the server log show the warning, and job logs follow in Phase 7 ([ARCHITECTURE.md §2.2](ARCHITECTURE.md#22-servermanager)). Choosing (b) changes one check in `BuildLaunchCommand`.

## OQ-10: Queue behavior when the next job needs another profile

- Context: Each job records the profile it was enqueued with, and the server serves one model at a time.
- Options: (a) stop before that job and ask; (b) restart a GUI-managed server automatically with the job's profile, never an adopted one; (c) group queued jobs by profile, as CrisperWeaver does.
- Proposal: (a) by default, (b) as a setting, (c) later.
- Status: open.

## Resolved

Owner directions of 2026-10-11:

| Decision | Recorded in |
| --- | --- |
| Documents in English; communication with the owner in Chinese | [AGENTS.md](../AGENTS.md#language) |
| This Claude Code session is `A003[ClaudeCode]Dev Gui`, responsible for GUI development | [agents/README.md](agents/README.md#identities) |
| Retry failed transcription windows instead of building elaborate resume; resume at window granularity where it comes for free | [ARCHITECTURE.md §1.1](ARCHITECTURE.md#11-owner-invariants), [§2.5](ARCHITECTURE.md#25-steprunner-interface-and-the-asr-window-loop) |
| The API key has low importance; the local key is `default` | [SPEC.md FR-7](SPEC.md#fr-7-settings-gui-side), [API-CRISPASR.md §2](API-CRISPASR.md#2-authentication) |
| Avoid writes to `C:\`; anything unavoidable stays in the system TEMP directory | [AGENTS.md](../AGENTS.md#hard-rules) rule 4 |
| Pluggable parameter panel: one parameter set per model, driven by the model's authoritative description | [ARCHITECTURE.md §1.1](ARCHITECTURE.md#11-owner-invariants), [§2.7](ARCHITECTURE.md#27-parameter-model-pluggable) |
| No Windows drive mounting; deliveries reach Windows as owner-copied folders bound to a commit | [AGENTS.md](../AGENTS.md#environment-boundaries), [agents/README.md](agents/README.md#delivery-to-windows) |
| OQ-1: the Windows executable is cross-compiled in WSL2 with MinGW-w64; the owner approved and installed the toolchain | [ENVIRONMENT.md §1](ENVIRONMENT.md#1-target-and-toolchains) |
| OQ-2: a local Git repository with a private GitHub remote, `grshlogan/CrispGUI`, which the owner created; the first commit and push were authorized for round 3 | [agents/README.md](agents/README.md#checkouts) |
| OQ-4 item 1: EUI-NEO v0.6.1 comes from the shared Learn library | [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| OQ-3: the owner installed the WSL2 packages | [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) |

Owner directions of 2026-10-12 to 2026-10-14:

| Decision | Recorded in |
| --- | --- |
| A003 becomes `A003[ClaudeCode]DocArch Planner` (decisions, documentation, direction); `A004[ClaudeCode]Master Coding` does all coding; A001 and A002 are the owner's Windows-side agents | [agents/README.md](agents/README.md#identities) |
| OQ-9: coding on Sonnet 5.5 or Haiku 5.5 and Review on Sonnet 5.5 (owner, round 12); A003 decided a lightweight `agent-dev-loop` without coding subagents by default and with a fresh Review subagent per round; A004 merges reviewed work itself (owner's choice, round 13) | [agents/README.md](agents/README.md#workflow) |
| Reviews run as fresh top-level Claude Code sessions that the owner starts, as the `agent-dev-loop` Skill's Claude Code CLI profile has it, after subagent Reviews died on the five-hour limit (owner, 2026-10-14) | [agents/README.md](agents/README.md#workflow), [tasks/README.md](agents/tasks/README.md#review-dispatch) |

### OQ-7: Local SRT generation

Decided by A003 as planner on 2026-10-13 (round 20). The GUI requests `verbose_json` and writes the SRT itself. Without shaping, its output equals the server's `srt` for the same segments byte for byte, which the recorded pair in `tests/data/deployed-parakeet/` checks ([P5-T1](agents/tasks/P5-T1-srt-writer/packet.md)); subtitle shaping (characters per cue, splits at sentence ends) comes later as a GUI-tier option that works for every backend. Reasons: the server's `srt` ignores `max_len` and `split_on_punct` ([API-CRISPASR §4](API-CRISPASR.md#4-response-formats)), and merging transcription windows needs segment timings. Recorded in [ARCHITECTURE.md §2.8](ARCHITECTURE.md#28-srt-writer) and [SPEC.md FR-3](SPEC.md#fr-3-transcription-single-file).
