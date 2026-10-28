"""Offline, synthetic-only tests for the SEC-001B1 probe."""
import json
from pathlib import Path
import tempfile
import unittest

from probe import EXPECTED, run, verify_fixture


class ProbeTests(unittest.TestCase):
    def test_fixture_without_line_ending(self):
        verify_fixture(EXPECTED)

    def test_fixture_with_lf(self):
        verify_fixture(EXPECTED + b"\n")

    def test_fixture_with_crlf(self):
        verify_fixture(EXPECTED + b"\r\n")

    def test_double_lf_rejected(self):
        with self.assertRaises(ValueError):
            verify_fixture(EXPECTED + b"\n\n")

    def test_wrong_contents_rejected(self):
        with self.assertRaises(ValueError):
            verify_fixture(b"unrelated")

    def test_success_writes_completed_result(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / "fixture.txt").write_bytes(EXPECTED + b"\n")
            run(root, hold_seconds=0)
            result = json.loads((root / "result.json").read_text())
            self.assertEqual(result["phase"], "completed")
            self.assertEqual(result["fixture"], "validated")
            self.assertIn("pid", result)
            self.assertIn("user", result["namespaces"])

    def test_no_overwrite_or_duplicate_run(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / "fixture.txt").write_bytes(EXPECTED)
            run(root, hold_seconds=0)
            with self.assertRaises(ValueError):
                run(root, hold_seconds=0)

    def test_no_output_on_bad_fixture(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / "fixture.txt").write_bytes(b"bad\n")
            with self.assertRaises(ValueError):
                run(root, hold_seconds=0)
            self.assertFalse((root / "result.json").exists())

    def test_reject_out_of_range_hold(self):
        with tempfile.TemporaryDirectory() as td:
            with self.assertRaises(ValueError):
                run(Path(td), hold_seconds=31)


if __name__ == "__main__":
    unittest.main()
