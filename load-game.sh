#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
runtime="$script_dir/.venv/bin/python"
if [[ ! -x "$runtime" ]]; then
    printf '%s\n' 'Run ./setup.sh first.' >&2
    exit 1
fi
exec "$runtime" "$script_dir/load_game.py" "$@"
