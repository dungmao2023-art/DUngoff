# -*- coding: utf-8 -*-
"""Tệp tổng hợp (KHÔNG có dữ liệu thật) cho kiểm kê định dạng Excel: base.xlsx/.xls/.xlsm/.xlsb/.csv.
Dùng: python3 make_format_fixtures.py <thư_mục>   (cần openpyxl và soffice cho .xls).
Lưu ý trung thực:
- .xlsm: gói OOXML có kiểu nội dung macroEnabled + vbaProject.bin GIẢ (không phải VBA thật; không tạo được VBA thật ở môi trường này).
- .xlsb: tệp BIFF12 tối thiểu viết tay (chỉ số thực + chuỗi); KHÔNG phải tệp do Excel ghi. Kết quả mở nó chỉ là tham khảo."""
import datetime as dt, os, shutil, struct, subprocess, sys, zipfile
import openpyxl
from openpyxl.comments import Comment
from openpyxl.styles import Font, PatternFill


def base(path):
    wb = openpyxl.Workbook(); ws = wb.active; ws.title = "Dữ liệu"
    ws.append(["Mã", "Tên", "Số", "Ngày", "Gấp đôi", "Tra cứu"])
    data = [("00123", "Bút bi", 1500.5, dt.datetime(2026, 3, 1)), ("00456", "Vở kẻ ngang", 20, dt.datetime(2026, 1, 15)),
            ("78901", "Thước", 300, dt.datetime(2026, 2, 10))]
    for i, (m, n, v, d) in enumerate(data, start=2):
        ws.append([m, n, v, d, f"=C{i}*2", f"=VLOOKUP(A{i},$A$2:$B$4,2,FALSE)"])
        ws.cell(i, 1).number_format = "@"; ws.cell(i, 4).number_format = "dd/mm/yyyy"; ws.cell(i, 3).number_format = "#,##0.00"
    ws["A6"] = "12345678901234567890"; ws["A6"].number_format = "@"
    ws["A1"].font = Font(bold=True); ws["A1"].fill = PatternFill("solid", fgColor="FFFF00")
    ws["B2"].comment = Comment("ghi chú", "t"); ws["B3"].hyperlink = "https://example.com/"
    ws.merge_cells("E6:F6"); ws.column_dimensions["B"].width = 30
    wb.save(path)


def xlsm(src, dst):
    zin = zipfile.ZipFile(src)
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
        for it in zin.infolist():
            data = zin.read(it.filename)
            if it.filename == "[Content_Types].xml":
                s = data.decode("utf-8")
                s = s.replace("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml", "application/vnd.ms-excel.sheet.macroEnabled.main+xml")
                s = s.replace("</Types>", '<Default Extension="bin" ContentType="application/vnd.ms-office.vbaProject"/></Types>') if 'Extension="bin"' not in s else s
                data = s.encode("utf-8")
            if it.filename == "xl/_rels/workbook.xml.rels":
                s = data.decode("utf-8")
                s = s.replace("</Relationships>", '<Relationship Id="rIdVba" Type="http://schemas.microsoft.com/office/2006/relationships/vbaProject" Target="vbaProject.bin"/></Relationships>')
                data = s.encode("utf-8")
            z.writestr(it, data)
        z.writestr("xl/vbaProject.bin", b"\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1" + b"FAKE-VBA-FOR-TEST" * 40)


def rec(rid, payload=b""):
    h = bytes([rid]) if rid < 0x80 else bytes([(rid & 0x7F) | 0x80, rid >> 7])
    n = len(payload); sz = b""
    while True:
        b = n & 0x7F; n >>= 7
        sz += bytes([b | (0x80 if n else 0)])
        if not n: break
    return h + sz + payload


def wstr(s):
    return struct.pack("<I", len(s)) + s.encode("utf-16-le")


def xlsb(dst):
    wbk = rec(131) + rec(143) + rec(156, struct.pack("<II", 0, 1) + wstr("rId1") + wstr("Dữ liệu")) + rec(144) + rec(132)
    rows = [["Mã", "Tên", "Số"], ["00123", "Bút bi", 1500.5], ["00456", "Vở", 20.0]]
    sh = rec(129) + rec(147, b"\x00" * 0 + struct.pack("<II", 0, 0) + b"\x00" * 0) if False else rec(129)
    sh += rec(145)
    for r, row in enumerate(rows):
        sh += rec(0, struct.pack("<IIHHB", r, 0, 0, 0, 0) + struct.pack("<I", 1) + struct.pack("<II", 0, len(row) - 1))
        for c, v in enumerate(row):
            if isinstance(v, str):
                sh += rec(6, struct.pack("<II", c, 0) + wstr(v))
            else:
                sh += rec(5, struct.pack("<II", c, 0) + struct.pack("<d", v))
    sh += rec(146) + rec(130)
    ct = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="xml" ContentType="application/xml"/>'
          '<Override PartName="/xl/workbook.bin" ContentType="application/vnd.ms-excel.sheet.binary.macroEnabled.main"/>'
          '<Override PartName="/xl/worksheets/sheet1.bin" ContentType="application/vnd.ms-excel.worksheet"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.bin"/></Relationships>')
    wrels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
             '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.bin"/></Relationships>')
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ct); z.writestr("_rels/.rels", rels)
        z.writestr("xl/workbook.bin", wbk); z.writestr("xl/_rels/workbook.bin.rels", wrels); z.writestr("xl/worksheets/sheet1.bin", sh)


def main(d):
    os.makedirs(d, exist_ok=True)
    b = os.path.join(d, "base.xlsx"); base(b)
    subprocess.run(["soffice", "--headless", "--convert-to", "xls", "--outdir", d, b], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    xlsm(b, os.path.join(d, "base.xlsm")); xlsb(os.path.join(d, "base.xlsb"))
    with open(os.path.join(d, "base.csv"), "w", encoding="utf-8", newline="") as f:
        f.write('Mã,Tên,Số,Ngày\n00123,Bút bi,"1.500,5",01/03/2026\n00456,"Vở, kẻ ngang",20,15/01/2026\n12345678901234567890,Thước,300,10/02/2026\n')
    print(sorted(os.listdir(d)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "formats")
