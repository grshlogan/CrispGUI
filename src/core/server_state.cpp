#include "core/server_state.h"

#include "util/json.h"

#include <utility>

namespace crisp::core {
namespace {

const util::JsonValue* Member(const util::JsonValue& object, const char* key) {
    return object.IsObject() ? object.Find(key) : nullptr;
}

std::string StringMember(const util::JsonValue& object, const char* key) {
    const util::JsonValue* value = Member(object, key);
    return value != nullptr && value->IsString() ? value->AsString() : std::string();
}

ServerCommand Command(ServerCommand::Kind kind) {
    ServerCommand command;
    command.kind = kind;
    return command;
}

}  // namespace

std::string ServerEndpoint::Url() const {
    return platform::HttpUrl(host, port, "");
}

std::string ConnectHost(const std::string& listenHost) {
    std::string host = listenHost;
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
        host = host.substr(1, host.size() - 2);
    }
    if (host.empty() || host == "0.0.0.0") {
        return kDefaultServerHost;
    }
    return host == "::" ? "::1" : host;
}

ServerEndpoint EndpointOf(const Profile& profile) {
    return {ConnectHost(profile.host), profile.port != 0 ? profile.port : kDefaultServerPort};
}

HealthReport ClassifyHealth(const platform::HttpResponse& response) {
    HealthReport report;
    switch (response.failure) {
    case platform::HttpFailure::kRefused:
        report.kind = HealthKind::kRefused;
        report.detail = response.error;
        return report;
    case platform::HttpFailure::kTimeout:
        report.kind = HealthKind::kForeign;
        report.detail = "No answer in time: " + response.error;
        return report;
    case platform::HttpFailure::kCancelled:
    case platform::HttpFailure::kOther:
        report.kind = HealthKind::kForeign;
        report.detail = response.error;
        return report;
    case platform::HttpFailure::kNone:
        break;
    }
    const util::JsonParseResult parsed = util::ParseJson(response.body);
    const std::string status = parsed.Ok() ? StringMember(parsed.value, "status") : std::string();
    if (response.status == 200 && status == "ok") {
        report.kind = HealthKind::kCrispAsr;
        report.backend = StringMember(parsed.value, "backend");
    } else if (response.status == 503 && status == "loading") {
        report.kind = HealthKind::kCrispAsr;
        report.loading = true;
    } else {
        report.kind = HealthKind::kForeign;
        report.detail = "GET /health answered HTTP " + std::to_string(response.status) +
                        (response.status == 200 ? ", but not as CrispASR does" : "");
    }
    return report;
}

ModelsReport ClassifyModels(const platform::HttpResponse& response) {
    ModelsReport report;
    if (!response.Ok()) {
        report.detail = response.error;
        return report;
    }
    if (response.status == 401) {
        report.detail = "The server did not accept the API key (HTTP 401).";
        return report;
    }
    if (response.status != 200) {
        report.detail = "GET /v1/models answered HTTP " + std::to_string(response.status) + ".";
        return report;
    }
    const util::JsonParseResult parsed = util::ParseJson(response.body);
    const util::JsonValue* data = parsed.Ok() ? Member(parsed.value, "data") : nullptr;
    if (data == nullptr || !data->IsArray() || data->Size() == 0 || StringMember(data->At(0), "id").empty()) {
        report.detail = "GET /v1/models gave an answer without a model.";
        return report;
    }
    report.ok = true;
    report.model = StringMember(data->At(0), "id");
    report.backend = StringMember(data->At(0), "backend");
    return report;
}

bool ServerStatus::CanStart() const {
    return state == ServerState::kStopped || state == ServerState::kFailed || state == ServerState::kUnreachable;
}

bool ServerStatus::CanStop() const {
    return state == ServerState::kStarting || state == ServerState::kRunning || state == ServerState::kUnreachable ||
           (state == ServerState::kChecking && pendingStart);
}

ServerStateMachine::ServerStateMachine(ServerEndpoint endpoint, std::string defaultApiKey)
    : defaultApiKey_(std::move(defaultApiKey)) {
    status_.endpoint = std::move(endpoint);
}

ServerCommands ServerStateMachine::NewEpoch(ServerCommands commands) {
    ++epoch_;
    ServerCommand monitor = Command(ServerCommand::Kind::kMonitor);
    monitor.epoch = epoch_;
    monitor.endpoint = status_.endpoint;
    commands.insert(commands.begin(), monitor);
    return commands;
}

