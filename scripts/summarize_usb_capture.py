#!/usr/bin/env python3
"""Decode completed Element Kb usbmon captures without touching hardware."""
import argparse
from collections import Counter
from dataclasses import asdict
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from tangential.element import ElementReport
from tangential.usbmon import read_pcap


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output already exists")
    counts = Counter()
    events = []
    setup_by_urb = {}
    descriptors = []
    outputs = []
    with args.capture.open("rb") as stream:
        for packet in read_pcap(stream):
            counts[(packet.bus, packet.device, packet.event, packet.transfer_type, packet.endpoint)] += 1
            if packet.setup:
                setup_by_urb[packet.urb_id] = packet.setup
            if packet.event == "C" and packet.transfer_type == 2:
                setup = setup_by_urb.pop(packet.urb_id, b"")
                if setup[:4] == bytes.fromhex("81060022") and packet.status == 0:
                    descriptors.append(packet.payload.hex())
            if packet.event == "C" and packet.transfer_type == 1 and packet.endpoint == 0x81 and packet.status == 0:
                report = ElementReport.decode(packet.payload)
                events.append({"timestamp_ns": packet.timestamp_ns, "bus": packet.bus,
                               "device": packet.device, "report_hex": packet.payload.hex(),
                               **asdict(report)})
            if packet.event == "S" and packet.transfer_type == 1 and packet.endpoint == 1:
                outputs.append(packet.payload.hex())
    result = {"capture": str(args.capture), "packets": sum(counts.values()),
              "record_counts": [{"bus": k[0], "device": k[1], "event": k[2],
                                 "transfer_type": k[3], "endpoint": k[4], "count": v}
                                for k, v in counts.items()],
              "hid_report_descriptors": descriptors, "input_events": events,
              "output_reports": outputs}
    with args.output.open("x") as out:
        json.dump(result, out, indent=2)
        out.write("\n")
    print(f"{result['packets']} packets; {len(events)} input events; {len(descriptors)} HID descriptors")


if __name__ == "__main__":
    main()

