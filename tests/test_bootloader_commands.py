import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from enter_knobx1_bootloader import make_command


class BootloaderCommandTests(unittest.TestCase):
    def test_allows_only_physical_unlock_flow_and_bootloader_entry(self):
        for command in ((0xFE, 0), (0xFE, 5), (0xFE, 6), (0xFE, 7), (0xFE, 8), (0x0B,)):
            report = make_command(command)
            self.assertEqual(len(report), 33)
            self.assertEqual(report[0], 0)
            self.assertEqual(report[1:1 + len(command)], bytes(command))

    def test_rejects_eeprom_reset_keymap_write_and_macro_write(self):
        for command in ((0x0A,), (0x05, 0, 0, 1, 0, 0), (0x0F, 0, 0, 1),
                        (0xFE, 4, 0, 0, 0, 0, 0), (0xFE, 0x0C)):
            with self.subTest(command=command), self.assertRaises(ValueError):
                make_command(command)

