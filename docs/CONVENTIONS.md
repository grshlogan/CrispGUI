# Conventions

The hard rules and the language rule (English in the repository, Chinese with the owner) are in [AGENTS.md](../AGENTS.md); they apply to every change. This document holds the coding conventions.

## UI strings

- User-facing UI strings: English first, with a Chinese translation planned (EUI-NEO has full CJK and IME support). Do not hardcode strings in UI code; route them through a string table from day one.
- Parameter labels and help texts come from the string table through the keys in the parameter catalog ([ARCHITECTURE.md §2.7](ARCHITECTURE.md#27-parameter-model-pluggable)).
- Text styles: body text and components use the default font (HarmonyOS Sans SC Regular when present); section headings use `UiFonts::medium` and page titles `UiFonts::title` (`src/ui/fonts.h`). Wrapped text sets `maxWidth` as well as `width`, because EUI-NEO measures the height with `maxWidth` ([ENVIRONMENT.md §4.4](ENVIRONMENT.md#44-eui-neo-runtime-facts)).
- Messages from `src/core/` (parse errors, readiness problems) are English technical detail; the UI shows them after a translated prefix from the string table.

## C++ conventions

- C++17. No C++20/23 features.
- Formatting: clang-format with `.clang-format` (LLVM-based, 4-space indent, 120 columns). No style debates in review; run the formatter. `clang-format` is not installed yet, so code follows the style by hand until it is.
- Naming: `PascalCase` for types and functions, `camelCase` for variables and fields, `kConstantName` for constants, files named after their primary type (`server_manager.h` / `ServerManager`). Entry points that EUI-NEO requires keep its names (`app::dslAppConfig`, `app::compose`).
- Errors: application code uses neither exceptions nor RTTI, because EUI-NEO's `eui_neo_configure_app()` compiles the application target with `-fno-exceptions -fno-rtti` in every configuration except Debug ([ENVIRONMENT.md §4.2](ENVIRONMENT.md#42-eui-neo-build-facts)). The project's own targets add both flags in every configuration, so a violation fails in Debug too. Module APIs return `Result<T>`-style values (an expected/optional or a small custom type), code prefers non-throwing standard APIs (`std::from_chars`, the `std::error_code` overloads of `std::filesystem`), and every error carries a user-actionable message.
- No global mutable state. Singletons only for logging and the settings store. EUI-NEO's entry points are free functions, so `src/main.cpp` owns the single `Application` object, the composition root, and all other application state lives in it.
- Warnings: project targets build with `-Wall -Wextra` and must stay warning-free. EUI-NEO is added as a system subdirectory, so its headers' warnings stay out of the build output.
- Callbacks: EUI-NEO stores `onClick`/`onChange` callbacks and runs them after `compose` returns, so a callback captures by value or captures `this` of a long-lived object, never references to locals.
- Threads: see [ARCHITECTURE.md §2.6](ARCHITECTURE.md#26-threading-model). Never touch UI state from a worker thread; use EUI-NEO async completion callbacks or the UI inbox plus `app::requestUpdate()`.
- Memory: `std::unique_ptr`/`std::vector` by default; raw `new`/`delete` are forbidden. C-style interop (the EUI-NEO API, Win32, POSIX, WinHTTP, libcurl) stays behind module boundaries.
- Portability: platform-specific code lives in `src/platform/` with Windows and Linux implementations; everything else is platform-neutral, because development builds run on Linux. The Windows build runs on `msvcrt.dll` (MinGW-w64), so paths and process arguments go through the wide-character Win32 APIs (`CreateProcessW`, `_wfopen`), with UTF-8 converted to UTF-16 at the platform boundary.
- Parameters: no UI code path for a specific model or parameter. A new parameter is catalog data, and a new model is a profile, optionally with a manifest.

## Git conventions

Commits and pushes need the owner's authorization ([AGENTS.md](../AGENTS.md#repository-operations)); the remote and the commit identity are in [agents/README.md](agents/README.md#checkouts).

- Default branch: `main`. Commit per completed task. Subject format: `phase-N: short imperative summary` for phase work (for example `phase-2: JSONC parser with string-aware comment stripping`) and `docs: short imperative summary` for documentation-only changes.
- Do not commit build outputs. EUI-NEO is not copied into the repository ([ENVIRONMENT.md §3](ENVIRONMENT.md#3-repository-layout-policy)).
- Large generated files never enter the repository; audio fixtures stay small or outside Git.

## Testing expectations

- Unit tests are plain executables built on `tests/check.h`, one per test, registered with CTest under the `unit` label; no third-party test framework. Windows deliveries carry them, and `run-tests.cmd` runs them on the owner's machine.
- Unit tests for: the JSONC parser (including escaped quotes inside strings), profile readiness, `args[]` parsing and the launcher's apply conditions, the parameter resolver (profile, manifest, and capability sources), the request builder, job state transitions, the persistence round trip (including a torn last line), the window loop (boundary guard, retry classification, resume), and SRT writer equivalence with the server's format.
- HTTP tests run against a local stub server, with no real engine. Integration tests with the real server run only on Windows and are gated behind a profile selection.
- Every bug fix gets a regression test.
