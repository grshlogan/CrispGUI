#pragma once

#include "core/launch_command.h"
#include "core/profile_store.h"
#include "core/server_log.h"
#include "core/server_state.h"

#include <cstddef>
#include <functional>
#include <memory>

// Starts, watches, adopts, and stops the CrispASR server (SPEC FR-1; docs/ARCHITECTURE.md,
// section 2.2). The UI thread owns the object; two worker threads do the blocking work and post
// what they observe to an inbox that Update() applies.
namespace crisp::core {

struct ServerManagerOptions {
    ServerEndpoint endpoint{kDefaultServerHost, kDefaultServerPort};  // checked at launch
    std::string apiKey = kDefaultApiKey;  // for servers the GUI did not start
    int pollIntervalMs = 1500;            // between two checks of /health
    int connectTimeoutMs = 5000;          // a refused connection takes about 2 s on Windows
    int requestTimeoutMs = 5000;
    int stopGraceMs = 3000;  // POSIX: SIGTERM, then SIGKILL after this
    int adoptedStopTimeoutMs = 5000;
    std::size_t logCapacity = 5000;  // lines
};

class ServerManager {
public:
    // |wake| is called from worker threads when Update() has something to apply, such as
    // app::requestUpdate(); it is never called after Shutdown() returns.
    ServerManager(ServerManagerOptions options, std::function<void()> wake);
    ~ServerManager();
    ServerManager(const ServerManager&) = delete;
    ServerManager& operator=(const ServerManager&) = delete;

    // Starts the worker threads and the check at launch.
    void Begin();
    // Stops the worker threads. A server keeps running, so the next launch adopts it.
    void Shutdown();

    // Applies what the threads reported. Call on the UI thread before reading the state; returns
    // true when anything changed.
    bool Update();

    // Starts a server from |profile| after checking its port; ignored unless Status().CanStart().
    void Start(const Profile& profile, const CrispAsrPaths& paths, const LaunchOptions& options);
    // Stops the server, cancels a start, or ends an adopted server; ignored unless CanStop().
    void Stop();

    const ServerStatus& Status() const;
    // The API key that the manager's own checks of the current server use (see
    // ServerStateMachine::ApiKey()); while Status().state is kRunning, a request to
    // Status().endpoint needs exactly this key.
    const std::string& ApiKey() const;
    const LogBuffer& Log() const;
    void ClearLog();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace crisp::core
