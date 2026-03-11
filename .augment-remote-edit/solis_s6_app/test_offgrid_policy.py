import sys
import unittest
from pathlib import Path
from unittest.mock import Mock


APP_DIR = Path(__file__).resolve().parent
if str(APP_DIR) not in sys.path:
    sys.path.insert(0, str(APP_DIR))

import main  # noqa: E402


class OffGridPolicyTests(unittest.TestCase):
    def setUp(self):
        self.orig_get_storage_control_bits = main.get_storage_control_bits
        self.orig_set_storage_control_bits = main.set_storage_control_bits
        self.orig_set_hybrid_control_bit = main.set_hybrid_control_bit

    def tearDown(self):
        main.get_storage_control_bits = self.orig_get_storage_control_bits
        main.set_storage_control_bits = self.orig_set_storage_control_bits
        main.set_hybrid_control_bit = self.orig_set_hybrid_control_bit

    def test_enabling_off_grid_forces_export_and_grid_charge_off(self):
        main.set_hybrid_control_bit = Mock(return_value=True)
        main.set_storage_control_bits = Mock(return_value=True)

        ok, error = main._apply_control_change("storage", 2, True)

        self.assertTrue(ok)
        self.assertIsNone(error)
        main.set_hybrid_control_bit.assert_called_once_with(3, False)
        main.set_storage_control_bits.assert_called_once_with({2: True, 0: False, 6: False, 11: False, 5: False})

    def test_cannot_enable_grid_charge_while_off_grid_active(self):
        main.get_storage_control_bits = Mock(return_value={"off_grid": True})
        main.set_storage_control_bits = Mock(return_value=True)

        ok, error = main._apply_control_change("storage", 5, True)

        self.assertFalse(ok)
        self.assertEqual(error, "off_grid_policy")
        main.set_storage_control_bits.assert_not_called()

    def test_cannot_enable_export_while_off_grid_active(self):
        main.get_storage_control_bits = Mock(return_value={"off_grid": True})
        main.set_hybrid_control_bit = Mock(return_value=True)

        ok, error = main._apply_control_change("hybrid", 3, True)

        self.assertFalse(ok)
        self.assertEqual(error, "off_grid_policy")
        main.set_hybrid_control_bit.assert_not_called()

    def test_grid_charge_allowed_when_off_grid_inactive(self):
        main.get_storage_control_bits = Mock(return_value={"off_grid": False})
        main.set_storage_control_bits = Mock(return_value=True)

        ok, error = main._apply_control_change("storage", 5, True)

        self.assertTrue(ok)
        self.assertIsNone(error)
        main.set_storage_control_bits.assert_called_once_with({5: True})


if __name__ == "__main__":
    unittest.main()