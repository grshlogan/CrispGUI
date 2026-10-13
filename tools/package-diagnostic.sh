#!/usr/bin/env bash
# Packages one diagnostic tool of the MinGW release build as a small Windows delivery
# (docs/agents/README.md): the executable and a BUILD-INFO.txt, zipped, with the zip's SHA-256.
# Usage: tools/package-diagnostic.sh <target> [handoff-out]
# Requires a clean tree whose HEAD is on origin/main, and a release build in build/mingw
# (docs/ENVIRONMENT.md, section 2.2).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
target="${1:?usage: tools/package-diagnostic.sh <target> [handoff-out]}"
build="$root/build/mingw"
out="${2:-$(dirname "$root")/CrispGUI-handoff/outgoing}"

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

name="crispgui-diag-$target-${sha:0:7}"
stage="$out/$name"
rm -rf "$stage" "$out/$name.zip" "$out/$name.zip.sha256"
mkdir -p "$stage"
cp "$build/$target.exe" "$stage/"
{
  echo "CrispGUI diagnostic: $target.exe"
  echo "Commit: $sha (on origin/main)"
  echo "Packaged: $(date -u +%Y-%m-%dT%H:%M:%SZ) in WSL2 ($WSL_DISTRO_NAME)"
  echo "Toolchain: MinGW-w64 GCC $(dpkg-query -W -f='${Version}' g++-mingw-w64-x86-64-posix), $(cmake --version | head -1)"
  echo "Source: tools/diagnostics/$target.cpp; its first comment says what it does and how to run it"
} | sed 's/$/\r/' > "$stage/BUILD-INFO.txt"

(cd "$out" && cmake -E tar cf "$name.zip" --format=zip "$name" && sha256sum "$name.zip" > "$name.zip.sha256")
echo "$out/$name.zip"
