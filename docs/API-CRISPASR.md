# CrispASR Server API Reference

> The contract CrispGUI programs against. Checked on 2026-10-11 by reading `reference/crispasr/crispasr_server.cpp` and `reference/crispasr/crispasr_output.cpp` (CrispASR v0.8.42 snapshot) and the owner's launcher `reference/crispasr/start-crispasr.ps1`; the CLI flags were checked against `examples/cli/cli.cpp` of the same version in the Learn library in Phase 2, and the upload against its HTTP library, cpp-httplib 0.57.1 (`examples/server/httplib.h`, cited as `httplib.h`), in Phase 4. Line numbers refer to `crispasr_server.cpp` unless another file is named. On 2026-10-13 the deployed build answered the ten checks of `api_probe` as this contract describes (W4-3, with the parakeet profile loaded): `/health`, `/v1/models`, `/progress`, the `401`, the `400` for a bad `response_format` and for audio that does not decode, `verbose_json`, `srt`, a window whose timestamps stay in original-file time, and a cancellation ([Phase 4 report](devlog/phase-4.md#windows-results-owners-machine)); behavior beyond those checks is `not verified`. The Chinese audit `reference/audits/audit-reports/crispasr-audit.md` has more background; §10 lists where it is inaccurate.

## 1. Endpoints

| Endpoint | Method | Auth | Purpose | v1 use |
|---|---|---|---|---|
| `/v1/audio/transcriptions` | POST (multipart) | yes | Transcription; OpenAI-compatible fields plus CrispASR extensions | Core |
| `/health` | GET | no | `{"status":"ok","backend":"<name>"}`; `503 {"status":"loading"}` only while `/load` swaps a model (2388-2396) | Readiness and adoption |
| `/progress` | GET | yes | `{"busy":bool,"progress":N}`, N in 0..100 or -1 when idle (2403-2410); see §7 | In-flight progress |
| `/v1/models` | GET | yes | `{"object":"list","data":[{"id":"<resolved model path>","object":"model","owned_by":"crispasr","backend":"<name>"}]}` (2433-2444) | Loaded-model display |
| `/backends` | GET | yes | `{"backends":[<compiled-in names>],"active":"<name>"}` (2415-2428) | Status detail |
| `/load` | POST | yes | Hot-swaps model and backend only; aligner and other startup options stay (2319-2383) | Not used: profiles differ in aligner, so a profile switch is a restart |
| `/inference` | POST (multipart) | yes | Legacy native JSON, the only output that carries `audio_event` (1833-1985) | Future pipeline step 4 |
| `/v1/audio/separation` | POST (multipart) | yes | Source separation, `multipart/mixed` WAV stems; `503` unless started with `--separate-model` (1684-1828) | Future pipeline step 1 |
| `/v1/translate` | POST (JSON) | yes | Text translation with a translation backend such as m2m100 (2459) | Not used; translation goes to the owner's Hy-MT2 node |
| `/v1/audio/speech`, `/v1/audio/speech-to-speech`, `/v1/voices`, `/v1/chat/completions` | various | yes | TTS, speech-to-speech, voice files, text chat | Not used |

The v0.8.42 server has no `/v1/audio/translations` route; speech translation is the `translate` field of the transcription endpoint.

## 2. Authentication

- The server collects keys from its startup option `--api-keys` (`params.server_api_keys`; `examples/cli/cli.cpp` of the CrispASR 0.8.42 snapshot in the Learn library) and from the `CRISPASR_API_KEYS` environment variable, both comma-separated, and merges them (1332-1336). With no keys, every request is authorized (314-324).
- A client sends `Authorization: Bearer <key>` or `X-API-Key: <key>` (302-312). A missing or wrong key gets `401` with `{"error":{"message":"invalid or missing API key","type":"invalid_api_key"}}` and `WWW-Authenticate: Bearer` (438-443).
- `/health` is public. Every other route requires a key when keys are configured.
- The owner's launcher sets `CRISPASR_API_KEYS` to the profile's `api_keys`, or to `default` when that is empty (`start-crispasr.ps1:185`), so servers started by the launcher require a key. Owner direction (2026-10-11): everything runs locally and the key is `default`; it carries little importance. The GUI sends the configured key (default `default`) on every request and sets the same variable for servers it starts.

