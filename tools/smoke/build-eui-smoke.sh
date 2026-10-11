#!/usr/bin/env bash
# Phase 0 smoke builds of EUI-NEO from the shared Learn library (docs/TASKS.md, Phase 0).
# Usage: tools/smoke/build-eui-smoke.sh [linux|mingw|all]    (default: all)
# Writes only under build/. Never run EUI-NEO's shadertoy_probe against the Learn snapshot:
# it writes into its compile-time source path (tests/probes/shadertoy_probe.cpp:516).
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
eui="${CRISPGUI_EUI_NEO_DIR:-$root/../Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1}"
target="${1:-all}"

case "$target" in
  linux|mingw|all) ;;
  *) echo "usage: $0 [linux|mingw|all]" >&2; exit 2 ;;
esac
if [ ! -f "$eui/CMakeLists.txt" ]; then
  echo "EUI-NEO not found at $eui; set CRISPGUI_EUI_NEO_DIR" >&2
  exit 1
fi

common=(-G Ninja -DCMAKE_BUILD_TYPE=Release
        -DEUI_DEPS_MODE=bundled -DFETCHCONTENT_FULLY_DISCONNECTED=ON
        -DEUI_BUILD_APPS=ON -DEUI_BUILD_USER_APPS=OFF -DEUI_ENABLE_MODULES=OFF
        -DEUI_ENABLE_INSTALL=OFF -DEUI_BUILD_TEST_FIXTURES=ON)

build() {
  local dir="$root/build/eui-smoke-$1"
  shift
  cmake -S "$eui" -B "$dir" "${common[@]}" "$@"
  cmake --build "$dir"
}

if [ "$target" != mingw ]; then
  build linux -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=ON
fi
if [ "$target" != linux ]; then
  # -static also covers the test executables, which EUI-NEO links without its static options.
  build mingw -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/mingw-w64-x86_64.cmake" \
    -DCMAKE_EXE_LINKER_FLAGS=-static
fi
