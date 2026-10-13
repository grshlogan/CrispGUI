#include "check.h"
#include "test_support.h"

#include "core/srt_writer.h"
#include "core/transcript.h"
#include "platform/files.h"

#include <cstdint>
#include <string>
#include <vector>

using crisp::core::ParseVerboseJson;
using crisp::core::SegmentsToSrt;
using crisp::core::SrtSpeakerPrefix;
using crisp::core::SrtTimestamp;
using crisp::core::Transcript;
using crisp::core::TranscriptSegment;

namespace {

TranscriptSegment Segment(std::int64_t startMs, std::int64_t endMs, const std::string& text,
                          const std::string& speaker = std::string()) {
    TranscriptSegment segment;
    segment.startMs = startMs;
    segment.endMs = endMs;
    segment.text = text;
    segment.speaker = speaker;
    return segment;
}

// The bytes of a file recorded from the deployed server (tests/data/README.md).
bool ReadRecorded(const char* name, std::string& bytes) {
    const auto file =
        crisp::platform::ReadFileBytes(crisp::test::TestDataDirectory() / "deployed-parakeet" / name, 1 << 20);
    bytes = file.bytes;
    return file.Ok();
}

// A recorded verbose_json answer, parsed.
bool ParseRecorded(const char* name, Transcript& transcript) {
    std::string bytes;
    std::string error;
    return ReadRecorded(name, bytes) && ParseVerboseJson(bytes, transcript, error);
}

}  // namespace

int main() {
    // The server's own answers (tests/data/README.md). The whole sample: the writer's output for the
    // recorded verbose_json equals the server's srt for the same audio, byte for byte.
    {
        Transcript whole;
        std::string serverSrt;
        CHECK(ParseRecorded("transcript.verbose.json", whole));
        CHECK(ReadRecorded("transcript.srt", serverSrt));
        CHECK(!serverSrt.empty());
        CHECK(SegmentsToSrt(whole.segments) == serverSrt);
    }
    // The window of the sample: times in the file's time, one cue.
    {
        Transcript window;
        CHECK(ParseRecorded("window.verbose.json", window));
        CHECK(window.segments.size() == 1);
        if (window.segments.size() == 1) {
            CHECK(SegmentsToSrt(window.segments) ==
                  "1\n00:00:05,070 --> 00:00:08,710\n" + window.segments[0].text + "\n\n");
        }
    }

    // Cues are numbered from 1 in the order given, and each ends with an empty line.
    {
        const std::vector<TranscriptSegment> segments = {Segment(0, 1500, "a"), Segment(1500, 3000, "b"),
                                                         Segment(3000, 4500, "c")};
        const std::string srt = SegmentsToSrt(segments);
        CHECK(srt ==
              "1\n00:00:00,000 --> 00:00:01,500\na\n\n"
              "2\n00:00:01,500 --> 00:00:03,000\nb\n\n"
              "3\n00:00:03,000 --> 00:00:04,500\nc\n\n");
        CHECK(srt.find('\r') == std::string::npos);
        CHECK(srt.compare(0, 3, "\xEF\xBB\xBF") != 0);
    }
    // A segment without text gives no cue and leaves no gap in the numbering.
    {
        CHECK(SegmentsToSrt({Segment(0, 1000, "a"), Segment(1000, 2000, ""), Segment(2000, 3000, "b")}) ==
              "1\n00:00:00,000 --> 00:00:01,000\na\n\n"
              "2\n00:00:02,000 --> 00:00:03,000\nb\n\n");
        CHECK(SegmentsToSrt({Segment(0, 1000, ""), Segment(1000, 2000, "x")}) ==
              "1\n00:00:01,000 --> 00:00:02,000\nx\n\n");
        CHECK(SegmentsToSrt({Segment(0, 1000, ""), Segment(1000, 2000, "")}).empty());
    }
    // No segments, no text.
    CHECK(SegmentsToSrt({}).empty());
    // The text is written as it is: spaces, line feeds, and UTF-8 stay.
    {
        const std::string text = " \xE3\x81\x82 a\nb ";
        CHECK(SegmentsToSrt({Segment(0, 10, text)}) == "1\n00:00:00,000 --> 00:00:00,010\n" + text + "\n\n");
        CHECK(SegmentsToSrt({Segment(0, 10, " ")}) == "1\n00:00:00,000 --> 00:00:00,010\n \n\n");
    }

    // Times: two digits for the hours at least, two for minutes and seconds, three for the milliseconds.
    CHECK(SrtTimestamp(0) == "00:00:00,000");
    CHECK(SrtTimestamp(1) == "00:00:00,001");
    CHECK(SrtTimestamp(999) == "00:00:00,999");
    CHECK(SrtTimestamp(1000) == "00:00:01,000");
    CHECK(SrtTimestamp(1234) == "00:00:01,234");
    CHECK(SrtTimestamp(59999) == "00:00:59,999");
    CHECK(SrtTimestamp(60000) == "00:01:00,000");
    CHECK(SrtTimestamp(3599999) == "00:59:59,999");
    CHECK(SrtTimestamp(3600000) == "01:00:00,000");
    CHECK(SrtTimestamp(3723450) == "01:02:03,450");
    CHECK(SrtTimestamp(86399999) == "23:59:59,999");
    CHECK(SrtTimestamp(359999999) == "99:59:59,999");
    CHECK(SrtTimestamp(360000000) == "100:00:00,000");
    CHECK(SrtTimestamp(360000001) == "100:00:00,001");
    CHECK(SrtTimestamp(36000000000LL) == "10000:00:00,000");
    // A negative time, which a server never sends, is written as zero.
    CHECK(SrtTimestamp(-1) == "00:00:00,000");
    CHECK(SrtTimestamp(-3723450) == "00:00:00,000");
    CHECK(SegmentsToSrt({Segment(-10, 3723450, "t")}) == "1\n00:00:00,000 --> 01:02:03,450\nt\n\n");

    // Speaker labels: verbose_json carries "B" where the server's srt carries "(speaker 1) ".
    CHECK(SegmentsToSrt({Segment(0, 1000, "hello", "B")}) == "1\n00:00:00,000 --> 00:00:01,000\n(speaker 1) hello\n\n");
    CHECK(SegmentsToSrt({Segment(0, 1000, "hello", "")}) == "1\n00:00:00,000 --> 00:00:01,000\nhello\n\n");
    CHECK(SrtSpeakerPrefix("").empty());
    CHECK(SrtSpeakerPrefix("A") == "(speaker 0) ");
    CHECK(SrtSpeakerPrefix("B") == "(speaker 1) ");
    CHECK(SrtSpeakerPrefix("Z") == "(speaker 25) ");
    // From 26 up the server writes two letters: the first is the number divided by 26 as a letter,
    // the second the remainder.
    CHECK(SrtSpeakerPrefix("BA") == "(speaker 26) ");
    CHECK(SrtSpeakerPrefix("BB") == "(speaker 27) ");
    CHECK(SrtSpeakerPrefix("CA") == "(speaker 52) ");
    CHECK(SrtSpeakerPrefix("ZZ") == "(speaker 675) ");
    // Labels that no number gives keep their name in the shape of a participant's label.
    CHECK(SrtSpeakerPrefix("AB") == "(AB) ");
    CHECK(SrtSpeakerPrefix("a") == "(a) ");
    CHECK(SrtSpeakerPrefix("B1") == "(B1) ");
    CHECK(SrtSpeakerPrefix("ABC") == "(ABC) ");
    CHECK(SrtSpeakerPrefix("Mustermann") == "(Mustermann) ");
    return crisp::test::Result();
}
