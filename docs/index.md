# Documentation Index

The owner (the user) approves requirements and decisions and is the acceptance authority. This index lists every current entry point and its responsibility; a new document is created only when a task has a reader for it, and it is registered here.

| Entry | Responsibility | Status |
| --- | --- | --- |
| [AGENTS.md](../AGENTS.md) | Agent rules: scope, language, hard rules, evidence, identities, environment boundaries | Current |
| [README.md](../README.md) | Human overview and repository map | Current |
| [START_HERE.md](START_HERE.md) | New-session reading order and project map | Current |
| [STATUS.md](STATUS.md) | Present facts, blockers, verification boundary, next event | Current |
| [OPEN-QUESTIONS.md](OPEN-QUESTIONS.md) | The single register of undecided questions, and where resolved ones are recorded | Current |
| [SPEC.md](SPEC.md) | v1 requirements and acceptance | Approved; revised 2026-10-11 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Owner invariants, components, parameter model, window loop, threading, decisions, risks | Planned design; parts proposed |
| [API-CRISPASR.md](API-CRISPASR.md) | Server contract checked against the v0.8.42 source, plus corrections to the audits | Current |
| [WORKFLOW.md](WORKFLOW.md) | The six-step pipeline and v1's place in it | Current |
| [TASKS.md](TASKS.md) | Phased plan with acceptance per phase | Current; Phases 0 to 3 complete, Phase 4 implemented, its corrections in task packets |
| [CONVENTIONS.md](CONVENTIONS.md) | Coding conventions (project profile, part 1) | Current |
| [ENVIRONMENT.md](ENVIRONMENT.md) | Build environment, toolchain state, runtime defaults, verification entry points (project profile, part 2) | Current |
| [agents/README.md](agents/README.md) | Identity registry, checkouts, delivery to Windows, authorizations, workflow, source imports | Current |
| [agents/tasks/README.md](agents/tasks/README.md) | Task packets, receipts, Review reports, the Review prompt, and the task index | Current |
| [identity/](identity/) | Identity contexts: one file per identity, written only by that identity; [TEMPLATE.md](identity/TEMPLATE.md) is the layout | Current |
| [reference/README.md](../reference/README.md) | Provenance, licenses, and reading order of the read-only reference library | Current |
| [devlog/phase-0.md](devlog/phase-0.md), [devlog/phase-1.md](devlog/phase-1.md), [devlog/phase-2.md](devlog/phase-2.md), [devlog/phase-3.md](devlog/phase-3.md), [devlog/phase-4.md](devlog/phase-4.md) | Phase reports, one `devlog/phase-N.md` per phase ([TASKS.md](TASKS.md)) | Phases 0 to 3 complete; Phase 4 waiting for its Windows checks |
| [tools/windows/CHECKLIST.md](../tools/windows/CHECKLIST.md) | The Windows checks of the current phase, carried by its delivery | Current: Phase 5 (W5-1 to W5-7) |
| [tools/windows/WINDOWS-AGENT.md](../tools/windows/WINDOWS-AGENT.md) | Brief for the Windows-side agent that runs the checks | Current |
| [tools/dev/README.md](../tools/dev/README.md) | WSL2 development helpers: WSLg captures, the Markdown check, a stand-in CrispASR root | Current |
| [tests/data/README.md](../tests/data/README.md) | Provenance of the server answers the tests use | Current |