## 3. Request parameters for `/v1/audio/transcriptions`

Parsing rules that apply to every field:

- cpp-httplib parses the multipart body while it reads it, before the handler checks the key (`FormDataParser`, `httplib.h:9201`): the body starts with `--<boundary>` and CRLF, each part ends with CRLF and `--<boundary>`, and the last delimiter is followed by `--`. Part header values may contain UTF-8 bytes (`is_field_content`, `httplib.h:10149`). The file name is the text between the quotes of `filename="..."`, without escape processing, and `filename*=UTF-8''<percent-encoded>` (RFC 5987) replaces it (`httplib.h:9306`). A body it cannot parse gets `400` before the handler runs, with the error handler's generic body (§5). `Expect: 100-continue` is answered before the body is read (`httplib.h:13558`).
- A field may arrive as a multipart part or as a query/form parameter. Values are trimmed of spaces and tabs, and an empty value counts as absent (`form_string`, 257-272).
- Each request starts from a copy of the server's startup parameters (`whisper_params rp = params`, 2074) and overrides only the fields present. An absent field keeps the profile's startup value; a boolean can be switched off per request (`vad=false`).
- Booleans accept `true`/`1`/`yes` and `false`/`0`/`no`, case-insensitive; any other value keeps the default (373-385).
- A number that does not parse completely (`0.5abc`) silently keeps the default (327-371). The server does not report such values, so the GUI validates them before sending.
- `language` is validated against the backend given by the startup `--backend` option: whisper accepts only its language table, and monolingual backends (moonshine, gigaam) accept only their language. Violations return `400` (403-423, 2045-2049).
- `model` is never read; the server always uses its loaded model (2027-2314).
- `response_format` must be one of `json`, `verbose_json`, `text`, `srt`, `vtt`, `diarized_json`, otherwise `400` (2058-2064).

### 3.1 Tier A: per-request parameters

| Field | Type / default | Notes |
|---|---|---|
| `file` | file, required | Audio or video; the server decodes it. Per the CLI help, as recorded in the original specification, wav/mp3/flac/ogg decode natively and m4a/aac/opus/webm/wma through the engine's internal ffmpeg. The part's file name matters: the server writes the upload to a temporary file that keeps the name's extension (1 to 8 letters or digits after the last dot), which steers the decoder's format detection (`write_temp_audio`, 172-189). The GUI sends the file's own name, with quotes, backslashes, and control characters replaced |
| `response_format` | string, `json` | The GUI uses `verbose_json` (§4) |
| `language` | string, startup `-l` | e.g. `ja`; `auto` triggers language identification |
| `prompt` | string | Initial prompt; applied only when non-empty (2181-2182) |
| `temperature`, `seed` | float, u64 | Startup defaults; `seed` matters only when `temperature > 0` |
| `max_new_tokens` / `max_tokens` | int | Same value; `max_tokens` is read last and wins (2053-2054) |
| `frequency_penalty` | float | |
| `beam_size`, `best_of` | int | Backend-dependent |
| `vad` | bool | Voice activity detection before slicing |
| `vad_threshold`, `vad_min_speech_duration_ms`, `vad_min_silence_duration_ms`, `vad_max_speech_duration_s`, `vad_speech_pad_ms` | float / int | VAD tuning (2093-2098) |
| `hotwords`, `hotwords_boost` | string / float | Backend-dependent |
| `suppress_regex`, `suppress_nst`, `grammar`, `grammar_rule` | string / bool | Decoder constraints, whisper-family |
| `sensitivity` | `conservative`, `balanced`, `aggressive` | Sets the four thresholds below; applied first so explicit thresholds in the same request win (2114-2129) |
| `entropy_thold`, `logprob_thold`, `no_speech_thold`, `temperature_inc`, `no_fallback` | float / bool | Decoder fallback |
| `chunk_seconds` | int | Fixed chunking when VAD is off; sending it marks it explicit (2138-2140) |
| `chunk_overlap`, `lcs_dedup`, `lcs_min_length`, `parakeet_decoder`, `att_context` | various | Long-audio stitching and backend specifics |
| `offset_t_ms`, `duration_ms` | int | Transcription window; returned timestamps stay in original-file time (§6) |
| `punctuation` | bool | Per-request punctuation toggle |
| `translate`, `source_lang`, `target_lang` | bool / string | Backend-dependent speech translation |
| `diarize`, `diarize_method`, `diarize_embedder`, `diarize_cluster_threshold`, `diarize_max_speakers` | various | Future pipeline step 2 |
| `detect_language`, `lid_backend`, `lid_model` | various | Language identification |
| `no_timestamps`, `word_thold`, `max_context`, `audio_ctx`, `carry_initial_prompt`, `no_auto_aligner`, `show_alternatives`, `alt_n`, `return_logits` | various | Less common decoder options |
| `vad_export`, `vad_export_raw`, `vad_import`, `vad_import_strict` | various | VAD boundary reuse across requests |
| `strict_pipeline`, `require_vad`, `require_word_timestamps`, `require_punctuation` | bool | A required stage that fails returns `400` instead of a degraded `200` |
| `max_len`, `split_on_word`, `split_on_punct` | int / bool | Parsed and passed to the backend (2142-2144), but the server's SRT and VTT formatters are called without them (2287-2289). They take effect only where a backend uses them while decoding, presumably whisper (`hypothesis`). The GUI applies subtitle shaping itself (ARCHITECTURE §2.8) |
| `stream` | bool | Server-sent events only for backends with streaming capability: text deltas without timestamps, no window support (2196-2253). Not used |

