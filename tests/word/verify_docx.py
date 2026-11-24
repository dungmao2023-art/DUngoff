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
print("\nKẾT QUẢ DOCX:", "CÓ LỖI" if fails else "TẤT CẢ ĐẠT")
sys.exit(1 if fails else 0)
