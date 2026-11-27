# SPDX-License-Identifier: MPL-2.0
"""Dependency-free safety tests. python -m unittest discover -s tools/pdfcore -p test_pdf_save.py"""
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pdf_save as s

VALID = b"%PDF-1.7\n%%EOF\n"
class Writer:
    def __init__(self, callback=None, data=VALID):
        self.callback = callback
        self.data = data
    def save(self, path, linearize=True):
        Path(path).write_bytes(self.data)
        if self.callback:
            self.callback()

class SaveSafetyTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.dest = self.root / "out.pdf"

    def assertNoTemporary(self):
        self.assertEqual(list(self.root.glob("~dung-*")), [])

    def test_page_ranges_zero_based_and_ordered(self):
        self.assertEqual(s.parse_pages("1, 3-4, 1", 4), [0, 2, 3, 0])
        self.assertEqual(s.parse_pages("-2, 3-", 4), [0, 1, 2, 3])
        self.assertEqual(s.parse_pages("2-", 4), [1, 2, 3])

    def test_page_range_rejects_invalid_and_oversized(self):
        for spec in ("0", "5", "2-1", "1,,2", "1 2", "-", "+1", "1,"):
            with self.subTest(spec=spec), self.assertRaises(s.PdfError):
                s.parse_pages(spec, 4)
        with self.assertRaises(s.PdfError):
            s.parse_pages("1-", 1_000_001)
        with self.assertRaises(s.PdfError):
            s.parse_pages("1-600000,1-600000", 600000)

    def test_temp_creation_failure_is_pdf_error(self):
        with patch.object(s.tempfile, "mkstemp", side_effect=OSError("disk full")):
            with self.assertRaises(s.PdfError):
                s.atomic_save(Writer(), str(self.dest))
        self.assertFalse(self.dest.exists())
        self.assertNoTemporary()

    def test_new_file(self):
        result = s.atomic_save(Writer(), str(self.dest))
        self.assertEqual(self.dest.read_bytes(), VALID)
        self.assertIsNone(result.backup)
        self.assertNoTemporary()

    def test_existing_not_replaced_without_consent(self):
        self.dest.write_bytes(b"old")
        with self.assertRaises(s.PdfError):
            s.atomic_save(Writer(), str(self.dest))
        self.assertEqual(self.dest.read_bytes(), b"old")

    def test_backup_on_replace(self):
        self.dest.write_bytes(b"old")
        result = s.atomic_save(Writer(), str(self.dest), True)
        self.assertEqual(Path(result.backup).read_bytes(), b"old")
        self.assertEqual(self.dest.read_bytes(), VALID)
        self.assertNoTemporary()

    def test_competing_new_file(self):
        with self.assertRaises(s.PdfError):
            s.atomic_save(Writer(lambda: self.dest.write_bytes(b"other")), str(self.dest))
        self.assertEqual(self.dest.read_bytes(), b"other")
        self.assertNoTemporary()

    def test_changed_destination_not_replaced(self):
        self.dest.write_bytes(b"old")
        with self.assertRaises(s.PdfError):
            s.atomic_save(Writer(lambda: self.dest.write_bytes(b"other longer")), str(self.dest), True)
        self.assertEqual(self.dest.read_bytes(), b"other longer")
        self.assertNoTemporary()

    def test_invalid_output(self):
        with self.assertRaises(s.PdfError):
            s.atomic_save(Writer(data=b"broken"), str(self.dest))
        self.assertFalse(self.dest.exists())
        self.assertNoTemporary()

    def test_source_alias(self):
        self.dest.write_bytes(b"old")
        with self.assertRaises(s.PdfError):
            s._ensure_distinct_destination([str(self.dest)], str(self.dest))
        alias = self.root / "alias.pdf"
        try:
            os.link(self.dest, alias)
        except OSError as exc:
            self.skipTest(str(exc))
        with self.assertRaises(s.PdfError):
            s._ensure_distinct_destination([str(self.dest)], str(alias))

    def test_failed_backup_preserves_destination(self):
        self.dest.write_bytes(b"old")
        def partial_write(source, target):
            target.write(b"partial")
            raise OSError("disk full")
        with patch.object(s.shutil, "copyfileobj", side_effect=partial_write):
            with self.assertRaises(s.PdfError):
                s.atomic_save(Writer(), str(self.dest), True)
        self.assertEqual(self.dest.read_bytes(), b"old")
        self.assertEqual(list(self.root.glob("*.dung-backup-*")), [])
        self.assertNoTemporary()

    def test_backup_collision_preserves_both_files(self):
        self.dest.write_bytes(b"old")
        backup = self.root / "out.dung-backup-fixed.pdf"
        with patch.object(s, "_backup_path", return_value=str(backup)):
            with self.assertRaises(s.PdfError):
                s.atomic_save(Writer(lambda: backup.write_bytes(b"other backup")), str(self.dest), True)
        self.assertEqual(self.dest.read_bytes(), b"old")
        self.assertEqual(backup.read_bytes(), b"other backup")
        self.assertNoTemporary()

    def test_replace_failure_preserves_complete_backup(self):
        self.dest.write_bytes(b"old")
        with patch.object(s.os, "replace", side_effect=OSError("file locked")):
            with self.assertRaises(s.PdfError):
                s.atomic_save(Writer(), str(self.dest), True)
        self.assertEqual(self.dest.read_bytes(), b"old")
        backups = list(self.root.glob("*.dung-backup-*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), b"old")
        self.assertNoTemporary()

if __name__ == "__main__":
    unittest.main()