### 3.2 Tier B: startup-only parameters

These come from the profile through the server command line; sending them per request has no effect: `backend`, `model` (`-m`), `aligner` (`-am`), `force_aligner`, `device` (`CUDA_VISIBLE_DEVICES`), `api_keys`, `build` (which executable), `host`, `port`, `env`, thread count, and any `args[]` flag without a request field.

The owner's launcher is the authoritative profile-to-command mapping (`start-crispasr.ps1:162-221`):

```text
<root>\<build>\crispasr.exe --server --backend <backend> -m <root>\models\<model>
  [-am <root>\models\<aligner>]   only if the aligner file exists; otherwise warn and continue
  [-l <language>]                 if language is non-empty
  [--vad]                         if vad is true
  [--force-aligner]               if force_aligner is true
  [--chunk-seconds <n>]           if chunk_seconds is present, including 0
  [-bs <n>]                       if beam_size is non-zero
  [-tp <f>]                       if temperature > 0
  [--frequency-penalty <f>]       if frequency_penalty > 0
  [-n <n>]                        if max_new_tokens is non-zero
  [--seed <n>]                    if seed > 0
  [--no-punctuation]              if no_punctuation is true
  [-sp]                           if split_on_punct is true
  [args...]                       verbatim
  --host <host, default 127.0.0.1> --port <port, default 5114>
```

The launcher refuses to start when the executable or the model file is missing. Child environment: `CRISPASR_MODELS_DIR=<root>\models`, `CRISPASR_CACHE_DIR=<root>\cache`, `CRISPASR_SCRATCH_DIR=<root>\scratch`, `CRISPASR_API_KEYS=<api_keys or default>`, `CUDA_VISIBLE_DEVICES=<device>` (removed when `device` is empty), and every entry of the profile's `env` object. The GUI adds `TMP` and `TEMP` (§6, item 8). The launcher uses `--force-aligner`; the 0.8.42 CLI also accepts `-falign` for it (`examples/cli/cli.cpp`). The CLI reads every flag with the same rules as `src/core/cli_flags.cpp`: a flag that takes a value takes the next argument, and the last occurrence of a flag wins, so a flag in `args[]` overrides the same setting from a tuning key.

## 4. Response formats

