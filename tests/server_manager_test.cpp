#include "check.h"
#include "server_test_support.h"

#include "core/profile_store.h"
#include "core/server_manager.h"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>

namespace fs = std::filesystem;
using crisp::core::CrispAsrPaths;
using crisp::core::LaunchOptions;
using crisp::core::LogBuffer;
using crisp::core::Profile;
using crisp::core::ServerManager;
using crisp::core::ServerManagerOptions;
using crisp::core::ServerMessage;
using crisp::core::ServerState;
using crisp::core::ServerStatus;

namespace {

// Applies the manager's reports until |done| holds or |timeoutMs| passes.
bool WaitFor(ServerManager& manager, const std::function<bool(const ServerStatus&)>& done, int timeoutMs) {
    return crisp::test::WaitUntil(
        [&] {
            manager.Update();
            return done(manager.Status());
        },
        timeoutMs);
}

bool InState(ServerManager& manager, ServerState state, int timeoutMs) {
    const bool reached = WaitFor(manager, [state](const ServerStatus& status) { return status.state == state; }, timeoutMs);
    if (!reached) {
        std::fprintf(stderr, "server_manager_test: state %d instead of %d; detail: %s\n",
                     static_cast<int>(manager.Status().state), static_cast<int>(state), manager.Status().detail.c_str());
    }
    return reached;
}

bool LogHas(const LogBuffer& log, const std::string& text) {
    for (std::size_t i = 0; i < log.Size(); ++i) {
        if (log.At(i).find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void PrintLog(const LogBuffer& log) {
    for (std::size_t i = 0; i < log.Size(); ++i) {
        std::fprintf(stderr, "  | %s\n", log.At(i).c_str());
    }
}

// A CrispASR folder whose server executable is the stub.
struct Deployment {
    fs::path root;
    CrispAsrPaths paths;
    fs::path model;
    LaunchOptions options;

    explicit Deployment(const fs::path& folder) {
        root = folder;
        paths = crisp::core::MakeCrispAsrPaths(root);
        model = paths.models / "test-model.gguf";
        crisp::test::WriteFile(model, "");
        crisp::test::CopyStub(root / "bin" / crisp::core::kServerExecutableName);
        options.serverTemp = root / "appdata" / "tmp" / "server";
    }

    // A profile on |port|; |extra| adds keys, such as an env object for the stub.
    Profile MakeProfile(const std::string& name, int port, const std::string& extra) const {
        const fs::path file = paths.profiles / "Test" / (name + ".conf");
        crisp::test::WriteFile(file, "{\"build\": \"bin\", \"backend\": \"stubby\", \"model\": \"test-model.gguf\", "
                                     "\"api_keys\": \"testkey\", \"language\": \"ja\", \"port\": " +
                                         std::to_string(port) + extra + "}");
        return crisp::core::LoadProfile(file, paths);
    }
};

}  // namespace

int main() {
    std::error_code error;
    if (!fs::is_regular_file(crisp::test::StubExecutable(), error)) {
        std::fprintf(stderr, "server_manager_test: the stub server is missing\n");
        return 1;
    }
    const Deployment deployment(crisp::test::FreshScratchDirectory("server_manager_test-scratch"));

    ServerManagerOptions options;
    options.endpoint = {"127.0.0.1", crisp::test::FreePort()};
    options.pollIntervalMs = 200;
    options.requestTimeoutMs = 2000;
    options.stopGraceMs = 500;
    options.adoptedStopTimeoutMs = 4000;

    {
        ServerManager manager(options, nullptr);
        manager.Begin();
        // Nothing runs on the port at launch.
        CHECK(InState(manager, ServerState::kStopped, 15000));
        CHECK(manager.Status().healthKnown);

        // Start, run, and stop a server from a profile.
        const int port = crisp::test::FreePort();
        const Profile profile = deployment.MakeProfile(
            "stub", port, ", \"vad\": true, \"env\": {\"STUB_LOAD_MS\": \"300\", \"STUB_PROFILE_VALUE\": \"from profile\"}");
        CHECK(profile.Usable());
        manager.Start(profile, deployment.paths, deployment.options);
        CHECK(manager.Status().state == ServerState::kChecking && manager.Status().pendingStart);
        CHECK(InState(manager, ServerState::kRunning, 20000));
        CHECK(WaitFor(manager, [](const ServerStatus& status) { return !status.model.empty(); }, 10000));
        const ServerStatus running = manager.Status();
        CHECK(running.owned && !running.adopted && running.pid > 0);
        CHECK(running.endpoint.port == port);
        CHECK(running.backend == "stubby");
        CHECK(running.model == deployment.model.u8string());  // read with the profile's key
        CHECK(running.profileFolder == "Test" && running.profileName == "stub");
        CHECK(running.commandLine.find("--vad") != std::string::npos);
        CHECK(fs::is_directory(deployment.options.serverTemp, error));
        // The server's output reached the log, with what the launcher would set.
        const LogBuffer& log = manager.Log();
        CHECK(LogHas(log, "[CrispRustGUI] Starting Test/stub: "));
        CHECK(LogHas(log, "crispasr-server: listening on 127.0.0.1:" + std::to_string(port)));
        CHECK(LogHas(log, "stub: env CRISPASR_API_KEYS=testkey"));
        CHECK(LogHas(log, "stub: env CRISPASR_MODELS_DIR=" + deployment.paths.models.u8string()));
        CHECK(LogHas(log, "stub: env STUB_PROFILE_VALUE=from profile"));
        CHECK(LogHas(log, "stub: env TEMP=" + deployment.options.serverTemp.u8string()));
        CHECK(LogHas(log, "stub: cwd=" + deployment.root.u8string()));
        CHECK(LogHas(log, "CRISPASR_API_KEYS=(hidden)"));  // the GUI's own note hides keys
        CHECK(manager.Status().CanStop() && !manager.Status().CanStart());

        manager.Stop();
        CHECK(manager.Status().state == ServerState::kStopping);
        CHECK(InState(manager, ServerState::kStopped, 10000));
        CHECK(!manager.Status().owned && manager.Status().pid == 0);
        CHECK(LogHas(log, "[CrispRustGUI] The server process exited with"));
        CHECK(crisp::test::Get(port, "/health").failure == crisp::platform::HttpFailure::kRefused);

        // A process that exits with code 0 before it answers has failed.
        const Profile exits = deployment.MakeProfile(
            "exits", crisp::test::FreePort(), ", \"env\": {\"STUB_LOAD_MS\": \"100\", \"STUB_EXIT_CODE\": \"0\"}");
        manager.Start(exits, deployment.paths, deployment.options);
        CHECK(InState(manager, ServerState::kFailed, 20000));
        CHECK(manager.Status().message == ServerMessage::kExitedWhileStarting);
        CHECK(manager.Status().detail == "exit code 0");
        CHECK(LogHas(log, "stub: exiting with code 0"));

        // Another program holds the port: nothing is started.
        {
            const int busyPort = crisp::test::FreePort();
            crisp::platform::ChildProcess foreign;
            CHECK(crisp::test::StartStub(foreign, crisp::test::StubExecutable(), busyPort,
                                         {{"STUB_LOAD_MS", "0", false}, {"STUB_MODE", "foreign", false}}));
            CHECK(crisp::test::WaitForAnswer(busyPort, 15000));
            manager.ClearLog();
            manager.Start(deployment.MakeProfile("busy", busyPort, ""), deployment.paths, deployment.options);
            CHECK(InState(manager, ServerState::kFailed, 15000));
            CHECK(manager.Status().message == ServerMessage::kPortBusy);
            CHECK(manager.Status().detail == "GET /health answered HTTP 404");
            CHECK(!LogHas(log, "Starting"));
            crisp::test::EndProcess(foreign);
        }

        // A missing model refuses the start before a process exists.
        manager.Start(deployment.MakeProfile("missing", crisp::test::FreePort(), ", \"model\": \"gone.gguf\""),
                      deployment.paths, deployment.options);
        CHECK(InState(manager, ServerState::kFailed, 15000));
        CHECK(manager.Status().message == ServerMessage::kCannotStart);
        CHECK(manager.Status().detail.find("gone.gguf") != std::string::npos);

#if !defined(_WIN32)
        // A server that ignores SIGTERM is ended after the grace period.
        const Profile stubborn = deployment.MakeProfile(
            "stubborn", crisp::test::FreePort(), ", \"env\": {\"STUB_LOAD_MS\": \"0\", \"STUB_IGNORE_TERM\": \"1\"}");
        manager.Start(stubborn, deployment.paths, deployment.options);
        CHECK(InState(manager, ServerState::kRunning, 20000));
        manager.Stop();
        CHECK(InState(manager, ServerState::kStopped, 10000));
        CHECK(LogHas(log, "did not stop within"));
#endif
        if (crisp::test::FailureCount() > 0) {
            PrintLog(log);
        }
        manager.Shutdown();
    }

    // A server running before the GUI starts is adopted, and its stop goes through the port.
    {
        const int port = crisp::test::FreePort();
        crisp::platform::ChildProcess outside;
        CHECK(crisp::test::StartStub(outside, deployment.root / "bin" / crisp::core::kServerExecutableName, port,
                                     {{"STUB_LOAD_MS", "0", false}, {"CRISPASR_API_KEYS", "default", false}}));
        CHECK(crisp::test::WaitForAnswer(port, 15000));
        ServerManagerOptions adoptOptions = options;
        adoptOptions.endpoint = {"127.0.0.1", port};
        ServerManager manager(adoptOptions, nullptr);
        manager.Begin();
        CHECK(InState(manager, ServerState::kRunning, 15000));
        CHECK(manager.Status().adopted && !manager.Status().owned);
        CHECK(manager.Status().message == ServerMessage::kAdoptedAtLaunch);
        CHECK(WaitFor(manager, [](const ServerStatus& status) { return !status.model.empty(); }, 10000));
        CHECK(manager.Status().model == "stub-model.gguf");  // read with the default key
        manager.Stop();
        CHECK(InState(manager, ServerState::kStopped, 15000));
        crisp::platform::ExitStatus status;
        CHECK(crisp::test::WaitUntil([&] { return outside.TryWait(status); }, 5000));
        if (crisp::test::FailureCount() > 0) {
            PrintLog(manager.Log());
        }
        manager.Shutdown();
        crisp::test::EndProcess(outside);
    }
    return crisp::test::Result();
}
