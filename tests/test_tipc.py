import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from tangential.tipc import FrameDecoder, transport_event


class TipcTests(unittest.TestCase):
    def test_reassembles_split_and_coalesced_messages(self):
        body = struct.pack("!Iii", 0x0A, -2, 0)
        frame = struct.pack("!I", len(body)) + body
        decoder = FrameDecoder()
        self.assertEqual(decoder.feed(frame[:7]), [])
        self.assertEqual(decoder.feed(frame[7:] + frame), [body, body])
        self.assertEqual(transport_event(body), {"command":"Transport","jog":-2,"shuttle":0})

    def test_distinguishes_action_release_from_stop_action(self):
        release = transport_event(struct.pack("!II", 0x0B, 0x20000089))
        stop = transport_event(struct.pack("!II", 0x08, 0x20000000))
        self.assertEqual(release, {"command":"ActionOff","control_id":"0x20000089"})
        self.assertEqual(stop, {"command":"ActionOn","control_id":"0x20000000"})

    def test_rejects_invalid_length_before_accumulating_a_huge_frame(self):
        for size in (0, 3, 0xFFFFFFFF):
            with self.subTest(size=size), self.assertRaises(ValueError):
                FrameDecoder().feed(struct.pack("!I", size))

    def test_rejects_truncated_transport_and_action(self):
        for data in (b"", struct.pack("!I", 0x0A), struct.pack("!I", 0x08)):
            with self.subTest(data=data), self.assertRaises(ValueError):
                transport_event(data)
