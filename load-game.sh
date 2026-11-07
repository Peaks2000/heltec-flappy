#!/usr/bin/env bash
set -euo pipefail
if [[ "${1:-}" == '--keep-open' ]]; then
    shift
    trap 'result=$?; printf "\nPress Enter to close. "; read -r _ || true; exit "$result"' EXIT
fi
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
runtime="$script_dir/.venv/bin/python"
if [[ ! -x "$runtime" ]] || ! "$runtime" -c 'import esptool' >/dev/null 2>&1; then
    bash "$script_dir/setup.sh"
fi
"$runtime" "$script_dir/load_game.py" "$@"
