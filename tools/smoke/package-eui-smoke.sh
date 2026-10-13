#!/usr/bin/env bash
# Packages the MinGW EUI-NEO smoke build as a Windows delivery (docs/agents/README.md).
# Usage: tools/smoke/package-eui-smoke.sh [handoff-out]
# Requires a clean tree whose HEAD is on origin/main, and a build from tools/smoke/build-eui-smoke.sh.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$root/build/eui-smoke-mingw"
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

name="crispgui-eui-smoke-${sha:0:7}"
stage="$out/$name"
rm -rf "$stage" "$out/$name.zip" "$out/$name.zip.sha256"
mkdir -p "$stage"

cp "$build/eui_demo.exe" "$build/multipage_table.exe" "$stage/"
cp -r "$build/assets" "$stage/"
: > "$stage/tests.txt"
for kind in unit probe; do
  for test in $(ctest --test-dir "$build" -N -L "^$kind\$" | sed -n 's/^ *Test *#[0-9]*: *//p'); do
    # shadertoy_probe writes into its compile-time source path, which does not exist on Windows.
    [ "$test" = shadertoy_probe ] && continue
    cp "$build/$test.exe" "$stage/"
    printf '%s %s\r\n' "$kind" "$test" >> "$stage/tests.txt"
  done
done
for script in run-tests.cmd run-tests.ps1; do
  sed 's/\r*$/\r/' "$root/tools/windows/$script" > "$stage/$script"
done

mingw_version="$(dpkg-query -W -f='${Version}' g++-mingw-w64-x86-64-posix)"
headers_version="$(dpkg-query -W -f='${Version}' mingw-w64-x86-64-dev)"
{
  echo "CrispGUI Phase 0 smoke delivery"
  echo "Commit: $sha (on origin/main)"
  echo "Packaged: $(date -u +%Y-%m-%dT%H:%M:%SZ) in WSL2 ($WSL_DISTRO_NAME)"
  echo "Toolchain: MinGW-w64 GCC $mingw_version, headers $headers_version, $(cmake --version | head -1), Ninja $(ninja --version)"
  echo "Toolchain file: cmake/toolchains/mingw-w64-x86_64.cmake, blob $(git -C "$root" rev-parse HEAD:cmake/toolchains/mingw-w64-x86_64.cmake)"
  echo "EUI-NEO: v0.6.1, commit 1647709068e36834ab26da5a97ef4ad98aa34c9d, built by tools/smoke/build-eui-smoke.sh"
  echo "Visual check: eui_demo.exe, multipage_table.exe"
  echo "Automatic check: run-tests.cmd runs $(grep -c '^unit ' "$stage/tests.txt") unit tests and $(grep -c '^probe ' "$stage/tests.txt") probes"
} | sed 's/$/\r/' > "$stage/BUILD-INFO.txt"

(cd "$out" && cmake -E tar cf "$name.zip" --format=zip "$name" && sha256sum "$name.zip" > "$name.zip.sha256")
echo "$out/$name.zip"
