# -*- coding: utf-8 -*-
"""Kiểm tra tệp DOCX do `dung-smoketest --word 1` lưu: định dạng phải còn nguyên sau khi lưu.
Chạy: python3 tests/word/verify_docx.py <word-dinh-dang.docx>   (thoát 1 nếu sai)"""
import sys
import zipfile

import docx

path = sys.argv[1]
d = docx.Document(path)
fails = []


def check(ok, what):
    print(("[ĐẠT]  " if ok else "[LỖI]  ") + what)
    if not ok:
        fails.append(what)


paras = d.paragraphs
check(any(p.style.name == "Heading 1" and p.text == "Tiêu đề thử" for p in paras), "Đoạn 'Tiêu đề thử' giữ kiểu Heading 1")
bold_ok = any(r.bold and r.text == "Chữ đậm" for p in paras for r in p.runs)
check(bold_ok, "Chữ 'Chữ đậm' giữ định dạng đậm")
normal_ok = any((not r.bold) and "thường" in r.text for p in paras for r in p.runs)
check(normal_ok, "Chữ ' thường' không bị đậm")
check(any("Mục một" in p.text for p in paras) and any("Mục hai" in p.text for p in paras), "Hai mục danh sách còn nguyên nội dung")
check([(len(t.rows), len(t.columns)) for t in d.tables] == [(2, 3)], "Bảng 3 cột x 2 hàng được lưu")
xml = zipfile.ZipFile(path).read("word/document.xml").decode("utf-8")
check(xml.count("<w:numPr>") >= 2, "Hai đoạn có danh sách (w:numPr)")
check("<w:hyperlink" in xml, "Liên kết được lưu (w:hyperlink)")
rels = zipfile.ZipFile(path).read("word/_rels/document.xml.rels").decode("utf-8")
check("https://example.com/" in rels, "Đích liên kết https://example.com/ còn nguyên")
sec = d.sections[0]
def mm(x): return round(x / 36000.0, 1) if x is not None else None
print("       lề thực tế (mm): trái", mm(sec.left_margin), "phải", mm(sec.right_margin), "trên", mm(sec.top_margin), "dưới", mm(sec.bottom_margin))
check(all(v is not None for v in (mm(sec.left_margin), mm(sec.right_margin), mm(sec.top_margin), mm(sec.bottom_margin))) and
      abs(mm(sec.left_margin) - 30) < 1 and abs(mm(sec.right_margin) - 15) < 1 and abs(mm(sec.top_margin) - 20) < 1 and abs(mm(sec.bottom_margin) - 20) < 1,
      "Lề chuẩn VN (trái 30, phải 15, trên 20, dưới 20 mm) được lưu vào DOCX")
print("\nKẾT QUẢ DOCX:", "CÓ LỖI" if fails else "TẤT CẢ ĐẠT")
sys.exit(1 if fails else 0)
