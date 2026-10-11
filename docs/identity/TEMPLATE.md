# [Full display name, for example A001[PiDeck]Lead Coding Agent] Identity Context

## Ownership and maintenance rules

1. Only the identity named in this context creates and maintains it. Other identities have read-only access. Report errors to its owner or the Coordinator rather than rewriting, replacing, or renaming its context.
2. Keep the document below the hard limit of 1500 lines. About 1000 lines is a reference size, not a quota; keep shorter contexts concise. Remove obsolete investigation details before reaching the limit. Store long evidence in the project's approved evidence location and retain an index here.
3. At every update, remove stale conclusions, noise, and broken references. Resolve conflicts with source, configuration, or STATUS using reproducible evidence. Keep each fact in its authoritative location.
4. Use the project's evidence labels. For CrispRustGUI, these are `verified`, `observed`, `historical`, `planned`, `hypothesis`, and `not verified`, as defined by [AGENTS.md](../../AGENTS.md). Use `deprecated` only to describe a superseded item, not as proof of a fact. Every verified claim needs traceable evidence; every hypothesis needs a verification method.
5. Maintain one context per stable identity, not per conversation, platform, or session. Use the stable ID as the filename, such as `A001.md`, and place the display name in the body. A new session resumes this document after confirming the previous writer stopped.

Initialization note: remove this note when instantiating the template. Replace every bracketed placeholder, remove illustrative examples, and adapt the sections to the identity's actual role. The template itself is not an identity registration, task packet, permission grant, or verified host capability. Project rules take precedence.

## 1. Identity and Session Information

| Field | Value |
| --- | --- |
| Display name | `[Stable ID][Agent/host]Role summary` |
| Stable ID | `[For example A001; match the task packet, report, and receipt identity_id]` |
| Parent identity | `[None, or the parent stable ID for a child identity]` |
| Agent and host | `[Agent family and actual host, recorded separately from model name]` |
| Session reference | `[Native session ID if verified, otherwise unavailable and a logical label]` |
| Last updated | `[YYYY-MM-DD and optional evidence-capture timestamp]` |
| Workspace | `[Checkout alias and verified cwd or independent worktree]` |
| Sole writer | `[This identity's stable ID; other identities are read-only]` |

Session references and stable IDs describe identity; neither creates permissions. State the authorization source and stopped-writer confirmation before taking over write ownership.

## 2. Scope and Boundaries

- Core responsibilities: `[One to three concrete responsibilities owned by this identity]`.
- Authorized operations: `[Exact paths, commands, task/packet version, and owner authorization]`.
- Protected boundaries: `[Other identities' contexts, out-of-scope modules, references, credentials, and host settings]`.
- Primary workflow: `[agent-dev-loop profile, selected migration Skill, reference-audit, or not applicable]`.
- Admission limits: `[Host versions, security controls, isolation, and required unreadable inputs]`.

Distinguish standing authorization from the current operation. Git writes, credential or host changes, and destructive cleanup need their applicable separate authorization.

## 3. Current Facts Snapshot

Record the latest stable facts about this identity's assigned work. Put historical changes in section 5, not here.

- Bound task or plan: `[Task ID/packet version or authoritative plan path]`.
- Current stage: `[Observed progress; label later stages planned]`.
- Latest verification: `[Exact command, cwd alias, platform, real exit code, and output or evidence pointer]`.
- Uncovered or skipped checks: `[Check, reason, and effect on the conclusion]`.
- Artifacts: `[Concrete files and evidence locations; no invented outputs]`.
- Frozen candidate: `[Candidate ID/digest and Review binding, or none]`.

Use `not run` for a check that did not execute. Bash pipelines require `pipefail`; PowerShell native commands require the actual `$LASTEXITCODE`. Bind repository results to exact SHAs, not branch names alone.

## 4. Key Paths and Contracts

List the files this identity actually uses. Replace or remove these examples when instantiating the context.

| Type | Path | Access | Identity or contract |
| --- | --- | --- | --- |
| Owned implementation | `src/parser/` | Write when assigned | Example exclusive Coding scope |
| Data contract | `contracts/schema.json` | Read | Example upstream schema agreement |
| Assigned plan | `docs/plan/plan_parser.md` | Write when assigned | Example progress record |
| Upstream evidence | `docs/evidence/upstream.log` | Read-only | Example evidence, not installation authority |

Keep shared project facts in STATUS, plans, decisions, or research as appropriate. Use this context for identity handoff and evidence indexes, not a second project-status source.

## 5. Decisions and Handoff Stream

Keep the newest entry first. Retain details for roughly the latest three handoffs; merge older entries into a summary or an immutable evidence index. Preserve evidence before removing the only copy of a receipt.

### [YYYY-MM-DD] Handoff or Progress Record

- Goal and completed work: `[Concrete result and conclusion: COMPLETED, BLOCKED, UNCERTAIN, or NOT RUN]`.
- Decision and rationale: `[Why the choice was made; distinguish proposal from approval]`.
- Changed paths: `[Exact owned paths; preserve existing owner changes]`.
- Verification: `[Commands, cwd alias, platform, exit codes, and evidence pointers]`.
- Candidate/result binding: `[Checked SHA, conflict-resolution SHA if any, and integration SHA, or not applicable]`.
- Uncovered scope and risks: `[What the evidence does not prove]`.
- Next identity and event: `[Named receiver and concrete next step]`.

A commit cannot contain its own SHA or later push evidence. Record the capture boundary honestly. Use the Git history and remote ref to verify final results; obtain separate authorization before a follow-up receipt commit.

## 6. Next Actions and Blockers

- [ ] Next action: `[Concrete action, owner, and permission or input required]`.
- [ ] Minimal blocker or decision request:
  - Problem: `[None, or the specific unmet contract]`.
  - Reproduction and evidence: `[Command, exit code, SHA/status, or relevant log excerpt]`.
  - Excluded alternatives: `[Evidence that rules out proposed workarounds]`.
  - Decision required: `[Owner/Coordinator choice needed to continue]`.

When admission fails, preserve files and report the real state. Preparation is not dispatch, passing checks is not owner acceptance, and a branch name is not proof of synchronization.

## 7. Related Identities and Document Entry Points

- Coordinator: `[Assigned identity and context, or not assigned]`.
- Upstream/downstream identities: `[IDs and read-only context paths]`.
- Project state and index: [STATUS](../STATUS.md), [documentation index](../index.md).
- Project workflow: [identity registry and authorization rules](../agents/README.md).
- This identity's evidence: `[Approved evidence path or immutable Git revision/path; retain an index here]`.

## 8. Maintenance Checklist

- [ ] Context stays below 1500 lines and contains no filler added to meet a target size.
- [ ] Stale conclusions, noise, broken references, and unsupported claims are removed.
- [ ] Every verified claim has traceable evidence; every hypothesis has a verification method.
- [ ] Current facts agree with authoritative project documents without duplicating their responsibility.
- [ ] The newest handoff is first; older details retain an accessible evidence index.
- [ ] Identity, session reference, host security, workflow role, and authorization remain distinct.
- [ ] Required skipped checks and capture-boundary limitations are explicit.
