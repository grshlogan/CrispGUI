# Test data

Server answers for the tests. `tests/CMakeLists.txt` copies them to `testdata/` next to the test executables, and Windows deliveries carry that folder.

| File | Read by | Provenance |
| --- | --- | --- |
| `verbose-synthetic.json` | `transcript_test`; the stub's replay in `api_client_test` | Synthetic: written by A003 on 2026-10-12 in the layout of `crispasr_segments_to_openai_verbose_json` (`reference/crispasr/crispasr_output.cpp:1163-1253`), with two-space indentation, three decimals for `duration`, two for times, and six for `temperature` and `avg_logprob`. No server produced it. It has word times, a speaker label, token IDs, and a text with quotes and a backslash |

Answers recorded from the deployed server with `api_probe` ([OQ-4](../../docs/OPEN-QUESTIONS.md#oq-4-source-material-needed) item 3) are added here byte for byte, each with the delivery's commit, the probe report, the audio file's name and SHA-256, and the date of the run.
