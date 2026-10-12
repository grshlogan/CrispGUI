#include "platform/process.h"

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

extern char** environ;

namespace crisp::platform {
namespace {

// strerror_r comes in a GNU and an XSI flavor; these overloads accept either result.
[[maybe_unused]] const char* ErrorText(const char* returned, const char*) {
    return returned;
}
[[maybe_unused]] const char* ErrorText(int, const char* buffer) {
    return buffer;
}

std::string ErrnoText(int error) {
    char buffer[256] = {};
    return std::string(ErrorText(strerror_r(error, buffer, sizeof(buffer)), buffer)) + " (errno " +
           std::to_string(error) + ")";
}

std::string ReadLink(const std::string& path) {
    char buffer[4096];
    const ssize_t length = readlink(path.c_str(), buffer, sizeof(buffer) - 1);
    return length > 0 ? std::string(buffer, static_cast<std::size_t>(length)) : std::string();
}

// False once |pid| has exited, also while it waits as a zombie for its parent.
bool IsRunning(long pid) {
    const std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::FILE* file = std::fopen(path.c_str(), "r");
    if (file == nullptr) {
        return false;
    }
    char buffer[1024];
    const std::size_t length = std::fread(buffer, 1, sizeof(buffer), file);
    std::fclose(file);
    // "<pid> (<name>) <state> ...", where the name may itself contain parentheses.
    const std::string text(buffer, length);
    const std::size_t paren = text.rfind(')');
    if (paren == std::string::npos || paren + 2 >= text.size()) {
        return false;
    }
    const char state = text[paren + 2];
    return state != 'Z' && state != 'X';
}

bool WaitUntilGone(long pid, std::chrono::steady_clock::time_point deadline) {
    while (IsRunning(pid)) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return true;
}

// The socket inode of a TCP listener on |port| in /proc/net/tcp or /proc/net/tcp6.
bool FindListeningInode(int port, unsigned long& inode) {
    for (const char* table : {"/proc/net/tcp", "/proc/net/tcp6"}) {
        std::FILE* file = std::fopen(table, "r");
        if (file == nullptr) {
            continue;
        }
        char line[512];
        bool header = true;
        bool found = false;
        while (!found && std::fgets(line, sizeof(line), file) != nullptr) {
            if (header) {
                header = false;
                continue;
            }
            char local[128] = {};
            unsigned state = 0;
            unsigned long value = 0;
            if (std::sscanf(line, "%*s %127s %*s %x %*s %*s %*s %*s %*s %lu", local, &state, &value) != 3) {
                continue;
            }
            const char* colon = std::strrchr(local, ':');
            if (colon != nullptr && state == 0x0A && std::strtol(colon + 1, nullptr, 16) == port) {
                inode = value;
                found = true;
            }
        }
        std::fclose(file);
        if (found) {
            return true;
        }
    }
    return false;
}

// The process holding the socket with |inode|, among the processes this user may inspect.
long FindSocketOwner(unsigned long inode) {
    const std::string target = "socket:[" + std::to_string(inode) + "]";
    DIR* processes = opendir("/proc");
    if (processes == nullptr) {
        return 0;
    }
    long owner = 0;
    while (owner == 0) {
        const dirent* entry = readdir(processes);
        if (entry == nullptr) {
            break;
        }
        char* end = nullptr;
        const long pid = std::strtol(entry->d_name, &end, 10);
        if (pid <= 0 || *end != '\0') {
            continue;
        }
        const std::string fdDirectory = "/proc/" + std::string(entry->d_name) + "/fd";
        DIR* descriptors = opendir(fdDirectory.c_str());
        if (descriptors == nullptr) {
            continue;
        }
        while (const dirent* descriptor = readdir(descriptors)) {
            if (descriptor->d_name[0] != '.' && ReadLink(fdDirectory + "/" + descriptor->d_name) == target) {
                owner = pid;
                break;
            }
        }
        closedir(descriptors);
    }
    closedir(processes);
    return owner;
}

}  // namespace

struct ChildProcess::Impl {
    pid_t pid = 0;
    int output = -1;
    std::mutex mutex;  // orders TryWait's reaping before Terminate, so a reused PID is never signaled
    bool reaped = false;
    ExitStatus status;
};

ChildProcess::ChildProcess() : impl_(new Impl) {}

ChildProcess::~ChildProcess() {
    if (impl_->output >= 0) {
        close(impl_->output);
    }
}

bool ChildProcess::Start(const ProcessSpec& spec, std::string& error) {
    if (impl_->pid != 0) {
        error = "The process was already started.";
        return false;
    }
    int pipeEnds[2];
    if (pipe2(pipeEnds, O_CLOEXEC) != 0) {
        error = "Could not create a pipe: " + ErrnoText(errno);
        return false;
    }
    const int nullDevice = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (nullDevice < 0) {
        error = "Could not open /dev/null: " + ErrnoText(errno);
        close(pipeEnds[0]);
        close(pipeEnds[1]);
        return false;
    }

    std::vector<std::string> inherited;
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        inherited.emplace_back(*entry);
    }
    std::vector<std::string> environment = ApplyEnvironmentChanges(std::move(inherited), spec.environment, false);
    std::vector<char*> envp;
    for (std::string& entry : environment) {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);

