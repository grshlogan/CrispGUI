#include "check.h"
#include "test_support.h"

#include "core/server_state.h"
#include "platform/files.h"

#include <string>
#include <vector>

using crisp::core::ClassifyHealth;
using crisp::core::ClassifyModels;
using crisp::core::ConnectHost;
using crisp::core::HealthKind;
using crisp::core::HealthReport;
using crisp::core::ModelsReport;
using crisp::core::ServerCommand;
using crisp::core::ServerCommands;
using crisp::core::ServerEndpoint;
using crisp::core::ServerMessage;
using crisp::core::ServerState;
using crisp::core::ServerStateMachine;
using crisp::core::SpawnReport;
using crisp::core::StartRequest;
using crisp::platform::ExitStatus;
using crisp::platform::HttpFailure;
using crisp::platform::HttpResponse;
using Kind = ServerCommand::Kind;

namespace {

std::vector<Kind> Kinds(const ServerCommands& commands) {
    std::vector<Kind> kinds;
    for (const ServerCommand& command : commands) {
        kinds.push_back(command.kind);
    }
    return kinds;
}

// The epoch of the kMonitor command in |commands|; 0 without one.
std::uint64_t MonitorEpoch(const ServerCommands& commands) {
    for (const ServerCommand& command : commands) {
        if (command.kind == Kind::kMonitor) {
            return command.epoch;
        }
    }
    return 0;
}

HealthReport Health(std::uint64_t epoch, HealthKind kind, const std::string& backend = "cohere") {
    HealthReport report;
    report.epoch = epoch;
    report.kind = kind;
    report.backend = kind == HealthKind::kCrispAsr ? backend : std::string();
    report.detail = kind == HealthKind::kForeign ? "GET /health answered HTTP 404" : std::string();
    return report;
}

StartRequest Request(int port = 5200) {
    StartRequest request;
    request.profile.folder = "Japanese";
    request.profile.name = "cohere-asr-ja-q8";
    request.profile.port = port;
    return request;
}

SpawnReport Spawned(std::uint64_t serial, long pid) {
    SpawnReport report;
    report.serial = serial;
    report.ok = true;
    report.pid = pid;
    report.apiKey = "profilekey";
    report.warnings = {"The aligner is missing"};
    report.commandLine = "crispasr --server";
    return report;
}

HttpResponse Http(int status, const std::string& body) {
    HttpResponse response;
    response.status = status;
    response.body = body;
    return response;
}

// A machine that has started a server from Request() and sees it running. |epoch| is the
// epoch of the start.
struct Running {
    ServerStateMachine machine{ServerEndpoint{"127.0.0.1", 5114}, "default"};
    std::uint64_t epoch = 0;
    std::uint64_t serial = 0;

    Running() {
        machine.Begin();
        machine.OnHealth(Health(1, HealthKind::kRefused));
        epoch = MonitorEpoch(machine.Start(Request()));
        const ServerCommands spawn = machine.OnHealth(Health(epoch, HealthKind::kRefused));
        serial = spawn.empty() ? 0 : spawn[0].serial;
        machine.OnSpawned(Spawned(serial, 42));
        machine.OnHealth(Health(epoch, HealthKind::kCrispAsr));
    }
};

}  // namespace

