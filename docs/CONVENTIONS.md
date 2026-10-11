# Conventions

The hard rules and the language rule (English in the repository, Chinese with the owner) are in [AGENTS.md](../AGENTS.md); they apply to every change. This document holds the coding conventions.

## UI strings

- User-facing UI strings: English first, with a Chinese translation planned (EUI-NEO has full CJK and IME support). Do not hardcode strings in UI code; route them through a string table from day one.
- Parameter labels and help texts come from the string table through the keys in the parameter catalog ([ARCHITECTURE.md §2.7](ARCHITECTURE.md#27-parameter-model-pluggable)).

## C++ conventions

- C++17. No C++20/23 features.
- Formatting: clang-format with the style file added in Phase 1 (LLVM-based, 100 columns). No style debates in review; run the formatter.
- Naming: `PascalCase` for types and functions, `camelCase` for variables and fields, `kConstantName` for constants, files named after their primary type (`server_manager.h` / `ServerManager`). Entry points that EUI-NEO requires keep its names (`app::dslAppConfig`, `app::compose`).
- Errors: application code uses neither exceptions nor RTTI, because EUI-NEO's `eui_neo_configure_app()` compiles the application target with `-fno-exceptions -fno-rtti` in every configuration except Debug ([ENVIRONMENT.md §4.2](ENVIRONMENT.md#42-eui-neo-build-facts)). Module APIs return `Result<T>`-style values (an expected/optional or a small custom type), code prefers non-throwing standard APIs (`std::from_chars`, the `std::error_code` overloads of `std::filesystem`), and every error carries a user-actionable message.
- No global mutable state. Singletons only for logging and the settings store.
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

- Unit tests for: the JSONC parser (including escaped quotes inside strings), profile readiness, `args[]` parsing and the launcher's apply conditions, the parameter resolver (profile, manifest, and capability sources), the request builder, job state transitions, the persistence round trip (including a torn last line), the window loop (boundary guard, retry classification, resume), and SRT writer equivalence with the server's format.
- HTTP tests run against a local stub server, with no real engine. Integration tests with the real server run only on Windows and are gated behind a profile selection.
- Every bug fix gets a regression test.
