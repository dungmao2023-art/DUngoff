"""Check the chart actually created by Calc in the active workbook, not an exported report."""
import sys
import posixpath
import zipfile
from xml.etree import ElementTree as ET
C = "{http://schemas.openxmlformats.org/drawingml/2006/chart}"
S = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
R = "{http://schemas.openxmlformats.org/officeDocument/2006/relationships}"
P = "{http://schemas.openxmlformats.org/package/2006/relationships}"
def part(z, owner, id):
    folder, name = posixpath.split(owner)
    rels = ET.fromstring(z.read(posixpath.join(folder, "_rels", name + ".rels")))
    rel = next(x for x in rels.iter(P+"Relationship") if x.get("Id") == id)
    assert rel.get("TargetMode") != "External"
    target = rel.get("Target")
    return target.lstrip("/") if target.startswith("/") else posixpath.normpath(posixpath.join(folder,target))
with zipfile.ZipFile(sys.argv[1]) as z:
    workbook = ET.fromstring(z.read("xl/workbook.xml"))
    first = workbook.find(".//"+S+"sheet")
    sheetpart = part(z,"xl/workbook.xml", first.get(R+"id"))
    root = ET.fromstring(z.read(sheetpart))
    for ref,val in (("B2","12"),("B3","34")):
        cell = next(x for x in root.iter(S+"c") if x.get("r")==ref)
        assert float(cell.findtext(S+"v")) == float(val), "source changed: "+ref
    drawing = root.find(S+"drawing")
    assert drawing is not None, "no drawing in active sheet"
    drawingpart = part(z,sheetpart,drawing.get(R+"id"))
    drawingxml = ET.fromstring(z.read(drawingpart))
    chartref = drawingxml.find(".//"+C+"chart")
    assert chartref is not None, "no chart linked to sheet drawing"
    chart = ET.fromstring(z.read(part(z,drawingpart,chartref.get(R+"id"))))
    assert chart.find(".//"+C+"barChart") is not None, "expected real column/bar chart"
    formulas = [n.text.replace("$","").replace("'","").replace(" ","") for n in chart.iter(C+"f") if n.text]
    assert any("B2:B3" in x for x in formulas), ("chart not linked to source values",formulas)
    assert any("A2:A3" in x for x in formulas), ("chart not linked to source categories",formulas)
print("PASS native Calc chart linked to active worksheet and original numeric cells")
