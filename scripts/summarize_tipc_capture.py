#!/usr/bin/env python3
"""Summarize native Hub-to-application events in a local IPv4/TCP pcap."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from tangential.tipc import FrameDecoder, transport_event


def summarize(path):
    data = path.read_bytes()
    if len(data) < 24 or data[:4] != bytes.fromhex("d4c3b2a1") or struct.unpack_from("<I", data, 20)[0] != 1:
        raise ValueError("Expected little-endian Ethernet pcap")
    streams = defaultdict(list)
    offset = 24
    while offset + 16 <= len(data):
        seconds, fraction, size, original = struct.unpack_from("<IIII", data, offset)
        offset += 16
        if size > 1024 * 1024 or size > original:
            raise ValueError("Invalid capture record size")
        if offset + size > len(data):
            break  # Permit inspection of a capture still being written.
        packet = data[offset:offset + size]
        offset += size
        if len(packet) < 54 or packet[12:14] != b"\x08\x00":
            continue
        ip_length = (packet[14] & 15) * 4
        if ip_length < 20 or packet[23] != 6:
            continue
        tcp = 14 + ip_length
        if len(packet) < tcp + 20:
            continue
        tcp_length = (packet[tcp + 12] >> 4) * 4
        ip_end = 14 + struct.unpack_from("!H", packet, 16)[0]
        if tcp_length < 20 or ip_end > len(packet):
            raise ValueError("Truncated IP/TCP packet")
        ports = struct.unpack_from("!HH", packet, tcp)
        if 64246 not in ports:
            continue
        payload = packet[tcp + tcp_length:ip_end]
        if payload:
            sequence = struct.unpack_from("!I", packet, tcp + 4)[0]
            streams[ports].append((sequence, payload, seconds * 1_000_000 + fraction))
    flows = []
    for ports, segments in streams.items():
        segments.sort(key=lambda item: item[0])
        expected = segments[0][0]
        decoder = FrameDecoder()
        counts = Counter()
        feedback = defaultdict(Counter)
        events = []
        warning = None
        for sequence, payload, stamp in segments:
            if sequence > expected:
                warning = "TCP gap; decoding stopped"
                break
            overlap = max(0, expected - sequence)
            payload = payload[overlap:]
            expected += len(payload)
            try:
                for frame in decoder.feed(payload):
                    command = struct.unpack_from("!I", frame)[0]
                    counts[f"0x{command:02x}"] += 1
                    if ports[1] == 64246 and command in (0xa3, 0xa4) and len(frame) == 12:
                        target, state = struct.unpack_from("!II", frame, 4)
                        feedback[f"0x{command:02x}:0x{target:08x}"][str(state)] += 1
                    if ports[0] == 64246:
                        event = transport_event(frame)
                        if event:
                            events.append({"timestamp_us": stamp, **event})
            except ValueError as error:
                warning = str(error) + "; stream may begin mid-frame"
                break
        flows.append({"ports": ports, "command_counts": dict(counts), "events": events,
                      "control_feedback": {key: dict(states) for key, states in sorted(feedback.items())},
                      "trailing_bytes": len(decoder.buffer), "warning": warning})
    return {"capture": str(path), "flows": flows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = summarize(args.capture)
    if args.output:
        with args.output.open("x") as out:
            json.dump(result, out, indent=2)
            out.write("\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
