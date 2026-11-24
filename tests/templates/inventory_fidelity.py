# -*- coding: utf-8 -*-
"""Kiểm kê toàn vẹn hiển thị ↔ in trên thư viện mẫu (không chặn CI, chỉ báo cáo theo nhóm lỗi).
Dùng kết quả của `dung-smoketest --templates` (fidelity.csv + pdf/). Với mỗi mẫu so tệp gốc với PDF:
  - số trang engine hiển thị vs số trang PDF;
  - độ phủ chữ thân bài, bảng, đầu/cuối trang (≥97%);
  - ảnh: số ảnh trong tệp gốc vs số ảnh trong PDF;
  - font: font tệp gốc dùng mà PDF không nhúng (bị thay thế hoặc thiếu).
Chạy: python inventory_fidelity.py <thư mục mẫu> <thư mục kết quả> <nhãn nền: linux|windows>
Phân loại nhóm lỗi (gợi ý, kèm bằng chứng):  A = vá ở app/gói font;  B = có thể buộc sửa engine;  C = chấp nhận + cảnh báo."""
import csv
import collections
import re
import sys
import unicodedata
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

from pypdf import PdfReader

tdir, out = Path(sys.argv[1]), Path(sys.argv[2])
plat = sys.argv[3] if len(sys.argv) > 3 else "?"
W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
A_NS = "{http://schemas.openxmlformats.org/drawingml/2006/main}"


def toks(text):
    text = unicodedata.normalize("NFC", text).lower()
    return [t for t in re.findall(r"\w+", text, flags=re.UNICODE) if len(t) >= 2]


def coverage(src_text, have):
    t = toks(src_text)
    if not t:
        return None
    return sum(1 for x in t if x in have) / len(t)


def docx_parts(path):
    """Trả (thân bài, bảng, đầu-cuối trang, số ảnh, tập font khai báo)."""
    z = zipfile.ZipFile(path)
    body_txt, tbl_txt, hf_txt, fonts = [], [], [], set()
    images = 0
    doc = ET.fromstring(z.read("word/document.xml"))
    for tbl in doc.iter(W + "tbl"):
        tbl_txt.append("".join(t.text or "" for t in tbl.iter(W + "t")))
    tbl_ids = {id(t) for tbl in doc.iter(W + "tbl") for t in tbl.iter(W + "t")}
    for t in doc.iter(W + "t"):
        if id(t) not in tbl_ids:
            body_txt.append((t.text or "") + " ")
    images += sum(1 for _ in doc.iter("{http://schemas.openxmlformats.org/drawingml/2006/picture}pic"))
    for n in z.namelist():
        if re.match(r"word/(header|footer)\d*\.xml", n):
            x = ET.fromstring(z.read(n))
            hf_txt.append(" ".join(t.text or "" for t in x.iter(W + "t")))
            images += sum(1 for _ in x.iter("{http://schemas.openxmlformats.org/drawingml/2006/picture}pic"))
        if n.startswith("word/") and n.endswith(".xml"):
            try:
                x = ET.fromstring(z.read(n))
            except ET.ParseError:
                continue
            for rf in x.iter(W + "rFonts"):
                for k in ("ascii", "hAnsi", "cs", "eastAsia"):
                    v = rf.get(W + k)
                    if v:
                        fonts.add(v)
    return " ".join(body_txt), " ".join(tbl_txt), " ".join(hf_txt), images, fonts


def pptx_parts(path):
    z = zipfile.ZipFile(path)
    body, tbl, fonts, images = [], [], set(), 0
    for n in z.namelist():
        if re.match(r"ppt/slides/slide\d+\.xml", n):
            x = ET.fromstring(z.read(n))
            for tb in x.iter(A_NS + "tbl"):
                tbl.append(" ".join(t.text or "" for t in tb.iter(A_NS + "t")))
            tids = {id(t) for tb in x.iter(A_NS + "tbl") for t in tb.iter(A_NS + "t")}
            for t in x.iter(A_NS + "t"):
                if id(t) not in tids:
                    body.append((t.text or "") + " ")
            images += sum(1 for _ in x.iter("{http://schemas.openxmlformats.org/presentationml/2006/main}pic"))
            for tag in ("latin", "ea", "cs"):
                for f in x.iter(A_NS + tag):
                    v = f.get("typeface")
                    if v and not v.startswith("+"):
                        fonts.add(v)
    return " ".join(body), " ".join(tbl), "", images, fonts


