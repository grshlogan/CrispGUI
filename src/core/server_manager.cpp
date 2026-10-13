#include "core/server_manager.h"

#include "platform/http.h"
#include "platform/process.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace crisp::core {
namespace {

using Clock = std::chrono::steady_clock;

struct Event {
    enum class Kind { kHealth, kModels, kSpawned, kExited, kStopFinished, kLog };
    Kind kind = Kind::kLog;
    HealthReport health;
    ModelsReport models;
    SpawnReport spawned;
    std::uint64_t serial = 0;
    platform::ExitStatus exit;
    bool ok = false;
    std::string error;
    std::vector<std::string> lines;
};

// Where the worker threads post what they observe; Update() takes it on the UI thread.
class Inbox {
public:
    explicit Inbox(std::function<void()> wake) : wake_(std::move(wake)) {}

    void Post(Event event) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            events_.push_back(std::move(event));
        }
        if (wake_) {
            wake_();
        }
    }

    void PostLines(std::vector<std::string> lines) {
        Event event;
        event.kind = Event::Kind::kLog;
        event.lines = std::move(lines);
        Post(std::move(event));
    }

    void Note(const std::string& text) { PostLines({kNotePrefix + text}); }

    void StopFinished(bool ok, const std::string& error) {
        Event event;
        event.kind = Event::Kind::kStopFinished;
        event.ok = ok;
        event.error = error;
        Post(std::move(event));
    }

    std::vector<Event> Take() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Event> events;
        events.swap(events_);
        return events;
    }

private:
    std::function<void()> wake_;
    std::mutex mutex_;
    std::vector<Event> events_;
};

// The environment changes of a launch, for the log; API keys stay hidden.
std::string EnvironmentSummary(const std::vector<platform::EnvironmentChange>& changes) {
    std::string text;
    for (const platform::EnvironmentChange& change : changes) {
        if (!text.empty()) {
            text += ", ";
        }
        if (change.remove) {
            text += change.name + " removed";
        } else if (change.name == "CRISPASR_API_KEYS") {
            text += change.name + "=(hidden)";
        } else {
            text += change.name + "=" + change.value;
        }
    }
    return text;
}

// Checks /health of the current endpoint at a steady pace and reads /v1/models on request.
class HealthPoller {
public:
    HealthPoller(Inbox& inbox, const ServerManagerOptions& options) : inbox_(inbox), options_(options) {}

    void Monitor(const ServerEndpoint& endpoint, std::uint64_t epoch) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            endpoint_ = endpoint;
            epoch_ = epoch;
        }
        wakeUp_.notify_one();
    }

    void CheckNow() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            checkNow_ = true;
        }
        wakeUp_.notify_one();
    }

    void ReadModels(const ServerEndpoint& endpoint, std::uint64_t epoch, const std::string& apiKey) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back({endpoint, epoch, apiKey});
        }
        wakeUp_.notify_one();
    }

    void Start() {
        thread_ = std::thread([this] { Run(); });
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        wakeUp_.notify_one();
        cancel_.Cancel();  // ends a request in progress, so the join is quick
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    struct ModelsJob {
        ServerEndpoint endpoint;
        std::uint64_t epoch = 0;
        std::string apiKey;
    };

    platform::HttpRequest Request(const ServerEndpoint& endpoint, const char* path) const {
        platform::HttpRequest request;
        request.host = endpoint.host;
        request.port = endpoint.port;
        request.path = path;
        request.connectTimeoutMs = options_.connectTimeoutMs;
        request.timeoutMs = options_.requestTimeoutMs;
        request.maxBodyBytes = 64 * 1024;
        return request;
    }

    void Run() {
        Clock::time_point next = Clock::now();
        std::unique_lock<std::mutex> lock(mutex_);
        while (!shutdown_) {
            const bool idle = endpoint_.port == 0 || (!checkNow_ && Clock::now() < next);
            if (jobs_.empty() && idle) {
                if (endpoint_.port == 0) {
                    wakeUp_.wait(lock);  // nothing to check before the first Monitor
                } else {
                    wakeUp_.wait_until(lock, next);
                }
                continue;
            }
            if (!jobs_.empty()) {
                const ModelsJob job = std::move(jobs_.front());
                jobs_.pop_front();
                lock.unlock();
                ReadModelsOnce(job);
                lock.lock();
                continue;
            }
            const bool forced = checkNow_;
            checkNow_ = false;
            const ServerEndpoint endpoint = endpoint_;
            const std::uint64_t epoch = epoch_;
            lock.unlock();
            CheckOnce(endpoint, epoch, forced);
            lock.lock();
            next = Clock::now() + std::chrono::milliseconds(options_.pollIntervalMs);
        }
    }

    void CheckOnce(const ServerEndpoint& endpoint, std::uint64_t epoch, bool forced) {
        const platform::HttpResponse response = platform::HttpGet(Request(endpoint, "/health"), &cancel_);
        if (response.failure == platform::HttpFailure::kCancelled) {
            return;
        }
        HealthReport report = ClassifyHealth(response);
        report.epoch = epoch;
        // Unchanged results are not posted, so an idle server does not keep the UI busy.
        const bool changed = !posted_ || report.epoch != last_.epoch || report.kind != last_.kind ||
                             report.loading != last_.loading || report.backend != last_.backend ||
                             report.detail != last_.detail;
        if (!forced && !changed) {
            return;
        }
        last_ = report;
        posted_ = true;
        Event event;
        event.kind = Event::Kind::kHealth;
        event.health = std::move(report);
        inbox_.Post(std::move(event));
    }

    void ReadModelsOnce(const ModelsJob& job) {
        platform::HttpRequest request = Request(job.endpoint, "/v1/models");
        if (!job.apiKey.empty()) {
            request.headers.emplace_back("Authorization", "Bearer " + job.apiKey);
        }
        const platform::HttpResponse response = platform::HttpGet(request, &cancel_);
        if (response.failure == platform::HttpFailure::kCancelled) {
            return;
        }
        Event event;
        event.kind = Event::Kind::kModels;
        event.models = ClassifyModels(response);
        event.models.epoch = job.epoch;
        inbox_.Post(std::move(event));
    }

    Inbox& inbox_;
    const ServerManagerOptions& options_;
    platform::HttpCancel cancel_;
    std::mutex mutex_;
    std::condition_variable wakeUp_;
    ServerEndpoint endpoint_;
    std::uint64_t epoch_ = 0;
    bool checkNow_ = false;
    bool shutdown_ = false;
    std::deque<ModelsJob> jobs_;
    std::thread thread_;
    // The poller thread's own record of the last report it posted.
    HealthReport last_;
    bool posted_ = false;
};

