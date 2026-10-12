#include "check.h"
#include "server_test_support.h"

#include "core/transcript.h"
#include "platform/files.h"
#include "platform/process.h"
#include "util/json.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using crisp::platform::ChildProcess;
using crisp::platform::ExitStatus;
using crisp::platform::ProcessSpec;

namespace {

// tools/api_probe: next to the tests in a delivery, one folder up in a build tree.
fs::path ProbeExecutable() {
    const std::string name = std::string("api_probe") + crisp::test::kExecutableSuffix;
    const fs::path here = crisp::platform::ExecutableDirectory();
    std::error_code error;
    return fs::is_regular_file(here / name, error) ? here / name : here.parent_path() / name;
}

// Runs the probe with |arguments| and returns its exit code, or -1.
int RunProbe(const std::vector<std::string>& arguments, std::string& output) {
    ProcessSpec spec;
    spec.executable = ProbeExecutable();
    spec.arguments = arguments;
    ChildProcess probe;
    std::string error;
    if (!probe.Start(spec, error)) {
        std::fprintf(stderr, "api_probe_test: %s\n", error.c_str());
        return -1;
    }
    ExitStatus status;
    output = crisp::test::ReadUntilExit(probe, status, 50000);
    if (!probe.TryWait(status)) {
        crisp::test::EndProcess(probe);
        return -1;
    }
    return status.signaled ? -1 : static_cast<int>(status.code);
}

std::string Read(const fs::path& path) {
    return crisp::platform::ReadFileBytes(path, 1 << 20).bytes;
}

}  // namespace

int main() {
    std::error_code fsError;
    if (!fs::is_regular_file(ProbeExecutable(), fsError) ||
        !fs::is_regular_file(crisp::test::StubExecutable(), fsError)) {
        std::fprintf(stderr, "api_probe_test: api_probe or the stub server is missing\n");
        return 1;
    }
    const fs::path scratch = crisp::test::FreshScratchDirectory("api_probe_test.scratch");
    const fs::path audio = scratch / fs::u8path(u8"音声 テスト.wav");
    CHECK(crisp::test::WriteFile(audio, std::string("RIFF\x24\x00\x00\x00WAVEfmt data", 20)));
    std::string output;

    // A usage error.
    CHECK(RunProbe({"--port"}, output) == 2);

    // Nothing listens: the first check fails, and the probe stops there.
    {
        const fs::path out = scratch / "refused";
        CHECK(RunProbe({"--port", std::to_string(crisp::test::FreePort()), "--out", out.u8string(), audio.u8string()},
                       output) == 1);
        CHECK(Read(out / "report.txt").find("[1] FAIL GET /health") != std::string::npos);
        const auto report = crisp::util::ParseJson(Read(out / "report.json"));
        CHECK(report.Ok() && report.value.Find("failures")->NumberText() == "1");
    }

    // Every check against the stub, which takes 800 ms per transcription.
    const int port = crisp::test::FreePort();
    ChildProcess server;
    CHECK(crisp::test::StartStub(server, crisp::test::StubExecutable(), port,
                                 {{"STUB_LOAD_MS", "0", false},
                                  {"STUB_TRANSCRIBE_MS", "800", false},
                                  {"CRISPASR_API_KEYS", "secret", false}}));
    CHECK(crisp::test::WaitForAnswer(port, 15000));
    const fs::path out = scratch / "full";
    const int code = RunProbe({"--port", std::to_string(port), "--key", "secret", "--window", "1000,3000", "--out",
                               out.u8string(), audio.u8string()},
                              output);
    CHECK(code == 0);
    if (code != 0) {
        std::fprintf(stderr, "%s\n", output.c_str());
    }
    crisp::test::EndProcess(server);

    for (const char* name : {"report.txt", "report.json", "health.json", "models.json", "error-key.json",
                             "error-format.json", "error-decode.json", "not-audio.mp3", "transcript.verbose.json",
                             "transcript.srt", "window.verbose.json"}) {
        CHECK(fs::is_regular_file(out / name, fsError));
    }
    const std::string text = Read(out / "report.txt");
    CHECK(text.find("[10] PASS cancel a transcription") != std::string::npos);
    CHECK(text.find("    progress t=+") != std::string::npos);
    CHECK(text.find("Failures: 0") != std::string::npos);
    const auto report = crisp::util::ParseJson(Read(out / "report.json"));
    CHECK(report.Ok());
    if (report.Ok()) {
        const crisp::util::JsonValue* checks = report.value.Find("checks");
        CHECK(checks != nullptr && checks->Size() == 10);
        CHECK(report.value.Find("failures")->NumberText() == "0");
        CHECK(report.value.Find("audio")->Find("bytes")->NumberText() == "20");
    }
    crisp::core::Transcript transcript;
    std::string error;
    CHECK(crisp::core::ParseVerboseJson(Read(out / "transcript.verbose.json"), transcript, error));
    CHECK(transcript.segments.size() == 3);
    CHECK(Read(out / "transcript.srt").rfind("1\n00:00:00,000 --> 00:00:02,500\n", 0) == 0);

    fs::remove_all(scratch, fsError);
    return crisp::test::Result();
}
