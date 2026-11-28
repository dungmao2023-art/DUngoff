// SPDX-License-Identifier: MPL-2.0
#pragma once
// Isolated bundled UNO helper. No document macros or external link updates.
inline constexpr const char* kCreatePivotScript = R"DUNG_PIVOT(import sys, pathlib, subprocess, tempfile, time, uuid, zipfile
import uno
from xml.etree import ElementTree as ET
def prop(name, value):
    p = uno.createUnoStruct("com.sun.star.beans.PropertyValue")
    p.Name, p.Value = name, value
    return p
def main():
    program, src, dst, sheet, area, row, col, val, fn = sys.argv[1:]
    src, dst = pathlib.Path(src).resolve(), pathlib.Path(dst).resolve()
    if src == dst or dst.exists() or fn not in ("SUM", "COUNT"):
        raise ValueError("Invalid output or aggregation")
    with tempfile.TemporaryDirectory(prefix="dung-pivot-") as tmp:
        endpoint = "pipe,name=dung_" + uuid.uuid4().hex + ";urp;"
        exe = pathlib.Path(program) / ("soffice.exe" if sys.platform == "win32" else "soffice")
        proc = subprocess.Popen([str(exe), "--headless", "--nologo", "--norestore",
            "-env:UserInstallation=" + (pathlib.Path(tmp)/"profile").as_uri(), "--accept=" + endpoint],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        desktop = doc = None
        try:
            local = uno.getComponentContext()
            resolver = local.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver", local)
            for _ in range(50):
                try:
                    ctx = resolver.resolve("uno:" + endpoint + "StarOffice.ComponentContext")
                    break
                except Exception:
                    if proc.poll() is not None:
                        raise RuntimeError("Engine exited")
                    time.sleep(.3)
            else:
                raise RuntimeError("UNO connection failed")
            desktop = ctx.ServiceManager.createInstanceWithContext("com.sun.star.frame.Desktop", ctx)
            doc = desktop.loadComponentFromURL(src.as_uri(), "_blank", 0, (
                prop("Hidden", True), prop("ReadOnly", False),
                prop("MacroExecutionMode", uno.getConstantByName("com.sun.star.document.MacroExecMode.NEVER_EXECUTE")),
                prop("UpdateDocMode", uno.getConstantByName("com.sun.star.document.UpdateDocMode.NO_UPDATE"))))
            if doc is None or not doc.supportsService("com.sun.star.sheet.SpreadsheetDocument"):
                raise ValueError("Not a spreadsheet")
            sheets = doc.getSheets()
            sh = sheets.getByIndex(int(sheet))
            rng = sh.getCellRangeByName(area)
            addr = rng.getRangeAddress()
            width = addr.EndColumn - addr.StartColumn + 1
            height = addr.EndRow - addr.StartRow + 1
            row, col, val = int(row), int(col), int(val)
            if width < 2 or height < 2 or width*height > 50000 or width > 100:
                raise ValueError("Invalid source range")
            selected = [row, val] + ([col] if col >= 0 else [])
            if len(set(selected)) != len(selected) or min(selected)<0 or max(selected)>=width:
                raise ValueError("Choose different fields")
            headers = [rng.getCellByPosition(i,0).getString().strip() for i in range(width)]
            if any(not h for h in headers) or len(set(h.casefold() for h in headers)) != width:
                raise ValueError("Source headers must be unique and nonempty")
            name = "Pivot"
            i = 2
            while sheets.hasByName(name):
                name = "Pivot" + str(i)
                i += 1
            sheets.insertNewByName(name, sheets.getCount())
            target = sheets.getByName(name)
            tables = target.getDataPilotTables()
            desc = tables.createDataPilotDescriptor()
            desc.setSourceRange(addr)
            fields = desc.getDataPilotFields()
            orient = "com.sun.star.sheet.DataPilotFieldOrientation"
            fields.getByIndex(row).Orientation = uno.Enum(orient, "ROW")
            if col >= 0:
                fields.getByIndex(col).Orientation = uno.Enum(orient, "COLUMN")
            data = fields.getByIndex(val)
            data.Orientation = uno.Enum(orient, "DATA")
            data.Function = uno.Enum("com.sun.star.sheet.GeneralFunction", fn)
            tables.insertNewByName("DungPivot_" + uuid.uuid4().hex[:8], target.getCellRangeByName("A1").getCellAddress(), desc)
            if tables.getCount() != 1:
                raise RuntimeError("Pivot missing")
            doc.storeAsURL(dst.as_uri(), (prop("FilterName", "Calc MS Excel 2007 XML"), prop("Overwrite", False)))
            with zipfile.ZipFile(dst) as z:
                ns = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
                defs = [n for n in z.namelist() if n.startswith("xl/pivotTables/pivotTable") and n.endswith(".xml")]
                if not any(ET.fromstring(z.read(n)).get("name","").startswith("DungPivot_") for n in defs):
                    raise RuntimeError("No real PivotTable in output")
                if not any(n.startswith("xl/pivotCache/pivotCacheDefinition") for n in z.namelist()):
                    raise RuntimeError("No pivot cache")
        finally:
            if doc is not None:
                try: doc.close(True)
                except Exception: pass
            if desktop is not None:
                try: desktop.terminate()
                except Exception: pass
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("PIVOT_OK", flush=True)
if __name__ == "__main__":
    main()
)DUNG_PIVOT";
