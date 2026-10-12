#pragma once

#include "util/json.h"

#include <string>
#include <string_view>

// The comment syntax of CrispASR profiles (JSONC), after Remove-JsonComments in the owner's
// launcher (reference/crispasr/start-crispasr.ps1) and docs/ARCHITECTURE.md, section 2.1.
namespace crisp::core {

// Removes comments: outside string literals, "//" and "#" start a comment that runs to the end
// of the line. Inside a string, a backslash escapes the next character, so an escaped quote
// does not end the string, and "//" and "#" stay (profile URLs contain "//"). Line breaks stay,
// so line numbers in parse errors match the file. Like the launcher, which works line by line,
// a string never continues past the end of its line.
//
// Unlike the launcher, escapes work: its escape branch compares a character with the two-
// character literal '\\' and never fires, so a string containing \" followed by // breaks there.
std::string StripJsonComments(std::string_view text);

// StripJsonComments, then strict JSON.
util::JsonParseResult ParseJsonc(std::string_view text);

}  // namespace crisp::core
