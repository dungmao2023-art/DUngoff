# SPDX-License-Identifier: MPL-2.0
"""Run the actual bundled script on a real workbook; check the generated pivot."""
import pathlib, re, subprocess, sys, tempfile, zipfile
from xml.etree import ElementTree as ET
from openpyxl import Workbook
root=pathlib.Path(__file__).resolve().parents[2]
script=(root/"app/src/excel/CreatePivotScript.h").read_text(encoding="utf-8").split('R"DUNG_PIVOT(',1)[1].split(')DUNG_PIVOT"',1)[0]
program,python=sys.argv[1:]
S="{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
with tempfile.TemporaryDirectory() as t:
    d=pathlib.Path(t)
    source=d/"source.xlsx"
    wb=Workbook()
    ws=wb.active
    for r in [("Category","Month","Amount"),("A","Jan",12),("B","Jan",34),("A","Feb",8)]:
        ws.append(r)
    wb.save(source)
    original=source.read_bytes()
    helper=d/"pivot.py"
    helper.write_text(script,encoding="utf-8")
    for fn, column, expected in [("SUM","-1",54),("COUNT","1",3)]:
        target=d/(fn+".xlsx")
        p=subprocess.run([python,str(helper),program,str(source),str(target),"0","A1:C4","0",column,"2",fn],capture_output=True,timeout=100)
        assert p.returncode==0 and b"PIVOT_OK" in p.stdout,(p.stdout,p.stderr)
        with zipfile.ZipFile(target) as z:
            defs=[n for n in z.namelist() if n.startswith("xl/pivotTables/pivotTable") and n.endswith(".xml")]
            assert len(defs)==1
            pivot=ET.fromstring(z.read(defs[0]))
            field=pivot.find(S+"dataFields").find(S+"dataField")
            assert field.get("subtotal","sum")==fn.lower(),field.attrib
            vals=[float(c.findtext(S+"v")) for c in ET.fromstring(z.read("xl/worksheets/sheet2.xml")).iter(S+"c") if c.get("t","n")=="n" and c.findtext(S+"v") is not None]
            assert expected in vals,vals
            assert (pivot.find(S+"colFields") is not None)==(column!=" -1".strip())
        assert source.read_bytes()==original
print("PASS real SUM and COUNT PivotTable on separate sheet, original workbook unchanged")
