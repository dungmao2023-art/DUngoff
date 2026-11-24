# -*- coding: utf-8 -*-
"""Kiểm thử lõi PDF. Chạy: python3 -I -m unittest discover -s tools/pdfcore"""
import hashlib
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import pikepdf
from reportlab.pdfgen import canvas

import dung_pdf as d


def make_pdf(path, pages, text_prefix="Trang"):
    c = canvas.Canvas(path)
    for i in range(pages):
        c.drawString(72, 720, f"{text_prefix} {i + 1} - Dũng Office")
        c.showPage()
    c.save()
    return path


def sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


class DungPdfTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="dungpdf-")
        self.a = make_pdf(os.path.join(self.tmp, "a.pdf"), 3, "A")
        self.b = make_pdf(os.path.join(self.tmp, "b.pdf"), 2, "B")

    def p(self, name):
        return os.path.join(self.tmp, name)

    def test_merge_counts_pages_and_keeps_sources(self):
        before = (sha(self.a), sha(self.b))
        d.merge([self.a, self.b], self.p("gop.pdf"))
        with pikepdf.open(self.p("gop.pdf")) as out:
            self.assertEqual(len(out.pages), 5)
        self.assertEqual((sha(self.a), sha(self.b)), before)

    def test_merge_refuses_existing_target(self):
        d.merge([self.a, self.b], self.p("gop.pdf"))
        with self.assertRaises(d.PdfError):
            d.merge([self.a, self.b], self.p("gop.pdf"))

    def test_replace_creates_backup_of_old_file(self):
        d.merge([self.a, self.b], self.p("gop.pdf"))
        old = sha(self.p("gop.pdf"))
        res = d.extract(self.a, "1", self.p("gop.pdf"), allow_replace=True)
        self.assertIsNotNone(res.backup)
        self.assertEqual(sha(res.backup), old)

    def test_extract_page_ranges(self):
        d.extract(self.a, "1,3", self.p("tach.pdf"))
        with pikepdf.open(self.p("tach.pdf")) as out:
            self.assertEqual(len(out.pages), 2)

    def test_parse_pages_rejects_bad_ranges(self):
        for spec in ["0", "4", "3-1", "", "x"]:
            with self.assertRaises((d.PdfError, ValueError)):
                d.parse_pages(spec, 3)

    def test_parse_pages_open_ended(self):
        self.assertEqual(d.parse_pages("2-", 4), [1, 2, 3])

    def test_split_each_writes_one_file_per_page(self):
        out_dir = self.p("split")
        os.makedirs(out_dir)
        files = d.split_each(self.a, out_dir)
        self.assertEqual(len(files), 3)
        for f in files:
            with pikepdf.open(f) as one:
                self.assertEqual(len(one.pages), 1)

    def test_rotate_changes_only_output(self):
        before = sha(self.a)
        d.rotate(self.a, "1", 90, self.p("xoay.pdf"))
        with pikepdf.open(self.p("xoay.pdf")) as out:
            self.assertEqual(int(out.pages[0].Rotate), 90)
        self.assertEqual(sha(self.a), before)

    def test_rotate_rejects_non_right_angle(self):
        with self.assertRaises(d.PdfError):
            d.rotate(self.a, "1", 45, self.p("x.pdf"))

    def test_compress_keeps_pages_and_source(self):
        before = sha(self.a)
        res = d.compress(self.a, self.p("nen.pdf"))
        with pikepdf.open(self.p("nen.pdf")) as out:
            self.assertEqual(len(out.pages), 3)
        self.assertEqual(sha(self.a), before)
        self.assertIsInstance(res.warnings, list)

    def test_corrupt_input_gives_vietnamese_error(self):
        bad = self.p("hong.pdf")
        with open(bad, "wb") as f:
            f.write(b"khong phai pdf")
        with self.assertRaises(d.PdfError) as ctx:
            d.open_readonly(bad)
        self.assertIn("Không mở được", str(ctx.exception))

    def test_encrypted_needs_password(self):
        enc = self.p("mat-khau.pdf")
        with pikepdf.open(self.a) as src:
            src.save(enc, encryption=pikepdf.Encryption(owner="owner", user="1234"))
        enc_hash = sha(enc)
        with self.assertRaises(d.PdfError) as ctx:
            d.open_readonly(enc)
        self.assertIn("mật khẩu", str(ctx.exception))
        with self.assertRaises(d.PdfError):
            d.open_readonly(enc, password="sai")
        res = d.extract(enc, "1", self.p("giai-ma.pdf"), password="1234")
        with pikepdf.open(res.written) as out:
            self.assertEqual(len(out.pages), 1)
        self.assertEqual(sha(enc), enc_hash)  # tệp gốc vẫn còn nguyên

    def test_large_document_merge(self):
        big = make_pdf(self.p("lon.pdf"), 300, "Lon")
        d.merge([big, big], self.p("lon-gop.pdf"))
        with pikepdf.open(self.p("lon-gop.pdf")) as out:
            self.assertEqual(len(out.pages), 600)

    def test_truncated_pdf_is_rejected(self):
        cut = self.p("cut.pdf")
        with open(self.a, "rb") as f:
            data = f.read()
        with open(cut, "wb") as f:
            f.write(data[: len(data) // 2])
        with self.assertRaises(d.PdfError):
            d.open_readonly(cut)

    def test_no_temp_files_left_behind(self):
        d.merge([self.a, self.b], self.p("gop.pdf"))
        leftovers = [n for n in os.listdir(self.tmp) if n.startswith("~dung-")]
        self.assertEqual(leftovers, [])


if __name__ == "__main__":
    unittest.main()
