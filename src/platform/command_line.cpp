#include "platform/command_line.h"

#include <algorithm>
#include <cstddef>

namespace crisp::platform {
namespace {

char ToUpperAscii(char c) {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

// The name of a NAME=value entry. A leading '=' belongs to the name ("=C:=C:\\work").
std::string_view EntryName(std::string_view entry) {
    const std::size_t equals = entry.find('=', 1);
    return entry.substr(0, equals == std::string_view::npos ? entry.size() : equals);
}

bool NamesEqual(std::string_view a, std::string_view b, bool ignoreCase) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char left = ignoreCase ? ToUpperAscii(a[i]) : a[i];
        const char right = ignoreCase ? ToUpperAscii(b[i]) : b[i];
        if (left != right) {
            return false;
        }
    }
    return true;
}

bool NameLess(std::string_view a, std::string_view b, bool ignoreCase) {
    const std::size_t length = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < length; ++i) {
        const auto left = static_cast<unsigned char>(ignoreCase ? ToUpperAscii(a[i]) : a[i]);
        const auto right = static_cast<unsigned char>(ignoreCase ? ToUpperAscii(b[i]) : b[i]);
        if (left != right) {
            return left < right;
        }
    }
    return a.size() < b.size();
}

}  // namespace

std::string QuoteWindowsArgument(std::string_view argument) {
    if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(argument);
    }
    std::string quoted = "\"";
    for (std::size_t i = 0;; ++i) {
        std::size_t backslashes = 0;
        while (i < argument.size() && argument[i] == '\\') {
            ++backslashes;
            ++i;
        }
        if (i == argument.size()) {
            // Before the closing quote, every backslash needs an escape.
            quoted.append(backslashes * 2, '\\');
            break;
        }
        if (argument[i] == '"') {
            quoted.append(backslashes * 2 + 1, '\\');
            quoted.push_back('"');
        } else {
            quoted.append(backslashes, '\\');
            quoted.push_back(argument[i]);
        }
    }
    quoted.push_back('"');
    return quoted;
}

std::string BuildWindowsCommandLine(std::string_view program, const std::vector<std::string>& arguments) {
    // The C runtime reads the program name up to the closing quote, without escapes, and a
    // Windows path cannot contain a quote.
    std::string commandLine = "\"" + std::string(program) + "\"";
    for (const std::string& argument : arguments) {
        commandLine.push_back(' ');
        commandLine += QuoteWindowsArgument(argument);
    }
    return commandLine;
}

std::vector<std::string> ApplyEnvironmentChanges(std::vector<std::string> environment,
                                                 const std::vector<EnvironmentChange>& changes, bool ignoreCase) {
    for (const EnvironmentChange& change : changes) {
        environment.erase(std::remove_if(environment.begin(), environment.end(),
                                         [&](const std::string& entry) {
                                             const std::string_view name = EntryName(entry);
                                             return !name.empty() && name[0] != '=' &&
                                                    NamesEqual(name, change.name, ignoreCase);
                                         }),
                          environment.end());
        if (!change.remove) {
            environment.push_back(change.name + "=" + change.value);
        }
    }
    std::stable_sort(environment.begin(), environment.end(), [ignoreCase](const std::string& a, const std::string& b) {
        return NameLess(EntryName(a), EntryName(b), ignoreCase);
    });
    return environment;
}

}  // namespace crisp::platform
