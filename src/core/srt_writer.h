#pragma once

#include "core/transcript.h"

#include <cstdint>
#include <string>
#include <vector>

// The SRT writer (docs/ARCHITECTURE.md, section 2.8). The GUI asks the server for verbose_json and
// writes the SRT itself. Without subtitle shaping its text equals the server's own `srt` answer for
// the same segments, byte for byte (docs/SPEC.md, FR-3). The format is that of CrispASR 0.8.42's
// crispasr_segments_to_srt; the code is written from the description of the format.
namespace crisp::core {

// "HH:MM:SS,mmm" for a time in whole milliseconds: two digits for the hours at least (more above
// 99 hours), two for the minutes and the seconds, three for the milliseconds. A negative time,
// which a server never sends, is written as zero.
std::string SrtTimestamp(std::int64_t milliseconds);

// The text that comes before a cue's words for the speaker label of verbose_json. The server's srt
// carries the raw label, "(speaker N) ", where verbose_json carries the normalized one: "A" for 0
// up to "Z" for 25, then two letters, "BA" for 26. The prefix inverts that. Any other label gets
// "(<label>) ", the shape of the raw labels that name a participant, and no label gets nothing.
std::string SrtSpeakerPrefix(const std::string& label);

// One cue per segment whose text is not empty, in the order given, numbered from 1 over the cues
// written: the number, the two times, the text behind its speaker prefix, and an empty line, each
// line ended by a line feed. The text is written as it is, in UTF-8, without a byte order mark and
// without trimming. No segments, or none with text, give an empty string.
std::string SegmentsToSrt(const std::vector<TranscriptSegment>& segments);

}  // namespace crisp::core
