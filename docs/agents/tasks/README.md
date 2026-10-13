# Task Packets

Each code task of CrispGUI has a folder here, `docs/agents/tasks/<task-id>/`. The folder is the record of the task: what was asked, what was done, and what the independent Review found. The workflow that uses these files is in [agents/README.md](../README.md#workflow).

| File | Writer | Content |
| --- | --- | --- |
| `packet.md` | A003 | The task: problem, write scope, acceptance criteria, gates, Review profile, next event. A change of scope or acceptance becomes `packet-v2.md`; a published packet is never rewritten |
| `receipt.md` | A004 | Base, branch, changes, the mapping from each criterion to code and checks, every command with its exit code, the answers to Review findings, uncovered items, and the state |
| `review-R01.md`, `review-R02.md`, ... | The Review of that round | One report per round, written by the reviewer itself; never edited afterwards |

Task IDs are `P<phase>-T<n>`, for example `P4-T1`. Branches are `a004/<task-id>` in `<CODING_ROOT>`.

## Task index

Maintained by A003 when it consumes a receipt.

| Task | Title | State | Merged commit |
| --- | --- | --- | --- |
| [P4-T1](P4-T1-timeout-test/packet.md) | A timeout check that allows for WinHTTP's coarse timer | Consumed (round 16): R01 PASS; Windows AC-5 waits for the next delivery's W4-1 | `c483964` (reviewed as `46bb1ea`, same patch ID) |
| [P4-T2](P4-T2-dashboard-start/packet.md) | Start buttons on the dashboard's first screen | Consumed (round 17): R01 PASS; Windows check W4-2 of the next delivery | `5649b4f` |
| [P4-T4](P4-T4-rename/packet.md) | The project's names in code and deliveries become CrispGUI | Coding (round 17) | — |
| [P4-T5](P4-T5-server-answers/packet.md) | The deployed server's answers as test data | Waits for P4-T4 | — |
| [P4-T3](P4-T3-delivery/packet-v2.md) (v2) | The agent brief in deliveries, and the next Phase 4 delivery | Waits for P4-T1, P4-T2, P4-T4, and P4-T5 | — |

Order: P4-T1, P4-T2, P4-T4, P4-T5, then P4-T3.

States: `Ready for A004` → `Coding` → `Review Rxx` → `Merged` (A004) → `Consumed` (A003). `Blocked` names what is missing and who supplies it.

## Receipt

```markdown
# <task-id> receipt
- Identity, session, model: A004, <session label>, <model>
- Packet: packet.md (v1)
- Base: origin/main <sha> on <UTC date>; branch a004/<task-id>
- State: Coding | Review R0n | Merged | Blocked: <reason>
- Candidate commits reviewed: R01 <sha>, R02 <sha>, ...

## Changes
| Path | Why | Criterion |

## Acceptance mapping
| Criterion | Code (symbol or file:line) | Check that fails without the change | Evidence |

## Commands
| Where (alias) | Command | Exit code | Result |

## Review findings and answers
| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |

## Uncovered, skipped, and decisions requested
```

Commands run under `set -o pipefail` and record the real exit code. A check that did not run is `not run`, with the reason.

## Review dispatch

A004 starts one fresh subagent per round: a general-purpose agent on Sonnet 5.5, never a fork of A004's conversation. It receives the prompt below with the placeholders filled and nothing else from the coding conversation. A new round (`R02`, ...) gets a new subagent and the previous report.

```text
You are the independent Review <Rxx> for task <TASK_ID> of CrispGUI, a C++17 Windows desktop client built in WSL2. You did not write this code, and you never change it.

Inputs:
- Packet: <CODING_ROOT>/docs/agents/tasks/<TASK_DIR>/<PACKET_FILE> (packet.md, or the newest packet-vN.md when one exists). It defines the problem, the write scope, the acceptance criteria, the gates, and the Review profile. Read it first.
- Candidate: commit <SHA> in the repository at <CODING_ROOT>; base <BASE_SHA>. Coding receipt (leads to check, not conclusions): <CODING_ROOT>/docs/agents/tasks/<TASK_DIR>/receipt.md.
- Previous report: <PREVIOUS_REPORT or "none">.
- Rules: AGENTS.md, docs/CONVENTIONS.md, and the packet's "Read first" list, all as they are in the candidate.

Method:
1. Export the candidate into a new empty directory in your scratchpad: git -C <CODING_ROOT> archive <SHA> | tar -x -C <DIR>. Build, test, and experiment only there; never in <CODING_ROOT>.
2. Run the packet's gates in <DIR> with set -o pipefail, adding -DCRISPGUI_EUI_NEO_DIR=/home/claude/projects/Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1 and -DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts to each cmake configure. Record every command and its real exit code.
3. Read git -C <CODING_ROOT> diff <BASE_SHA>..<SHA> and each changed file in full, with the callers it affects. For each acceptance criterion, find the code that meets it and a check that would fail without it; prove that with negative evidence in <DIR> (the check failing on the base, or with the change reverted or mutated), then restore and rerun.
4. Check the write scope (only paths the packet allows), the hard rules of AGENTS.md, the conventions, error paths, resource cleanup, and thread safety where the change touches them. A finding needs evidence from the candidate: a command, a reproduction, or file:line.
5. Profile strict: also run the packet's mutation matrix, one mutation at a time, restoring each.

Verdict: PASS only when every criterion is met with evidence and no P0 or P1 finding remains. FAIL lists the findings. BLOCKED when a gate cannot run. UNCERTAIN when the packet is ambiguous or contradicts the rules.

Output: write your report to <CODING_ROOT>/docs/agents/tasks/<TASK_DIR>/review-<Rxx>.md in the format of the "Review report" section of docs/agents/tasks/README.md in the candidate. That file is the only thing you may write outside <DIR>. Then reply with the verdict and the report path only.
```

## Review report

```markdown
# <task-id> Review <Rxx>
- Reviewer: fresh subagent, <model>, dispatched by A004
- Packet: packet.md (v<n>); candidate <sha>; base <sha>; previous report: <path or none>
- Verdict: PASS | FAIL | BLOCKED | UNCERTAIN

## Gates rerun
| Command | Exit code | Result |

## Acceptance
| Criterion | Code | Check and negative evidence | Met |

## Findings
| ID | Level (P0–P3) | Where | Reproduction, expected, actual | Repair direction |

## Not checked, and why
```

Levels: P0 data loss or a security hole; P1 a criterion not met or a core behavior broken; P2 a local defect; P3 a suggestion. P0 and P1 block a PASS.