// Starts and ends server processes and reads their output, one process at a time.
class ProcessController {
public:
    ProcessController(Inbox& inbox, const ServerManagerOptions& options) : inbox_(inbox), options_(options) {}

    void Start() {
        thread_ = std::thread([this] { Run(); });
    }

    // Ends the thread; a server process keeps running.
    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        wakeUp_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void Post(const ServerCommand& command) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(command);
        }
        wakeUp_.notify_one();
    }

private:
    struct OwnProcess {
        std::uint64_t serial = 0;
        platform::ChildProcess child;
        std::thread monitor;
        std::atomic<bool> stop{false};
        std::atomic<bool> exited{false};
    };

    void Run() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!shutdown_) {
            if (queue_.empty()) {
                if (!killAt_) {
                    wakeUp_.wait(lock);
                } else if (Clock::now() < *killAt_) {
                    wakeUp_.wait_until(lock, *killAt_);
                } else {
                    killAt_.reset();
                    lock.unlock();
                    ForceStop();
                    lock.lock();
                }
                continue;
            }
            const ServerCommand command = std::move(queue_.front());
            queue_.pop_front();
            lock.unlock();
            Execute(command);
            lock.lock();
        }
        lock.unlock();
        if (process_) {
            // Stop reading the output and let go of the handles; the server keeps running.
            process_->stop = true;
            if (process_->monitor.joinable()) {
                process_->monitor.join();
            }
            process_.reset();
        }
    }

    void Execute(const ServerCommand& command) {
        switch (command.kind) {
        case ServerCommand::Kind::kSpawn:
            Spawn(command);
            break;
        case ServerCommand::Kind::kTerminateOwn:
            TerminateOwn();
            break;
        case ServerCommand::Kind::kTerminateAdopted:
            TerminateAdopted(command.endpoint);
            break;
        case ServerCommand::Kind::kRelease:
            Release(command.serial);
            break;
        default:
            break;
        }
    }

    void Spawn(const ServerCommand& command) {
        const StartRequest& request = *command.start;
        Event event;
        event.kind = Event::Kind::kSpawned;
        SpawnReport& report = event.spawned;
        report.serial = command.serial;

        const LaunchCommand launch = BuildLaunchCommand(request.profile, request.paths, request.options);
        std::error_code error;
        if (launch.Ok()) {
            std::filesystem::create_directories(request.options.serverTemp, error);
        }
        if (!launch.Ok() || error) {
            report.cannotStart = true;
            report.error = !launch.Ok() ? launch.error
                                        : "Could not create " + request.options.serverTemp.u8string() + ": " +
                                              error.message();
            inbox_.Note("Not started: " + report.error);
            inbox_.Post(std::move(event));
            return;
        }
        if (process_) {
            Release(process_->serial);  // one process at a time; the state machine prevents this
        }

        inbox_.Note("Starting " + request.profile.folder + "/" + request.profile.name + ": " +
                    launch.CommandLineText());
        inbox_.Note("Working directory: " + launch.process.workingDirectory.u8string());
        inbox_.Note("Environment: " + EnvironmentSummary(launch.process.environment));
        for (const std::string& warning : launch.warnings) {
            inbox_.Note("Warning: " + warning);
        }
        auto process = std::make_unique<OwnProcess>();
        process->serial = command.serial;
        std::string startError;
        if (!process->child.Start(launch.process, startError)) {
            report.error = startError;
            inbox_.Note("Not started: " + startError);
            inbox_.Post(std::move(event));
            return;
        }
        report.ok = true;
        report.pid = process->child.Id();
        report.apiKey = launch.apiKey;
        report.warnings = launch.warnings;
        report.commandLine = launch.CommandLineText();
        inbox_.Note("Started process " + std::to_string(report.pid) + ".");
        // The start report goes out before the monitor can report the exit.
        inbox_.Post(std::move(event));
        OwnProcess& watched = *process;
        process->monitor = std::thread([this, &watched] { Monitor(watched); });
        process_ = std::move(process);
    }

    void TerminateOwn() {
        if (!process_ || process_->exited) {
            return;  // the exit report is on its way
        }
        inbox_.Note("Stopping process " + std::to_string(process_->child.Id()) + ".");
        std::string error;
        if (!process_->child.Terminate(false, error)) {
            inbox_.StopFinished(false, error);
            return;
        }
#if !defined(_WIN32)
        killAt_ = Clock::now() + std::chrono::milliseconds(options_.stopGraceMs);
#endif
    }

    void ForceStop() {
        if (!process_ || process_->exited) {
            return;
        }
        inbox_.Note("The process did not stop within " + std::to_string(options_.stopGraceMs / 1000) +
                    " s; ending it.");
        std::string error;
        if (!process_->child.Terminate(true, error)) {
            inbox_.StopFinished(false, error);
        }
    }

    void TerminateAdopted(const ServerEndpoint& endpoint) {
        platform::PortOwner owner;
        std::string error;
        if (!platform::FindPortOwner(endpoint.port, owner, error)) {
            inbox_.StopFinished(false, error);
            return;
        }
        if (owner.pid == 0) {
            inbox_.Note("No process listens on port " + std::to_string(endpoint.port) + " any more.");
            inbox_.StopFinished(true, std::string());
            return;
        }
        const std::string who = "process " + std::to_string(owner.pid) +
                                (owner.executable.empty() ? std::string() : " (" + owner.executable.u8string() + ")");
        if (owner.executable.empty() || !platform::SameFileName(owner.executable, kServerExecutableName)) {
            inbox_.StopFinished(false, "Port " + std::to_string(endpoint.port) + " belongs to " + who +
                                           ", which is not " + kServerExecutableName +
                                           "; the GUI stops only CrispASR servers.");
            return;
        }
        inbox_.Note("Stopping the adopted server, " + who + ".");
        if (!platform::TerminatePortOwner(owner, options_.adoptedStopTimeoutMs, error)) {
            inbox_.StopFinished(false, error);
            return;
        }
        inbox_.Note("The adopted server has stopped.");
        inbox_.StopFinished(true, std::string());
    }

    void Release(std::uint64_t serial) {
        if (!process_ || process_->serial != serial) {
            return;
        }
        process_->stop = true;
        if (process_->monitor.joinable()) {
            process_->monitor.join();
        }
        process_.reset();
        killAt_.reset();
    }

    // The monitor thread: forwards output lines and reports the exit.
    void Monitor(OwnProcess& process) {
        LogSplitter splitter;
        std::vector<std::string> lines;
        char buffer[4096];
        platform::ExitStatus status;
        bool exited = false;
        Clock::time_point exitTime;
        Clock::time_point lastPost = Clock::now();
        const auto flushLines = [&] {
            if (!lines.empty()) {
                inbox_.PostLines(std::move(lines));
                lines.clear();
            }
            lastPost = Clock::now();
        };
        while (!process.stop) {
            bool ended = false;
            const std::size_t count = process.child.ReadOutput(buffer, sizeof(buffer), 100, ended);
            if (count > 0) {
                splitter.Feed(std::string_view(buffer, count), lines);
            }
            if (Clock::now() - lastPost >= std::chrono::milliseconds(50)) {
                flushLines();
            }
            if (!exited && process.child.TryWait(status)) {
                exited = true;
                exitTime = Clock::now();
            }
            // After the exit, read what is left; give up after a moment in case another process
            // inherited the pipe.
            if (exited && (ended || Clock::now() - exitTime > std::chrono::milliseconds(500))) {
                break;
            }
            if (ended && !exited) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        if (process.stop) {
            flushLines();
            return;
        }
        splitter.Flush(lines);
        lines.push_back(kNotePrefix + std::string("The server process exited with ") +
                        platform::DescribeExitStatus(status) + ".");
        flushLines();
        process.exited = true;
        Event event;
        event.kind = Event::Kind::kExited;
        event.serial = process.serial;
        event.exit = status;
        inbox_.Post(std::move(event));
    }

    Inbox& inbox_;
    const ServerManagerOptions& options_;
    std::mutex mutex_;
    std::condition_variable wakeUp_;
    std::deque<ServerCommand> queue_;
    bool shutdown_ = false;
    std::thread thread_;
    // The controller thread's own state.
    std::unique_ptr<OwnProcess> process_;
    std::optional<Clock::time_point> killAt_;
};

}  // namespace

