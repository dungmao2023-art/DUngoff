# -*- coding: utf-8 -*-
"""Kiểm tra độ phủ chữ của 2 mẫu PPT dùng phông Aptos sau khi xuất PDF (cần >= 97%).
Dùng lại hàm của inventory_fidelity (đọc PDF + tệp gốc). Thoát mã 1 nếu có mẫu < 97%."""
import csv, re, sys, unicodedata
from pathlib import Path
from pypdf import PdfReader
import zipfile
from xml.etree import ElementTree as ET

args = [a for a in sys.argv[1:] if not a.startswith("--")]
# --control: biến thể ĐỐI CHỨNG ÂM TÍNH (vd. v5: Always=false, đã biết KHÔNG áp dụng thay phông trên Windows).
# Kỳ vọng phủ chữ THẤP; thấp thì in [ĐỐI CHỨNG] và thoát 0, cao bất ngờ thì báo để xem lại ghi chú trong Engine.cpp.
CONTROL = "--control" in sys.argv
tdir, out = Path(args[0]), Path(args[1])
A = "{http://schemas.openxmlformats.org/drawingml/2006/main}"

def toks(t):
    t = unicodedata.normalize("NFC", t).lower()
    return [x for x in re.findall(r"\w+", t, flags=re.UNICODE) if len(x) >= 2]

bad = 0
rows = list(csv.DictReader(open(out / "fidelity.csv", encoding="utf-8", newline="")))
print("Số mẫu:", len(rows))
def fonts_of(pdf):
    out=set()
    for pg in PdfReader(str(pdf)).pages:
        res=pg.get("/Resources") or {}
        f=res.get("/Font") or {}
        for k in f:
            try: out.add(str(f[k].get_object().get("/BaseFont")))
            except Exception: pass
    return sorted(out)
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
    print("   phông trong PDF:", ", ".join(fonts_of(pdf)))
    ok = cov >= 0.97
    name = r["relpath"].split("/")[-1]
    if CONTROL:
        print("[%s] %s: phủ chữ %.1f%% (%d từ) - đối chứng âm tính, kỳ vọng thấp" %
              ("ĐỐI CHỨNG đúng kỳ vọng" if not ok else "BẤT NGỜ: đối chứng đạt 97%", name, cov * 100, len(t)))
        if ok:
            bad += 1
        continue
    print("[%s] %s: phủ chữ %.1f%% (%d từ)" % ("ĐẠT" if ok else "LỖI", name, cov * 100, len(t)))
    if not ok:
        bad += 1
print("KẾT QUẢ:", ("ĐỐI CHỨNG " if CONTROL else "") + ("ĐẠT" if not bad else "CÓ LỖI"))
sys.exit(1 if bad else 0)