ServerCommand ServerStateMachine::ReadModels() const {
    ServerCommand command = Command(ServerCommand::Kind::kReadModels);
    command.epoch = epoch_;
    command.endpoint = status_.endpoint;
    command.apiKey = apiKey_;
    return command;
}

void ServerStateMachine::ClearServer() {
    status_.pendingStart = false;
    status_.owned = false;
    status_.adopted = false;
    status_.pid = 0;
    status_.backend.clear();
    status_.model.clear();
    status_.modelsDetail.clear();
    status_.message = ServerMessage::kNone;
    status_.detail.clear();
}

ServerCommands ServerStateMachine::Adopt(ServerMessage message, const HealthReport& report) {
    ClearServer();
    pending_.reset();
    status_.state = ServerState::kRunning;
    status_.adopted = true;
    status_.profileFolder.clear();
    status_.profileName.clear();
    status_.warnings.clear();
    status_.commandLine.clear();
    status_.backend = report.backend;
    status_.message = message;
    apiKey_ = defaultApiKey_;
    return {ReadModels()};
}

void ServerStateMachine::Fail(ServerMessage message, const std::string& detail) {
    pending_.reset();
    terminateWhenSpawned_ = false;
    // The profile, warnings, and command line stay visible next to the failure.
    ClearServer();
    status_.state = ServerState::kFailed;
    status_.message = message;
    status_.detail = detail;
}

ServerCommands ServerStateMachine::Begin() {
    status_.state = ServerState::kChecking;
    status_.pendingStart = false;
    return NewEpoch({Command(ServerCommand::Kind::kCheckNow)});
}

ServerCommands ServerStateMachine::Start(StartRequest request) {
    if (!status_.CanStart()) {
        return {};
    }
    ClearServer();
    status_.state = ServerState::kChecking;
    status_.pendingStart = true;
    status_.endpoint = EndpointOf(request.profile);
    status_.profileFolder = request.profile.folder;
    status_.profileName = request.profile.name;
    status_.warnings.clear();
    status_.commandLine.clear();
    status_.healthKnown = false;
    pending_ = std::make_shared<const StartRequest>(std::move(request));
    return NewEpoch({Command(ServerCommand::Kind::kCheckNow)});
}

ServerCommands ServerStateMachine::Stop() {
    switch (status_.state) {
    case ServerState::kChecking:
        if (!status_.pendingStart) {
            return {};
        }
        pending_.reset();
        ClearServer();
        status_.state = ServerState::kStopped;
        return NewEpoch({});
    case ServerState::kStarting:
        stateBeforeStop_ = status_.state;
        status_.state = ServerState::kStopping;
        if (status_.pid == 0) {
            terminateWhenSpawned_ = true;  // the process is being created
            return {};
        }
        return {Command(ServerCommand::Kind::kTerminateOwn)};
    case ServerState::kRunning:
    case ServerState::kUnreachable: {
        stateBeforeStop_ = status_.state;
        status_.state = ServerState::kStopping;
        if (status_.owned) {
            return {Command(ServerCommand::Kind::kTerminateOwn)};
        }
        ServerCommand command = Command(ServerCommand::Kind::kTerminateAdopted);
        command.endpoint = status_.endpoint;
        return {command};
    }
    default:
        return {};
    }
}

