# P4-T3 The agent brief in deliveries, and the next Phase 4 delivery — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P4-T3` / v1 / 2026-10-13 (round 13) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 (delivery record), the owner (Windows checks) |
| Profile | `standard` for the script change; the delivery itself is a build, not a Review candidate |
| Starts when | P4-T1 and P4-T2 are merged |
| Base and branch | `origin/main` with both merged; branch `a004/P4-T3` in `<CODING_ROOT>` |
| Write scope | `tools/package-windows.sh`, `tools/package-diagnostic.sh`; this task folder; the new delivery in `<HANDOFF_OUT>` (`/home/claude/projects/CrispGUI-handoff/outgoing/`) |
| Protected | Everything else; earlier deliveries in `<HANDOFF_OUT>` and the files the owner returned there stay untouched |
| Read first | [AGENTS.md](../../../../AGENTS.md); [agents/README.md, delivery to Windows](../../README.md#delivery-to-windows); [ENVIRONMENT.md §2.2](../../../ENVIRONMENT.md#22-commands); [tools/windows/WINDOWS-AGENT.md](../../../../tools/windows/WINDOWS-AGENT.md) |

## Change

- Both packaging scripts copy `tools/windows/WINDOWS-AGENT.md` into the delivery folder with CRLF line endings, as `package-windows.sh` already does for `CHECKLIST.md`.
- Review `R01` of that change (fresh Sonnet 5.5 subagent, [tasks/README.md](../README.md#review-dispatch)); its gates are G1 and G2 of [P4-T1](../P4-T1-timeout-test/packet.md#gates) plus running the changed scripts' argument and clean-tree checks; then merge.

## Delivery

After the merge, in `<CODING_ROOT>` with a clean tree whose HEAD is the merged commit on `origin/main`:

1. Clean builds: remove `build/linux` and `build/mingw`, then G1, G2, and G4 of P4-T1. Record the step counts and that no warnings appeared.
2. `tools/package-windows.sh`. Check that the folder holds `WINDOWS-AGENT.md`, `CHECKLIST.md`, and `BUILD-INFO.txt` with CRLF endings, and that `BUILD-INFO.txt` names the merged commit.
3. Record in `receipt.md`: the zip's name, size in bytes, and SHA-256, the commit, and the file list of the folder.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | Deliveries made by either script contain `WINDOWS-AGENT.md` with CRLF endings | Reading; `file` on the packaged copy |
| AC-2 | The delivery is built from a clean tree at the merged commit, with clean builds that pass G1, G2, and G4 | Receipt commands and exit codes |
| AC-3 | The receipt records name, size, SHA-256, commit, and file list | Reading |

## Next event

A004 notifies A003 with the receipt path. A003 records the delivery and gives the owner the Windows steps (W4-1 to W4-4).
