import argparse
import os
import sys
import time
from pathlib import Path

import esptool
from esptool.bin_image import LoadFirmwareImage
from serial.tools import list_ports

ROOT = Path(__file__).resolve().parent


def image_bytes():
    data = (ROOT / 'firmware/game.bin').read_bytes()
    image = LoadFirmwareImage('ESP32-S3', data)
    if image.chip_id != 9 or image.checksum != image.calculate_checksum():
        raise ValueError('Invalid ESP32-S3 game image')
    if not image.segments:
        raise ValueError('Empty game image')
    for segment in image.segments:
        start, end = segment.addr, segment.addr + len(segment.data)
        if not (0x40378000 <= start < end <= 0x40390000 or
                0x3fca0000 <= start < end <= 0x3fcc0000):
            raise ValueError('Image contains a segment outside the game RAM regions')
    if not any(0x40378000 <= s.addr <= image.entrypoint < s.addr + len(s.data)
               for s in image.segments):
        raise ValueError('Entry point is outside instruction RAM')
    return data


def main():
    parser = argparse.ArgumentParser(description='Load Flappy into the Heltec V3 RAM.')
    parser.add_argument('--port', help='Serial port, such as /dev/ttyUSB0 or COM3')
    parser.add_argument('--mac', help='Optional expected ESP32 base MAC address')
    parser.add_argument('--check', action='store_true', help='Validate the image without connecting')
    args = parser.parse_args()
    data = image_bytes()
    if args.check:
        print(f'Valid RAM-only game image: {len(data)} bytes. No device accessed.')
        return
    port = args.port
    if not port:
        candidates = [p.device for p in list_ports.comports()
                      if p.vid == 0x10c4 and p.pid == 0xea60]
        if len(candidates) != 1:
            raise ValueError('Specify --port for your Heltec V3; automatic selection requires exactly one CP2102 adapter')
        port = candidates[0]
    if os.name == 'posix' and os.path.exists(port) and not os.access(port, os.R_OK | os.W_OK):
        raise PermissionError(f'Linux denied access to {port}. Run ./setup-usb.sh once, then try again.')
    esp = esptool.detect_chip(port=port, baud=115200)
    try:
        if esp.CHIP_NAME != 'ESP32-S3':
            raise ValueError('An ESP32-S3 is required; nothing loaded')
        actual_mac = ':'.join(f'{b:02x}' for b in esp.read_mac())
        if args.mac and args.mac.lower().replace('-', ':') != actual_mac:
            raise ValueError('Device MAC does not match; nothing loaded')
        esptool.load_ram(esp, data)
        esp._port.timeout = 0.5
        received = bytearray()
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            received.extend(esp._port.read(1))
            if b'RAM_FLAPPY_READY OLED_ACK=1\r\n' in received:
                print('Ready. Press PRG to play and jump. RST returns to Meshtastic.')
                return
        raise RuntimeError('Game did not acknowledge its display; press RST to return to Meshtastic')
    finally:
        esp._port.close()


if __name__ == '__main__':
    try:
        main()
    except (Exception, KeyboardInterrupt) as error:
        print(f'Load stopped: {error}', file=sys.stderr)
        if not isinstance(error, PermissionError):
            print('Disconnect the node in other apps and check the selected USB port.', file=sys.stderr)
        print('No flash-write or erase operation was used.', file=sys.stderr)
        sys.exit(1)
