#pragma once

#include "platform/command_line.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Platform seam for child processes, with Windows (process_win32.cpp) and POSIX
// (process_posix.cpp) implementations; process.cpp holds the platform-neutral part.
namespace crisp::platform {

struct ProcessSpec {
    std::filesystem::path executable;
    std::vector<std::string> arguments;          // UTF-8, without the program name
    std::filesystem::path workingDirectory;      // empty: the working directory of this process
    std::vector<EnvironmentChange> environment;  // applied in order to this process's environment
};

struct ExitStatus {
    bool signaled = false;  // POSIX: the process was ended by a signal
    std::int64_t code = 0;  // the exit code, or the signal number when |signaled|
};

// "exit code 0", "exit code 3221225477 (0xC0000005)", or "signal 9".
std::string DescribeExitStatus(const ExitStatus& status);

// A child process whose standard output and standard error share one pipe, with standard input on
// the null device. On Windows it has no console window; on POSIX it runs in its own process
// group. The child is not tied to this process: destroying the object closes the pipe and the
// handles but leaves the child running, so a server survives the GUI (docs/ARCHITECTURE.md,
// section 2.2). ReadOutput and TryWait belong to one thread; Terminate may run on another.
class ChildProcess {
public:
    ChildProcess();
    ~ChildProcess();
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    // Starts |spec|. On failure returns false and describes the problem in |error|.
    bool Start(const ProcessSpec& spec, std::string& error);

    // The process ID; 0 before a successful Start.
    long Id() const;

    // Waits up to |timeoutMs| for output and reads what is available, at most |capacity| bytes.
    // Returns the number of bytes read; sets |ended| once every writer has closed the pipe.
    std::size_t ReadOutput(char* buffer, std::size_t capacity, int timeoutMs, bool& ended);

    // True once the process has exited, with its |status|. Never blocks.
    bool TryWait(ExitStatus& status);

    // Ends the process: SIGTERM, or SIGKILL with |force|, on POSIX; TerminateProcess on Windows,
    // where |force| makes no difference. Does nothing once TryWait has seen the exit. Returns
    // false with |error| when the request failed.
    bool Terminate(bool force, std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// The process that listens on a local TCP port: how the GUI finds an adopted server to stop it.
struct PortOwner {
    long pid = 0;
    std::filesystem::path executable;  // empty when it could not be read
};

// Finds the process listening on TCP |port| on any local IPv4 or IPv6 address; |owner.pid| is 0
// when nothing listens. Returns false with |error| when the owner cannot be determined.
bool FindPortOwner(int port, PortOwner& owner, std::string& error);

// Ends |owner| if its executable is still |owner.executable|, then waits up to |timeoutMs| for it
// to exit. On POSIX it sends SIGTERM first and SIGKILL after half the time. Returns false with
// |error| when the process could not be ended or did not exit in time.
bool TerminatePortOwner(const PortOwner& owner, int timeoutMs, std::string& error);

// True when |a| and |b| name the same file name, compared as the platform compares names
// (without regard to case on Windows).
bool SameFileName(const std::filesystem::path& a, const std::filesystem::path& b);

}  // namespace crisp::platform
