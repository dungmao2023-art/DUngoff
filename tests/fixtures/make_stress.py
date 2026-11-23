# -*- coding: utf-8 -*-
"""Sinh tệp XLSX tổng hợp ~5000 dòng để kiểm thử hiệu năng và round-trip.

Tệp này KHÔNG chứa dữ liệu thật của người dùng; chỉ mô phỏng cấu trúc:
nhiều sheet, công thức (SUMIFS, IF, VLOOKUP), Table, biểu đồ, định dạng
có điều kiện (databar), danh sách xác thực dữ liệu, khung đóng băng.
Chạy: python3 -I tests/fixtures/make_stress.py <thư_mục_đích>
Tệp sinh ra không được commit (xem .gitignore).
"""
import datetime as dt
import os
import random
import sys

from openpyxl import Workbook
from openpyxl.chart import BarChart, Reference
from openpyxl.formatting.rule import DataBarRule
from openpyxl.utils import get_column_letter
from openpyxl.worksheet.datavalidation import DataValidation
from openpyxl.worksheet.table import Table, TableStyleInfo

ROWS = 5000
PRODUCTS = ["Bút bi", "Vở kẻ ngang", "Thước kẻ", "Bìa hồ sơ", "Kẹp giấy", "Mực in"]
REGIONS = ["Hà Nội", "Đà Nẵng", "TP. Hồ Chí Minh", "Cần Thơ"]


def build(dest_dir):
    random.seed(42)  # tái lập được
    wb = Workbook()

    # Danh mục
    dm = wb.active
    dm.title = "DanhMuc"
    dm.append(["Sản phẩm", "Đơn giá", "Nhóm"])
    for i, p in enumerate(PRODUCTS, start=2):
        dm.append([p, 5000 + i * 3000, "Văn phòng phẩm"])

    # Đơn hàng: 5000 dòng, công thức thành tiền
    dh = wb.create_sheet("DonHang")
    headers = ["Mã", "Ngày", "Sản phẩm", "Số lượng", "Đơn giá", "Thành tiền", "Khu vực", "Ghi chú"]
    dh.append(headers)
    start = dt.date(2026, 1, 1)
    for r in range(2, ROWS + 2):
        product = random.choice(PRODUCTS)
        dh.append([
            f"DH{r:05d}",
            start + dt.timedelta(days=random.randint(0, 330)),
            product,
            random.randint(1, 50),
            f"=VLOOKUP(C{r},DanhMuc!$A$2:$B$7,2,FALSE)",
            f"=D{r}*E{r}",
            random.choice(REGIONS),
            "" if random.random() < 0.9 else "Giao gấp",
        ])
        dh.cell(row=r, column=2).number_format = "dd/mm/yyyy"
        dh.cell(row=r, column=5).number_format = "#,##0"
        dh.cell(row=r, column=6).number_format = "#,##0"
    last = ROWS + 1
    dh.freeze_panes = "A2"
    tab = Table(displayName="BangDonHang", ref=f"A1:H{last}")
    tab.tableStyleInfo = TableStyleInfo(name="TableStyleMedium2", showRowStripes=True)
    dh.add_table(tab)
    dh.conditional_formatting.add(f"F2:F{last}", DataBarRule(start_type="min", end_type="max", color="5B9BD5"))
    dv = DataValidation(type="list", formula1='"Hà Nội,Đà Nẵng,TP. Hồ Chí Minh,Cần Thơ"', allow_blank=True)
    dh.add_data_validation(dv)
    dv.add(f"G2:G{last}")
    for col, width in zip("ABCDEFGH", [12, 12, 18, 12, 12, 14, 18, 14]):
        dh.column_dimensions[col].width = width

    # Tổng hợp theo sản phẩm: SUMIFS và IF
    th = wb.create_sheet("TongHop")
    th.append(["Sản phẩm", "Số lượng", "Doanh thu", "Trạng thái"])
    for i, p in enumerate(PRODUCTS, start=2):
        th.append([
            p,
            f"=SUMIFS(DonHang!$D$2:$D${last},DonHang!$C$2:$C${last},A{i})",
            f"=SUMIFS(DonHang!$F$2:$F${last},DonHang!$C$2:$C${last},A{i})",
            f'=IF(C{i}>=1000000,"Đạt","Chưa đạt")',
        ])
    th.append(["Tổng", f"=SUM(B2:B{len(PRODUCTS) + 1})", f"=SUM(C2:C{len(PRODUCTS) + 1})", ""])
    th.freeze_panes = "A2"
    for col in "CDEF":
        th.column_dimensions[col].width = 16

    chart = BarChart()
    chart.title = "Doanh thu theo sản phẩm"
    data = Reference(th, min_col=3, min_row=1, max_row=len(PRODUCTS) + 1)
    cats = Reference(th, min_col=1, min_row=2, max_row=len(PRODUCTS) + 1)
    chart.add_data(data, titles_from_data=True)
    chart.set_categories(cats)
    th.add_chart(chart, "F2")

    os.makedirs(dest_dir, exist_ok=True)
    path = os.path.join(dest_dir, "stress-5000-dong.xlsx")
    wb.save(path)
    return path


if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "fixtures"
    print("Đã tạo:", build(target))
