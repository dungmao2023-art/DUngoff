# SPDX-License-Identifier: MPL-2.0
"""Standalone UNO capability probes, not proof of live LOK editing.

Every case creates a new synthetic document and verifies saved OOXML.
Failure in one case does not hide results for the others.
"""
import os
import pathlib
import subprocess
import tempfile
import time
import uuid
import zipfile
import xml.etree.ElementTree as ET
import sys

prog = pathlib.Path(os.environ["DUNG_PROGRAM_DIR"]).resolve(strict=True)
sys.path.insert(0, str(prog))
import uno
from com.sun.star.beans import PropertyValue

W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
X = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
P = "{http://schemas.openxmlformats.org/presentationml/2006/main}"


def pv(name, value):
    p = PropertyValue()
    p.Name, p.Value = name, value
    return p


def save(doc, path, fmt):
    doc.storeToURL(uno.systemPathToFileUrl(str(path)), (pv("FilterName", fmt),))


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def split(doc, root):
    table = doc.createInstance("com.sun.star.text.TextTable")
    table.initialize(2, 2)
    doc.Text.insertTextContent(doc.Text.createTextCursor(), table, False)
    table.getCellByName("A1").String = "KEEP_A"
    table.getCellByName("B2").String = "KEEP_B"
    cursor = table.createCursorByCellName("A1")
    require(cursor.splitRange(2, False), "splitRange returned false")
    path = root / "split.docx"
    save(doc, path, "MS Word 2007 XML")
    with zipfile.ZipFile(path) as z:
        xml = ET.fromstring(z.read("word/document.xml"))
    require(len(list(xml.iter(W+"tc"))) > 4, "no additional cell")
    text = "".join(n.text or "" for n in xml.iter(W+"t"))
    require("KEEP_A" in text and "KEEP_B" in text, "cell contents lost")


def toc(doc, root):
    text = doc.Text
    cursor = text.createTextCursor()
    cursor.ParaStyleName = "Heading 1"
    text.insertString(cursor, "HEADING_PROBE", False)
    cursor.gotoEnd(False)
    text.insertControlCharacter(cursor, 0, False)
    cursor.gotoEnd(False)
    cursor.ParaStyleName = "Text Body"
    index = doc.createInstance("com.sun.star.text.ContentIndex")
    index.CreateFromOutline = True
    text.insertTextContent(cursor, index, False)
    index.update()
    path = root / "toc.docx"
    save(doc, path, "MS Word 2007 XML")
    with zipfile.ZipFile(path) as z:
        xml = ET.fromstring(z.read("word/document.xml"))
    require(any("TOC" in (n.text or "") for n in xml.iter(W+"instrText")), "missing real TOC field")
    require(sum("HEADING_PROBE" in (n.text or "") for n in xml.iter(W+"t")) >= 2, "heading absent from TOC result")


def validation(doc, root):
    sh = doc.Sheets.getByIndex(0)
    sh.getCellRangeByName("A1").String = "001"
    sh.getCellRangeByName("B1").String = "Đạt"
    sh.getCellRangeByName("D1").String = "Đạt"
    sh.getCellRangeByName("D2").String = "Chờ"
    cells = sh.getCellRangeByName("B1:B2")
    rule = cells.Validation
    rule.Type = uno.Enum("com.sun.star.sheet.ValidationType", "LIST")
    rule.setFormula1("$D$1:$D$2")
    rule.IgnoreBlankCells = True
    rule.ShowErrorMessage = True
    rule.ErrorAlertStyle = uno.Enum("com.sun.star.sheet.ValidationAlertStyle", "STOP")
    cells.Validation = rule
    cp = cells.CellProtection
    cp.IsLocked = False
    cells.CellProtection = cp
    sh.protect("")
    require(sh.isProtected(), "sheet not protected")
    path = root / "validation.xlsx"
    save(doc, path, "Calc MS Excel 2007 XML")
    with zipfile.ZipFile(path) as z:
        xml = ET.fromstring(z.read("xl/worksheets/sheet1.xml"))
    dv = next(iter(xml.iter(X+"dataValidation")), None)
    require(dv is not None and dv.get("type") == "list" and dv.get("sqref") == "B1:B2", "validation missing")
    require(dv.get("allowBlank") in ("1", "true") and dv.get("showErrorMessage") in ("1", "true"), "validation policy lost")
    require(dv.get("errorStyle", "stop") == "stop", "stop policy lost")
    require(next(iter(xml.iter(X+"sheetProtection")), None) is not None, "protection lost")


