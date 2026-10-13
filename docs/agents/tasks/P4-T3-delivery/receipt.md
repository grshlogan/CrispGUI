# P4-T3 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet-v2.md (v2; v1 was replaced before any work started)
- Base: origin/main `813318f` (P4-T1, P4-T2, P4-T4, and P4-T5 pushed at `c727a30`; `813318f` is documentation and `.gitattributes` only) on 2026-10-13 (UTC); branch `a004/P4-T3`. The candidate was written on `c727a30` and rebased onto `813318f` before the Review, which touched none of its files
- State: the scripts are merged (pushed to `origin/main` as a fast-forward after a PASS); the delivery is built from the merged commit and recorded in the section "Delivery"
- Candidate commits reviewed: R01 `bd925e81cb74f6397763f58001f1175264a44e21` (PASS, [review-R01.md](review-R01.md)). The candidate was first committed as `c2b560e7c038a11b25d6b343ee6348343077bece` on `c727a30` and rebased onto `813318f` before the Review, with the same patch (`git range-diff` shows `=`; both have `git patch-id --stable` `931706dc52308f0ff61447e04a3f728577628931`). `origin/main` had not moved past `813318f` when the branch was pushed, so the pushed commits contain the reviewed candidate itself. The commits that add the report and this receipt follow it; the pushed tip cannot be named here, and the notice to A003 and `git log origin/main` carry it

The task has two parts: the change to the packaging scripts, which the Review checks, and the delivery, which is built from the merged commit and recorded in the section "Delivery" after the merge.

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `tools/package-windows.sh` | `WINDOWS-AGENT.md` joins the files that the delivery copies from `tools/windows/` with CRLF line ends, in the loop that already does this for `CHECKLIST.md`; the comment names it | AC-1 |
| `tools/package-diagnostic.sh` | The diagnostic delivery copies `WINDOWS-AGENT.md` with CRLF line ends, next to the executable | AC-1 |
| `docs/agents/tasks/P4-T3-delivery/receipt.md` | This receipt | — |

`git diff --stat` lists the two scripts (5 insertions, 2 deletions) and nothing else outside this task folder.

## Acceptance mapping

