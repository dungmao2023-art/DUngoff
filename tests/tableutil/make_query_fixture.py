# SPDX-License-Identifier: MPL-2.0
"""Generate a small valid OOXML workbook using only Python's standard library."""
from pathlib import Path
import sys
from zipfile import ZipFile, ZIP_DEFLATED
from xml.etree.ElementTree import Element, SubElement, tostring

NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
REL = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
PKG = "http://schemas.openxmlformats.org/package/2006/relationships"
CT = "http://schemas.openxmlformats.org/package/2006/content-types"
names = ["Đơn hàng", "Danh mục", "Công thức"]
data = [
    [["Mã", "Khách hàng", "Số tiền"], ["001", "Nguyễn Văn An", 0.1],
     ["001", "Nguyễn Văn An", 0.2], ["1", "Trần Thị Bình", 12.3456], ["002", "Lê Hà", 0]],
    [["Mã", "Khu vực"], ["001", "An Khê"], ["1", "Gia Lai"]],
    [["Mã", "Số tiền"], ["001", "=1+2"]],
]

def xml(node):
    return tostring(node, encoding="utf-8", xml_declaration=True)

def sheet(rows):
    root = Element("worksheet", {"xmlns": NS})
    table = SubElement(root, "sheetData")
    for r, values in enumerate(rows, 1):
        row = SubElement(table, "row", {"r": str(r)})
        for c, value in enumerate(values):
            cell = SubElement(row, "c", {"r": chr(65+c)+str(r)})
            if isinstance(value, str) and value.startswith("="):
                SubElement(cell, "f").text = value[1:]
                SubElement(cell, "v").text = "3"
            elif isinstance(value, str):
                cell.set("t", "inlineStr")
                SubElement(SubElement(cell, "is"), "t").text = value
            else:
                SubElement(cell, "v").text = str(value)
    return xml(root)

types = Element("Types", {"xmlns": CT})
SubElement(types, "Default", Extension="rels", ContentType="application/vnd.openxmlformats-package.relationships+xml")
SubElement(types, "Default", Extension="xml", ContentType="application/xml")
SubElement(types, "Override", PartName="/xl/workbook.xml",
           ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml")
root_rels = Element("Relationships", {"xmlns": PKG})
SubElement(root_rels, "Relationship", Id="rId1", Type=REL+"/officeDocument", Target="xl/workbook.xml")
book = Element("workbook", {"xmlns": NS, "xmlns:r": REL})
sheets = SubElement(book, "sheets")
rels = Element("Relationships", {"xmlns": PKG})
for i, name in enumerate(names, 1):
    part = f"worksheets/sheet{i}.xml"
    SubElement(sheets, "sheet", {"name": name, "sheetId": str(i), "r:id": f"rId{i}"})
    SubElement(rels, "Relationship", Id=f"rId{i}", Type=REL+"/worksheet", Target=part)
    SubElement(types, "Override", PartName="/xl/"+part,
               ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml")
dest = Path(sys.argv[1])
dest.parent.mkdir(parents=True, exist_ok=True)
with ZipFile(dest, "w", compression=ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", xml(types))
    z.writestr("_rels/.rels", xml(root_rels))
    z.writestr("xl/workbook.xml", xml(book))
    z.writestr("xl/_rels/workbook.xml.rels", xml(rels))
    for i, rows in enumerate(data, 1):
        z.writestr(f"xl/worksheets/sheet{i}.xml", sheet(rows))
with ZipFile(dest) as z:
    assert z.testzip() is None
print(dest)