def pivot(doc, root):
    sh = doc.Sheets.getByIndex(0)
    sh.getCellRangeByName("A1:B4").setDataArray((("Category", "Amount"), ("A", 12), ("B", 34), ("A", 8)))
    tables = sh.getDataPilotTables()
    desc = tables.createDataPilotDescriptor()
    desc.setSourceRange(sh.getCellRangeByName("A1:B4").getRangeAddress())
    fields = desc.getDataPilotFields()
    fields.getByIndex(0).Orientation = uno.Enum("com.sun.star.sheet.DataPilotFieldOrientation", "ROW")
    data = fields.getByIndex(1)
    data.Orientation = uno.Enum("com.sun.star.sheet.DataPilotFieldOrientation", "DATA")
    data.Function = uno.Enum("com.sun.star.sheet.GeneralFunction", "SUM")
    tables.insertNewByName("DungPivot", sh.getCellRangeByName("F1").getCellAddress(), desc)
    require(tables.hasByName("DungPivot"), "pivot missing in model")
    path = root / "pivot.xlsx"
    save(doc, path, "Calc MS Excel 2007 XML")
    with zipfile.ZipFile(path) as z:
        require(any(n.startswith("xl/pivotTables/pivotTable") and n.endswith(".xml") for n in z.namelist()), "no real pivot")
        require(any(n.startswith("xl/pivotCache/pivotCacheDefinition") and n.endswith(".xml") for n in z.namelist()), "no pivot cache")


def shapes(doc, root):
    page = doc.getDrawPages().getByIndex(0)
    # Collection must belong to the same remote UNO context; caller sets global ctx.
    collection = remote_ctx.ServiceManager.createInstanceWithContext("com.sun.star.drawing.ShapeCollection", remote_ctx)
    for i in range(2):
        shape = doc.createInstance("com.sun.star.drawing.RectangleShape")
        page.add(shape)
        shape.setPosition(uno.createUnoStruct("com.sun.star.awt.Point", 2000+i*5000, 2000))
        shape.setSize(uno.createUnoStruct("com.sun.star.awt.Size", 3000, 2000))
        shape.String = "SHAPE_"+str(i)
        collection.add(shape)
    group = page.group(collection)
    require(group is not None and group.getCount() == 2, "group missing")
    path = root / "group.pptx"
    save(doc, path, "Impress MS PowerPoint 2007 XML")
    with zipfile.ZipFile(path) as z:
        xml = ET.fromstring(z.read("ppt/slides/slide1.xml"))
    require(any(len(list(g.iter(P+"sp"))) >= 2 for g in xml.iter(P+"grpSp")), "group not exported")


def main():
    global remote_ctx
    cases = [("writer-split", "swriter", split), ("writer-toc", "swriter", toc),
             ("calc-validation-protection", "scalc", validation), ("calc-pivot", "scalc", pivot),
             ("impress-group", "simpress", shapes)]
    failures = []
    with tempfile.TemporaryDirectory(prefix="dung-model-") as tmp:
        root = pathlib.Path(tmp)
        endpoint = "pipe,name=dung_" + uuid.uuid4().hex + ";urp;"
        proc = subprocess.Popen([str(prog/"soffice.exe"), "--headless", "--norestore", "--nologo",
                                 "-env:UserInstallation="+(root/"profile").as_uri(), "--accept="+endpoint])
        desktop = None
        try:
            local = uno.getComponentContext()
            resolver = local.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver", local)
            remote_ctx = None
            for _ in range(60):
                try:
                    remote_ctx = resolver.resolve("uno:"+endpoint+"StarOffice.ComponentContext")
                    break
                except Exception:
                    time.sleep(1)
            require(remote_ctx is not None, "UNO connection failed")
            desktop = remote_ctx.ServiceManager.createInstanceWithContext("com.sun.star.frame.Desktop", remote_ctx)
            for name, factory, run in cases:
                doc = None
                try:
                    doc = desktop.loadComponentFromURL("private:factory/"+factory, "_blank", 0, (pv("Hidden", True),))
                    require(doc is not None, "document creation failed")
                    run(doc, root)
                    print("PASS", name, flush=True)
                except Exception as exc:
                    failures.append(name)
                    print("FAIL", name, repr(exc), flush=True)
                finally:
                    if doc is not None:
                        try:
                            doc.close(True)
                        except Exception:
                            pass
        finally:
            if desktop is not None:
                try:
                    desktop.terminate()
                except Exception:
                    pass
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)
    print("STANDALONE UNO ONLY; live LOK NOT RUN; failures:", failures, flush=True)
    return int(bool(failures))


if __name__ == "__main__":
    sys.exit(main())
