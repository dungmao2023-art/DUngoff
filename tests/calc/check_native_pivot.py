"""Verify actual Calc pivot refresh, not a static summary or a modified flag."""
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree as E
S = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
def verify(path, source_value, total):
    with zipfile.ZipFile(path) as z:
        root = E.fromstring(z.read("xl/worksheets/sheet1.xml"))
        cells = {c.get("r"): c for c in root.iter(S+"c")}
        assert float(cells["D2"].findtext(S+"v")) == source_value
        defs = [n for n in z.namelist() if n.startswith("xl/pivotTables/pivotTable") and n.endswith(".xml")]
        assert len(defs) == 1, defs
        pivot = E.fromstring(z.read(defs[0]))
        assert pivot.get("name") == "PivotThu"
        loc = pivot.find(S+"location")
        assert loc is not None and loc.get("ref")
        assert any(n.startswith("xl/pivotCache/pivotCacheDefinition") for n in z.namelist())
        # Fixture has 10 rows: sum 125000*(1+...+10), plus D2 increment 100000.
        # Require new grand total in the actual pivot output area.
        def address(s):
            col = 0
            for ch in s:
                if ch.isalpha():
                    col = col * 26 + ord(ch.upper()) - ord("A") + 1
            return col, int("".join(c for c in s if c.isdigit()))
        lo, hi = map(address, loc.get("ref").split(":"))
        values = []
        for ref, cell in cells.items():
            col, row = address(ref)
            if lo[0] <= col <= hi[0] and lo[1] <= row <= hi[1] and cell.get("t", "n") == "n":
                value = cell.findtext(S+"v")
                if value is not None:
                    values.append(float(value))
        assert total in values, ("pivot refresh did not update actual total", values, loc.get("ref"))
verify(sys.argv[1], 225000, 6975000)
verify(Path(sys.argv[1]).with_name("native-pivot-roundtrip.xlsx"), 325000, 7075000)
print("PASS native PivotTable retained and refreshed after XLSX reopen")