- `verbose_json` (1163-1253 of `crispasr_output.cpp`): top-level `task`, `language`, `duration`, `text`, `segments`. `duration` is in seconds with three decimals and covers the processed audio after `offset_t_ms`/`duration_ms` trimming (609), so for a window request it is the part of the window inside the file: 4.696 s for `offset_t_ms=5000` and `duration_ms=10000` on a 9.696 s file (W4-3). `text` joins segment texts with single spaces. Each segment has `id`, `start` and `end` (seconds with two decimals, so centisecond precision), `text`, `temperature`, `avg_logprob`, `no_speech_prob` (always `0.0`), `speaker` only when diarization produced one (normalized to `A`, `B`, ...), `words` (`[{"word","start","end"}]`) when the backend or aligner produced word timings, and `tokens` when available. Times are in original-file time.
- `srt` (1124-1134 of `crispasr_output.cpp`, 2287): one cue per segment with non-empty text: index from 1, `HH:MM:SS,mmm --> HH:MM:SS,mmm`, the text, a blank line. A diarized segment's text is prefixed with its raw speaker label, such as `(speaker 0) ` (552-554). No `max_len` or `split_on_punct` shaping is applied. Content type `application/x-subrip; charset=utf-8`.
- `vtt`: the same cues after a `WEBVTT` header, with `.` before the milliseconds.
- `json`: `{"text":"..."}`. `text`: plain text.
- `diarized_json`: like `verbose_json`, with `speaker` always present (default `A`) and `chunk_id` on multi-chunk runs; speaker labels are local to a chunk (1279-1337 of `crispasr_output.cpp`).
- A window that starts past the end of the audio returns `200` with no segments and `duration` 0 (597-601).
- With `vad_export`, the server adds `vad_segments` to a JSON answer by re-serializing it with nlohmann::json: compact, keys sorted, numbers in their shortest form (488-498). The GUI's parser accepts both layouts and ignores members it does not use (`src/core/transcript.h`).

## 5. Errors

| Status | When | Body | GUI handling |
|---|---|---|---|
| `400` | Missing `file`, invalid `language` or `response_format`, and every transcription failure: undecodable or empty audio, strict-pipeline failures, malformed `vad_import` (2034-2064, 2259-2261); also a multipart body cpp-httplib cannot parse | `{"error":{"message":"...","type":"invalid_request_error"}}`, optionally with `code` and `param` (425-436); for an unparsable body the generic "not found" body | Show `message`; not retried |
| `401` | Missing or wrong API key | see §2 | Point to the API-key setting |
| `413` | Request body over 512 MiB (1547-1548). cpp-httplib stops reading once the body passes the limit (`httplib.h:11767-11770`) | The generic "not found" body: the 413 has an empty body, which the error handler fills (3919-3925; source reading). Because the rest of the body stays unread, a client that is still sending may see a reset connection instead of the answer (`hypothesis`) | The client checks the size of the whole body before sending and does not send one over the limit; not retried |
| `500` | Exception inside a handler (3931-3943) | `{"error":{"message":"internal error: ..."}}` | Retry once |
| `503` | Model swap through `/load` in progress (2030-2033) | `{"error":{"message":"model is still loading"}}` | Wait until `/health` is `ok`, then retry |
| `404` | Unknown route | `{"error":"not found. Use POST /v1/audio/transcriptions"}` (a different shape) | Treat as a client bug |
| no response | Connection refused or reset: the server is stopped, still loading at startup, or crashed | — | Check the process state (§6) |

Rely on the status code; the body shape is not uniform.

## 6. Lifecycle and resource behavior

