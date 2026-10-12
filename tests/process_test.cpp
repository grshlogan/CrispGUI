#include "check.h"
#include "server_test_support.h"

#include "platform/process.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using crisp::platform::ChildProcess;
using crisp::platform::DescribeExitStatus;
using crisp::platform::ExitStatus;
using crisp::platform::FindPortOwner;
using crisp::platform::PortOwner;
using crisp::platform::ProcessSpec;
using crisp::platform::SameFileName;
using crisp::platform::TerminatePortOwner;

namespace {

bool Has(const std::string& output, const std::string& line) {
    return output.find(line + "\n") != std::string::npos || output.find(line + "\r\n") != std::string::npos;
}

void SetVariable(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

}  // namespace

int main() {
    const fs::path stub = crisp::test::StubExecutable();
    std::error_code error;
    if (!fs::is_regular_file(stub, error)) {
        std::fprintf(stderr, "process_test: the stub server is missing at %s\n", stub.u8string().c_str());
        return 1;
    }
    const fs::path scratch = crisp::test::FreshScratchDirectory("process_test-scratch");

    // Arguments, environment changes, and the working directory reach the child; its output
    // and exit code come back. The folder name and one argument are not ASCII, and one argument
    // needs quoting on Windows.
    {
        const fs::path folder = scratch / fs::u8path("ünï 测试");
        const fs::path copy = folder / (std::string("stub") + crisp::test::kExecutableSuffix);
        CHECK(crisp::test::CopyStub(copy));
        SetVariable("CUDA_VISIBLE_DEVICES", "7");
        ProcessSpec spec;
        spec.executable = copy;
        spec.arguments = {"--server", "-m", "a b\"c\\", "--port", "1", "日本"};
        spec.workingDirectory = folder;
        spec.environment = {{"STUB_EXIT_CODE", "3", false},
                            {"STUB_LOAD_MS", "0", false},
                            {"STUB_PROFILE_VALUE", "x y", false},
                            {"CUDA_VISIBLE_DEVICES", "", true}};
        ChildProcess process;
        std::string startError;
        CHECK(process.Start(spec, startError));
        CHECK(startError.empty() && process.Id() > 0);
        ExitStatus status;
        const std::string output = crisp::test::ReadUntilExit(process, status, 10000);
        CHECK(!status.signaled && status.code == 3);
        CHECK(Has(output, "stub: argv[1]=--server"));
        CHECK(Has(output, "stub: argv[3]=a b\"c\\"));
        CHECK(Has(output, "stub: argv[6]=日本"));
        CHECK(Has(output, "stub: env STUB_PROFILE_VALUE=x y"));
        CHECK(Has(output, "stub: env CUDA_VISIBLE_DEVICES unset"));
        CHECK(Has(output, "stub: cwd=" + folder.u8string()));
        if (status.code != 3 || !Has(output, "stub: argv[3]=a b\"c\\")) {
            std::fprintf(stderr, "process_test: output was:\n%s\n", output.c_str());
        }
        ExitStatus again;
        CHECK(process.TryWait(again) && again.code == 3);
        std::string terminateError;
        CHECK(process.Terminate(true, terminateError));  // nothing to do after the exit
    }

    // Many lines arrive in order, and the end of the output is seen.
    {
        ChildProcess process;
        CHECK(crisp::test::StartStub(process, stub, 1, {{"STUB_LINES", "2000", false}, {"STUB_EXIT_CODE", "0", false}}));
        ExitStatus status;
        const std::string output = crisp::test::ReadUntilExit(process, status, 20000);
        CHECK(status.code == 0);
        CHECK(Has(output, "stub: line 1") && Has(output, "stub: line 2000"));
        CHECK(output.find("stub: line 1999") < output.find("stub: line 2000"));
    }

    // A missing executable fails with a message.
    {
        ProcessSpec spec;
        spec.executable = scratch / "no-such-program";
        ChildProcess process;
        std::string startError;
        CHECK(!process.Start(spec, startError));
        CHECK(!startError.empty());
        ExitStatus status;
        CHECK(!process.TryWait(status));
    }

    // A listening server: found by its port, and ended through its port owner.
    {
        const int port = crisp::test::FreePort();
        const fs::path copy = scratch / "bin" / (std::string("crispasr") + crisp::test::kExecutableSuffix);
        CHECK(crisp::test::CopyStub(copy));
        ChildProcess process;
        CHECK(crisp::test::StartStub(process, copy, port, {{"STUB_LOAD_MS", "0", false}}));
        CHECK(crisp::test::WaitForAnswer(port, 15000));
        PortOwner owner;
        std::string ownerError;
        CHECK(FindPortOwner(port, owner, ownerError));
        CHECK(owner.pid == process.Id());
        CHECK(SameFileName(owner.executable, copy));
#if defined(_WIN32)
        CHECK(SameFileName(owner.executable, "CrispASR.EXE"));  // Windows names ignore case
#else
        CHECK(!SameFileName(owner.executable, "CrispASR"));
#endif
        std::string terminateError;
        CHECK(TerminatePortOwner(owner, 5000, terminateError));
        ExitStatus status;
        CHECK(crisp::test::WaitUntil([&] { return process.TryWait(status); }, 5000));
        PortOwner gone;
        CHECK(FindPortOwner(port, gone, ownerError) && gone.pid == 0);
        // A changed executable is refused.
        PortOwner stale = owner;
        stale.executable = scratch / "other";
        CHECK(!TerminatePortOwner(stale, 1000, terminateError));
    }

    // Terminate ends a running process; on POSIX first politely, then by force.
    {
        const int port = crisp::test::FreePort();
        ChildProcess process;
        CHECK(crisp::test::StartStub(process, stub, port, {{"STUB_LOAD_MS", "0", false}}));
        CHECK(crisp::test::WaitForAnswer(port, 15000));
        std::string terminateError;
        CHECK(process.Terminate(false, terminateError));
        ExitStatus status;
        CHECK(crisp::test::WaitUntil([&] { return process.TryWait(status); }, 5000));
#if defined(_WIN32)
        CHECK(!status.signaled && status.code == 1);
#else
        CHECK(status.signaled && status.code == 15);
        CHECK(DescribeExitStatus(status) == "signal 15");

        ChildProcess stubborn;
        CHECK(crisp::test::StartStub(stubborn, stub, crisp::test::FreePort(),
                                     {{"STUB_LOAD_MS", "0", false}, {"STUB_IGNORE_TERM", "1", false}}));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CHECK(stubborn.Terminate(false, terminateError));
        ExitStatus still;
        CHECK(!crisp::test::WaitUntil([&] { return stubborn.TryWait(still); }, 500));
        CHECK(stubborn.Terminate(true, terminateError));
        CHECK(crisp::test::WaitUntil([&] { return stubborn.TryWait(still); }, 5000));
        CHECK(still.signaled && still.code == 9);
#endif
    }

    CHECK(DescribeExitStatus(ExitStatus{false, 0}) == "exit code 0");
    CHECK(DescribeExitStatus(ExitStatus{false, 3221225477}) == "exit code 3221225477 (0xC0000005)");
    return crisp::test::Result();
}
