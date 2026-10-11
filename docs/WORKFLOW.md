# Workflow Context — The Six-Step Media Pipeline

> Why this GUI exists and where v1 sits. Source: user's pipeline design (2026-10-10), requirements in `E:\AI\ASR\docs\product\pipeline.md`, capability audit in `reference/audits/audit-reports/crispasr-audit.md`.

## The full pipeline (user's vision)

```
Input video/audio
  │
  ├─ Step 1  Separate vocals / ambient sound
  ├─ Step 2  Distinguish dialogue speakers via voiceprint prompt
  ├─ Step 3  ASR transcription → .srt subtitles          [v1 GUI scope]
  ├─ Step 4  Tone/events via a specific model → .srt     [events expressed as subtitles]
  ├─ Step 5  Translate the ASR subtitles                 [Hy-MT2-30B-A3B @ 127.0.0.1:5113]
  └─ Step 6  Merge/integrate all .srt outputs
```

## Status per step (as of 2026-10-11)

| Step | Engine capability | Model readiness | GUI v1 |
|---|---|---|---|
| 1. Separation | Built-in (`/v1/audio/separation`; mel-band-roformer / htdemucs) | Weights not yet placed | Out of scope; data model supports it |
| 2. Speaker distinction | Request-level diarization parameters | Untested | Out of scope; future UI = speaker label naming |
| 3. ASR → SRT | Working | 4 Japanese profiles tested end-to-end | **Core of v1** |
| 4. Events/tone → SRT | `sensevoice` backend compiled in; output format needs verification | Weights not yet placed | Out of scope; second model/service question to resolve later |
| 5. Translation | External node (user-managed, do not modify) | Hy-MT2-30B-A3B running on 5113 | Out of scope; GUI will act as a client only |
| 6. Merge | Local processing | N/A | Out of scope |

## Design consequences for the GUI

1. **The job model is a step chain from day one** — see `ARCHITECTURE.md` §2.4/§2.5. v1 executes one step, but the queue, persistence, and resume logic are written against `steps[]`.
2. **Each step's parameters are frozen in the job snapshot** at enqueue time (reproducibility). Parameter edits between jobs are the user's normal workflow.
3. **Translation (step 5) will be a plain OpenAI-compatible client** to `127.0.0.1:5113` with `model=default`, `api_key=default` — the node is user-managed and must never be modified or restarted by this GUI. It is a separate "service entry" in the future service registry, marked as *external/unmanaged*.
4. **Step outputs are files** (e.g., separated stems, SRTs) so steps compose through the filesystem, matching the existing manual pipeline style.
5. **Step 4's "events as subtitles"** means an SRT where the spoken text may be replaced or annotated with event labels — the merge step (6) decides presentation. This is a product decision deferred to the pipeline phase.

## What v1 delivers toward the pipeline

- Reliable batch transcription to SRT (the pipeline's core step, today manual).
- A queue that can later run multi-step chains without a rewrite.
- Service management patterns (start with profile / stop / health) that will extend to additional model services (e.g., a future SenseVoice service for step 4).

## Explicit non-goals for v1

- No orchestration UI for chaining steps.
- No modification of the Hy-MT2 node or any CrispASR internals.
- No new model downloads or management beyond profile readiness checks.
