# -*- coding: utf-8 -*-
"""Đọc w:pgMar của các tệp ung-vien-le-N.docx: ứng viên i đặt trái (21+i+1) mm; ứng viên nào làm lề trái đổi thì có tác dụng."""
import glob, re, sys, zipfile
for f in sorted(glob.glob(sys.argv[1] + "/ung-vien-le-*.docx")):
    x = zipfile.ZipFile(f).read("word/document.xml").decode("utf-8")
    m = re.search(r"<w:pgMar[^>]*>", x)
    g = lambda k: round(int(re.search(k + r'="(-?\d+)"', m.group(0)).group(1)) * 25.4 / 1440, 1) if m else None
    print(f.split("/")[-1].split(chr(92))[-1], "trái", g("w:left"), "phải", g("w:right"), "trên", g("w:top"), "dưới", g("w:bottom"))

for f in glob.glob(sys.argv[1] + "/chan-doan-kich-thuoc.docx"):
    x = zipfile.ZipFile(f).read("word/document.xml").decode("utf-8")
    print("CHẨN ĐOÁN kích thước trang (đặt 297x210 mm):", re.search(r"<w:pgSz[^>]*>", x).group(0) if re.search(r"<w:pgSz[^>]*>", x) else None)
