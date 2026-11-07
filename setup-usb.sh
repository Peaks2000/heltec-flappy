#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
if (( EUID != 0 )); then
    exec pkexec /usr/bin/bash "$script_dir/setup-usb.sh"
fi
install -m 644 "$script_dir/70-heltec-flappy.rules" /etc/udev/rules.d/70-heltec-flappy.rules
udevadm control --reload-rules
udevadm trigger --action=change --subsystem-match=tty --property-match=ID_VENDOR_ID=10c4 --property-match=ID_MODEL_ID=ea60
udevadm settle
printf '%s\n' 'USB access configured for the active desktop user.'