ServerCommands ServerStateMachine::OnHealth(const HealthReport& report) {
    if (report.epoch != epoch_) {
        return {};
    }
    status_.healthKnown = true;
    status_.health = report.kind;
    status_.healthDetail = report.detail;
    const bool crispAsr = report.kind == HealthKind::kCrispAsr;
    switch (status_.state) {
    case ServerState::kChecking:
        if (crispAsr) {
            return Adopt(pending_ ? ServerMessage::kAdoptedInsteadOfStart : ServerMessage::kAdoptedAtLaunch, report);
        }
        if (!pending_) {
            status_.state = ServerState::kStopped;
            return {};
        }
        if (report.kind == HealthKind::kForeign) {
            Fail(ServerMessage::kPortBusy, report.detail);
            return {};
        }
        {
            ServerCommand spawn = Command(ServerCommand::Kind::kSpawn);
            spawn.serial = ++serial_;
            spawn.start = std::move(pending_);
            pending_.reset();
            status_.state = ServerState::kStarting;
            status_.pendingStart = false;
            return {spawn};
        }
    case ServerState::kStarting:
        if (crispAsr && status_.pid != 0) {
            status_.state = ServerState::kRunning;
            status_.backend = report.backend;
            return {ReadModels()};
        }
        return {};
    case ServerState::kRunning:
        if (crispAsr) {
            if (status_.message == ServerMessage::kNotResponding) {
                status_.message = ServerMessage::kNone;
                status_.detail.clear();
            }
            if (!report.backend.empty() && report.backend != status_.backend) {
                status_.backend = report.backend;
                return {ReadModels()};  // /load swapped the model
            }
            return {};
        }
        if (status_.owned) {
            status_.message = ServerMessage::kNotResponding;
            status_.detail = report.detail;
        } else {
            status_.state = ServerState::kUnreachable;
            status_.message = ServerMessage::kUnreachable;
            status_.detail = report.detail;
        }
        return {};
    case ServerState::kUnreachable:
        if (crispAsr) {
            status_.state = ServerState::kRunning;
            status_.message = ServerMessage::kNone;
            status_.detail.clear();
            status_.backend = report.backend;
            return {ReadModels()};
        }
        status_.detail = report.detail;
        return {};
    case ServerState::kStopped:
    case ServerState::kFailed:
        return crispAsr ? Adopt(ServerMessage::kFoundRunning, report) : ServerCommands{};
    case ServerState::kStopping:
        return {};
    }
    return {};
}

ServerCommands ServerStateMachine::OnModels(const ModelsReport& report) {
    if (report.epoch != epoch_ ||
        (status_.state != ServerState::kRunning && status_.state != ServerState::kUnreachable)) {
        return {};
    }
    if (report.ok) {
        status_.model = report.model;
        if (!report.backend.empty()) {
            status_.backend = report.backend;
        }
        status_.modelsDetail.clear();
    } else {
        status_.modelsDetail = report.detail;
    }
    return {};
}

ServerCommands ServerStateMachine::OnSpawned(const SpawnReport& report) {
    if (report.serial != serial_) {
        return {};
    }
    const bool stopping = status_.state == ServerState::kStopping && terminateWhenSpawned_;
    if (status_.state != ServerState::kStarting && !stopping) {
        return {};
    }
    if (!report.ok) {
        if (stopping) {
            terminateWhenSpawned_ = false;
            ClearServer();
            status_.state = ServerState::kStopped;
            return NewEpoch({});
        }
        Fail(report.cannotStart ? ServerMessage::kCannotStart : ServerMessage::kSpawnFailed, report.error);
        return {};
    }
    status_.owned = true;
    status_.pid = report.pid;
    status_.warnings = report.warnings;
    status_.commandLine = report.commandLine;
    apiKey_ = report.apiKey;
    if (stopping) {
        terminateWhenSpawned_ = false;
        return {Command(ServerCommand::Kind::kTerminateOwn)};
    }
    // A report that /health answered may have come before this one and been ignored.
    return {Command(ServerCommand::Kind::kCheckNow)};
}

ServerCommands ServerStateMachine::OnExited(std::uint64_t serial, const platform::ExitStatus& status) {
    ServerCommand release = Command(ServerCommand::Kind::kRelease);
    release.serial = serial;
    if (serial != serial_ || !status_.owned) {
        return {release};
    }
    switch (status_.state) {
    case ServerState::kStopping:
        ClearServer();
        status_.state = ServerState::kStopped;
        return NewEpoch({release});
    case ServerState::kStarting:
        Fail(ServerMessage::kExitedWhileStarting, platform::DescribeExitStatus(status));
        return NewEpoch({release});
    case ServerState::kRunning:
        Fail(ServerMessage::kExited, platform::DescribeExitStatus(status));
        return NewEpoch({release});
    default:
        return {release};
    }
}

ServerCommands ServerStateMachine::OnStopFinished(bool ok, const std::string& error) {
    if (status_.state != ServerState::kStopping) {
        return {};
    }
    if (status_.owned) {
        if (ok) {
            return {};  // the exit report follows
        }
        status_.state = stateBeforeStop_;
        status_.message = ServerMessage::kStopFailed;
        status_.detail = error;
        return {};
    }
    if (!ok) {
        status_.state = stateBeforeStop_;
        status_.message = ServerMessage::kStopFailed;
        status_.detail = error;
        return {};
    }
    ClearServer();
    status_.state = ServerState::kStopped;
    return NewEpoch({});
}

}  // namespace crisp::core
