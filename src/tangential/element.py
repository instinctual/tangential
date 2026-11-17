"""Element Kb input reports, inferred from the preserved 2026-11-16 capture.

Top-left encoder and push were exercised. Other positions retain their numeric
indices until individually verified on hardware.
"""

from dataclasses import dataclass
import struct


REPORT_SIZE = 15
HID_REPORT_DESCRIPTOR = bytes.fromhex(
    "05 01 09 08 a1 01 05 f0 09 01 75 08 95 0f 81 02 "
    "05 f0 09 02 75 08 95 40 91 02 c0"
)


@dataclass(frozen=True)
class ElementReport:
    buttons: int
    encoder_deltas: tuple

    def encode(self):
        if not isinstance(self.buttons, int) or not 0 <= self.buttons <= 0xFFFFFF:
            raise ValueError("Button bitmap must fit in 24 bits")
        if len(self.encoder_deltas) != 12:
            raise ValueError("Expected twelve encoder deltas")
        if any(not isinstance(v, int) or not -128 <= v <= 127 for v in self.encoder_deltas):
            raise ValueError("Encoder deltas must be signed bytes")
        return self.buttons.to_bytes(3, "little") + struct.pack("12b", *self.encoder_deltas)

    @classmethod
    def decode(cls, report):
        if len(report) != REPORT_SIZE:
            raise ValueError("Expected a 15-byte Element input report")
        return cls(int.from_bytes(report[:3], "little"), struct.unpack("12b", report[3:]))