struct ServerManager::Impl {
    Impl(ServerManagerOptions managerOptions, std::function<void()> wake)
        : options(std::move(managerOptions)),
          inbox(std::move(wake)),
          machine(options.endpoint, options.apiKey),
          log(options.logCapacity),
          poller(inbox, options),
          controller(inbox, options) {}

    void Dispatch(const ServerCommands& commands) {
        for (const ServerCommand& command : commands) {
            switch (command.kind) {
            case ServerCommand::Kind::kMonitor:
                poller.Monitor(command.endpoint, command.epoch);
                break;
            case ServerCommand::Kind::kCheckNow:
                poller.CheckNow();
                break;
            case ServerCommand::Kind::kReadModels:
                poller.ReadModels(command.endpoint, command.epoch, command.apiKey);
                break;
            case ServerCommand::Kind::kSpawn:
            case ServerCommand::Kind::kTerminateOwn:
            case ServerCommand::Kind::kTerminateAdopted:
            case ServerCommand::Kind::kRelease:
                controller.Post(command);
                break;
            }
        }
    }

    ServerManagerOptions options;
    Inbox inbox;
    ServerStateMachine machine;
    LogBuffer log;
    HealthPoller poller;
    ProcessController controller;
    bool begun = false;
    bool shutDown = false;
};

