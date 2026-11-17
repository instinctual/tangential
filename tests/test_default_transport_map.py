import json
from pathlib import Path
import unittest


class DefaultTransportMapTests(unittest.TestCase):
    def test_selected_mf_controls_use_flames_default_transport_actions(self):
        path = Path(__file__).resolve().parents[1] / "artifacts/research/firmware/flame-default-mf-transport.json"
        data = json.loads(path.read_text())
        self.assertGreater(len(data["modes"]), 20)
        for mode in data["modes"]:
            with self.subTest(mode=mode["id"]):
                controls = mode["controls"]
                self.assertEqual(controls["Encoder:2"]["key"], "0x81000001")
                self.assertIn(controls["Encoder:2"]["argument"], (None, "0x00000000"))
                self.assertEqual(controls["Button:14"]["key"], "0x20000000")
                self.assertEqual(controls["Button:15"]["key"], "0x20000089")
