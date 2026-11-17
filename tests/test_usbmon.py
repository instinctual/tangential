import io
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from tangential.usbmon import read_pcap


def capture(payload, declared_payload=None, endian="<"):
    packet = bytearray(64)
    struct.pack_into(endian + "Q", packet, 0, 42)
    packet[8:12] = bytes((ord("C"), 1, 0x81, 3))
    struct.pack_into(endian + "H", packet, 12, 1)
    struct.pack_into(endian + "I", packet, 36, len(payload) if declared_payload is None else declared_payload)
    packet.extend(payload)
    header = struct.pack(endian + "IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 220)
    return header + struct.pack(endian + "IIII", 100, 20, len(packet), len(packet)) + packet


class UsbmonTests(unittest.TestCase):
    def test_preserves_endpoint_device_timestamp_and_signed_report(self):
        payload = bytes.fromhex("000000ff0000000000000000000000")
        p, = read_pcap(io.BytesIO(capture(payload)))
        self.assertEqual((p.endpoint, p.device, p.bus), (0x81, 3, 1))
        self.assertEqual(p.timestamp_ns, 100_000_020_000)
        self.assertEqual(p.payload, payload)

    def test_big_endian_capture(self):
        p, = read_pcap(io.BytesIO(capture(bytes(15), endian=">")))
        self.assertEqual((p.urb_id, p.bus, len(p.payload)), (42, 1, 15))

    def test_rejects_truncated_headers_and_payload(self):
        valid = capture(bytes(15))
        for length in (0, 23, 25, len(valid) - 1):
            with self.subTest(length=length), self.assertRaises(ValueError):
                list(read_pcap(io.BytesIO(valid[:length])))

    def test_rejects_snaplen_truncation(self):
        with self.assertRaises(ValueError):
            list(read_pcap(io.BytesIO(capture(bytes(15), declared_payload=64))))

    def test_rejects_unbounded_record_allocation(self):
        data = bytearray(capture(b""))
        struct.pack_into("<I", data, 32, 0xFFFFFFFF)
        with self.assertRaises(ValueError):
            list(read_pcap(io.BytesIO(data)))

    def test_saved_real_capture_contains_expected_hid_descriptor(self):
        p = Path(__file__).resolve().parents[1] / "artifacts/element-kb-20261116/reconnect.pcap"
        with p.open("rb") as stream:
            packets = list(read_pcap(stream))
        expected = bytes.fromhex("05010908a10105f009017508950f810205f00902750895409102c0")
        self.assertTrue(any(packet.payload == expected for packet in packets))
