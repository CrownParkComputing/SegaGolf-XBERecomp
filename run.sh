#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build="$root/out/build/linux-release"
if [ ! -x "$build/SegaGolf" ]; then
    printf 'Build SegaGolf first; see %s/README.md\n' "$root" >&2
    exit 1
fi
exec python3 "$root/tools/run_linked.py" "$@"
