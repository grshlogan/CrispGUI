# Current Status

Updated 2026-10-11 (round 3) by A003. The owner (the user) is the acceptance authority.

## Baseline

The repository holds the specification, the architecture plan, the server contract, the phased plan, the agent entry points, and a read-only reference library. It has no application source, build configuration, or test command yet. It is a Git repository on `main`, and this baseline is its first commit.

## Present facts

| Area | State | Evidence or detail |
| --- | --- | --- |
| Specification and design | SPEC approved; revised on 2026-10-11 with the owner's directions and source-review corrections. The parameter model and window loop details are proposed | [SPEC.md](SPEC.md), [ARCHITECTURE.md](ARCHITECTURE.md), [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) |
| Server contract | Checked against the v0.8.42 server source, which is byte-identical to the same file in the CrispASR 0.8.42 archive in the Learn library; deployed-build behavior `not verified` | [API-CRISPASR.md](API-CRISPASR.md), [reference/README.md](../reference/README.md) |
| Implementation | Not started; no `src/` | Directory listing |
| Repository | Git, branch `main`; the remote `origin` is the owner's private GitHub repository `grshlogan/CrispGUI` | [agents/README.md](agents/README.md#checkouts) |
| Profiles | The four Japanese reference profiles parse as JSON after string-aware comment stripping | Python check in WSL2, exit 0 (2026-10-11) |
| Windows build path | MinGW-w64 cross-compilation in WSL2 (owner decision, 2026-10-11). GCC 12 with POSIX threads is installed, and a probe of EUI-NEO's static MinGW link mode passes | [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) |
| Linux toolchain | No C++ compiler, CMake, Ninja, or EUI-NEO's Linux development packages yet; the package list is with the owner | [OQ-3](OPEN-QUESTIONS.md#oq-3-wsl2-toolchain-installation) |
| EUI-NEO source | v0.6.1 in the shared Learn library; not copied into this repository | [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| WSLg | `DISPLAY` and `WAYLAND_DISPLAY` set; GUI rendering not tested | Environment variables |
| Windows side | Runtime and acceptance only; no delivery made | [agents/README.md](agents/README.md#delivery-to-windows) |
| Identities | A003 established; A002 authored the audit reports | [agents/README.md](agents/README.md) |
| Workflow | None selected | [OQ-9](OPEN-QUESTIONS.md#oq-9-formal-workflow-and-review-policy) |

## Blockers

Phase 0 is blocked until the owner installs the WSL2 packages listed in [ENVIRONMENT.md §2.1](ENVIRONMENT.md#21-wsl2-toolchain) ([OQ-3](OPEN-QUESTIONS.md#oq-3-wsl2-toolchain-installation)). OQ-4 item 2 and OQ-5 to OQ-10 affect later phases and do not block Phase 0.

## Verification boundary

Evidence so far comes from reading the documents and reference sources, environment probes in WSL2, the profile parse check, and a MinGW-w64 probe build in the session scratchpad. Building this project, running it, the real server, and every Windows-native behavior are `not run`.

## Next event

The owner installs the WSL2 packages; A003 then runs Phase 0: EUI-NEO smoke builds for Linux and Windows and the first delivery to Windows.
