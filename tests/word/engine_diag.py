# -*- coding: utf-8 -*-
"""Chẩn đoán engine: tên lệnh trang/lề trong registry và trong mergedlo.dll. Chạy: python engine_diag.py <thư mục engine>"""
import glob, os, re, sys
eng = sys.argv[1]
pat = re.compile(r'oor:name="(\.uno:[A-Za-z]*(?:Margin|PageLR|PageUL|AttributePage|LRSpace|ULSpace|PageStyle|PageSetup|PageDialog)[A-Za-z]*)"')
for f in sorted(glob.glob(os.path.join(eng, "share", "registry", "*.xcd"))):
    names = sorted(set(pat.findall(open(f, encoding="utf-8", errors="ignore").read())))
    if names:
        print("REGISTRY", os.path.basename(f), names)
d = open(os.path.join(eng, "program", "mergedlo.dll"), "rb").read()
pat2 = re.compile(rb"(?:PageLRMargin|PageULMargin|AttributePage[A-Za-z]*|LeftRightMargin|UpperLowerMargin|PageMargin[A-Za-z]*)[A-Za-z.]{0,30}")
print("DLL", sorted(set(m.decode("ascii", "ignore") for m in pat2.findall(d)))[:80])
