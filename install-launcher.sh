#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
python3 - "$script_dir" <<'PY'
import os
import sys
from pathlib import Path

root = Path(sys.argv[1])
applications = Path(os.environ.get('XDG_DATA_HOME', str(Path.home()/'.local/share'))) / 'applications'
applications.mkdir(parents=True, exist_ok=True)
script = str(root/'load-game.sh')
for original, escaped in [('\\', '\\\\'), ('"', '\\"'), ('`', '\\`'), ('$', '\\$'), ('%', '%%')]:
    script = script.replace(original, escaped)
entry = '\n'.join([
    '[Desktop Entry]', 'Type=Application', 'Name=Heltec Flappy',
    f'Exec="{script}" --keep-open', f'Path={root}',
    'Terminal=true', 'Icon=applications-games', 'Categories=Game;', '',
])
(applications/'heltec-flappy.desktop').write_text(entry)
print('Installed Heltec Flappy in the application menu.')
PY