    std::string program = spec.executable.string();
    std::vector<std::string> arguments;
    arguments.push_back(program);
    arguments.insert(arguments.end(), spec.arguments.begin(), spec.arguments.end());
    std::vector<char*> argv;
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, nullDevice, STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipeEnds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipeEnds[1], STDERR_FILENO);
    const std::string directory = spec.workingDirectory.string();
    if (!directory.empty()) {
        posix_spawn_file_actions_addchdir_np(&actions, directory.c_str());
    }

    // A clean signal state, and an own process group so that a terminal's Ctrl+C for the GUI
    // does not reach the server.
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    sigset_t noSignals;
    sigemptyset(&noSignals);
    sigset_t defaultSignals;
    sigfillset(&defaultSignals);
    sigdelset(&defaultSignals, SIGKILL);
    sigdelset(&defaultSignals, SIGSTOP);
    posix_spawnattr_setsigmask(&attributes, &noSignals);
    posix_spawnattr_setsigdefault(&attributes, &defaultSignals);
    posix_spawnattr_setpgroup(&attributes, 0);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETPGROUP);

    pid_t pid = 0;
    const int result = posix_spawn(&pid, program.c_str(), &actions, &attributes, argv.data(), envp.data());
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    close(pipeEnds[1]);
    close(nullDevice);
    if (result != 0) {
        close(pipeEnds[0]);
        error = "Could not start " + program + ": " + ErrnoText(result);
        return false;
    }
    impl_->pid = pid;
    impl_->output = pipeEnds[0];
    return true;
}

long ChildProcess::Id() const {
    return static_cast<long>(impl_->pid);
}

std::size_t ChildProcess::ReadOutput(char* buffer, std::size_t capacity, int timeoutMs, bool& ended) {
    ended = false;
    if (impl_->output < 0) {
        ended = true;
        return 0;
    }
    pollfd descriptor = {impl_->output, POLLIN, 0};
    int ready = 0;
    do {
        ready = poll(&descriptor, 1, timeoutMs);
    } while (ready < 0 && errno == EINTR);
    if (ready <= 0) {
        return 0;
    }
    ssize_t count = 0;
    do {
        count = read(impl_->output, buffer, capacity);
    } while (count < 0 && errno == EINTR);
    if (count > 0) {
        return static_cast<std::size_t>(count);
    }
    if (count == 0 || errno != EAGAIN) {
        ended = true;
    }
    return 0;
}

bool ChildProcess::TryWait(ExitStatus& status) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->pid == 0) {
        return false;
    }
    if (!impl_->reaped) {
        int raw = 0;
        pid_t result = 0;
        do {
            result = waitpid(impl_->pid, &raw, WNOHANG);
        } while (result < 0 && errno == EINTR);
        if (result == impl_->pid) {
            impl_->reaped = true;
            impl_->status = WIFSIGNALED(raw) ? ExitStatus{true, WTERMSIG(raw)} : ExitStatus{false, WEXITSTATUS(raw)};
        } else if (result < 0 && errno == ECHILD) {
            // Reaped elsewhere (SIGCHLD ignored); the status is unknown.
            impl_->reaped = true;
            impl_->status = ExitStatus{false, -1};
        }
    }
    if (impl_->reaped) {
        status = impl_->status;
    }
    return impl_->reaped;
}

bool ChildProcess::Terminate(bool force, std::string& error) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->pid == 0 || impl_->reaped) {
        return true;
    }
    if (kill(impl_->pid, force ? SIGKILL : SIGTERM) != 0 && errno != ESRCH) {
        error = "Could not signal process " + std::to_string(impl_->pid) + ": " + ErrnoText(errno);
        return false;
    }
    return true;
}

bool FindPortOwner(int port, PortOwner& owner, std::string& error) {
    owner = PortOwner();
    unsigned long inode = 0;
    if (!FindListeningInode(port, inode)) {
        return true;
    }
    const long pid = FindSocketOwner(inode);
    if (pid == 0) {
        error = "The process listening on port " + std::to_string(port) + " belongs to another user.";
        return false;
    }
    owner.pid = pid;
    owner.executable = ReadLink("/proc/" + std::to_string(pid) + "/exe");
    return true;
}

bool TerminatePortOwner(const PortOwner& owner, int timeoutMs, std::string& error) {
    const std::string pid = std::to_string(owner.pid);
    if (ReadLink("/proc/" + pid + "/exe") != owner.executable.string()) {
        error = "Process " + pid + " is no longer " + owner.executable.string() + ".";
        return false;
    }
    const auto start = std::chrono::steady_clock::now();
    if (kill(static_cast<pid_t>(owner.pid), SIGTERM) != 0 && errno != ESRCH) {
        error = "Could not signal process " + pid + ": " + ErrnoText(errno);
        return false;
    }
    if (WaitUntilGone(owner.pid, start + std::chrono::milliseconds(timeoutMs / 2))) {
        return true;
    }
    kill(static_cast<pid_t>(owner.pid), SIGKILL);
    if (WaitUntilGone(owner.pid, start + std::chrono::milliseconds(timeoutMs))) {
        return true;
    }
    error = "Process " + pid + " did not exit within " + std::to_string(timeoutMs / 1000) + " s.";
    return false;
}

bool SameFileName(const std::filesystem::path& a, const std::filesystem::path& b) {
    return a.filename() == b.filename();
}

}  // namespace crisp::platform
