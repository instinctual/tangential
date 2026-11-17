"""Bounded reader for classic pcap captures with Linux usbmon's 64-byte header."""

from dataclasses import dataclass
import struct


@dataclass(frozen=True)
class UsbPacket:
    timestamp_ns: int
    urb_id: int
    event: str
    transfer_type: int
    endpoint: int
    device: int
    bus: int
    status: int
    setup: bytes
    payload: bytes


def read_pcap(stream):
    header = stream.read(24)
    if len(header) != 24:
        raise ValueError("Truncated pcap header")
    formats = {b"\xd4\xc3\xb2\xa1": ("<", 1000), b"\xa1\xb2\xc3\xd4": (">", 1000),
               b"\x4d\x3c\xb2\xa1": ("<", 1), b"\xa1\xb2\x3c\x4d": (">", 1)}
    if header[:4] not in formats:
        raise ValueError("Unsupported pcap format")
    endian, multiplier = formats[header[:4]]
    major, minor, _, _, snaplen, linktype = struct.unpack(endian + "HHIIII", header[4:])
    if (major, minor) != (2, 4) or linktype != 220:
        raise ValueError("Expected pcap 2.4 with USB_LINUX_MMAPPED link type 220")
    while True:
        record = stream.read(16)
        if not record:
            return
        if len(record) != 16:
            raise ValueError("Truncated pcap record header")
        seconds, fraction, captured, original = struct.unpack(endian + "IIII", record)
        if not 64 <= captured <= min(snaplen, 1024 * 1024) or captured > original:
            raise ValueError("Invalid or excessive pcap record size")
        packet = stream.read(captured)
        if len(packet) != captured:
            raise ValueError("Truncated pcap record")
        if packet[9] == 0:
            raise ValueError("Isochronous USB captures are not supported")
        payload_size = struct.unpack_from(endian + "I", packet, 36)[0]
        if payload_size > len(packet) - 64:
            raise ValueError("USB payload was truncated by the capture")
        setup = packet[40:48] if packet[9] == 2 and packet[8] == ord("S") else b""
        yield UsbPacket(
            seconds * 1_000_000_000 + fraction * multiplier,
            struct.unpack_from(endian + "Q", packet)[0], chr(packet[8]),
            packet[9], packet[10], packet[11], struct.unpack_from(endian + "H", packet, 12)[0],
            struct.unpack_from(endian + "i", packet, 28)[0], setup,
            packet[64:64 + payload_size],
        )

