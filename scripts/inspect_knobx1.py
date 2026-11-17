#!/usr/bin/env python3
"""Read KnobX1 metadata/mappings or passively record its input HID reports.

Standard-library only. Device commands are restricted to an explicit allowlist
of VIA/Vial getters. This script cannot change mappings, unlock, reset, enter
the bootloader, or flash firmware. The snapshot is not a firmware backup.
"""

import argparse
import datetime
import json
import lzma
import os
from pathlib import Path
import select
import socket
import time


DEVICE_ID = "0003:00005831:00004249"
RAW_DESCRIPTOR = bytes.fromhex(
    "06 60 ff 09 61 a1 01 09 62 15 00 26 ff 00 95 20 75 08 "
    "81 02 09 63 15 00 26 ff 00 95 20 75 08 91 02 c0"
)
SOURCES = [
    "https://github.com/vial-kb/vial-qmk/blob/vial/quantum/via.h",
    "https://github.com/vial-kb/vial-qmk/blob/vial/quantum/via.c",
    "https://github.com/vial-kb/vial-qmk/blob/vial/quantum/vial.h",
    "https://github.com/vial-kb/vial-qmk/blob/vial/quantum/vial.c",
]


def timestamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def discover():
    interfaces = []
    for node in sorted(Path("/sys/class/hidraw").glob("hidraw*")):
        device = node / "device"
        properties = dict(
            line.split("=", 1)
            for line in (device / "uevent").read_text().splitlines()
            if "=" in line
        )
        if properties.get("HID_ID") != DEVICE_ID:
            continue
        descriptor = (device / "report_descriptor").read_bytes()
        interfaces.append({
            "path": str(Path("/dev") / node.name),
            "sysfs": str(device.resolve()),
            "properties": properties,
            "report_descriptor_hex": descriptor.hex(),
            "configuration_interface": descriptor == RAW_DESCRIPTOR,
        })
    if not interfaces:
        raise RuntimeError("No KnobX1 HID interfaces found")
    usb_devices = []
    for node in Path("/sys/bus/usb/devices").glob("*"):
        try:
            vendor = (node / "idVendor").read_text().strip()
            product = (node / "idProduct").read_text().strip()
        except (FileNotFoundError, NotADirectoryError):
            continue
        if (vendor, product) != ("5831", "4249"):
            continue
        entry = {"sysfs": str(node)}
        for name in (
            "manufacturer", "product", "serial", "bcdDevice", "speed",
            "busnum", "devnum", "bNumInterfaces", "bMaxPower",
        ):
            entry[name] = (node / name).read_text().strip()
        entry["usb_descriptors_hex"] = (node / "descriptors").read_bytes().hex()
        usb_devices.append(entry)
    if len(usb_devices) != 1:
        raise RuntimeError("Expected exactly one physical KnobX1")
    return {"host": socket.gethostname(), "utc": timestamp(),
            "usb": usb_devices[0], "interfaces": interfaces}


class ReadOnlyVial:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_NONBLOCK)
        self.exchanges = []

    def close(self):
        os.close(self.fd)

    def get(self, *command):
        allowed = (
            command == (0x01,)
            or command == (0x11,)
            or (len(command) == 2 and command[0] == 0x02
                and command[1] in (0x01, 0x02, 0x04))
            or (len(command) == 4 and command[0] == 0x04)
            or (len(command) == 2 and command[0] == 0xFE
                and command[1] in (0x00, 0x01, 0x05))
            or (len(command) == 4 and command[:2] in ((0xFE, 0x02), (0xFE, 0x03)))
        )
        if not allowed:
            raise ValueError("Device command is not an allowed getter")
        # Each new file descriptor has its own input queue; drain stale replies.
        while select.select([self.fd], [], [], 0)[0]:
            os.read(self.fd, 64)
        payload = bytes(command).ljust(32, b"\0")
        written = os.write(self.fd, b"\0" + payload)
        if written != 33:
            raise RuntimeError("Short HID command write")
        if not select.select([self.fd], [], [], 2)[0]:
            raise TimeoutError("No response to getter " + bytes(command).hex())
        response = os.read(self.fd, 64)
        if len(response) != 32:
            raise RuntimeError("Unexpected HID response length")
        self.exchanges.append({"request": payload.hex(), "response": response.hex()})
        return response


