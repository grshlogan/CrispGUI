# P5-T1 The SRT writer: the server's cue format from verbose_json — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P5-T1` / v1 / 2026-10-13 (round 20) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 (Linux); the Windows acceptance of Phase 5 belongs to its delivery |
| Profile | `standard` (a pure function; no file or network access) |
| Base and branch | `origin/main` when A004 starts; branch `a004/P5-T1` in `<CODING_ROOT>` |
| Write scope | `src/core/srt_writer.{h,cpp}` (new), `CMakeLists.txt` (the new source in `crisp_core`), `tests/srt_writer_test.cpp` (new), `tests/CMakeLists.txt`; three follow-ups of Phase 4: the line on the word times in `tests/data/README.md` (also mark `transcript.srt` as read by the new test), the comment above `CheckRecorded` in `tests/transcript_test.cpp`, and the `durationMs` comment in `src/core/transcript.h`; this task folder |
| Protected | The recorded answers in `tests/data/deployed-parakeet/` (read only), the parser in `src/core/transcript.cpp`, every document outside this folder and `tests/data/README.md` |
| Read first | [AGENTS.md](../../../../AGENTS.md) (licenses); [ARCHITECTURE.md §2.8](../../../ARCHITECTURE.md#28-srt-writer); [SPEC.md FR-3](../../../SPEC.md#fr-3-transcription-single-file); [API-CRISPASR.md §4](../../../API-CRISPASR.md#4-response-formats); [OQ-7 in the resolved table](../../../OPEN-QUESTIONS.md#resolved) |

## Decision behind it

OQ-7 is decided (A003 as planner, round 20): the GUI requests `verbose_json` and writes the SRT itself. Without shaping, its output equals the server's `srt` for the same segments, byte for byte; shaping (characters per cue, splits at sentence ends) comes later as a GUI-tier option. This task is that writer, without shaping.

## The format

The server builds `srt` in `crispasr_segments_to_srt` (`reference/crispasr/crispasr_output.cpp:1124-1134`), with `max_len` 0 and no punctuation split (`crispasr_server.cpp:2287`), through the plain path of `crispasr_make_disp_segments` (`:371-408`), and formats times in `crispasr_to_timestamp` (`:23-35`). For segments as `ParseVerboseJson` returns them:

- One cue per segment whose text is not empty, in order; a segment with empty text gives no cue and no number.
- Each cue: its number counted from 1 over the cues written, `\n`; `HH:MM:SS,mmm --> HH:MM:SS,mmm` from `startMs` and `endMs`, `\n`; the text, `\n`; an empty line `\n`. Hours have at least two digits; minutes and seconds two; milliseconds three. Nothing else: no byte order mark, no carriage returns, no trimming of the text.
- Speaker labels: the server's SRT prefixes the raw label, such as `(speaker 1) `, while `verbose_json` carries the normalized label (`B`; `normalise_speaker`, `crispasr_output.cpp:1259-1277`). Invert the normalization for the standard labels (`A` to `Z` and two-letter labels) to the server's prefix; any other label gets a documented prefix of your choice, recorded in the receipt. Diarization is not part of v1, so recorded answers carry no labels.
- No segments: an empty string.

Write the code from this description. If you adapt code from `crispasr_output.cpp` (CrispASR, MIT), keep its copyright and license notice, as AGENTS.md requires.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | `transcript.verbose.json` of `tests/data/deployed-parakeet/`, parsed by `ParseVerboseJson` and written by the new writer, equals `transcript.srt` of the same folder byte for byte | `srt_writer_test`; negative evidence: a changed byte in the build's copy of `transcript.srt` makes it fail |
| AC-2 | Rule cases: several segments numbered 1 to n; an empty-text segment skipped without a gap in the numbering; times across hour and minute boundaries (3,723,450 ms gives `01:02:03,450`) and above 99 hours; a speaker label `B` gives the prefix `(speaker 1) `; no segments give an empty string; `window.verbose.json` gives one cue `00:00:05,070 --> 00:00:08,710` | `srt_writer_test`; one mutation of the writer per rule, each caught |
| AC-3 | The three follow-ups say: the words of the recorded answers are one per kana or kanji with punctuation attached to the word before it; a window's `duration` is the part of the window inside the file; `tests/data/README.md` names `srt_writer_test` as the reader of `transcript.srt` | Reading |
| AC-4 | Gates pass; the delivery's test count grows by the new test only | G1 to G4 of [P4-T1](../P4-T1-timeout-test/packet.md#gates) |

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch). After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts the task and writes `receipt.md`.
