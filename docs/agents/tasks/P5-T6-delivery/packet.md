# P5-T6 The Phase 5 delivery — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P5-T6` / v1 / 2026-10-14 (round 25) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 (delivery record), the owner (W5 checks) |
| Profile | None: a build from a reviewed and merged state, not a Review candidate |
| Starts when | P5-T5 is merged |
| Base | `origin/main` with P5-T5 merged and A003's Phase 5 `CHECKLIST.md` (in `tools/windows/` since round 25) |
| Write scope | This task folder; the new delivery in `<HANDOFF_OUT>` |
| Protected | Everything else; earlier deliveries and the files the owner returned stay untouched |
| Read first | [agents/README.md, delivery to Windows](../../README.md#delivery-to-windows); [tools/windows/CHECKLIST.md](../../../../tools/windows/CHECKLIST.md) (W5-1 to W5-7) |

## Delivery

In `<CODING_ROOT>` with a clean tree whose HEAD is the merged commit on `origin/main`:

1. Clean builds: G1, G2, and G4 of the [standard gates](../README.md#standard-gates) after removing `build/linux` and `build/mingw`; record the step counts and that no warnings appeared.
2. `tools/package-windows.sh`. Check that the folder holds `crispgui.exe`, `CHECKLIST.md` with the W5 checks, `WINDOWS-AGENT.md`, and `BUILD-INFO.txt` naming the commit and the number of tests, the texts with CRLF endings.
3. Record in `receipt.md`: the zip's name, size in bytes, and SHA-256, the commit, and the file list.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | The delivery is built from a clean tree at the merged commit, with clean builds that pass G1, G2, and G4 | Receipt commands and exit codes |
| AC-2 | Its `CHECKLIST.md` is the Phase 5 one (W5-1 to W5-7) | Reading the packaged copy |
| AC-3 | The receipt records name, size, SHA-256, commit, and file list | Reading |

## Next event

A004 notifies A003 with the receipt path; A003 records the delivery and gives the owner the steps.