int main() {
    // At launch nothing listens: stopped.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        CHECK(machine.Status().state == ServerState::kChecking);
        const ServerCommands begin = machine.Begin();
        CHECK(Kinds(begin) == std::vector<Kind>({Kind::kMonitor, Kind::kCheckNow}));
        CHECK(begin[0].endpoint == ServerEndpoint({"127.0.0.1", 5114}));
        CHECK(machine.OnHealth(Health(begin[0].epoch, HealthKind::kRefused)).empty());
        CHECK(machine.Status().state == ServerState::kStopped);
        CHECK(machine.Status().healthKnown && machine.Status().health == HealthKind::kRefused);
        CHECK(machine.Status().CanStart() && !machine.Status().CanStop());
    }

    // At launch a server answers: adopted, and its model is read with the default key.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        const std::uint64_t epoch = MonitorEpoch(machine.Begin());
        const ServerCommands commands = machine.OnHealth(Health(epoch, HealthKind::kCrispAsr, "qwen3"));
        CHECK(Kinds(commands) == std::vector<Kind>({Kind::kReadModels}));
        CHECK(commands[0].apiKey == "default" && commands[0].epoch == epoch);
        const auto& status = machine.Status();
        CHECK(status.state == ServerState::kRunning && status.adopted && !status.owned);
        CHECK(status.message == ServerMessage::kAdoptedAtLaunch && status.backend == "qwen3");
        CHECK(status.CanStop() && !status.CanStart());
        ModelsReport models;
        models.epoch = epoch;
        models.ok = true;
        models.model = "E:\\models\\qwen3.gguf";
        machine.OnModels(models);
        CHECK(machine.Status().model == "E:\\models\\qwen3.gguf");
        ModelsReport refused;
        refused.epoch = epoch;
        refused.detail = "The server did not accept the API key (HTTP 401).";
        machine.OnModels(refused);
        CHECK(machine.Status().modelsDetail == refused.detail);
    }

    // At launch another program answers: stopped, and the port shows as taken.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        const std::uint64_t epoch = MonitorEpoch(machine.Begin());
        machine.OnHealth(Health(epoch, HealthKind::kForeign));
        CHECK(machine.Status().state == ServerState::kStopped);
        CHECK(machine.Status().health == HealthKind::kForeign);
        CHECK(machine.Status().message == ServerMessage::kNone);
    }

    // A full start and stop.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        const std::uint64_t launch = MonitorEpoch(machine.Begin());
        machine.OnHealth(Health(launch, HealthKind::kRefused));

        const ServerCommands start = machine.Start(Request(5200));
        CHECK(Kinds(start) == std::vector<Kind>({Kind::kMonitor, Kind::kCheckNow}));
        CHECK(start[0].endpoint == ServerEndpoint({"127.0.0.1", 5200}));
        const std::uint64_t epoch = start[0].epoch;
        CHECK(epoch != launch);
        CHECK(machine.Status().state == ServerState::kChecking && machine.Status().pendingStart);
        CHECK(machine.Status().profileName == "cohere-asr-ja-q8" && machine.Status().CanStop());
        // A report from before the start changes nothing.
        CHECK(machine.OnHealth(Health(launch, HealthKind::kCrispAsr)).empty());
        CHECK(machine.Status().state == ServerState::kChecking);

        const ServerCommands spawn = machine.OnHealth(Health(epoch, HealthKind::kRefused));
        CHECK(Kinds(spawn) == std::vector<Kind>({Kind::kSpawn}));
        CHECK(spawn[0].start != nullptr && spawn[0].start->profile.name == "cohere-asr-ja-q8");
        const std::uint64_t serial = spawn[0].serial;
        CHECK(machine.Status().state == ServerState::kStarting && machine.Status().pid == 0);
        // /health of the starting server refuses connections until the model is loaded.
        CHECK(machine.OnHealth(Health(epoch, HealthKind::kRefused)).empty());

        CHECK(Kinds(machine.OnSpawned(Spawned(serial, 42))) == std::vector<Kind>({Kind::kCheckNow}));
        CHECK(machine.Status().owned && machine.Status().pid == 42);
        CHECK(machine.Status().warnings.size() == 1 && machine.Status().commandLine == "crispasr --server");
        const ServerCommands running = machine.OnHealth(Health(epoch, HealthKind::kCrispAsr));
        CHECK(machine.Status().state == ServerState::kRunning && !machine.Status().adopted);
        CHECK(Kinds(running) == std::vector<Kind>({Kind::kReadModels}));
        CHECK(running[0].apiKey == "profilekey");

        CHECK(Kinds(machine.Stop()) == std::vector<Kind>({Kind::kTerminateOwn}));
        CHECK(machine.Status().state == ServerState::kStopping);
        CHECK(machine.OnHealth(Health(epoch, HealthKind::kCrispAsr)).empty());
        const ServerCommands exited = machine.OnExited(serial, ExitStatus{false, 1});
        CHECK(Kinds(exited) == std::vector<Kind>({Kind::kMonitor, Kind::kRelease}));
        CHECK(exited[1].serial == serial);
        CHECK(machine.Status().state == ServerState::kStopped && !machine.Status().owned);
        CHECK(machine.Status().pid == 0 && machine.Status().message == ServerMessage::kNone);
        // A late report from the dead server does not adopt it.
        CHECK(machine.OnHealth(Health(epoch, HealthKind::kCrispAsr)).empty());
        CHECK(machine.Status().state == ServerState::kStopped);
    }

    // The pre-flight finds a server already running: adopted instead of starting a second one.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        machine.OnHealth(Health(MonitorEpoch(machine.Begin()), HealthKind::kRefused));
        const std::uint64_t epoch = MonitorEpoch(machine.Start(Request()));
        CHECK(Kinds(machine.OnHealth(Health(epoch, HealthKind::kCrispAsr))) == std::vector<Kind>({Kind::kReadModels}));
        CHECK(machine.Status().adopted && machine.Status().message == ServerMessage::kAdoptedInsteadOfStart);
        CHECK(machine.Status().profileName.empty());
    }

    // The pre-flight finds another program: failed, nothing started, Start may try again.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        machine.OnHealth(Health(MonitorEpoch(machine.Begin()), HealthKind::kRefused));
        const std::uint64_t epoch = MonitorEpoch(machine.Start(Request()));
        CHECK(machine.OnHealth(Health(epoch, HealthKind::kForeign)).empty());
        CHECK(machine.Status().state == ServerState::kFailed);
        CHECK(machine.Status().message == ServerMessage::kPortBusy);
        CHECK(machine.Status().detail == "GET /health answered HTTP 404");
        CHECK(machine.Status().CanStart());
    }

    // The process exits before /health answers, as when the port was taken: exit code 0 is
    // no success.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        machine.OnHealth(Health(MonitorEpoch(machine.Begin()), HealthKind::kRefused));
        const std::uint64_t epoch = MonitorEpoch(machine.Start(Request()));
        const std::uint64_t serial = machine.OnHealth(Health(epoch, HealthKind::kRefused))[0].serial;
        machine.OnSpawned(Spawned(serial, 7));
        const ServerCommands exited = machine.OnExited(serial, ExitStatus{false, 0});
        CHECK(Kinds(exited) == std::vector<Kind>({Kind::kMonitor, Kind::kRelease}));
        CHECK(machine.Status().state == ServerState::kFailed);
        CHECK(machine.Status().message == ServerMessage::kExitedWhileStarting);
        CHECK(machine.Status().detail == "exit code 0");
        CHECK(machine.Status().profileName == "cohere-asr-ja-q8");  // still shown next to the failure
        CHECK(machine.Status().CanStart());
    }

    // The running server exits on its own.
    {
        Running run;
        CHECK(run.machine.Status().state == ServerState::kRunning);
        run.machine.OnExited(run.serial, ExitStatus{false, 3221225477});
        CHECK(run.machine.Status().state == ServerState::kFailed);
        CHECK(run.machine.Status().message == ServerMessage::kExited);
        CHECK(run.machine.Status().detail == "exit code 3221225477 (0xC0000005)");
    }

    // The GUI's server stops answering and recovers.
    {
        Running run;
        run.machine.OnHealth(Health(run.epoch, HealthKind::kForeign));
        CHECK(run.machine.Status().state == ServerState::kRunning);
        CHECK(run.machine.Status().message == ServerMessage::kNotResponding);
        run.machine.OnHealth(Health(run.epoch, HealthKind::kCrispAsr));
        CHECK(run.machine.Status().message == ServerMessage::kNone);
        // /load swapped the backend: the model is read again.
        CHECK(Kinds(run.machine.OnHealth(Health(run.epoch, HealthKind::kCrispAsr, "whisper"))) ==
              std::vector<Kind>({Kind::kReadModels}));
    }

    // An adopted server becomes unreachable and comes back; its stop needs the port owner.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        const std::uint64_t epoch = MonitorEpoch(machine.Begin());
        machine.OnHealth(Health(epoch, HealthKind::kCrispAsr));
        machine.OnHealth(Health(epoch, HealthKind::kRefused));
        CHECK(machine.Status().state == ServerState::kUnreachable);
        CHECK(machine.Status().message == ServerMessage::kUnreachable);
        CHECK(machine.Status().CanStart() && machine.Status().CanStop());
        CHECK(Kinds(machine.OnHealth(Health(epoch, HealthKind::kCrispAsr))) == std::vector<Kind>({Kind::kReadModels}));
        CHECK(machine.Status().state == ServerState::kRunning && machine.Status().message == ServerMessage::kNone);

        const ServerCommands stop = machine.Stop();
        CHECK(Kinds(stop) == std::vector<Kind>({Kind::kTerminateAdopted}));
        CHECK(stop[0].endpoint.port == 5114);
        machine.OnStopFinished(false, "Access is denied.");
        CHECK(machine.Status().state == ServerState::kRunning);
        CHECK(machine.Status().message == ServerMessage::kStopFailed && machine.Status().detail == "Access is denied.");
        machine.Stop();
        CHECK(Kinds(machine.OnStopFinished(true, "")) == std::vector<Kind>({Kind::kMonitor}));
        CHECK(machine.Status().state == ServerState::kStopped && !machine.Status().adopted);
    }

    // Stop during the pre-flight cancels the start; its result is ignored.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        machine.OnHealth(Health(MonitorEpoch(machine.Begin()), HealthKind::kRefused));
        const std::uint64_t epoch = MonitorEpoch(machine.Start(Request()));
        CHECK(Kinds(machine.Stop()) == std::vector<Kind>({Kind::kMonitor}));
        CHECK(machine.Status().state == ServerState::kStopped);
        CHECK(machine.OnHealth(Health(epoch, HealthKind::kRefused)).empty());
        CHECK(machine.Status().state == ServerState::kStopped);
    }

    // Stop while the process is being created: it is ended as soon as it exists.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        machine.OnHealth(Health(MonitorEpoch(machine.Begin()), HealthKind::kRefused));
        const std::uint64_t epoch = MonitorEpoch(machine.Start(Request()));
        const std::uint64_t serial = machine.OnHealth(Health(epoch, HealthKind::kRefused))[0].serial;
        CHECK(machine.Stop().empty());
        CHECK(machine.Status().state == ServerState::kStopping);
        CHECK(Kinds(machine.OnSpawned(Spawned(serial, 9))) == std::vector<Kind>({Kind::kTerminateOwn}));
        machine.OnExited(serial, ExitStatus{false, 1});
        CHECK(machine.Status().state == ServerState::kStopped);
    }
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        machine.OnHealth(Health(MonitorEpoch(machine.Begin()), HealthKind::kRefused));
        const std::uint64_t epoch = MonitorEpoch(machine.Start(Request()));
        const std::uint64_t serial = machine.OnHealth(Health(epoch, HealthKind::kRefused))[0].serial;
        machine.Stop();
        SpawnReport failed;
        failed.serial = serial;
        failed.error = "Could not start";
        machine.OnSpawned(failed);
        CHECK(machine.Status().state == ServerState::kStopped);
    }

    // A start refused before a process exists, and one that fails to create it.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        machine.OnHealth(Health(MonitorEpoch(machine.Begin()), HealthKind::kRefused));
        std::uint64_t epoch = MonitorEpoch(machine.Start(Request()));
        SpawnReport refused;
        refused.serial = machine.OnHealth(Health(epoch, HealthKind::kRefused))[0].serial;
        refused.cannotStart = true;
        refused.error = "The model file is missing: m.gguf";
        machine.OnSpawned(refused);
        CHECK(machine.Status().state == ServerState::kFailed);
        CHECK(machine.Status().message == ServerMessage::kCannotStart);
        CHECK(machine.Status().detail == refused.error);

        epoch = MonitorEpoch(machine.Start(Request()));
        SpawnReport failed;
        failed.serial = machine.OnHealth(Health(epoch, HealthKind::kRefused))[0].serial;
        failed.error = "Access is denied. (error 5)";
        machine.OnSpawned(failed);
        CHECK(machine.Status().message == ServerMessage::kSpawnFailed);
        // A stale exit report only releases its process.
        CHECK(Kinds(machine.OnExited(99, ExitStatus{})) == std::vector<Kind>({Kind::kRelease}));
        CHECK(machine.Status().state == ServerState::kFailed);
    }

    // A server started elsewhere appears while stopped: adopted.
    {
        ServerStateMachine machine({"127.0.0.1", 5114}, "default");
        const std::uint64_t epoch = MonitorEpoch(machine.Begin());
        machine.OnHealth(Health(epoch, HealthKind::kRefused));
        machine.OnHealth(Health(epoch, HealthKind::kCrispAsr));
        CHECK(machine.Status().state == ServerState::kRunning);
        CHECK(machine.Status().message == ServerMessage::kFoundRunning);
        // Start is not offered while a server runs.
        CHECK(machine.Start(Request()).empty());
    }

    // Reading /health and /v1/models answers.
    {
        HttpResponse refused;
        refused.failure = HttpFailure::kRefused;
        refused.error = "Couldn't connect to server.";
        CHECK(ClassifyHealth(refused).kind == HealthKind::kRefused);
        HttpResponse timeout;
        timeout.failure = HttpFailure::kTimeout;
        timeout.error = "Timeout was reached.";
        CHECK(ClassifyHealth(timeout).kind == HealthKind::kForeign);
        CHECK(ClassifyHealth(timeout).detail == "No answer in time: Timeout was reached.");
        const HealthReport ok = ClassifyHealth(Http(200, "{\"status\": \"ok\", \"backend\": \"cohere\"}"));
        CHECK(ok.kind == HealthKind::kCrispAsr && ok.backend == "cohere" && !ok.loading);
        const HealthReport loading = ClassifyHealth(Http(503, "{\"status\": \"loading\"}"));
        CHECK(loading.kind == HealthKind::kCrispAsr && loading.loading);
        CHECK(ClassifyHealth(Http(200, "<html>hello</html>")).kind == HealthKind::kForeign);
        CHECK(ClassifyHealth(Http(200, "{\"status\": \"up\"}")).kind == HealthKind::kForeign);
        CHECK(ClassifyHealth(Http(503, "{}")).kind == HealthKind::kForeign);
        CHECK(ClassifyHealth(Http(404, "")).detail == "GET /health answered HTTP 404");

        const ModelsReport models = ClassifyModels(
            Http(200, "{\"object\": \"list\", \"data\": [{\"id\": \"E:\\\\m.gguf\", \"object\": \"model\", "
                      "\"owned_by\": \"crispasr\", \"backend\": \"cohere\"}]}"));
        CHECK(models.ok && models.model == "E:\\m.gguf" && models.backend == "cohere");
        CHECK(!ClassifyModels(Http(401, "{}")).ok);
        CHECK(ClassifyModels(Http(401, "{}")).detail == "The server did not accept the API key (HTTP 401).");
        CHECK(ClassifyModels(Http(500, "{}")).detail == "GET /v1/models answered HTTP 500.");
        CHECK(!ClassifyModels(Http(200, "{\"data\": []}")).ok);

        // The answers of the deployed server with the parakeet profile loaded, recorded by
        // api_probe (tests/data/README.md).
        const auto recorded = [](const char* name) {
            const auto file = crisp::platform::ReadFileBytes(
                crisp::test::TestDataDirectory() / "deployed-parakeet" / name, 1 << 16);
            CHECK(file.Ok());
            return file.bytes;
        };
        const HealthReport deployedHealth = ClassifyHealth(Http(200, recorded("health.json")));
        CHECK(deployedHealth.kind == HealthKind::kCrispAsr && !deployedHealth.loading);
        CHECK(deployedHealth.backend == "fastconformer-ctc");
        const ModelsReport deployedModels = ClassifyModels(Http(200, recorded("models.json")));
        CHECK(deployedModels.ok && deployedModels.backend == "fastconformer-ctc");
        CHECK(deployedModels.model ==
              "E:\\AI\\ASR\\CrispASR\\models\\Japanese\\[cstr]parakeet-ctc-1.1b-ja-GGUF\\"
              "parakeet-ctc-1.1b-ja-q8_0.gguf");
    }

    // Where the GUI connects for a listening address.
    CHECK(ConnectHost("") == "127.0.0.1");
    CHECK(ConnectHost("0.0.0.0") == "127.0.0.1");
    CHECK(ConnectHost("::") == "::1");
    CHECK(ConnectHost("[::1]") == "::1");
    CHECK(ConnectHost("localhost") == "localhost");
    CHECK(ServerEndpoint({"127.0.0.1", 5114}).Url() == "http://127.0.0.1:5114");
    CHECK(ServerEndpoint({"::1", 5114}).Url() == "http://[::1]:5114");
    return crisp::test::Result();
}
