# SPDX-License-Identifier: MPL-2.0
"""Synthetic XLSX: protected cells, editable dropdown and rule preservation."""
import sys
from pathlib import Path
from zipfile import ZipFile
from xml.etree import ElementTree as ET
from openpyxl import Workbook, load_workbook
from openpyxl.styles import Protection
from openpyxl.worksheet.datavalidation import DataValidation


def make(root):
    root.mkdir(parents=True, exist_ok=True)
    wb = Workbook()
    ws = wb.active
    ws.title = "Trang"
    ws.append(["Mã", "Trạng thái", "Số"])
    ws.append(["001", "Đạt", 12])
    ws.append(["002", "Chờ", "=C2*2"])
    for cell in ("B2", "B3"):
        ws[cell].protection = Protection(locked=False)
    ws.protection.sheet = True
    rule = DataValidation(type="list", formula1='"Đạt,Chờ"', allow_blank=True,
                          showErrorMessage=True, errorStyle="stop")
    rule.add("B2:B3")
    ws.add_data_validation(rule)
    wb.create_sheet("Khác")["A1"] = "unchanged"
    wb.save(root / "validation-protection-input.xlsx")


def verify(root):
    before = load_workbook(root / "validation-protection-input.xlsx")
    after = load_workbook(root / "validation-protection-output.xlsx")
    failures = []
    def check(ok, name):
        print(("PASS " if ok else "FAIL ") + name)
        if not ok:
            failures.append(name)
    baseline_path = root / "validation-protection-baseline.xlsx"
    baseline = load_workbook(baseline_path)
    check(baseline.sheetnames == before.sheetnames, "pristine sheet identity")
    check(baseline["Trang"]["B2"].value == "Đạt", "pristine cell unchanged")
    check(after.sheetnames == before.sheetnames, "sheet identity")
    ws = after["Trang"]
    check(ws.protection.sheet is True, "sheet stays protected")
    check(ws["A2"].protection.locked and ws["C3"].protection.locked, "locked cells stay locked")
    check(not ws["B2"].protection.locked and not ws["B3"].protection.locked, "editable cells stay unlocked")
    check(ws["B2"].value == "Chờ" and ws["B3"].value == "Chờ", "allowed edit saved")
    check(ws["A2"].value == "001" and ws["A3"].value == "002" and ws["C2"].value == 12 and ws["C3"].value == "=C2*2", "locked values, text codes and formula preserved")
    check(after["Khác"]["A1"].value == "unchanged", "other sheet preserved")
    def verify_rule(book, path, label):
        sheet = book["Trang"]
        rules = list(sheet.data_validations.dataValidation)
        matching = [r for r in rules if r.type == "list" and "B2" in r and "B3" in r]
        check(len(rules) == 1 and len(matching) == 1, label + ": single list rule retained")
        # The synthetic fixture always uses sheet1; report only policy attributes.
        with ZipFile(path) as package:
            root_xml = ET.fromstring(package.read("xl/worksheets/sheet1.xml"))
            nodes = root_xml.findall(".//{http://schemas.openxmlformats.org/spreadsheetml/2006/main}dataValidation")
            check(len(nodes) == 1, label + ": exactly one XML rule")
            if len(nodes) == 1:
                policy = nodes[0].attrib
                print(label, "XML policy", {k: policy.get(k, "<absent>") for k in
                    ("allowBlank", "showErrorMessage", "errorStyle", "showDropDown")})
                # OOXML boolean lexical forms: 1/true and 0/false. Older
                # openpyxl builds misread allowBlank even on the input fixture.
                check(policy.get("allowBlank") in ("1", "true"), label + ": allow blank preserved")
                check(policy.get("showErrorMessage") in ("1", "true"), label + ": error message enabled")
                check(policy.get("errorStyle", "stop") == "stop", label + ": stop error style preserved")
        if len(matching) != 1:
            return
        r = matching[0]
        check(str(r.sqref) == "B2:B3", label + ": validation range unchanged")
        check(r.formula1 == '"Đạt,Chờ"', label + ": exact Vietnamese list retained")
    verify_rule(before, root / "validation-protection-input.xlsx", "input")
    verify_rule(baseline, baseline_path, "pristine")
    verify_rule(after, root / "validation-protection-output.xlsx", "edited")
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    action, path = sys.argv[1:]
    {"make": make, "verify": verify}[action](Path(path))
