# -*- coding: utf-8 -*-
"""Kiểm tra nội dung tệp do tools/smoketest/excel_tests.cpp ghi ra.
Dùng: python3 -I verify_excel.py <excel-ket-qua.xlsx> [<stress-goc.xlsx> <stress-ket-qua.xlsx>]
Mọi thay đổi phải KHÔNG làm mất công thức / ghi chú / liên kết / kiểu dữ liệu."""
import datetime as dt
import sys

import openpyxl

fails = 0


def check(ok, what):
    global fails
    print(("[ĐẠT]  " if ok else "[LỖI]  ") + what)
    if not ok:
        fails += 1


def basic(path):
    wb = openpyxl.load_workbook(path)
    check(wb.sheetnames[:2] == ["Dữ liệu", "Phụ"] and len(wb.sheetnames) == 3 and "Đổi tên" not in wb.sheetnames,
          f"Danh sách sheet sau thêm/đổi tên/nhân đôi/xoá: {wb.sheetnames}")
    ws = wb["Dữ liệu"]
    rows = [r for r in range(2, ws.max_row + 1) if isinstance(ws.cell(r, 2).value, (int, float))]
    check(len(rows) == 5, f"Còn đủ 5 dòng số liệu: {rows}")
    check({ws.cell(r, 2).value for r in rows} == {1500.5, 300, 45, 20, 7}, "Giá trị số giữ nguyên, không bị ép kiểu")
    check({ws.cell(r, 1).value for r in rows} == {"Zút", "Thước", "Mực", "Vở", "Kẹp"},
          "Thay thế chỉ đổi văn bản (Bút→Zút), tên khác giữ nguyên")
    vals = [ws.cell(r, 2).value for r in rows]
    check(vals == sorted(vals, reverse=True), f"Sắp xếp giảm dần: {vals}")
    check(all(ws.cell(r, 4).value == f"=B{r}*2" for r in rows),
          "Công thức theo dòng khi sắp xếp/chèn hàng, và KHÔNG bị thay 'B'→'Z': "
          + str([ws.cell(r, 4).value for r in rows]))
    check(all(isinstance(ws.cell(r, 3).value, dt.datetime) for r in rows), "Cột ngày vẫn là kiểu ngày")
    ro = next(r for r in rows if ws.cell(r, 1).value == "Vở")
    check(ws.cell(ro, 1).comment is not None and "ghi chú của Vở" in ws.cell(ro, 1).comment.text,
          "Ghi chú đi theo dòng 'Vở' sau sắp xếp")
    rk = next(r for r in rows if ws.cell(r, 1).value == "Kẹp")
    check(ws.cell(rk, 1).hyperlink is not None, "Liên kết đi theo dòng 'Kẹp' sau sắp xếp")
    a1 = ws["A1"]
    check(bool(a1.font.b) and bool(a1.font.i), "A1 đậm + nghiêng")
    check(a1.font.sz == 16 and a1.font.name == "Arial", f"A1 cỡ 16, phông Arial (thực tế {a1.font.sz}, {a1.font.name})")
    check(str(a1.font.color.rgb).endswith("FF0000") if a1.font.color else False, "A1 màu chữ đỏ")
    check(str(a1.fill.fgColor.rgb).endswith("FFFF00"), "A1 màu nền vàng")
    check(a1.alignment.horizontal == "center" and bool(a1.alignment.wrap_text), "A1 căn giữa + xuống dòng")
    check(any("%" in ws.cell(r, 2).number_format for r in rows), "Có ô số ở định dạng phần trăm (giá trị vẫn là số)")
    check(any(ws.cell(r, 2).number_format.startswith("#,##0") for r in rows), "Có ô số ở định dạng #,##0")
    w = ws.column_dimensions["C"].width
    check(w is not None and w >= 15, f"Cột C đã đặt rộng ~4 cm (width={w}; đơn vị ký tự Excel phụ thuộc phông mặc định, Linux ra 18)")
    check(any(ws.row_dimensions[r].height and 38 < ws.row_dimensions[r].height < 47 for r in range(1, 12)),
          "Có hàng cao ~1,5 cm")
    check(ws.freeze_panes == "A2", f"Cố định hàng đầu: {ws.freeze_panes}")
    check(bool(ws.auto_filter.ref), f"Bộ lọc tự động: {ws.auto_filter.ref}")
    # AutoSum chỉ để thông tin
    for r in range(ws.max_row - 3, ws.max_row + 1):
        v = ws.cell(r, 2).value
        if isinstance(v, str) and v.upper().startswith("=SUM"):
            print(f"[TIN]  AutoSum chèn {v} ở B{r}")


def stress(orig, res):
    a = openpyxl.load_workbook(orig)["DonHang"]
    wb = openpyxl.load_workbook(res)
    b = wb["DonHang"]
    check(b.max_row == a.max_row, f"Đủ {a.max_row - 1} dòng sau sắp xếp ({b.max_row - 1})")
    key = lambda w: sorted((w.cell(r, 1).value, w.cell(r, 4).value) for r in range(2, w.max_row + 1))
    check(key(a) == key(b), "Không mất/trùng dòng nào (Mã + Số lượng)")
    check(all(str(b.cell(r, 6).value).startswith("=D") for r in range(2, b.max_row + 1)),
          "Công thức Thành tiền còn nguyên ở mọi dòng")
    wv = openpyxl.load_workbook(res, data_only=True)["DonHang"]
    f = [wv.cell(r, 6).value for r in range(2, wv.max_row + 1) if wv.cell(r, 6).value is not None]
    check(len(f) > 4000 and f == sorted(f, reverse=True), "Cột F đã sắp giảm dần")
    tbl = {k: (v if isinstance(v, str) else v.ref) for k, v in b.tables.items()}
    check(tbl.get("BangDonHang") == "A1:H5001", f"Bảng (Table) BangDonHang còn nguyên vùng A1:H5001: {tbl}")
    # Bộ lọc: tệp gốc đã có sẵn bộ lọc của Table; lệnh bật/tắt là ĐẢO TRẠNG THÁI nên chỉ ghi nhận.
    print(f"[TIN]  Sau khi bấm Bộ lọc trên vùng đã có sẵn bộ lọc: auto_filter={b.auto_filter.ref!r} (bật/tắt đảo trạng thái)")


if __name__ == "__main__":
    basic(sys.argv[1])
    from pathlib import Path
    folder = Path(sys.argv[1]).parent
    ws = openpyxl.load_workbook(folder / "words-range.xlsx").active
    check([ws[c].value for c in ("A1","B1","A2","B2")] == ["Mười hai","Năm","Bảy","Chín"],
          "Native paste đổi đúng 4 ô thành chữ Việt")
    check(all(ws[c].data_type == "s" for c in ("A1","B1","A2","B2")), "Kết quả là chữ thực trong XLSX")
    check(ws["C1"].value == "OUTSIDE" and ws["A3"].value == "BELOW", "Không thay ô ngoài vùng")
    undo = openpyxl.load_workbook(folder / "words-range-undo.xlsx").active
    check([undo[c].value for c in ("A1","B1","A2","B2")] == [12,"=2+3",7,9],
          "Một Undo khôi phục cả số và công thức")
    check(undo["C1"].value == "OUTSIDE" and undo["A3"].value == "BELOW", "Undo không đổi ô ngoài vùng")
    if len(sys.argv) >= 4:
        stress(sys.argv[2], sys.argv[3])
    print("\nKẾT QUẢ XÁC MINH EXCEL: " + ("CÓ LỖI" if fails else "ĐẠT"))
    sys.exit(1 if fails else 0)
