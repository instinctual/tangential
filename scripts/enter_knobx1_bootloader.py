#!/usr/bin/env python3
"""Enter the KnobX1 bootloader through Vial's physical unlock flow.

This is a separate, state-changing development tool. It changes only the volatile
Vial lock state and reboots into the existing bootloader. It cannot flash, erase,
reset EEPROM, or alter mappings. Prefer it to Bootmagic's EEPROM-reset gesture.
"""

import argparse
import json
import os
from pathlib import Path
import select
import time

from inspect_knobx1 import discover


def make_command(command):
    if command not in ((0xFE, 0), (0xFE, 5), (0xFE, 6), (0xFE, 7), (0xFE, 8), (0x0B,)):
        raise ValueError("Only Vial identity, lock flow and bootloader entry are permitted")
    return b"\0" + bytes(command).ljust(32, b"\0")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--enter-bootloader", action="store_true", required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--wait-seconds", type=int, default=150)
    args = parser.parse_args()
    if not 10 <= args.wait_seconds <= 300:
        parser.error("--wait-seconds must be between 10 and 300")
    expected_uid = json.loads(args.baseline.read_text())["vial_keyboard_uid"]
    interfaces = [i for i in discover()["interfaces"] if i["configuration_interface"]]
    if len(interfaces) != 1:
        raise RuntimeError("Expected exactly one KnobX1 configuration interface")
    fd = os.open(interfaces[0]["path"], os.O_RDWR | os.O_NONBLOCK)
    jumping = False
    unlock_started = False
    unlocked_by_us = False

    def exchange(command):
        while select.select([fd], [], [], 0)[0]:
            os.read(fd, 64)
        if os.write(fd, make_command(command)) != 33:
            raise RuntimeError("Short HID command write")
        if not select.select([fd], [], [], 2)[0]:
            raise TimeoutError("No Vial reply")
        reply = os.read(fd, 64)
        if len(reply) != 32:
            raise RuntimeError("Unexpected Vial reply size")
        return reply

    try:
        identity = exchange((0xFE, 0))
        if identity[4:12].hex() != expected_uid or int.from_bytes(identity[:4], "little") != 6:
            raise RuntimeError("Vial identity does not match the saved KnobX1 baseline")
        status = exchange((0xFE, 5))
        if status[2:4] != bytes((0, 1)) or any(b != 255 for b in status[4:]):
            raise RuntimeError("Unexpected physical unlock combination")
        if not status[0]:
            exchange((0xFE, 6))
            unlock_started = True
            print("Ready: hold the LARGE KnobX1 knob down for seven seconds, then release.", flush=True)
            deadline = time.monotonic() + args.wait_seconds
            last_count = None
            while time.monotonic() < deadline:
                time.sleep(0.12)
                status = exchange((0xFE, 7))
                if status[2] != last_count and (status[2] % 10 == 0 or status[0]):
                    print("Unlock counter:", status[2], "unlocked:", bool(status[0]), flush=True)
                    last_count = status[2]
                if status[0]:
                    unlocked_by_us = True
                    break
            else:
                raise TimeoutError("Physical unlock did not complete")
        print("Physical unlock complete; requesting bootloader entry. No firmware is being written.", flush=True)
        # A successful reboot may disconnect before the application replies.
        if os.write(fd, make_command((0x0B,))) != 33:
            raise RuntimeError("Short bootloader request write")
        jumping = True
    finally:
        if not jumping and unlocked_by_us:
            try:
                os.write(fd, make_command((0xFE, 8)))
            except OSError:
                pass
        elif not jumping and unlock_started:
            print("Unlock did not finish. Unplug/reconnect the KnobX1 to cancel its volatile unlock state.", flush=True)
        os.close(fd)


if __name__ == "__main__":
    main()

