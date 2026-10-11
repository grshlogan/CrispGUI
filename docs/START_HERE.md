# Start Here

## New task order

1. Read the root [AGENTS.md](../AGENTS.md) and confirm your identity, task, write scope, and host. When resuming an identity, read its context in [identity/](identity/).
2. Read [STATUS.md](STATUS.md) for the present baseline, blockers, and next event, and [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) for decisions still pending.
3. Read [index.md](index.md), then only the documents the task needs.
4. Before writing code, read [CONVENTIONS.md](CONVENTIONS.md) and [ENVIRONMENT.md](ENVIRONMENT.md); for anything that talks to the server, read [API-CRISPASR.md](API-CRISPASR.md).
5. Keep `reference/`, the CrispASR deployment, and everything on the Windows side read-only.

## Project map

| Concept | Location |
| --- | --- |
| Project, goal, non-goals | [README.md](../README.md); [SPEC.md](SPEC.md) §1 and §5 |
| Requirements and acceptance | [SPEC.md](SPEC.md) |
| Architecture, owner invariants, key decisions | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Server contract | [API-CRISPASR.md](API-CRISPASR.md) |
| Pipeline context | [WORKFLOW.md](WORKFLOW.md) |
| Active plan | [TASKS.md](TASKS.md) |
| Project profile: coding rules and build environment | [CONVENTIONS.md](CONVENTIONS.md), [ENVIRONMENT.md](ENVIRONMENT.md) |
| Identities, delivery to Windows, authorizations | [agents/README.md](agents/README.md) |
| Present state and open decisions | [STATUS.md](STATUS.md), [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) |
| Reference material and its provenance | [reference/README.md](../reference/README.md) |
| Shared upstream snapshots (EUI-NEO, the CrispASR source archive) | `/home/claude/projects/Learn`, read-only and indexed by its `README.md`; EUI-NEO in [ENVIRONMENT.md §4.1](ENVIRONMENT.md#41-eui-neo-snapshot) |
| Verification entry points | Project builds and `ctest`; `run-tests.cmd` in Windows deliveries ([ENVIRONMENT.md §6](ENVIRONMENT.md#6-verification-entry-points)) |
| Phase reports | [Phase 0](devlog/phase-0.md), [Phase 1](devlog/phase-1.md), [Phase 2](devlog/phase-2.md) |
| Root aliases | `<WSL_ROOT>`, `<HANDOFF_OUT>` in [agents/README.md](agents/README.md); `<CRISPASR_ROOT>` in [ENVIRONMENT.md §5](ENVIRONMENT.md#5-runtime-configuration-defaults-windows) |

## Scope gate

The specification is approved. Phases 0 and 1 of [TASKS.md](TASKS.md) are complete ([Phase 0 report](devlog/phase-0.md), [Phase 1 report](devlog/phase-1.md)), and Phase 2, the ProfileStore, is in progress. Installing packages, importing sources, and Git writes beyond the standing commit-and-push authorization each need the owner's authorization ([agents/README.md](agents/README.md#standing-authorizations)).

## Language

The language rule is in [AGENTS.md](../AGENTS.md#language): project-authored files in English, communication with the owner in Chinese.
