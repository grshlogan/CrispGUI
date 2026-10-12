#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

// The server's output as the log pane shows it (docs/ARCHITECTURE.md, section 2.2).
namespace crisp::core {

// Cleans one line of process output for display: drops carriage returns at its end and keeps
// only the text after a carriage return inside it (progress output that redraws a line), removes
// terminal escape sequences and control characters, turns tabs into spaces, and replaces invalid
// UTF-8 with U+FFFD.
std::string CleanLogLine(std::string_view raw);

// Turns a process's output bytes into display lines as they arrive. Lines end at line feeds; a
// line longer than |maxLineBytes| is broken at a character boundary.
class LogSplitter {
public:
    explicit LogSplitter(std::size_t maxLineBytes = 2000) : maxLineBytes_(maxLineBytes) {}

    // Adds |bytes| and appends the lines they complete to |lines|.
    void Feed(std::string_view bytes, std::vector<std::string>& lines);
    // Appends the unfinished last line, if any, to |lines|.
    void Flush(std::vector<std::string>& lines);

private:
    std::size_t maxLineBytes_;
    std::string pending_;
};

// The most recent lines, oldest first. The UI thread owns it.
class LogBuffer {
public:
    explicit LogBuffer(std::size_t capacity = 5000) : capacity_(capacity) {}

    void Append(std::string line);
    void Append(std::vector<std::string> lines);
    void Clear();

    std::size_t Size() const { return lines_.size(); }
    const std::string& At(std::size_t index) const { return lines_[index]; }
    // Changes with every append and clear, so a view knows when to redraw.
    std::uint64_t Version() const { return version_; }

private:
    std::size_t capacity_;
    std::deque<std::string> lines_;
    std::uint64_t version_ = 0;
};

}  // namespace crisp::core
