#include "check.h"
#include "test_support.h"

#include "core/profile_store.h"
#include "platform/files.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using crisp::core::CrispAsrPaths;
using crisp::core::JoinProfilePath;
using crisp::core::LoadProfile;
using crisp::core::MakeCrispAsrPaths;
using crisp::core::ParseProfile;
using crisp::core::Profile;
using crisp::core::ProfileScan;
using crisp::core::ScanProfiles;
using crisp::test::WriteFile;

namespace {

const char* const kAligner =
    "Japanese\\[cstr]wav2vec2-large-xlsr-53-japanese-GGUF\\wav2vec2-large-xlsr-53-japanese-q8_0.gguf";

// The reference profiles as reference/crispasr/profiles-japanese holds them (SHA-256 of the files).
struct ReferenceProfile {
    const char* name;
    const char* hash;
    const char* backend;
    const char* model;
    const char* download;
    double chunkSeconds;
    std::vector<std::string> args;
};

const ReferenceProfile kReferenceProfiles[] = {
    {"cohere-asr-ja-q8", "20cf5de2135ac1565b6e6554f5370b8a2d1546eecbed7e9e44ffe6b178c7d4fa", "cohere",
     "Japanese\\[CKHO]cohere-asr-ja-GGUF\\cohere-asr-ja-q8_0.gguf",
     "https://huggingface.co/CKHO/cohere-asr-ja-GGUF/resolve/main/cohere-asr-ja-q8_0.gguf", 30,
     {"--vad-threshold", "0.3"}},
    {"kotoba-whisper-v2.2-q8", "8294958706893ec9357f2769d56dcd0e49b0a7b664b78a5dcc52e88bbf6a94d7", "whisper",
     "Japanese\\[kenrouse]kotoba-whisper-v2.2-ggml\\kotoba-whisper-v2.2-ggml-q8_0.bin",
     "https://huggingface.co/kenrouse/kotoba-whisper-v2.2-ggml/resolve/main/kotoba-whisper-v2.2-ggml-q8_0.bin", 15,
     {"--vad-max-speech-duration-s", "15"}},
    {"parakeet-ctc-1.1b-ja-q8", "16652a5525adaa44f4a16da2fb07a7014c212063edee79dad529642c3c814b4e",
     "fastconformer-ctc", "Japanese\\[cstr]parakeet-ctc-1.1b-ja-GGUF\\parakeet-ctc-1.1b-ja-q8_0.gguf",
     "https://huggingface.co/cstr/parakeet-ctc-1.1b-ja-GGUF/resolve/main/parakeet-ctc-1.1b-ja-q8_0.gguf", 15,
     {"--vad-max-speech-duration-s", "15"}},
    {"qwen3-asr-1.7b-ja-anime-q8", "51df997e5e690d6260176288aefa6624b82de3ac3305340617991e8487b0bf06", "qwen3",
     "Japanese\\[cstr]qwen3-asr-1.7b-ja-anime-GGUF\\qwen3-asr-1.7b-ja-anime-q8_0.gguf",
     "https://huggingface.co/cstr/qwen3-asr-1.7b-ja-anime-GGUF/resolve/main/qwen3-asr-1.7b-ja-anime-q8_0.gguf", 30,
     {}},
};

std::string ReadBytes(const fs::path& path) {
    return crisp::platform::ReadFileBytes(path, crisp::core::kMaxProfileBytes).bytes;
}

Profile Parse(const std::string& text) {
    Profile profile;
    ParseProfile(text, profile);
    return profile;
}

bool HasWarning(const Profile& profile, const std::string& text) {
    for (const std::string& warning : profile.warnings) {
        if (warning.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

const std::string kMinimal = "{\"build\": \"cuda13\", \"backend\": \"cohere\", \"model\": \"m.gguf\"";

void CheckReferenceProfiles(const fs::path& data) {
    for (const ReferenceProfile& expected : kReferenceProfiles) {
        const Profile profile = LoadProfile(data / (std::string(expected.name) + ".conf"), CrispAsrPaths());
        CHECK(profile.Usable());
        CHECK(profile.warnings.empty());
        CHECK(profile.name == expected.name);
        CHECK(profile.folder == "profiles-japanese");
        CHECK(profile.contentHash == expected.hash);
        CHECK(profile.build == "cuda13");
        CHECK(profile.backend == expected.backend);
        CHECK(profile.model == expected.model);
        CHECK(profile.aligner == kAligner);
        CHECK(profile.download == expected.download);
        CHECK(profile.language == "ja");
        CHECK(profile.host == "127.0.0.1");
        CHECK(profile.port == 5114);
        CHECK(profile.device == "1");
        CHECK(profile.apiKeys.empty());
        CHECK(profile.env.empty());
        CHECK(profile.args == expected.args);
        CHECK(profile.Find("comment") != nullptr && !profile.comment.empty());
        CHECK(profile.Find("vad") != nullptr && profile.Find("vad")->AsBoolean());
        CHECK(profile.Find("force_aligner") != nullptr && profile.Find("force_aligner")->AsBoolean());
        CHECK(profile.Find("chunk_seconds") != nullptr && profile.Find("chunk_seconds")->AsNumber() == expected.chunkSeconds);
        CHECK(profile.Find("no_punctuation") != nullptr && !profile.Find("no_punctuation")->AsBoolean());
        CHECK(!profile.executableOk && !profile.modelOk);  // no CrispASR folder given
    }
    // The model-specific keys differ: the CTC profile sets no beam size or temperature.
    const Profile parakeet = LoadProfile(data / "parakeet-ctc-1.1b-ja-q8.conf", CrispAsrPaths());
    CHECK(parakeet.Find("beam_size") == nullptr && parakeet.Find("temperature") == nullptr);
    CHECK(parakeet.Find("max_new_tokens") == nullptr);
    const Profile kotoba = LoadProfile(data / "kotoba-whisper-v2.2-q8.conf", CrispAsrPaths());
    CHECK(kotoba.Find("max_new_tokens") != nullptr && kotoba.Find("max_new_tokens")->AsNumber() == 0.0);
}

void CheckParsing() {
    CHECK(Parse(kMinimal + "}").Usable());

    // Comments, URLs, and an escaped quote followed by // inside a string.
    const Profile escaped = Parse(kMinimal + ", // trailing comment\n"
                                             " \"comment\": \"a \\\"quoted\\\" // word\", # hash comment\n"
                                             " \"download\": \"https://example.org/a#b\"}");
    CHECK(escaped.Usable());
    CHECK(escaped.comment == "a \"quoted\" // word");
    CHECK(escaped.download == "https://example.org/a#b");

    // Encodings: a UTF-8 byte order mark is fine; UTF-16 is refused.
    CHECK(Parse("\xEF\xBB\xBF" + kMinimal + "}").Usable());
    CHECK(Parse("\xFF\xFE{\0}").error.find("UTF-16") != std::string::npos);

    // Broken files are errors, with the line of the problem.
    const Profile broken = Parse(kMinimal + ",\n  \"vad\": true\n  \"beam_size\": 1\n}");
    CHECK(!broken.Usable());
    CHECK(broken.error.find("line 3") != std::string::npos);
    CHECK(!Parse("").Usable());
    CHECK(!Parse("[1, 2]").Usable());
    CHECK(!Parse("{\"model\": \"Japanese\\model.gguf\"}").Usable());  // a single backslash

    // Required keys and types.
    CHECK(Parse("{\"build\": \"cuda13\", \"backend\": \"cohere\"}").error == "\"model\" is missing.");
    CHECK(Parse("{\"build\": \"cuda13\", \"backend\": \"cohere\", \"model\": \"\"}").error == "\"model\" is missing.");
    CHECK(Parse(kMinimal + ", \"vad\": \"true\"}").error == "\"vad\" must be true or false.");
    CHECK(Parse(kMinimal + ", \"beam_size\": 1.5}").error == "\"beam_size\" must be a whole number.");
    CHECK(Parse(kMinimal + ", \"temperature\": \"0\"}").error == "\"temperature\" must be a number.");
    CHECK(Parse(kMinimal + ", \"port\": \"5114\"}").error.find("\"port\"") == 0);
    CHECK(!Parse(kMinimal + ", \"port\": 70000}").Usable());
    CHECK(Parse(kMinimal + ", \"args\": \"--vad\"}").error == "\"args\" must be an array of strings.");
    CHECK(!Parse(kMinimal + ", \"args\": [true]}").Usable());
    CHECK(!Parse(kMinimal + ", \"env\": {\"A\": true}}").Usable());
    CHECK(!Parse(kMinimal + ", \"env\": []}").Usable());
    CHECK(!Parse(kMinimal + ", \"aligner\": 3}").Usable());

    // Values the launcher accepts in other shapes.
    const Profile shapes = Parse(kMinimal + ", \"device\": 1, \"port\": 0, \"aligner\": null, "
                                            "\"env\": {\"GGML_CUDA_DEBUG\": \"1\", \"N\": 2.50}, \"args\": [\"-t\", 8]}");
    CHECK(shapes.Usable());
    CHECK(shapes.device == "1");
    CHECK(shapes.port == 0);
    CHECK(!shapes.HasAligner());
    CHECK(shapes.env.size() == 2 && shapes.env[0].first == "GGML_CUDA_DEBUG" && shapes.env[1].second == "2.5");
    CHECK(shapes.args.size() == 2 && shapes.args[1] == "8");
    CHECK(shapes.Find("aligner") == nullptr);

    // Keys ignore case, as PowerShell property access does.
    const Profile mixedCase = Parse("{\"Build\": \"cuda13\", \"BACKEND\": \"qwen3\", \"Model\": \"m.gguf\", \"VAD\": true}");
    CHECK(mixedCase.Usable() && mixedCase.backend == "qwen3" && mixedCase.Find("vad") != nullptr);
    CHECK(Parse(kMinimal + ", \"Model\": \"other.gguf\"}").error.find("differ only in case") != std::string::npos);
    const Profile repeated = Parse(kMinimal + ", \"model\": \"second.gguf\"}");
    CHECK(repeated.Usable() && repeated.model == "second.gguf");
    CHECK(HasWarning(repeated, "more than once"));

    // Unknown keys stay in raw; unknown flags in args[] are warnings, not errors.
    const Profile unknown = Parse(kMinimal + ", \"gui\": {\"order\": [\"vad\"]}, \"args\": [\"--future-flag\", \"1\"]}");
    CHECK(unknown.Usable());
    CHECK(unknown.Find("gui") != nullptr && unknown.Find("gui")->IsObject());
    CHECK(HasWarning(unknown, "Unknown flag --future-flag"));
    CHECK(HasWarning(Parse(kMinimal + ", \"args\": [\"--vad-threshold\"]}"), "needs a value"));
}

void CheckPaths() {
    const fs::path base = fs::u8path("models");
    const fs::path expected = (base / "Japanese" / "a.gguf").make_preferred();
    CHECK(JoinProfilePath(base, "Japanese\\a.gguf") == expected);
    CHECK(JoinProfilePath(base, "Japanese/a.gguf") == expected);
    CHECK(JoinProfilePath(base, "\\Japanese\\a.gguf") == expected);  // Join-Path drops leading separators
    CHECK(JoinProfilePath(base, "E:\\models\\a.gguf").empty());
    CHECK(JoinProfilePath(base, "c:a.gguf").empty());
    CHECK(JoinProfilePath(base, "[CKHO]x\\y.gguf") == (base / "[CKHO]x" / "y.gguf").make_preferred());
}

void CheckScan(const fs::path& data) {
    const fs::path root = crisp::test::FreshScratchDirectory("profile_store_test.scratch") / "CrispASR";
    const CrispAsrPaths paths = MakeCrispAsrPaths(root);
    CHECK(paths.profiles == root / "profile" && paths.models == root / "models");

    // The deployment: an executable, the cohere model, the aligner; the other models are missing.
    CHECK(WriteFile(root / "cuda13" / crisp::core::kServerExecutableName, "stub"));
    CHECK(WriteFile(JoinProfilePath(paths.models, kReferenceProfiles[0].model), "model"));
    CHECK(WriteFile(JoinProfilePath(paths.models, kAligner), "aligner"));
    for (const ReferenceProfile& reference : kReferenceProfiles) {
        const std::string file = std::string(reference.name) + ".conf";
        CHECK(WriteFile(paths.profiles / "Japanese" / file, ReadBytes(data / file)));
    }
    CHECK(WriteFile(paths.profiles / "Japanese" / "broken.conf", "{ \"model\": "));
    CHECK(WriteFile(paths.profiles / "Japanese" / "notes.txt", "not a profile"));
    CHECK(WriteFile(paths.profiles / "Japanese" / "backup.conf.bak", "{}"));
    CHECK(WriteFile(paths.profiles / "english" / "Sample.JSON",
                    "{\"build\": \"cuda13\", \"backend\": \"whisper\", \"model\": \"English\\\\missing.bin\"}"));
    CHECK(WriteFile(paths.profiles / "top-level.conf", kMinimal + "}"));
#if !defined(_WIN32)
    CHECK(WriteFile(paths.profiles / "Japanese" / ".hidden.conf", kMinimal + "}"));
    CHECK(WriteFile(paths.profiles / ".trash" / "old.conf", kMinimal + "}"));
#endif

    const ProfileScan scan = ScanProfiles(root);
    CHECK(scan.Ok());
    CHECK(scan.folders.size() == 2);
    if (scan.folders.size() != 2) {
        return;
    }
    // Sorted without regard to case, as the launcher's Sort-Object sorts.
    CHECK(scan.folders[0].name == "english");
    CHECK(scan.folders[1].name == "Japanese");
    CHECK(scan.ProfileCount() == 6);

    const std::vector<Profile>& japanese = scan.folders[1].profiles;
    CHECK(japanese.size() == 5);
    if (japanese.size() == 5) {
        CHECK(japanese[0].name == "broken" && !japanese[0].Usable());
        CHECK(japanese[1].name == "cohere-asr-ja-q8");
        CHECK(japanese[1].executableOk && japanese[1].modelOk && japanese[1].alignerOk);
        CHECK(japanese[1].modelPath == JoinProfilePath(paths.models, kReferenceProfiles[0].model));
        CHECK(japanese[1].folder == "Japanese");
        CHECK(japanese[2].name == "kotoba-whisper-v2.2-q8");
        CHECK(japanese[2].executableOk && !japanese[2].modelOk && japanese[2].alignerOk);
        CHECK(japanese[4].name == "qwen3-asr-1.7b-ja-anime-q8");
        for (const Profile& profile : japanese) {
            CHECK(profile.name != ".hidden" && profile.name != "notes");
        }
    }
    const std::vector<Profile>& english = scan.folders[0].profiles;
    CHECK(english.size() == 1 && english[0].name == "Sample" && english[0].Usable());
    CHECK(english.size() == 1 && !english[0].modelOk && !english[0].HasAligner());

    // Unicode folder and file names.
    const fs::path unicodeFile = paths.profiles / fs::u8path("日本語") / fs::u8path("テスト.conf");
    CHECK(WriteFile(unicodeFile, kMinimal + "}"));
    const Profile unicode = LoadProfile(unicodeFile, paths);
    CHECK(unicode.Usable());
    CHECK(unicode.folder == "日本語" && unicode.name == "テスト");

    // Failures to read.
    CHECK(LoadProfile(root / "absent.conf", paths).error.find("could not be read") != std::string::npos);
    CHECK(WriteFile(root / "huge.conf", std::string(crisp::core::kMaxProfileBytes + 1, ' ')));
    CHECK(LoadProfile(root / "huge.conf", paths).error.find("larger than") != std::string::npos);
    CHECK(!ScanProfiles(root / "nowhere").Ok());
    CHECK(!ScanProfiles(fs::path()).Ok());

    std::error_code error;
    fs::remove_all(root.parent_path(), error);
}

}  // namespace

int main() {
    const fs::path data = crisp::test::TestDataDirectory() / "profiles-japanese";
    std::error_code error;
    if (!fs::is_directory(data, error)) {
        std::fprintf(stderr, "profile_store_test: test data missing at %s\n", data.u8string().c_str());
        return 1;
    }
    CheckReferenceProfiles(data);
    CheckParsing();
    CheckPaths();
    CheckScan(data);
    return crisp::test::Result();
}
