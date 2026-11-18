import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from tangential.element import ElementReport, HID_REPORT_DESCRIPTOR
from tangential.usbmon import read_pcap


CAPTURE = Path(__file__).resolve().parents[1] / "artifacts/element-mf-20261118T201737Z"


class RealMfTests(unittest.TestCase):
    def test_real_mf_matches_emulated_usb_and_hid_descriptors(self):
        data = json.loads((CAPTURE / "descriptors.json").read_text())
        real = next(d for d in data["devices"] if d["serial"] == "55024780")
        emulator = next(d for d in data["devices"] if d["serial"] == "99000101")
        self.assertEqual(real["usb_descriptors_hex"], emulator["usb_descriptors_hex"])
        descriptor = bytes.fromhex(real["interfaces"][0]["report_descriptors"][0])
        self.assertEqual(descriptor, HID_REPORT_DESCRIPTOR)

    def test_real_mf_ring_and_transport_button_positions(self):
        with (CAPTURE / "transport-inputs.pcap").open("rb") as stream:
            reports = [ElementReport.decode(p.payload) for p in read_pcap(stream)
                       if p.event == "C" and p.transfer_type == 1
                       and p.endpoint == 0x81 and p.status == 0]
        self.assertGreater(len(reports), 10)
        self.assertEqual({r.buttons for r in reports}, {0, 1 << 14, 1 << 15})
        active = {i for r in reports for i, v in enumerate(r.encoder_deltas) if v}
        self.assertEqual(active, {2})
        ring = [r.encoder_deltas[2] for r in reports if r.encoder_deltas[2]]
        self.assertGreater(ring[0], 0)  # Operator began clockwise.
        self.assertTrue(any(v < 0 for v in ring))

    def test_stock_map_exposes_distinct_jog_and_shuttle_labelled_controls(self):
        data = json.loads((CAPTURE / "default-page-mappings.json").read_text())
        self.assertEqual(data["capabilities"]["Shuttle"]["enabled"], "false")
        self.assertEqual(len(data["modes"]), 37)
        for mode in data["modes"]:
            with self.subTest(mode=mode["id"]):
                ring, ball = mode["encoders"]["2"], mode["encoders"]["0"]
                self.assertEqual(ring["Key"], "0x81000001")
                self.assertEqual(ring["CustomName"], "Jog")
                self.assertEqual(ball["Key"], "0x81000001")
                self.assertEqual(ball["CustomName"], "Shuttle")
                self.assertEqual(ball["Sensitivity"], "5")

    def test_real_mf_shuttle_label_uses_trackball_and_native_jog_channel(self):
        with (CAPTURE / "shuttle-inputs-03.pcap").open("rb") as stream:
            reports = [ElementReport.decode(p.payload) for p in read_pcap(stream)
                       if p.event == "C" and p.transfer_type == 1
                       and p.endpoint == 0x81 and p.status == 0]
        horizontal = [r.encoder_deltas[0] for r in reports]
        self.assertTrue(any(v < 0 for v in horizontal))
        self.assertTrue(any(v > 0 for v in horizontal))
        self.assertGreater(sum(abs(v) for v in horizontal),
                           sum(abs(r.encoder_deltas[2]) for r in reports))
        data = json.loads((CAPTURE / "shuttle-tipc-03-decoded.json").read_text())
        events = [e for flow in data["flows"] for e in flow["events"]
                  if e["command"] == "Transport"]
        self.assertTrue(events)
        self.assertEqual({e["shuttle"] for e in events}, {0})
        self.assertTrue(any(e["jog"] < 0 for e in events))
        self.assertTrue(any(e["jog"] > 0 for e in events))