def inspect():
    result = discover()
    raw = [i for i in result["interfaces"] if i["configuration_interface"]]
    if len(raw) != 1:
        raise RuntimeError("Expected one matching QMK raw HID descriptor")
    device = ReadOnlyVial(raw[0]["path"])
    try:
        response = device.get(0x01)
        if response[0] != 0x01:
            raise RuntimeError("VIA version query was not recognized")
        result["via_protocol_version"] = int.from_bytes(response[1:3], "big")
        response = device.get(0xFE, 0x00)
        result["vial_protocol_version"] = int.from_bytes(response[:4], "little")
        result["vial_keyboard_uid"] = response[4:12].hex()
        result["vial_capability_flags"] = response[12]
        response = device.get(0xFE, 0x05)
        result["vial_unlock_status_hex"] = response.hex()
        size = int.from_bytes(device.get(0xFE, 0x01)[:4], "little")
        if not 1 <= size <= 65536:
            raise RuntimeError("Unexpected Vial definition size")
        compressed = b"".join(
            device.get(0xFE, 0x02, page & 255, page >> 8)
            for page in range((size + 31) // 32)
        )[:size]
        result["vial_definition_compressed_hex"] = compressed.hex()
        decompressor = lzma.LZMADecompressor(memlimit=32 * 1024 * 1024)
        definition = decompressor.decompress(compressed, max_length=1024 * 1024)
        if not decompressor.eof:
            raise RuntimeError("Incomplete or oversized Vial definition")
        definition = json.loads(definition)
        result["vial_definition"] = definition
        response = device.get(0x11)
        if response[0] != 0x11:
            raise RuntimeError("VIA layer query was not recognized")
        layers = response[1]
        rows, cols = definition["matrix"]["rows"], definition["matrix"]["cols"]
        if not (1 <= layers <= 32 and rows == 1 and cols == 2):
            raise RuntimeError("Unexpected KnobX1 matrix/layer dimensions")
        result["layers"] = []
        for layer in range(layers):
            keys = []
            for col in range(cols):
                response = device.get(0x04, layer, 0, col)
                if response[:4] != bytes((0x04, layer, 0, col)):
                    raise RuntimeError("Keymap response did not match the query")
                keys.append("0x" + response[4:6].hex())
            response = device.get(0xFE, 0x03, layer, 0)
            result["layers"].append({
                "layer": layer, "matrix_row_0_keycodes": keys,
                "encoder_0_ccw": "0x" + response[:2].hex(),
                "encoder_0_cw": "0x" + response[2:4].hex(),
            })
        result["read_only_exchanges"] = device.exchanges
        result["protocol_sources"] = SOURCES
        result["scope"] = "Read-only definition and key/encoder mapping snapshot; not a firmware backup"
        return result
    finally:
        device.close()


def monitor(seconds, output):
    device = discover()
    descriptors = {}
    try:
        for interface in device["interfaces"]:
            if not interface["configuration_interface"]:
                fd = os.open(interface["path"], os.O_RDONLY | os.O_NONBLOCK)
                descriptors[fd] = interface["path"]
        with output.open("x") as stream:
            stream.write(json.dumps({"type": "start", "device": device}) + "\n")
            stream.flush()
            print("Listening to KnobX1 input reports for", seconds, "seconds", flush=True)
            start = time.monotonic()
            count = 0
            while time.monotonic() - start < seconds:
                remaining = seconds - (time.monotonic() - start)
                ready, _, _ = select.select(list(descriptors), [], [], min(1, max(0, remaining)))
                for fd in ready:
                    report = os.read(fd, 64)
                    if not report:
                        raise RuntimeError("KnobX1 disconnected")
                    entry = {"type": "report", "utc": timestamp(),
                             "elapsed_s": round(time.monotonic() - start, 3),
                             "interface": descriptors[fd], "hex": report.hex()}
                    stream.write(json.dumps(entry) + "\n")
                    stream.flush()
                    print(json.dumps(entry), flush=True)
                    count += 1
            stream.write(json.dumps({"type": "end", "reports": count, "utc": timestamp()}) + "\n")
            print("Captured", count, "reports to", output, flush=True)
    finally:
        for fd in descriptors:
            os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("inspect", "monitor"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=120)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error("--seconds must be between 1 and 300")
    if args.output.exists():
        parser.error("Output already exists; choose a new snapshot filename")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.mode == "inspect":
        result = inspect()
        with args.output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        brief = {k: result[k] for k in (
            "host", "via_protocol_version", "vial_protocol_version",
            "vial_keyboard_uid", "vial_definition", "layers", "scope",
        )}
        print(json.dumps(brief, indent=2))
        print("Saved", args.output)
    else:
        monitor(args.seconds, args.output)


if __name__ == "__main__":
    main()
