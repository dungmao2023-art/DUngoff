# -*- coding: utf-8 -*-
"""Tệp XLSX tổng hợp cho kiểm thử Excel cơ bản (không có dữ liệu thật). Dùng: python3 -I make_excel_fixture.py <thư_mục>"""
import datetime as dt
import os
import sys

from openpyxl import Workbook
from openpyxl.comments import Comment


def build(dest):
    wb = Workbook()
    ws = wb.active
    ws.title = "Dữ liệu"
    ws.append(["Tên", "Số", "Ngày", "Công thức", "Ghi chú"])
    rows = [("Bút", 1500.5, dt.datetime(2026, 3, 1)), ("Vở", 20, dt.datetime(2026, 1, 15)),
            ("Thước", 300, dt.datetime(2026, 2, 10)), ("Kẹp", 7, dt.datetime(2026, 5, 5)),
            ("Mực", 45, dt.datetime(2026, 4, 4))]
    for i, (n, v, d) in enumerate(rows, start=2):
        ws.append([n, v, d, f"=B{i}*2", ""])
        ws.cell(row=i, column=3).number_format = "dd/mm/yyyy"
    ws["A3"].comment = Comment("ghi chú của Vở", "t")      # đi theo dòng 'Vở' khi sắp xếp
    ws["A5"].hyperlink = "https://example.com/"            # đi theo dòng 'Kẹp'
    ws["A5"].value = "Kẹp"
    ws2 = wb.create_sheet("Phụ")
    ws2["A1"] = "x"
    # Fill tests use a separate worksheet and do not modify the five business rows.
    ws2["A2"], ws2["A3"], ws2["A4"] = 10, 20, 30
    ws2["B2"], ws2["C2"] = 5, 7
    ws2["E2"] = "=A2+$B$2"
    ws2["E3"], ws2["E4"] = "old3", "old4"
    ws2["G2"] = "=A2+$B$2"
    ws2["H2"], ws2["I2"] = "oldH", "oldI"
    # Reverse fill uses the last row/column as source, with relative references.
    ws2["D2"], ws2["D3"], ws2["D4"] = "old-up-2", "old-up-3", "=A4+$B$2"
    ws2["G3"], ws2["H3"], ws2["I3"] = "old-left-G", "old-left-H", "=C2+$B$2"
    # Numeric series replaces the whole selected range.
    for row in range(6, 10):
        ws2.cell(row, 5, "old-series")
    for col in range(7, 11):
        ws2.cell(6, col, "old-series")
    # Independent contiguous table for the ascending native sort.
    for row, values in enumerate([("Khóa", "Mã", "Giá trị"), (3, "003", 30), (1, "001", 10), (2, "002", 20)], start=10):
        for col, value in enumerate(values, start=11):
            ws2.cell(row, col, value)
    os.makedirs(dest, exist_ok=True)
    p = os.path.join(dest, "excel-co-ban.xlsx")
    wb.save(p)
    fill = Workbook()
    sheet = fill.active
    for i, value in enumerate((10, 20, 30, 40), 1):
        sheet.cell(i, 1, value)
    sheet["B1"] = "=A1*2"
    sheet["D1"] = "KEEP_FILL_NEIGHBOUR"
    fill.save(os.path.join(dest, "excel-native-fill-input.xlsx"))
    search = Workbook()
    sheet = search.active
    sheet.title = "Search"
    sheet["A1"], sheet["A2"], sheet["A3"] = "MẪU_CASE", "mẫu_case", "[literal.+]"
    sheet["A4"] = "00123"
    sheet["B1"] = '=LEN("MẪU_CASE")'
    other = search.create_sheet("Other")
    other["A1"] = "MẪU_CASE"
    search.save(os.path.join(dest, "excel-search-input.xlsx"))
    return p


if __name__ == "__main__":
    print("Đã tạo:", build(sys.argv[1] if len(sys.argv) > 1 else "fixtures"))
