#!/usr/bin/env bash
# Runs build/linux/crisprustgui of this checkout under WSLg with the current environment, saves
# window captures after the given delays (seconds since start) as <prefix>-<delay>.png, closes the
# window, and prints the exit code. GUI_SIZE=<width>x<height> resizes the window first.
# Example: GUI_SIZE=1100x720 CRISPGUI_CRISPASR_ROOT=tools/dev/fake-deployment tools/dev/guirun.sh /tmp/x/dash 3
set -u
prefix="$1"; shift
tools="$(cd "$(dirname "$0")" && pwd)"
cd "$tools/../.."
build/linux/crisprustgui > "$prefix.log" 2>&1 &
pid=$!
if [ -n "${GUI_SIZE:-}" ]; then
  sleep 1
  python3 -I "$tools/xctl.py" resize "$pid" "${GUI_SIZE%x*}" "${GUI_SIZE#*x}"
  elapsed=1
else
  elapsed=0
fi
for delay in "$@"; do
  sleep $((delay - elapsed)); elapsed=$delay
  python3 -I "$tools/xctl.py" shot "$pid" "$prefix-$delay.png"
done
python3 -I "$tools/xctl.py" close "$pid"
wait "$pid"
echo "exit=$? output=$(wc -c < "$prefix.log") bytes"
