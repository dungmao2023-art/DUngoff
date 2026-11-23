#!/usr/bin/env python3
"""Tạo tệp mẫu để kiểm thử round-trip của Dũng Office.

Chỉ dùng trong CI / máy phát triển (người dùng cuối không cần Python).
Cách dùng: python make_fixtures.py <thư_mục_đích>
"""
import csv
import datetime
import sys
from pathlib import Path

from openpyxl import Workbook
from openpyxl.comments import Comment
from openpyxl.styles import Alignment, Font, PatternFill


def make_xlsx(path: Path) -> None:
    wb = Workbook()
    ws = wb.active
    ws.title = "Doanh thu"
    ws.append(["Sản phẩm", "Số lượng", "Đơn giá", "Thành tiền", "Mã"])
    rows = [("Bút bi", 120, 5000, "00123"), ("Vở kẻ ngang", 80, 12000, "00456"), ("Thước kẻ", 45, 7000, "00789")]
    for i, (name, qty, price, code) in enumerate(rows, start=2):
        ws.cell(i, 1, name)
        ws.cell(i, 2, qty)
        ws.cell(i, 3, price).number_format = "#,##0"
        ws.cell(i, 4, f"=B{i}*C{i}").number_format = "#,##0"
        c = ws.cell(i, 5, code)          # chuỗi có số 0 ở đầu — KHÔNG được biến thành số
        c.number_format = "@"
    ws["A5"] = "Tổng cộng"
    ws["D5"] = "=SUM(D2:D4)"
    ws["D5"].number_format = "#,##0"
    ws["A6"] = "Tỉ lệ"
    ws["B6"] = 0.125
    ws["B6"].number_format = "0.00%"
    ws["A7"] = "Ngày lập"
    ws["B7"] = datetime.date(2026, 11, 23)
    ws["B7"].number_format = "dd/mm/yyyy"
    ws["A8"] = "Trang chủ"
    ws["A8"].hyperlink = "https://www.libreoffice.org/"
    ws["A1"].font = Font(bold=True, color="FFFFFF")
    ws["A1"].fill = PatternFill("solid", fgColor="1F5FBF")
    ws["B2"].comment = Comment("Số lượng đã kiểm kho", "Dũng")
    ws.merge_cells("A10:C10")
    ws["A10"] = "Ô gộp A10:C10"
    ws["A10"].alignment = Alignment(horizontal="center")
    ws.column_dimensions["A"].width = 18

    ws2 = wb.create_sheet("Ghi chú")
    ws2["A1"] = "Tổng từ sheet Doanh thu"
    ws2["B1"] = "='Doanh thu'!D5"
    ws2["A2"] = "Văn bản tiếng Việt: Ặ ẵ ữ ỹ đ Đ"
    wb.save(path)


def make_csv(path: Path) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Mã", "Tên"])
        w.writerow(["00123", "Bút"])


def make_docx(path: Path) -> bool:
    try:
        from docx import Document
    except ImportError:
        return False
    d = Document()
    d.add_heading("Báo cáo tháng 11", level=1)
    p = d.add_paragraph("Đoạn văn tiếng Việt có ")
    p.add_run("chữ đậm").bold = True
    p.add_run(" và ")
    p.add_run("chữ nghiêng").italic = True
    p.add_run(".")
    d.add_paragraph("Mục thứ nhất", style="List Bullet")
    d.add_paragraph("Mục thứ hai", style="List Bullet")
    t = d.add_table(rows=2, cols=3)
    for i, h in enumerate(["Sản phẩm", "Số lượng", "Ghi chú"]):
        t.cell(0, i).text = h
    for i, v in enumerate(["Bút bi", "120", "Đã kiểm kho"]):
        t.cell(1, i).text = v
    d.save(path)
    return True


def make_pptx(path: Path) -> bool:
    try:
        from pptx import Presentation
    except ImportError:
        return False
    prs = Presentation()
    s = prs.slides.add_slide(prs.slide_layouts[0])
    s.shapes.title.text = "Dũng Office"
    s.placeholders[1].text = "Bản trình chiếu mẫu"
    s2 = prs.slides.add_slide(prs.slide_layouts[1])
    s2.shapes.title.text = "Nội dung"
    s2.placeholders[1].text = "Ý thứ nhất\nÝ thứ hai"
    prs.save(path)
    return True


if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    out.mkdir(parents=True, exist_ok=True)
    make_xlsx(out / "mau-doanh-thu.xlsx")
    make_csv(out / "mau.csv")
    made = [out / "mau-doanh-thu.xlsx", out / "mau.csv"]
    if make_docx(out / "mau-van-ban.docx"):
        made.append(out / "mau-van-ban.docx")
    if make_pptx(out / "mau-trinh-chieu.pptx"):
        made.append(out / "mau-trinh-chieu.pptx")
    print("Đã tạo:", *made)
