# -*- coding: utf-8 -*-
"""Đối chiếu hiển thị ↔ in: số trang engine hiển thị so với số trang PDF xuất ra, và chữ trong tệp gốc có đủ trong PDF.
Chạy: python3 verify_fidelity.py <thư mục mẫu> <thư mục kết quả có fidelity.csv và pdf/>   (thoát 1 nếu có lệch)"""
import csv
import re
import sys
import unicodedata
from pathlib import Path

import docx
import pptx
from pypdf import PdfReader

tdir, out = Path(sys.argv[1]), Path(sys.argv[2])


def norm_tokens(text):
    text = unicodedata.normalize("NFC", text).lower()
    return [t for t in re.findall(r"\w+", text, flags=re.UNICODE) if len(t) >= 2]


def source_text(path):
    if path.suffix.lower() == ".docx":
        d = docx.Document(str(path))
        parts = [p.text for p in d.paragraphs]
        for t in d.tables:
            for row in t.rows:
                for c in row.cells:
                    parts.append(c.text)
        return "\n".join(parts)
    p = pptx.Presentation(str(path))
    parts = []
    for s in p.slides:
        for sh in s.shapes:
            if sh.has_text_frame:
                parts.append(sh.text_frame.text)
            if getattr(sh, "has_table", False) and sh.has_table:
                for row in sh.table.rows:
                    for c in row.cells:
                        parts.append(c.text)
    return "\n".join(parts)


rows = list(csv.DictReader(open(out / "fidelity.csv", encoding="utf-8", newline="")))
bad_pages, bad_text, ok = [], [], 0
low_cov = []
for r in rows:
    rel = r["relpath"]
    pdf = out / "pdf" / (r["index"] + ".pdf")
    if r["open_ok"] != "1" or r["pdf_ok"] != "1" or not pdf.exists():
        bad_pages.append((rel, "không mở/xuất được"))
        continue
    try:
        reader = PdfReader(str(pdf))
        pages = len(reader.pages)
        pdf_text = "\n".join((pg.extract_text() or "") for pg in reader.pages)
    except Exception as e:  # PDF đọc không được cũng là lỗi
        bad_pages.append((rel, "PDF không đọc được: %s" % e))
        continue
    parts = int(r["parts"])
    if pages != parts:
        bad_pages.append((rel, "engine hiển thị %d trang, PDF in ra %d trang" % (parts, pages)))
    src = tdir / rel
    toks = norm_tokens(source_text(src))
    if toks:
        have = set(norm_tokens(pdf_text))
        cov = sum(1 for t in toks if t in have) / len(toks)
        if cov < 0.97:
            bad_text.append((rel, "chỉ %.1f%% chữ của tệp gốc có trong PDF" % (cov * 100)))
        elif cov < 0.999:
            low_cov.append((rel, cov))
    if pages == parts and (not toks or cov >= 0.97):
        ok += 1

print("Số mẫu:", len(rows), " đạt cả số trang và chữ:", ok)
print("\nLệch số trang hiển thị/in (%d):" % len(bad_pages))
for rel, why in bad_pages[:60]:
    print("  -", rel, "→", why)
print("\nThiếu chữ trong PDF (%d):" % len(bad_text))
for rel, why in bad_text[:60]:
    print("  -", rel, "→", why)
print("\nChữ khớp 97–99,9%% (%d mẫu, ghi nhận):" % len(low_cov))
for rel, cov in low_cov[:20]:
    print("  -", rel, "%.1f%%" % (cov * 100))
bad = len(bad_pages) + len(bad_text)
print("\nKẾT QUẢ TOÀN VẸN MẪU:", "CÓ LỆCH (%d)" % bad if bad else "TẤT CẢ KHỚP")
sys.exit(1 if bad else 0)
