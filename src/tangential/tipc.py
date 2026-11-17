"""Bounded framing and transport-event decoding for Tangent TIPC."""

import struct


class FrameDecoder:
    def __init__(self, maximum=1024 * 1024):
        self.buffer = bytearray()
        self.maximum = maximum

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while len(self.buffer) >= 4:
            size = struct.unpack_from("!I", self.buffer)[0]
            if not 4 <= size <= self.maximum:
                raise ValueError("Invalid TIPC frame size")
            if len(self.buffer) < size + 4:
                break
            frames.append(bytes(self.buffer[4:size + 4]))
            del self.buffer[:size + 4]
        return frames


def transport_event(body):
    if len(body) < 4:
        raise ValueError("Missing TIPC command")
    command = struct.unpack_from("!I", body)[0]
    if command == 0x0A:
        if len(body) != 12:
            raise ValueError("Unexpected Transport message size")
        jog, shuttle = struct.unpack_from("!ii", body, 4)
        return {"command": "Transport", "jog": jog, "shuttle": shuttle}
    if command in (0x08, 0x0B):
        if len(body) != 8:
            raise ValueError("Unexpected Action message size")
        control = struct.unpack_from("!I", body, 4)[0]
        return {"command": "ActionOn" if command == 0x08 else "ActionOff",
                "control_id": f"0x{control:08x}"}
    return None
