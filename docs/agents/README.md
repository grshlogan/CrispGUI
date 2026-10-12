# Identities and Workflow

This document registers CrispRustGUI's identities, checkouts, delivery to Windows, authorizations, and workflow choice. Root rules are in [AGENTS.md](../../AGENTS.md).

## Identities

| Stable ID | Display name | Host | Role and write scope | Context |
| --- | --- | --- | --- | --- |
| `A002` | not recorded | not recorded | Author of the audit reports in `reference/audits/audit-reports/` (each report's header names A002, dated 2026-10-11). No write scope in this checkout | None in this repository |
| `A003` | `A003[ClaudeCode]Dev Gui` | Claude Code CLI on WSL2 | GUI development: application source, build files, and tests once a task assigns them; project documentation under owner instruction; its own context | [docs/identity/A003.md](../identity/A003.md) |

The owner (the user) assigns identities, approves requirements and decisions, authorizes Git, environment, and import operations, copies deliveries to Windows, and performs Windows-native acceptance. Coordinator, Coding, and Review roles are assigned per task once a workflow is selected.

## Checkouts

- One development checkout, `<WSL_ROOT>`, in the WSL2 distribution `claude-debian12`. Its current location is `/home/claude/projects/CrispGUI`. Identities on WSL2 share it, one writer at a time.
- Windows holds no development checkout. It hosts the CrispASR deployment (`<CRISPASR_ROOT>`, default `E:\AI\ASR\CrispASR`) and receives deliveries.
- `<WSL_ROOT>` is a Git repository on `main`, initialized on 2026-10-11 under the owner's instruction. Its remote `origin` is `https://github.com/grshlogan/CrispGUI`, a private repository the owner created, and the source of record. Pushes authenticate through the GitHub CLI credential helper that is already configured for `github.com`.
- Commits use the owner's Git identity, `grshlogan <grshlogan@gmail.com>`, the same as in the owner's Chronicle Harbor repository. It is passed with `git -c` on each commit; neither the repository nor the global Git configuration stores it.

## Delivery to Windows

Status: four deliveries made; the first three are accepted.

| Delivery | Commit | Archive SHA-256 | Contents | Windows result |
| --- | --- | --- | --- | --- |
| `crisprustgui-eui-smoke-7e4e89e.zip` (45,685,746 bytes) and its unpacked folder, in `<HANDOFF_OUT>` | `7e4e89eb7e021ea61bcb432aadcb3883c22e3479` | `fd35436b51a35e714d497178156561ba7f6335c7eefe1d343a61f4f5ba91ad5b` | Phase 0 smoke: `eui_demo.exe`, `multipage_table.exe`, 26 unit tests, 8 probes, `run-eui-smoke.cmd` | Passed on 2026-10-11 UTC (Windows 11 Pro for Workstations 10.0.22631): no visual problems; 33 passed, 1 skipped by design, 0 failed ([Phase 0 report](../devlog/phase-0.md#windows-results-owners-machine)) |
| `crisprustgui-1817b8e.zip` (6,626,850 bytes) and its unpacked folder, in `<HANDOFF_OUT>` | `1817b8ee4cdf82244b30fca50a349ac9815e2a04` | `dd5fe3845e9ec4f38ceb18f01dbe183c1c6cea6befbd7515c9289408addfd6a8` | Phase 1: `crisprustgui.exe`, `assets/`, 2 unit tests, `run-tests.cmd` | Passed on 2026-10-11 UTC: the owner reports that the tests passed with no problems; `test-results.txt` was not sent ([Phase 1 report](../devlog/phase-1.md#windows-results-owners-machine)) |
| `crisprustgui-b397dcd.zip` (26,739,068 bytes) and its unpacked folder, in `<HANDOFF_OUT>` | `b397dcda97db2f227dccf51393635f85e93c0530` | `12bc0217ac604a3bdeb4d0f275c05d83bcfcb05b7e2171faee955d875f85d3f2` | Phase 2: `crisprustgui.exe`, `assets/`, `fonts/`, 8 unit tests with `testdata/`, `run-tests.cmd` | Passed on 2026-10-12 UTC: the owner reports that the Windows test passed and that the dashboard's profile list agrees with the launcher; `test-results.txt` was not sent ([Phase 2 report](../devlog/phase-2.md#windows-results-owners-machine)) |
| `crisprustgui-5046d3e.zip` (31,598,175 bytes) and its unpacked folder, in `<HANDOFF_OUT>` | `5046d3e4ca9aadfd0ff9397909fef3f65c6a01bd` | `8c7b04d2074587bcc916654ef429d2e0168d03f2d89955cc337f973a63a4728a` | Phase 3: `crisprustgui.exe`, `assets/`, `fonts/`, 11 unit and 3 integration tests, `stub_crispasr.exe`, `testdata/`, `run-tests.cmd` | Pending |

Steps:

1. A delivery is built from a clean tree at a commit that has been pushed to `origin`.
2. Default form: the MinGW-w64 release build folder (the executable plus EUI-NEO's `assets/`), zipped as `crisprustgui-<short SHA>.zip` with its SHA-256 in `<name>.zip.sha256`. A `BUILD-INFO.txt` inside the folder names the commit, the build type, and the toolchain versions. The Windows side does no development and needs no source, so a Git bundle or a `git archive` snapshot is made only on request.
3. Deliveries go to `<HANDOFF_OUT>`, a directory outside the checkout; the proposed location is `/home/claude/projects/CrispGUI-handoff/outgoing/`. The owner copies the folder to Windows by hand or with a copy script.
4. Windows results come back through the owner (message, log excerpt, or screenshot). They are recorded against the delivered SHA and the platform.
5. Nothing reads or writes across the system boundary, and WSL2 never mounts Windows drives.

## Standing authorizations

- Commits and pushes (owner, 2026-10-11, round 5): A003 may commit per phase and task and push to `origin/main` without asking each round. Force pushes, history rewrites, branch deletion, and remote changes still need a separate authorization that names the target.

Everything else — other Git writes, system package installation, and source imports — needs a direct owner instruction or a task packet. One-time authorizations so far:

- 2026-10-11, round 3: `git init`, and one commit and push at the end of the round.
- 2026-10-11, round 4: commits and pushes during the round; downloading the EUI-NEO v0.6.1 tag archive into the Learn library; tidying the Learn library (moving the EUI-NEO snapshot, extracting and indexing the CrispASR archive, updating its `README.md`).

## Workflow

The owner has not selected a formal Coding/Review workflow ([OQ-9](../OPEN-QUESTIONS.md#oq-9-formal-workflow-and-review-policy)). Installed candidates, checked by file listing on 2026-10-11:

| Skill | Location | Use |
| --- | --- | --- |
| `agent-dev-loop` | `~/.claude/skills/codequality/agent-dev-loop/` (`SKILL.md` readable) | Coding, independent Review, and acceptance with task packets and frozen candidates |
| `reference-audit` | `~/.claude/skills/codequality/reference-audit/` | Independent audits of external projects; output is advisory |

A Skill file being present does not put it in a session's context; a task must name it and the session must read it.

## Source imports

Material that does not come from this repository arrives only through the owner, normally in the shared Learn library (`/home/claude/projects/Learn`, read-only, indexed by its `README.md`). Record each import with: source, purpose, fixed version or commit, license, location, archive SHA-256, read-only status, and missing content. Import approval does not authorize running scripts, installers, or package managers. Never transfer credentials, sessions, keyrings, or private data.

| Source | Purpose | Version | License | Location (in the Learn library) | Digest and notes |
| --- | --- | --- | --- | --- | --- |
| `sudoevolve/EUI-NEO` | UI framework | `v0.6.1`, commit `1647709068e36834ab26da5a97ef4ad98aa34c9d` | Apache-2.0 | `sources/github.com/sudoevolve/EUI-NEO/v0.6.1` (moved from `EUI-NEO-v0.6.1` on 2026-10-11) | Archive `.downloads/EUI-NEO-v0.6.1.zip`, SHA-256 `f0709a325b343691cf2d0ac1ce51cbe6d321e8a972304e0e7deffe6c71b3a2c6`, downloaded with the owner's authorization; its content matches the snapshot. Details in [ENVIRONMENT.md §4.1](../ENVIRONMENT.md#41-eui-neo-snapshot) |
| `CrispStrobe/CrispASR` | Engine CLI flags ([OQ-4](../OPEN-QUESTIONS.md#oq-4-source-material-needed) item 2); provenance of `reference/crispasr/` | `v0.8.42`, commit `3be8bcb4390b8080dc73f807b7abb34dc0aca9c3` | MIT | `sources/github.com/CrispStrobe/CrispASR/v0.8.42` | Archive `.downloads/CrispASR-v0.8.42.zip` (provided by the owner as `CrispASR-0.8.42.zip`), SHA-256 `2a6ba392a2d253c76a857e264e5564e7a47b4202a08a48e3863800fab1104664`; zip comment and `git ls-remote` agree on the commit; 3841 files |
| HarmonyOS Sans SC (Huawei) | UI font ([ENVIRONMENT.md §4.3](../ENVIRONMENT.md#43-ui-fonts)) | Version 1.0 per the fonts' `name` tables; six weights | `not verified`: the files say "Copyright 2021 Huawei Device Co., Ltd. All Rights Reserved." and carry no license text | Not in the Learn library: the owner placed the files in `<WSL_ROOT>/fonts/` on 2026-10-11 (round 6); ignored by Git at the owner's choice | SHA-256 of each file in ENVIRONMENT §4.3; deliveries carry Regular, Medium, and Bold. Missing: the license document |
