#pragma once

#include <string>
#include <string_view>
#include <vector>

// Platform-neutral helpers for starting processes: Windows command-line quoting and environment
// changes. They are plain string functions, so the Linux tests cover the Windows rules too.
namespace crisp::platform {

// One change to a child's environment: set |name| to |value|, or remove |name|.
struct EnvironmentChange {
    std::string name;
    std::string value;
    bool remove = false;
};

// Quotes |argument| so that a program reading its command line with the Microsoft C runtime's
// rules (CommandLineToArgvW) gets it back unchanged: arguments without spaces, tabs, or quotes
// stay as they are; others are enclosed in quotes, with backslashes doubled where they precede
// a quote.
std::string QuoteWindowsArgument(std::string_view argument);

// The command line CreateProcessW receives: |program| in quotes, then each argument quoted as
// needed, separated by spaces.
std::string BuildWindowsCommandLine(std::string_view program, const std::vector<std::string>& arguments);

// Applies |changes| in order to |environment|, a list of NAME=value entries, and returns the
// result sorted by name. With |ignoreCase|, as on Windows, names match and sort without regard to
// ASCII case. Entries whose name starts with '=' (the per-drive directories of Windows) are kept
// and never match a change.
std::vector<std::string> ApplyEnvironmentChanges(std::vector<std::string> environment,
                                                 const std::vector<EnvironmentChange>& changes, bool ignoreCase);

}  // namespace crisp::platform
