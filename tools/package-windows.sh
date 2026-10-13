#!/usr/bin/env bash
# Packages the MinGW release build of CrispGUI as a Windows delivery (docs/agents/README.md).
# Usage: tools/package-windows.sh [handoff-out]
# Requires a clean tree whose HEAD is on origin/main, and a release build in build/mingw
# (docs/ENVIRONMENT.md, section 2.2).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/mingw"
out="${1:-$(dirname "$root")/CrispGUI-handoff/outgoing}"

if [ -n "$(git -C "$root" status --porcelain)" ]; then
  echo "the working tree is not clean" >&2
  exit 1
fi
sha="$(git -C "$root" rev-parse HEAD)"
if ! git -C "$root" merge-base --is-ancestor "$sha" origin/main; then
  echo "HEAD ${sha:0:7} is not on origin/main; push it first" >&2
  exit 1
fi
if ! grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' "$build/CMakeCache.txt"; then
  echo "$build is not a release build" >&2
  exit 1
fi

name="crispgui-${sha:0:7}"
stage="$out/$name"
rm -rf "$stage" "$out/$name.zip" "$out/$name.zip.sha256"
mkdir -p "$stage"

cp "$build/crispgui.exe" "$stage/"
cp -r "$build/assets" "$stage/"
# The owner-provided UI fonts (not in Git); without them the UI uses EUI-NEO's default font.
if [ -d "$build/fonts" ]; then
  cp -r "$build/fonts" "$stage/"
fi
# Test data that the unit tests read next to their executables.
cp -r "$build/tests/testdata" "$stage/"
: > "$stage/tests.txt"
for kind in unit integration; do
  for test in $(ctest --test-dir "$build" -N -L "^$kind\$" | sed -n 's/^ *Test *#[0-9]*: *//p'); do
    cp "$build/tests/$test.exe" "$stage/"
    printf '%s %s\r\n' "$kind" "$test" >> "$stage/tests.txt"
  done
done
# The stand-in CrispASR server that the integration tests start (tests/stub_crispasr.cpp), and
# the probe of a running server (tools/api_probe), which api_probe_test also runs.
cp "$build/tests/stub_crispasr.exe" "$stage/"
cp "$build/api_probe.exe" "$stage/"
# The test runner, the probe's wrapper, the checks of this delivery, and the brief for the
# Windows-side agent, with CRLF line ends.
for file in run-tests.cmd run-tests.ps1 run-api-probe.cmd CHECKLIST.md WINDOWS-AGENT.md; do
  sed 's/\r*$/\r/' "$root/tools/windows/$file" > "$stage/$file"
done

mingw_version="$(dpkg-query -W -f='${Version}' g++-mingw-w64-x86-64-posix)"
headers_version="$(dpkg-query -W -f='${Version}' mingw-w64-x86-64-dev)"
{
  echo "CrispGUI delivery"
  echo "Commit: $sha (on origin/main)"
  echo "Packaged: $(date -u +%Y-%m-%dT%H:%M:%SZ) in WSL2 ($WSL_DISTRO_NAME)"
  echo "Toolchain: MinGW-w64 GCC $mingw_version, headers $headers_version, $(cmake --version | head -1), Ninja $(ninja --version)"
  echo "EUI-NEO: v0.6.1, commit 1647709068e36834ab26da5a97ef4ad98aa34c9d"
  if [ -d "$stage/fonts" ]; then
    echo "UI fonts (owner-provided, not in Git):"
    (cd "$stage/fonts" && sha256sum -- *.ttf | sed 's/^/  /')
  else
    echo "UI fonts: none; EUI-NEO's default font"
  fi
  echo "Run: crispgui.exe; run-tests.cmd runs $(grep -c '^unit ' "$stage/tests.txt") unit and $(grep -c '^integration ' "$stage/tests.txt") integration tests"
  echo "Checks: CHECKLIST.md lists the Windows checks of this delivery and what to send back"
} | sed 's/$/\r/' > "$stage/BUILD-INFO.txt"

(cd "$out" && cmake -E tar cf "$name.zip" --format=zip "$name" && sha256sum "$name.zip" > "$name.zip.sha256")
echo "$out/$name.zip"
