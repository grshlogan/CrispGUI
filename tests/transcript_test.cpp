#include "check.h"
#include "test_support.h"

#include "core/transcript.h"
#include "platform/files.h"

#include <cstdint>
#include <string>

using crisp::core::ParseVerboseJson;
using crisp::core::SecondsToMilliseconds;
using crisp::core::Transcript;

namespace {

bool Converts(const char* text, std::int64_t expected) {
    std::int64_t milliseconds = -12345;
    return SecondsToMilliseconds(text, milliseconds) && milliseconds == expected;
}

bool Rejects(const char* text) {
    std::int64_t milliseconds = 0;
    return !SecondsToMilliseconds(text, milliseconds);
}

// The error ParseVerboseJson reports for |body|; empty when it parses.
std::string ErrorOf(const std::string& body) {
    Transcript transcript;
    std::string error;
    return ParseVerboseJson(body, transcript, error) ? std::string() : error;
}

}  // namespace

int main() {
    // Seconds as the server writes them, exactly; more decimals round half away from zero.
    CHECK(Converts("0", 0));
    CHECK(Converts("0.00", 0));
    CHECK(Converts("3.48", 3480));
    CHECK(Converts("7.1", 7100));
    CHECK(Converts("12.345", 12345));
    CHECK(Converts("12.3456", 12346));
    CHECK(Converts("12.3454", 12345));
    CHECK(Converts("0.0005", 1));
    CHECK(Converts("0.00049", 0));
    CHECK(Converts("-0.0005", -1));
    CHECK(Converts("-0.01", -10));
    CHECK(Converts("1e2", 100000));
    CHECK(Converts("2.5E-1", 250));
    CHECK(Converts("1e-400", 0));
    CHECK(Converts("999999999999.999", 999999999999999));
    CHECK(Rejects("1000000000000"));
    CHECK(Rejects("1e400"));
    for (const char* text : {"", "01", "1.", ".5", "1e", "1e+", "abc", "1 ", "+1", "-", "0x10", "1.2.3"}) {
        CHECK(Rejects(text));
    }

    // The fixture written in the layout of crispasr_output.cpp:1163-1253.
    const auto fixture =
        crisp::platform::ReadFileBytes(crisp::test::TestDataDirectory() / "verbose-synthetic.json", 1 << 20);
    CHECK(fixture.Ok());
    Transcript transcript;
    std::string error;
    CHECK(ParseVerboseJson(fixture.bytes, transcript, error));
    CHECK(error.empty());
    CHECK(transcript.task == "transcribe");
    CHECK(transcript.language == "ja");
    CHECK(transcript.durationMs == 12345);
    CHECK(transcript.text == "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x81\xAF\xE3\x81\x84\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97"
                             "\xE3\x81\xA7\xE3\x81\x99\xE3\x81\xAD\xE3\x80\x82 \"\xE5\xBC\x95\xE7\x94\xA8\"\xE3\x81\xA8"
                             "\\\xE8\xA8\x98\xE5\x8F\xB7 \xE6\x9C\x80\xE5\xBE\x8C\xE3\x81\xAE\xE6\x96\x87\xE3\x80\x82");
    CHECK(transcript.segments.size() == 3);
    if (transcript.segments.size() == 3) {
        const auto& first = transcript.segments[0];
        CHECK(first.id == 0 && first.startMs == 0 && first.endMs == 3480);
        CHECK(first.speaker.empty());
        CHECK(first.words.size() == 3);
        if (first.words.size() == 3) {
            CHECK(first.words[1].word == "\xE3\x81\x84\xE3\x81\x84");
            CHECK(first.words[1].startMs == 920 && first.words[1].endMs == 1500);
            CHECK(first.words[2].endMs == 3480);
        }
        const auto& second = transcript.segments[1];
        CHECK(second.id == 1 && second.startMs == 3480 && second.endMs == 7100);
        CHECK(second.text == "\"\xE5\xBC\x95\xE7\x94\xA8\"\xE3\x81\xA8\\\xE8\xA8\x98\xE5\x8F\xB7");
        CHECK(second.speaker == "A");
        CHECK(second.words.empty());
        CHECK(transcript.segments[2].startMs == 7100 && transcript.segments[2].endMs == 12340);
    }

    // A window past the end of the audio: no segments and duration 0. Unknown members, such as
    // the vad_segments that a vad_export request adds, are ignored.
    CHECK(ErrorOf("{\"duration\": 0.000, \"segments\": [], \"vad_segments\": [{\"start\": 0}], \"x\": {\"y\": [1]}}")
              .empty());
    // The same answer after the server re-serializes it with nlohmann::json (sorted keys, shortest
    // numbers), which it does when it adds vad_segments (crispasr_server.cpp:488-498).
    {
        Transcript compact;
        CHECK(ParseVerboseJson("{\"duration\":6.0,\"language\":\"ja\",\"segments\":[{\"avg_logprob\":0.0,\"end\":2.5,"
                               "\"id\":0,\"no_speech_prob\":0.0,\"start\":0.0,\"temperature\":0.0,\"text\":\"a\"}],"
                               "\"task\":\"transcribe\",\"text\":\"a\",\"vad_segments\":[]}",
                               compact, error));
        CHECK(compact.durationMs == 6000 && compact.segments.size() == 1 && compact.segments[0].endMs == 2500);
    }

    // Answers that are not verbose_json.
    CHECK(ErrorOf("not json").rfind("The answer is not JSON: ", 0) == 0);
    CHECK(ErrorOf("[1]") == "The answer is not a JSON object.");
    CHECK(ErrorOf("{\"text\": \"only text\"}") == "The answer has no number \"duration\".");
    CHECK(ErrorOf("{\"duration\": 1.0}") == "The answer has no array \"segments\".");
    CHECK(ErrorOf("{\"duration\": 1.0, \"segments\": {}}") == "The answer has no array \"segments\".");
    CHECK(ErrorOf("{\"duration\": \"1.0\", \"segments\": []}") == "The answer has no number \"duration\".");
    CHECK(ErrorOf("{\"duration\": 1.0, \"language\": 5, \"segments\": []}") ==
          "The answer: \"language\" is not a string.");
    CHECK(ErrorOf("{\"duration\": 1.0, \"segments\": [5]}") == "Segment 0 is not an object.");
    CHECK(ErrorOf("{\"duration\": 1.0, \"segments\": [{\"end\": 1.0, \"text\": \"a\"}]}") ==
          "Segment 0 has no number \"start\".");
    CHECK(ErrorOf("{\"duration\": 1.0, \"segments\": [{\"start\": 0, \"end\": 1.0}]}") ==
          "Segment 0 has no \"text\".");
    CHECK(ErrorOf("{\"duration\": 1.0, \"segments\": [{\"id\": 1.5, \"start\": 0, \"end\": 1, \"text\": \"a\"}]}") ==
          "Segment 0: \"id\" is not a whole number.");
    CHECK(ErrorOf("{\"duration\": 1.0, \"segments\": [{\"start\": 0, \"end\": 1, \"text\": \"a\", \"words\": 3}]}") ==
          "Segment 0: \"words\" is not an array.");
    CHECK(ErrorOf("{\"duration\": 1.0, \"segments\": [{\"start\": 0, \"end\": 1, \"text\": \"a\", "
                  "\"words\": [{\"word\": \"a\", \"start\": 0}]}]}") == "Segment 0, word 0 has no number \"end\".");
    CHECK(ErrorOf("{\"duration\": 1e13, \"segments\": []}") == "The answer: \"duration\" is out of range.");
    return crisp::test::Result();
}
