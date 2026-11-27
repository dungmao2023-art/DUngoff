# SPDX-License-Identifier: MPL-2.0
"""Synthetic XLSX: protected cells, editable dropdown and rule preservation."""
import sys
from pathlib import Path
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
    check(after.sheetnames == before.sheetnames, "sheet identity")
    ws = after["Trang"]
    check(ws.protection.sheet is True, "sheet stays protected")
    check(ws["A2"].protection.locked and ws["C3"].protection.locked, "locked cells stay locked")
    check(not ws["B2"].protection.locked and not ws["B3"].protection.locked, "editable cells stay unlocked")
    check(ws["B2"].value == "Chờ" and ws["B3"].value == "Chờ", "allowed edit saved")
    check(ws["A2"].value == "001" and ws["A3"].value == "002" and ws["C2"].value == 12 and ws["C3"].value == "=C2*2", "locked values, text codes and formula preserved")
    check(after["Khác"]["A1"].value == "unchanged", "other sheet preserved")
    rules = list(ws.data_validations.dataValidation)
    matching = [r for r in rules if r.type == "list" and "B2" in r and "B3" in r]
    check(len(rules) == 1 and len(matching) == 1, "single list rule retained across range")
    if matching:
        r = matching[0]
        check(str(r.sqref) == "B2:B3", "validation range unchanged")
        check(r.formula1 == '"Đạt,Chờ"', "exact Vietnamese list retained")
        check(r.allowBlank and r.showErrorMessage and r.errorStyle in (None, "stop"), "blank and stop error policy preserved")
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    action, path = sys.argv[1:]
    {"make": make, "verify": verify}[action](Path(path))
