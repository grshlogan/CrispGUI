#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// A transcription result as CrispASR's verbose_json carries it (docs/API-CRISPASR.md, section 4),
// with times in whole milliseconds. The server writes segment and word times with two decimals and
// the duration with three, so the conversion is exact.
namespace crisp::core {

struct TranscriptWord {
    std::string word;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
};

struct TranscriptSegment {
    std::int64_t id = 0;
    std::int64_t startMs = 0;  // in the time of the whole file, also for a window request
    std::int64_t endMs = 0;
    std::string text;
    std::string speaker;                // "A", "B", ... after diarization; empty otherwise
    std::vector<TranscriptWord> words;  // empty when neither the backend nor the aligner timed words
};

struct Transcript {
    std::string task;  // "transcribe" or "translate"
    std::string language;
    // The processed audio: the window's length for a window request, 0 for a window that starts
    // past the end of the file.
    std::int64_t durationMs = 0;
    std::string text;
    std::vector<TranscriptSegment> segments;
};

// Reads a verbose_json body. Members the GUI does not use (temperature, avg_logprob, tokens, and
// whatever a newer server adds) are ignored; a used member that is missing or of the wrong type
// is an error.
bool ParseVerboseJson(std::string_view body, Transcript& transcript, std::string& error);

// A JSON number of seconds, as written, in whole milliseconds: exact up to three decimals, and
// rounded half away from zero beyond. False for text that is not a JSON number or is out of
// range (more than 10^15 ms).
bool SecondsToMilliseconds(std::string_view text, std::int64_t& milliseconds);

}  // namespace crisp::core
