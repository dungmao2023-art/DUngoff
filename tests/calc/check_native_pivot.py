"""Verify actual Calc pivot refresh, not a static summary or a modified flag."""
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree as E
S = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
def verify(path, source_value, total, moved=False):
    with zipfile.ZipFile(path) as z:
        root = E.fromstring(z.read("xl/worksheets/sheet1.xml"))
        cells = {c.get("r"): c for c in root.iter(S+"c")}
        assert float(cells["D2"].findtext(S+"v")) == source_value
        strings = []
        if "xl/sharedStrings.xml" in z.namelist():
            strings = ["".join(node.text or "" for node in si.iter(S+"t"))
                       for si in E.fromstring(z.read("xl/sharedStrings.xml")).iter(S+"si")]
        def cell_text(cell):
            if cell.get("t") == "s":
                return strings[int(cell.findtext(S+"v"))]
            if cell.get("t") == "inlineStr":
                return "".join(node.text or "" for node in cell.iter(S+"t"))
            return cell.findtext(S+"v") or ""
        assert cell_text(cells["B2"]) == ("Mặt hàng 2" if moved else "Mặt hàng 1")
        expected = {"Mặt hàng 0": 0, "Mặt hàng 1": 0, "Mặt hàng 2": 0}
        for i in range(1, 11):
            row = i + 1
            category = "Mặt hàng 2" if moved and i == 1 else f"Mặt hàng {i % 3}"
            amount = source_value if i == 1 else i * 125000
            assert cell_text(cells[f"B{row}"]) == category
            assert float(cells[f"D{row}"].findtext(S+"v")) == amount
            expected[category] += amount
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
        grouped_rows = {}
        numeric_rows = {}
        for ref, cell in cells.items():
            col, row = address(ref)
            if not (lo[0] <= col <= hi[0] and lo[1] <= row <= hi[1]):
                continue
            label = cell_text(cell)
            if label in expected:
                assert label not in grouped_rows, ("duplicate group", label)
                grouped_rows[label] = row
            if cell.get("t", "n") == "n":
                value = cell.findtext(S+"v")
                if value is not None:
                    values.append(float(value))
                    numeric_rows.setdefault(row, []).append(float(value))
        assert total in values, ("pivot refresh did not update actual total", values, loc.get("ref"))
        assert grouped_rows.keys() == expected.keys(), (grouped_rows, expected)
        for label, amount in expected.items():
            assert amount in numeric_rows.get(grouped_rows[label], []), (label, amount, numeric_rows)
        assert sum(expected.values()) == total
verify(sys.argv[1], 225000, 6975000)
verify(Path(sys.argv[1]).with_name("native-pivot-roundtrip.xlsx"), 325000, 7075000)
verify(Path(sys.argv[1]).with_name("native-pivot-regroup.xlsx"), 325000, 7075000, moved=True)
print("PASS native PivotTable retained and refreshed values and category grouping after XLSX reopen")
