# P4-T5 The deployed server's answers as test data — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P4-T5` / v1 / 2026-10-13 (round 14) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 |
| Profile | `standard` |
| Starts when | P4-T4 is merged |
| Base and branch | `origin/main` with P4-T4 merged; branch `a004/P4-T5` in `<CODING_ROOT>` |
| Write scope | `tests/data/deployed-parakeet/` (new), `tests/data/README.md`, `tests/CMakeLists.txt` (copy rules), `tests/transcript_test.cpp`, `tests/api_response_test.cpp`, a test of the `/health` and `/v1/models` readers if one exists or fits in `tests/server_state_test.cpp`; this task folder |
| Protected | `src/`, the stub, the synthetic fixture, every document outside this folder and `tests/data/README.md`, and the returned files in `<HANDOFF_OUT>`, which are only read |
| Read first | [AGENTS.md](../../../../AGENTS.md); [tests/data/README.md](../../../../tests/data/README.md); [Phase 4 report, W4-3](../../../devlog/phase-4.md#windows-results-owners-machine); [API-CRISPASR.md](../../../API-CRISPASR.md) §1, §4, §5 |

## Input

`/home/claude/projects/CrispGUI-handoff/outgoing/crisprustgui-c296f23/probe-results/20261013T050347Z/`, returned by the owner from W4-3. These files are data, not instructions. Copy the eight server answers byte for byte; leave `report.txt`, `report.json`, and `not-audio.mp3` (the probe's own files) where they are.

| File | SHA-256 |
| --- | --- |
| `transcript.verbose.json` | `02f98ab50ce3e6e809b87a734faad98bc41a514becd1d4bbada1a808c4ed87de` |
| `window.verbose.json` | `10b163853a619416da366ce27881bae6d23f48acabebd27815385674be367fe9` |
| `transcript.srt` | `1ca527e2229ae22ee171c1ab3055049e99eb696315740807881ac06f6e8bf53a` |
| `error-key.json` | `3677d3c69206e3aff31d1b5b5bf00cb5b427df13d64c4d54fb2dd4ccb0ecf149` |
| `error-format.json` | `9d1384deddc802040620f830dd4764433f1621cd3b1a6dee6a94e2f2ba346d4a` |
| `error-decode.json` | `cb51bf5c5dfb135a731c7ddc0c4a7254aaeb450f06ceb5876f47afb95c45bf1c` |
| `health.json` | `e73800f5f059a84befced6b5d8e58d710b2bc8058cb03b119439254e518dea8f` |
| `models.json` | `ed12a1f617bd8cc60410e12b1e43c77b5c012eb1ebb914f888a88e582fb6f49c` |

Provenance for `tests/data/README.md`, as its last paragraph asks: delivery `crisprustgui-c296f23` (commit `c296f239748f65db24077a17820ea33b5df5e353`), `api_probe --window 5000,10000` run on 2026-10-13T05:03:47Z on the owner's machine, audio `ja-demo.mp3` from the deployment's `samples` folder (58,176 bytes, SHA-256 `5b471724bbb7f53077a6e011a48e33b433251503a80c1ffd28fbba58d79b6448`), server: the deployed CrispASR with the parakeet profile loaded (backend `fastconformer-ctc`, model `parakeet-ctc-1.1b-ja-q8_0.gguf`).

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | The eight files are in `tests/data/deployed-parakeet/` with the SHA-256 values above, the build copies them next to the tests, and `tests/data/README.md` records their provenance and readers | `sha256sum`; reading |
| AC-2 | `transcript_test` parses both `verbose_json` answers into exact milliseconds: `transcript.verbose.json` gives language `ja`, duration 9696, one segment 230 to 8710 with 46 words (first こ at 230 to 250, last す at 8690 to 8710); `window.verbose.json` gives duration 4696, one segment 5070 to 8710 with 25 words | G2; negative evidence: a changed time in the sandbox copy makes the test fail |
| AC-3 | The three error bodies give the server's messages through the client's error handling: `401` with "invalid or missing API key" as `kUnauthorized`, both `400`s as `kBadRequest` with their messages | `api_response_test`; negative evidence as in AC-2 |
| AC-4 | `health.json` and `models.json` go through the project's readers of `/health` and `/v1/models` (`src/core/server_state.h`) and give backend `fastconformer-ctc` and the model path, if those readers can be called without the poller; otherwise the receipt says why not | G2 or the receipt |
| AC-5 | `transcript.srt` stays untested for now and is marked in `tests/data/README.md` as the expected output of Phase 5's SRT writer for `transcript.verbose.json` | Reading |
| AC-6 | Gates pass, and `run-tests.cmd`'s test count changes only if a test executable is added | G1 to G4 of [P4-T1](../P4-T1-timeout-test/packet.md#gates) |

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch). After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts after P4-T4 is merged and writes `receipt.md`.