def pdf_info(pdf):
    r = PdfReader(str(pdf))
    text, fonts, imgs = [], set(), 0
    for pg in r.pages:
        text.append(pg.extract_text() or "")
        try:
            res = pg.get("/Resources")
            res = res.get_object() if res is not None else {}
            fd = res.get("/Font")
            if fd is not None:
                for _, f in fd.get_object().items():
                    bf = str(f.get_object().get("/BaseFont", ""))
                    fonts.add(re.sub(r"^/?[A-Z]{6}\+", "", bf).lstrip("/"))
            xo = res.get("/XObject")
            if xo is not None:
                for _, o in xo.get_object().items():
                    o = o.get_object()
                    if o.get("/Subtype") == "/Image":
                        imgs += 1
        except Exception:
            pass
    return len(r.pages), "\n".join(text), fonts, imgs


def fnorm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower().replace("-", "").replace(",", ""))


rows = list(csv.DictReader(open(out / "fidelity.csv", encoding="utf-8", newline="")))
groups = collections.defaultdict(list)
font_missing = collections.Counter()
font_files = collections.defaultdict(list)
n_ok = 0
for r in rows:
    rel = r["relpath"]
    pdf = out / "pdf" / (r["index"] + ".pdf")
    if r["open_ok"] != "1" or r["pdf_ok"] != "1" or not pdf.exists():
        groups["B1 không mở/xuất được"].append((rel, r.get("kind", "?")))
        continue
    try:
        pages, ptext, pfonts, pimgs = pdf_info(pdf)
    except Exception as e:
        groups["B2 PDF không đọc được"].append((rel, str(e)[:80]))
        continue
    src = tdir / rel
    try:
        body, tbl, hf, simgs, sfonts = docx_parts(src) if src.suffix.lower() == ".docx" else pptx_parts(src)
    except Exception as e:
        groups["? không phân tích được tệp gốc"].append((rel, str(e)[:80]))
        continue
    have = set(toks(ptext))
    problems = []
    parts = int(r["parts"])
    if pages != parts:
        problems.append(("A/B: số trang hiển thị ≠ in", "engine %d, PDF %d" % (parts, pages)))
    for label, txt in (("thân bài", body), ("bảng", tbl), ("đầu/cuối trang", hf)):
        c = coverage(txt, have)
        if c is not None and c < 0.97:
            problems.append(("thiếu chữ " + label, "%.1f%%" % (c * 100)))
    if simgs and pimgs < simgs:
        problems.append(("thiếu ảnh trong PDF", "gốc %d, PDF %d" % (simgs, pimgs)))
    pf = {fnorm(f) for f in pfonts}
    miss = sorted(f for f in sfonts if fnorm(f) not in pf and not any(fnorm(f) in x or x in fnorm(f) for x in pf if x))
    for f in miss:
        font_missing[f] += 1
        font_files[f].append(rel)
    if miss and pages == parts:
        problems.append(("font gốc không có trong PDF (bị thay)", ", ".join(miss[:4])))
    if problems:
        for k, why in problems:
            groups[k].append((rel, why))
    else:
        n_ok += 1

print("NỀN: %s | số mẫu: %d | khớp mọi tiêu chí: %d" % (plat, len(rows), n_ok))
for k in sorted(groups):
    print("\n[NHÓM] %s — %d mẫu" % (k, len(groups[k])))
    for rel, why in groups[k][:8]:
        print("   -", rel, "→", why)
    if len(groups[k]) > 8:
        print("   ... và %d mẫu nữa" % (len(groups[k]) - 8))
print("\n[FONT] font khai báo trong mẫu mà PDF không nhúng (top 25; số mẫu bị ảnh hưởng):")
for f, c in font_missing.most_common(25):
    print("   - %-32s %3d mẫu  vd: %s" % (f, c, font_files[f][0]))
print("\nKẾT QUẢ KIỂM KÊ:", "CÓ NHÓM LỆCH" if groups else "TẤT CẢ KHỚP", "(không chặn CI)")