ServerManager::ServerManager(ServerManagerOptions options, std::function<void()> wake)
    : impl_(new Impl(std::move(options), std::move(wake))) {}

ServerManager::~ServerManager() {
    Shutdown();
}

void ServerManager::Begin() {
    if (impl_->begun) {
        return;
    }
    impl_->begun = true;
    // The first commands are queued before the threads run, so the first check has its endpoint.
    impl_->Dispatch(impl_->machine.Begin());
    impl_->poller.Start();
    impl_->controller.Start();
}

void ServerManager::Shutdown() {
    if (!impl_->begun || impl_->shutDown) {
        return;
    }
    impl_->shutDown = true;
    impl_->poller.Stop();
    impl_->controller.Stop();
}

bool ServerManager::Update() {
    std::vector<Event> events = impl_->inbox.Take();
    for (Event& event : events) {
        switch (event.kind) {
        case Event::Kind::kHealth:
            impl_->Dispatch(impl_->machine.OnHealth(event.health));
            break;
        case Event::Kind::kModels:
            impl_->Dispatch(impl_->machine.OnModels(event.models));
            break;
        case Event::Kind::kSpawned:
            impl_->Dispatch(impl_->machine.OnSpawned(event.spawned));
            break;
        case Event::Kind::kExited:
            impl_->Dispatch(impl_->machine.OnExited(event.serial, event.exit));
            break;
        case Event::Kind::kStopFinished:
            impl_->Dispatch(impl_->machine.OnStopFinished(event.ok, event.error));
            break;
        case Event::Kind::kLog:
            impl_->log.Append(std::move(event.lines));
            break;
        }
    }
    return !events.empty();
}

void ServerManager::Start(const Profile& profile, const CrispAsrPaths& paths, const LaunchOptions& options) {
    impl_->Dispatch(impl_->machine.Start(StartRequest{profile, paths, options}));
}

void ServerManager::Stop() {
    impl_->Dispatch(impl_->machine.Stop());
}

const ServerStatus& ServerManager::Status() const {
    return impl_->machine.Status();
}

const std::string& ServerManager::ApiKey() const {
    return impl_->machine.ApiKey();
}

const LogBuffer& ServerManager::Log() const {
    return impl_->log;
}

void ServerManager::ClearLog() {
    impl_->log.Clear();
}

}  // namespace crisp::core
