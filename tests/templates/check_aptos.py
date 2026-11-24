# -*- coding: utf-8 -*-
"""Kiểm tra độ phủ chữ của 2 mẫu PPT dùng phông Aptos sau khi xuất PDF (cần >= 97%).
Dùng lại hàm của inventory_fidelity (đọc PDF + tệp gốc). Thoát mã 1 nếu có mẫu < 97%."""
import csv, re, sys, unicodedata
from pathlib import Path
from pypdf import PdfReader
import zipfile
from xml.etree import ElementTree as ET

tdir, out = Path(sys.argv[1]), Path(sys.argv[2])
A = "{http://schemas.openxmlformats.org/drawingml/2006/main}"

def toks(t):
    t = unicodedata.normalize("NFC", t).lower()
    return [x for x in re.findall(r"\w+", t, flags=re.UNICODE) if len(x) >= 2]

bad = 0
rows = list(csv.DictReader(open(out / "fidelity.csv", encoding="utf-8", newline="")))
print("Số mẫu:", len(rows))
for r in rows:
    pdf = out / "pdf" / (r["index"] + ".pdf")
    if r["open_ok"] != "1" or r["pdf_ok"] != "1" or not pdf.exists():
        print("[LỖI] không mở/xuất được:", r["relpath"]); bad += 1; continue
    z = zipfile.ZipFile(tdir / r["relpath"])
    src = []
    for n in z.namelist():
        if re.match(r"ppt/slides/slide\d+\.xml", n):
            src += [t.text or "" for t in ET.fromstring(z.read(n)).iter(A + "t")]
    have = set(toks("\n".join((p.extract_text() or "") for p in PdfReader(str(pdf)).pages)))
    t = toks(" ".join(src))
    cov = sum(1 for x in t if x in have) / max(1, len(t))
    ok = cov >= 0.97
    print("[%s] %s: phủ chữ %.1f%% (%d từ)" % ("ĐẠT" if ok else "LỖI", r["relpath"].split("/")[-1], cov * 100, len(t)))
    if not ok:
        bad += 1
print("KẾT QUẢ:", "ĐẠT" if not bad else "CÓ LỖI")
sys.exit(1 if bad else 0)