1. The server loads the model and runs warmup before it creates its HTTP listener (1357-1487, then 1537 and 4025). During startup, connections are refused; `/health` never answers "loading" at that time.
2. `/health` answers `503 {"status":"loading"}` only while `/load` swaps the model (2331-2377).
3. The result of `svr.listen()` is ignored (4025), and the "listening on" line is printed before it (3948). When the port is taken, the process loads the model, prints that line, fails to bind, and returns 0 from `crispasr_run_server`, and `main` returns that value in server mode (`examples/cli/cli.cpp:2683-2685` of the 0.8.42 snapshot), so the process exits with code 0 (source reading; the deployed build is `not verified`). Detect a failed start by the process exiting, not by the log or the exit code.
4. One process serves one ASR model. `/load` swaps only model and backend (2363-2365), so a profile switch means a restart.
5. The server writes its log to stderr.
6. There is no cancel endpoint. A client that disconnects does not stop the transcription in progress, and the next request waits for the model lock (756). Check 10 of `api_probe` measures how long the deployed server stays busy after a cancelled request.
7. The multipart body is buffered in memory; the limit is 512 MB per request (1547).
8. On Windows, each upload is written to the directory returned by `GetTempPathA` (`TMP`, then `TEMP`, then the user profile) and deleted after decoding (190-207, 555-569). `CRISPASR_SCRATCH_DIR` applies only to the non-Windows branch (124-149, 209). The GUI therefore points `TMP` and `TEMP` of the child process at its app data directory ([AGENTS.md](../AGENTS.md#hard-rules) rule 4).
9. Every request decodes the whole upload before applying `offset_t_ms`/`duration_ms` (561-607), and timestamps are shifted back by `offset_t_ms` afterwards (1041-1057). A window request therefore re-uploads and re-decodes the full file.
10. If the GUI exits, the server keeps running; the next GUI start detects it through `/health` and adopts it. If the server dies, in-flight requests fail with connection errors.

## 7. Progress

- `GET /progress` returns `{"busy": bool, "progress": N}` and requires the API key (2403-2410).
- `busy` is true while any transcription holds a model lock. `progress` is the slice position of the most recent writer, 0..100, or -1 when idle (518-540).
- It advances when each VAD slice or fixed chunk starts (`i * 100 / slices`) and reads 100 after the last slice while post-processing runs (820-868).
- It is set only after the model lock is acquired (756-757). While a request is uploading, decoding, or running VAD, `/progress` still reports idle, or the previous job.
- The value is global: a request from another client also shows here. v1 sends one request at a time.

## 8. Pipeline step capability map (as of 2026-10-11)

| Step | Status | How | Gap |
|---|---|---|---|
| 1. Source separation | Built-in, untested | `POST /v1/audio/separation`, needs `--separate-model` at startup; mel-band-roformer or htdemucs weights placed manually | Weights not placed; behavior unverified |
| 2. Speaker diarization | Request fields exist, untested | `diarize=true` plus `diarize_method` on transcription requests | Output and quality unverified |
| 3. ASR to SRT | Working: the four Japanese profiles were tested through the CLI; the same engine serves HTTP | This GUI's core | — |
| 4. Events and tone to SRT | `sensevoice` backend compiled in, untested; event labels only through `/inference` | A second model or service; conversion of native JSON to SRT | Needs its own profile and verification |
| 5. Translation | External node working (Hy-MT2-30B-A3B at `127.0.0.1:5113`, OpenAI-compatible) | A separate HTTP client; not part of the CrispASR server | Future GUI work |
| 6. Merge SRTs | Not built | GUI-side processing | Designed in the pipeline phase |

## 9. Concurrency notes

- The HTTP layer accepts requests concurrently, but inference is serialized on one model instance unless the server runs worker instances, which are used only for "pure ASR" requests without aligner or post-processing (1584-1596, `reference/crispasr/concurrency.md`). The Japanese profiles use an aligner, so their requests always serialize. The GUI sends one transcription request at a time.
- Progress comes from `/progress` (§7) plus the GUI's own window count.

## 10. Corrections to the audit reports

The audit reports are kept unchanged in `reference/`; these corrections take precedence:

- `crispasr-audit.md` §4.1: with diarization, SRT text carries the raw label such as `(speaker 0) `, not `(A)`; only `verbose_json` and `diarized_json` normalize labels (`crispasr_output.cpp:552-554`, `1259-1277`).
- `crispasr-audit.md` §5.1: the launcher passes `--backend`, not `-b`, and passes no `-t`; §3.2 above is the mapping.
- `crispasr-audit.md` §5.2: `503 loading` occurs only during `/load`, never during startup (§6).
- An earlier SPEC pointed to "audit §3.5" for progress and ARCHITECTURE to "audit §3.6" for health; neither section exists. The audit covers both in §5.2; this document covers them in §1, §6, and §7.
- An earlier version of this document listed `/v1/audio/translations`; the route does not exist (§1).
- `eui-neo-audit.md` §3.2 suggests polling `/health` with EUI-NEO's `requestText`. That function starts only once per key, sends no headers, and on Windows without libcurl downloads through files in the system temp directory (`reference/eui-neo/core-platform/network.cpp:186`). The GUI uses its own client.
- `crisperweaver-audit.md` pattern P1 calls HTTP resume simpler. CrisperWeaver's checkpoint relies on segments streamed during transcription, which the HTTP endpoint does not provide; transcription windows take its place (ARCHITECTURE §2.5).
