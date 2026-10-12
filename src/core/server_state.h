#pragma once

#include "core/launch_command.h"
#include "core/profile_store.h"
#include "platform/http.h"
#include "platform/process.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The server's lifecycle as a state machine without threads or I/O (SPEC FR-1;
// docs/ARCHITECTURE.md, section 2.2). ServerManager feeds it what its threads observe and carries
// out the commands it returns, so every transition can be tested on its own.
namespace crisp::core {

// Where the GUI reaches the server.
struct ServerEndpoint {
    std::string host;
    int port = 0;

    bool operator==(const ServerEndpoint& other) const { return host == other.host && port == other.port; }
    bool operator!=(const ServerEndpoint& other) const { return !(*this == other); }
    std::string Url() const;  // "http://127.0.0.1:5114"
};

// The address to connect to for a server listening on |listenHost|: the loopback address in place
// of a wildcard or an empty host, without brackets around an IPv6 address.
std::string ConnectHost(const std::string& listenHost);

// The endpoint of the server a profile starts: its host and port, or the launcher's defaults.
ServerEndpoint EndpointOf(const Profile& profile);

enum class HealthKind {
    kRefused,   // nothing listens: the port is free
    kCrispAsr,  // GET /health answered as CrispASR does
    kForeign,   // another program holds the port, or the answer is not CrispASR's
};

struct HealthReport {
    std::uint64_t epoch = 0;  // the state machine drops reports of an older epoch
    HealthKind kind = HealthKind::kRefused;
    bool loading = false;  // 503 {"status":"loading"}: /load is swapping the model
    std::string backend;
    std::string detail;  // what was seen, for a foreign or refused port
};

// Reads a GET /health result: 200 {"status":"ok"} or 503 {"status":"loading"} is CrispASR, a
// refused connection is a free port, and anything else another program
// (docs/API-CRISPASR.md, sections 1 and 6).
HealthReport ClassifyHealth(const platform::HttpResponse& response);

struct ModelsReport {
    std::uint64_t epoch = 0;
    bool ok = false;
    std::string model;    // data[0].id: the model path the server resolved
    std::string backend;  // data[0].backend
    std::string detail;   // why it failed, such as a refused API key
};

// Reads a GET /v1/models result (docs/API-CRISPASR.md, section 1).
ModelsReport ClassifyModels(const platform::HttpResponse& response);

enum class ServerState {
    kStopped,
    kChecking,     // looking for a server on the port: at launch, or before starting one
    kStarting,     // the GUI's server process runs and loads its model; the port is not open yet
    kRunning,      // /health answers
    kUnreachable,  // an adopted server stopped answering
    kStopping,
    kFailed,  // a start was refused or the server exited unexpectedly; Start may try again
};

// What the status line reports; the UI translates it and appends the English detail.
enum class ServerMessage {
    kNone,
    kAdoptedAtLaunch,        // a server was already running when the GUI started
    kAdoptedInsteadOfStart,  // Start found a server already running on the port
    kFoundRunning,           // a server started elsewhere appeared on the port
    kPortBusy,               // another program holds the port; nothing was started
    kCannotStart,            // the executable or the model is missing, or the profile is unusable
    kSpawnFailed,            // the process could not be created
    kExitedWhileStarting,    // the process ended before /health answered
    kExited,                 // the running server's process ended
    kStopFailed,
    kNotResponding,  // the GUI's server process runs but /health fails
    kUnreachable,    // an adopted server does not answer
};

struct ServerStatus {
    ServerState state = ServerState::kChecking;
    bool pendingStart = false;  // kChecking before a start rather than at launch
    bool owned = false;         // the GUI started this server and holds its process
    bool adopted = false;       // found running; it may belong to someone else
    ServerEndpoint endpoint;
    std::string profileFolder;  // the profile of a server the GUI starts or started
    std::string profileName;
    long pid = 0;  // of a server the GUI started
    std::string backend;
    std::string model;
    std::string modelsDetail;  // why /v1/models gave no answer
    ServerMessage message = ServerMessage::kNone;
    std::string detail;
    std::vector<std::string> warnings;  // from the launch, such as a missing aligner
    std::string commandLine;            // of a server the GUI started
    bool healthKnown = false;           // the port has been checked
    HealthKind health = HealthKind::kRefused;
    std::string healthDetail;

    bool CanStart() const;
    bool CanStop() const;
};

struct StartRequest {
    Profile profile;
    CrispAsrPaths paths;
    LaunchOptions options;
};

// What the threads report about a start.
struct SpawnReport {
    std::uint64_t serial = 0;
    bool ok = false;
    bool cannotStart = false;  // refused before creating a process (BuildLaunchCommand)
    std::string error;
    long pid = 0;
    std::string apiKey;
    std::vector<std::string> warnings;
    std::string commandLine;
};

// What the state machine asks the threads to do.
struct ServerCommand {
    enum class Kind {
        kMonitor,           // check |endpoint| from now on, tagging reports with |epoch|
        kCheckNow,          // check at once and report even an unchanged result
        kSpawn,             // build the command for |start| and start process |serial|
        kTerminateOwn,      // end the GUI's server process
        kTerminateAdopted,  // end the process that listens on |endpoint|'s port
        kReadModels,        // GET /v1/models from |endpoint| with |apiKey|
        kRelease,           // process |serial| has exited; let go of it
    };
    Kind kind = Kind::kCheckNow;
    std::uint64_t epoch = 0;
    std::uint64_t serial = 0;
    ServerEndpoint endpoint;
    std::string apiKey;
    std::shared_ptr<const StartRequest> start;
};

using ServerCommands = std::vector<ServerCommand>;

class ServerStateMachine {
public:
    // |endpoint| is checked at launch; |defaultApiKey| is used for servers the GUI did not start.
    ServerStateMachine(ServerEndpoint endpoint, std::string defaultApiKey);

    ServerCommands Begin();  // the check at launch
    ServerCommands Start(StartRequest request);
    ServerCommands Stop();

    ServerCommands OnHealth(const HealthReport& report);
    ServerCommands OnModels(const ModelsReport& report);
    ServerCommands OnSpawned(const SpawnReport& report);
    ServerCommands OnExited(std::uint64_t serial, const platform::ExitStatus& status);
    // The end of kTerminateAdopted, or a failed kTerminateOwn.
    ServerCommands OnStopFinished(bool ok, const std::string& error);

    const ServerStatus& Status() const { return status_; }

private:
    ServerCommands NewEpoch(ServerCommands commands);
    ServerCommand ReadModels() const;
    ServerCommands Adopt(ServerMessage message, const HealthReport& report);
    void Fail(ServerMessage message, const std::string& detail);
    void ClearServer();

    ServerStatus status_;
    std::string defaultApiKey_;
    std::string apiKey_;  // for the current server
    std::uint64_t epoch_ = 0;
    std::uint64_t serial_ = 0;  // of the GUI's current or last process
    std::shared_ptr<const StartRequest> pending_;
    bool terminateWhenSpawned_ = false;
    ServerState stateBeforeStop_ = ServerState::kStopped;
};

}  // namespace crisp::core
