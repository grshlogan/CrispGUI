# Test data

Server answers for the tests. `tests/CMakeLists.txt` copies them to `testdata/` next to the test executables, and Windows deliveries carry that folder.

| File | Read by | Provenance |
| --- | --- | --- |
| `verbose-synthetic.json` | `transcript_test`; the stub's replay in `api_client_test` | Synthetic: written by A003 on 2026-10-12 in the layout of `crispasr_segments_to_openai_verbose_json` (`reference/crispasr/crispasr_output.cpp:1163-1253`), with two-space indentation, three decimals for `duration`, two for times, and six for `temperature` and `avg_logprob`. No server produced it. It has word times, a speaker label, token IDs, and a text with quotes and a backslash |

Answers recorded from the deployed server with `api_probe` ([OQ-4](../../docs/OPEN-QUESTIONS.md#oq-4-source-material-needed) item 3) are added here byte for byte, each with the delivery's commit, the probe report, the audio file's name and SHA-256, and the date of the run.

## Recorded from the deployed server

`deployed-parakeet/` holds eight answers of the deployed CrispASR with the parakeet profile loaded (`/health` backend `fastconformer-ctc`, model `parakeet-ctc-1.1b-ja-q8_0.gguf`), kept byte for byte. They are the answers of the owner's W4-3 run:

- Delivery: `crisprustgui-c296f23` (commit `c296f239748f65db24077a17820ea33b5df5e353`), `api_probe.exe --window 5000,10000`, run on the owner's machine on 2026-10-13T05:03:47Z.
- Probe report: `report.txt` and `report.json` of that run, in `probe-results/20261013T050347Z/` inside the delivery's folder in `<HANDOFF_OUT>` ([agents/README.md](../../docs/agents/README.md#delivery-to-windows)). They stay there; ten of ten checks passed.
- Audio: `ja-demo.mp3` from the deployment's `samples` folder, 58,176 bytes, SHA-256 `5b471724bbb7f53077a6e011a48e33b433251503a80c1ffd28fbba58d79b6448`.

| File | SHA-256 | Read by | What it is |
| --- | --- | --- | --- |
| `deployed-parakeet/transcript.verbose.json` | `02f98ab50ce3e6e809b87a734faad98bc41a514becd1d4bbada1a808c4ed87de` | `transcript_test` | `verbose_json` of the whole sample: duration 9.696 s, one segment from 0.23 s to 8.71 s with 46 word times, one per character |
| `deployed-parakeet/window.verbose.json` | `10b163853a619416da366ce27881bae6d23f48acabebd27815385674be367fe9` | `transcript_test` | `verbose_json` of the window `offset_t_ms=5000`, `duration_ms=10000`: times in the file's time, duration 4.696 s (the part inside the file), one segment from 5.07 s to 8.71 s with 25 word times |
| `deployed-parakeet/transcript.srt` | `1ca527e2229ae22ee171c1ab3055049e99eb696315740807881ac06f6e8bf53a` | none yet | The server's `srt` for the whole sample: one cue. It is the expected output of Phase 5's SRT writer for `transcript.verbose.json` ([OQ-7](../../docs/OPEN-QUESTIONS.md#oq-7-local-srt-generation)) and stays untested until then |
| `deployed-parakeet/error-key.json` | `3677d3c69206e3aff31d1b5b5bf00cb5b427df13d64c4d54fb2dd4ccb0ecf149` | `api_response_test` | The body of the `401` for a wrong API key |
| `deployed-parakeet/error-format.json` | `9d1384deddc802040620f830dd4764433f1621cd3b1a6dee6a94e2f2ba346d4a` | `api_response_test` | The body of the `400` for `response_format=bogus` |
| `deployed-parakeet/error-decode.json` | `cb51bf5c5dfb135a731c7ddc0c4a7254aaeb450f06ceb5876f47afb95c45bf1c` | `api_response_test` | The body of the `400` for a file that is not audio |
| `deployed-parakeet/health.json` | `e73800f5f059a84befced6b5d8e58d710b2bc8058cb03b119439254e518dea8f` | `server_state_test` | `GET /health` |
| `deployed-parakeet/models.json` | `ed12a1f617bd8cc60410e12b1e43c77b5c012eb1ebb914f888a88e582fb6f49c` | `server_state_test` | `GET /v1/models`: one model, with its Windows path as the server resolved it |
