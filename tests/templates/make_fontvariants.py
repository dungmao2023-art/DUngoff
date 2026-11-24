# -*- coding: utf-8 -*-
"""Tạo các hồ sơ engine thử nghiệm (registrymodifications.xcu) cho bảng thay thế phông Aptos.
Dùng: make_fontvariants.py <thư mục gốc>  -> <gốc>/vN/profile/user/registrymodifications.xcu"""
import sys
from pathlib import Path
FONTS = ["Aptos", "Aptos Display", "Aptos Narrow", "Aptos SemiBold", "Aptos Light", "Aptos Black"]
HEAD = ('<?xml version="1.0" encoding="UTF-8"?>\n<oor:items xmlns:oor="http://openoffice.org/2001/registry" '
        'xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">\n')
def build(always, subst, replacement=True):
    s = HEAD
    if replacement:
        s += '<item oor:path="/org.openoffice.Office.Common/Font/Substitution"><prop oor:name="Replacement" oor:op="fuse"><value>true</value></prop></item>\n'
    for i, f in enumerate(FONTS):
        s += ('<item oor:path="/org.openoffice.Office.Common/Font/Substitution/FontPairs"><node oor:name="dung-aptos-%d" oor:op="replace">'
              '<prop oor:name="Always" oor:op="fuse"><value>%s</value></prop>'
              '<prop oor:name="OnScreenOnly" oor:op="fuse"><value>false</value></prop>'
              '<prop oor:name="ReplaceFont" oor:op="fuse"><value>%s</value></prop>'
              '<prop oor:name="SubstituteFont" oor:op="fuse"><value>%s</value></prop></node></item>\n') % (i, "true" if always else "false", f, subst)
    return s + "</oor:items>\n"
V = {
  "v1": build(False, "Carlito;Liberation Sans"),        # hiện tại
  "v2": build(True,  "Carlito;Liberation Sans"),        # Always = true
  "v3": build(True,  "Carlito"),                         # một phông duy nhất
  "v4": build(True,  "Liberation Sans"),                 # đối chứng: có áp dụng bảng không
  "v5": build(False, "Carlito"),                         # Always=false + một phông (cấu hình đề xuất)
}
root = Path(sys.argv[1])
for k, v in V.items():
    d = root / k / "profile" / "user"
    d.mkdir(parents=True, exist_ok=True)
    (d / "registrymodifications.xcu").write_text(v, encoding="utf-8")
    print("tạo", d)
