import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from tangential.element import ElementReport, HID_REPORT_DESCRIPTOR


class ElementReportTests(unittest.TestCase):
    def test_captured_clockwise_event(self):
        result = ElementReport.decode(bytes.fromhex("000000010000000000000000000000"))
        self.assertEqual(result.buttons, 0)
        self.assertEqual(result.encoder_deltas, (1,) + (0,) * 11)

    def test_captured_counterclockwise_event(self):
        result = ElementReport.decode(bytes.fromhex("000000ff0000000000000000000000"))
        self.assertEqual(result.encoder_deltas, (-1,) + (0,) * 11)

    def test_captured_push_and_release(self):
        self.assertEqual(ElementReport.decode(bytes.fromhex("010000000000000000000000000000")).buttons, 1)
        self.assertEqual(ElementReport.decode(bytes(15)).buttons, 0)

    def test_encoder_and_button_data_can_coexist(self):
        report = ElementReport(0x020101, (-128, 127) + (0,) * 10)
        self.assertEqual(ElementReport.decode(report.encode()), report)

    def test_refuses_truncated_and_oversized_reports(self):
        for size in (0, 14, 16, 64):
            with self.subTest(size=size), self.assertRaises(ValueError):
                ElementReport.decode(bytes(size))

    def test_refuses_values_that_would_overflow_on_wire(self):
        for buttons, deltas in [(-1, (0,) * 12), (0x1000000, (0,) * 12),
                                (0, (0,) * 11), (0, (128,) + (0,) * 11)]:
            with self.subTest(buttons=buttons, deltas=deltas), self.assertRaises(ValueError):
                ElementReport(buttons, deltas).encode()

    def test_descriptor_matches_captured_length(self):
        self.assertEqual(len(HID_REPORT_DESCRIPTOR), 27)

