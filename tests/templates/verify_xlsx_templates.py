# SPDX-License-Identifier: MPL-2.0
"""Kiểm tra mẫu XLSX trong gói, không sửa file nguồn."""
from pathlib import Path
import csv, sys, zipfile, xml.etree.ElementTree as ET
root = Path(sys.argv[1])
ns = {"s": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}
with (root / "00_HUONG_DAN_VA_DANH_MUC/DANH_MUC_TAP_TIN.csv").open(encoding="utf-8-sig", newline="") as fp:
    rows = list(csv.DictReader(fp))
xlsx = [r for r in rows if r["Dinh_dang"] == "XLSX"]
assert len(xlsx) >= 12, "thiếu mẫu Excel"
for r in xlsx:
    p = root / r["Duong_dan"]
    with zipfile.ZipFile(p) as z:
        assert z.testzip() is None, p
        for n in z.namelist():
            if n.endswith((".xml", ".rels")):
                ET.fromstring(z.read(n))
        s = ET.fromstring(z.read("xl/worksheets/sheet1.xml"))
        formulas = s.findall(".//s:f", ns)
        assert len(formulas) >= 40, p
        assert not any("[" in (x.text or "") for x in formulas), p
        assert s.find("s:autoFilter", ns) is not None, p
    print("OK", p.name, len(formulas))
print("Verified", len(xlsx), "XLSX")