`<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`.

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: deliveries made by either script contain `WINDOWS-AGENT.md` with CRLF endings | `tools/package-windows.sh` (the loop over `run-tests.cmd run-tests.ps1 run-api-probe.cmd CHECKLIST.md WINDOWS-AGENT.md`), `tools/package-diagnostic.sh` (the `sed` line after the executable's `cp`) | The scripts as they are at the base, run the same way (their files copied to a tree of their own in the scratchpad, with the same stand-in `git` and the real `build/mingw`), make deliveries without the file: `ls … \| grep -c WINDOWS-AGENT.md` prints 0 for both | With a stand-in `git` first on `PATH` (it answers only `status --porcelain` with a clean tree, `rev-parse HEAD` with the fake commit `0123456…`, and `merge-base --is-ancestor` with success; the scripts' code is untouched), `package-windows.sh` and `package-diagnostic.sh winhttp_timeouts` write `crispgui-0123456/` and `crispgui-diag-winhttp_timeouts-0123456/` under `<scratchpad>/pkg-test`. In both, `file` says "with CRLF line terminators", no line lacks the CR (`grep -c -v $'\r$'` prints 0; 41 CR bytes for 41 lines), and `cmp` against the source converted with `sed 's/\r*$/\r/'` finds no difference. The full delivery also holds `crispgui.exe`, `CHECKLIST.md`, `BUILD-INFO.txt` (CRLF; "Run: crispgui.exe; run-tests.cmd runs 15 unit and 5 integration tests"), `testdata/`, and the test executables |
| AC-2: the delivery is built from a clean tree at the merged commit, with clean builds that pass G1, G2, and G4 | — | — | After the merge: section "Delivery" |
| AC-3: the delivery is `crispgui-<sha>.zip` and holds `crispgui.exe` and no file named after CrispRustGUI | — | — | After the merge: section "Delivery" |
| AC-4: the receipt records name, size, SHA-256, commit, and file list | — | — | After the merge: section "Delivery" |

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2) with `set -o pipefail`; the logs are in `<scratchpad>/logs/`.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin`; `git switch --no-track -c a004/P4-T3 origin/main` at `c727a30` | 0; 0 | Branch created; `813318f` appeared upstream while I worked (no overlap with my files) |
| `<CODING_ROOT>` | `bash -n tools/package-windows.sh`; `bash -n tools/package-diagnostic.sh` | 0; 0 | Syntax is fine |
| `<CODING_ROOT>` | `tools/package-diagnostic.sh` with no target | 1 | "usage: tools/package-diagnostic.sh <target> [handoff-out]" |
| `<CODING_ROOT>` | `tools/package-windows.sh <scratchpad>/pkg-dirty`; `tools/package-diagnostic.sh winhttp_timeouts <scratchpad>/pkg-dirty`, on this tree, which is dirty with the edits | 1; 1 | "the working tree is not clean" for both; the output folder was not created |
| `<CODING_ROOT>` | `PATH=<scratchpad>/fakebin-t3:$PATH bash tools/package-windows.sh <scratchpad>/pkg-test`; the same for `tools/package-diagnostic.sh winhttp_timeouts` | 0; 0 | The two deliveries of the mapping above, each with its `.zip` and `.zip.sha256`; nothing was written to `<HANDOFF_OUT>` |
| `<CODING_ROOT>` | The same two scripts at the base, from copies in `<scratchpad>/base-tree/tools/` (with `build` and `tools/windows` linked to the real ones) | 0; 0 | Deliveries without `WINDOWS-AGENT.md` |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0 | Incremental, 0 compiler warnings, 0 CMake warnings |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed |
| `<CODING_ROOT>` | `python3 -I tools/dev/mdcheck.py .`; `git diff --check` | 0; 0 | 0 problems; no whitespace errors |
| `<CODING_ROOT>` | `git commit` of the candidate as `c2b560e`; `git fetch origin`; `git -c user.name=grshlogan -c user.email=grshlogan@gmail.com rebase origin/main`; `git range-diff c727a30..c2b560e origin/main..HEAD`; `git patch-id --stable` of both | 0; 0; 0; 0; 0 | Rebased onto `813318f` as `bd925e8`; `1: c2b560e = 1: bd925e8`; equal patch IDs |
| export of `bd925e8` | Review R01 (fresh Sonnet 5.5 subagent): G1, G2, and G4 from clean exports, the scripts' refusals, both happy paths with a stand-in `git`, nine mutations, the base scripts for comparison | see [review-R01.md](review-R01.md) | PASS: no P0, P1, or P2 finding, three P3 notes; G1 (223 steps), G2 (20 of 20), and G4 (226 steps) exit 0 with no warnings |
| `<CODING_ROOT>` | `git fetch origin` before the push | 0 | `origin/main` still `813318f`; the push is a fast-forward |

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| R01 / F1 (P3) | AGREE, not applied | The header comment of `tools/package-diagnostic.sh` still says the delivery is "the executable and a BUILD-INFO.txt"; it now also holds `WINDOWS-AGENT.md`. A comment change after the PASS would make the merged patch differ from the reviewed one, so it waits for the next change of that script |
| R01 / F2 (P3) | AGREE | For A003: steps 2 and 4 of the delivery steps in `docs/agents/README.md` are out of date in different directions: a diagnostic delivery now carries the brief but no `CHECKLIST.md`, and the Phase 0 smoke script, which the packet does not name, carries neither |
| R01 / F3 (P3) | AGREE | For A003: `tools/windows/WINDOWS-AGENT.md` tells the agent to run a delivery's `CHECKLIST.md`, which a diagnostic delivery does not have; the brief could say what to do then |

## Delivery

Pending: built from the merged commit after the Review, as the packet describes.

## Uncovered, skipped, and decisions requested

- The Windows side is `not run`: the delivery is run by the owner or the Windows-side agent.
- The packaging checks for a commit that is not on `origin/main` and for a non-release build are code this change does not touch; the Review may drive them as it did for P4-T4.
- The real packaging run is part of the delivery below and needs a clean tree on `origin/main`; this candidate's runs used a stand-in `git` and wrote under the scratchpad.
