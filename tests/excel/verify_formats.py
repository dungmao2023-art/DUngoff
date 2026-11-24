# -*- coding: utf-8 -*-
"""Đối chiếu kết quả kiểm kê định dạng Excel. Dùng: python3 -I verify_formats.py <thư_mục_kết_quả> <nhật_ký_smoketest>
Mỗi dòng: [ĐẠT] / [LỆCH] <đuôi> <kiểm tra>. [LỆCH] là phát hiện cần phân loại (A vá ở app / B sửa engine / C chấp nhận + cảnh báo)."""
import datetime as dt, os, re, sys, zipfile
import openpyxl

out, logp = sys.argv[1], sys.argv[2]
log = open(logp, encoding="utf-8", errors="replace").read()
inv = {}
for m in re.finditer(r"^INV (\S+) ([^=\s]+)=(.*)$", log, re.M):
    inv.setdefault(m.group(1), {}).setdefault(m.group(2), []).append(m.group(3).strip())
res = []
def rep(ok, ext, what, note=""):
    res.append(ok)
    print(("[ĐẠT]  " if ok else "[LỆCH] ") + f"{ext:5s} {what}" + (f"  ({note})" if note else ""))
def g(ext, k, default=""):
    v = inv.get(ext, {}).get(k)
    return v[0] if v else default

print("Engine:", g("*", "engine", "?"))
for ext in ("xlsx", "xls", "xlsm", "xlsb", "csv"):
    if g(ext, "mở") != "ok":
        rep(False, ext, "mở được trên engine", g(ext, "mở", "không có dòng INV (engine treo hoặc tệp thiếu)"))
        continue
    rep(True, ext, "mở được trên engine", f"{g(ext,'số_sheet')} sheet, loại {g(ext,'loại')}")
    rep(int(g(ext, "điểm_ảnh_tối", "0") or 0) > 200, ext, "vẽ ra nội dung (không trang trắng)", "điểm tối=" + g(ext, "điểm_ảnh_tối"))
    ro = g(ext, "chỉ_đọc") == "có"
    if ext == "xlsx":
        rep(not ro, ext, "mở ở chế độ sửa được"); rep(g(ext, "đã_sửa") == "có", ext, "sửa 1 ô được")
        rep(g(ext, "lưu_cùng_đuôi").startswith("ok"), ext, "lưu bản sao cùng đuôi", g(ext, "lưu_cùng_đuôi"))
    else:
        rep(ro, ext, "mở ở chế độ chỉ đọc (chính sách bảo vệ dữ liệu)")
        rep(g(ext, "đã_sửa") == "không", ext, "chặn được việc sửa ô", "đã_sửa=" + g(ext, "đã_sửa"))
        rep(g(ext, "lưu_cùng_đuôi").startswith("bị_chặn"), ext, "chặn ghi cùng đuôi (không ghi thiếu dữ liệu)", g(ext, "lưu_cùng_đuôi")[:100])
        rep(g(ext, "xlsx_không_xác_nhận").startswith("bị_chặn"), ext, "Lưu thành .xlsx đòi xác nhận mất mát", g(ext, "xlsx_không_xác_nhận")[:100])
        rep(g(ext, "xlsx_có_xác_nhận").startswith("ok"), ext, "Lưu thành .xlsx sau xác nhận thành công", g(ext, "xlsx_có_xác_nhận")[:100])
        rep(bool(inv.get(ext, {}).get("cảnh_báo")), ext, "có cảnh báo tiếng Việt khi mở", (g(ext, "cảnh_báo") or "KHÔNG CÓ")[:100])

def check_xlsx(path, ext, expect_formula=True, full=True):
    if not os.path.exists(path):
        rep(False, ext, "có tệp .xlsx kết quả", os.path.basename(path)); return
    wb = openpyxl.load_workbook(path)
    ws = wb.worksheets[0]
    wbv = openpyxl.load_workbook(path, data_only=True).worksheets[0]
    a2 = ws["A2"].value
    rep(isinstance(a2, str) and a2 == "00123", ext, "số 0 đầu của mã giữ nguyên (A2='00123' dạng chuỗi)", repr(a2))
    a6 = ws["A6"].value if full else None
    if full:
        rep(isinstance(a6, str) and a6 == "12345678901234567890", ext, "số dài 20 chữ số giữ nguyên dạng chuỗi (A6)", repr(a6))
    rep(ws["B2"].value in ("Bút bi",) and ws["B3"].value is not None, ext, "chữ tiếng Việt có dấu giữ nguyên", repr(ws["B2"].value))
    c2 = ws["C2"].value
    rep(isinstance(c2, (int, float)) and abs(c2 - 1500.5) < 1e-9, ext, "số 1500.5 giữ giá trị (C2)", repr(c2))
    if full:
        d2 = ws["D2"].value
        rep(isinstance(d2, dt.datetime) and d2.date() == dt.date(2026, 3, 1), ext, "ngày 01/03/2026 giữ đúng ngày (D2)", repr(d2))
        rep("d" in ws["D2"].number_format.lower() and "m" in ws["D2"].number_format.lower(), ext, "định dạng ngày giữ (D2)", ws["D2"].number_format)
        rep("#,##0.00" in ws["C2"].number_format or ws["C2"].number_format.count("0") >= 3, ext, "định dạng số #,##0.00 giữ (C2)", ws["C2"].number_format)
    if expect_formula:
        e2, f2 = ws["E2"].value, ws["F2"].value
        rep(isinstance(e2, str) and e2.replace(" ", "").upper() == "=C2*2", ext, "công thức =C2*2 giữ (không thành giá trị)", repr(e2))
        rep(isinstance(f2, str) and "VLOOKUP" in f2.upper(), ext, "công thức VLOOKUP giữ", repr(f2))
        rep(wbv["E2"].value in (3001, 3001.0), ext, "giá trị tính ra của công thức đúng (3001)", repr(wbv["E2"].value))
    if full:
        rep(bool(ws["A1"].font and ws["A1"].font.b), ext, "chữ đậm giữ (A1)")
        fg = ws["A1"].fill.fgColor.rgb if ws["A1"].fill and ws["A1"].fill.fill_type else None
        rep(fg is not None and str(fg).upper().endswith("FFFF00"), ext, "màu nền vàng giữ (A1)", str(fg))
        rep(ws["B2"].comment is not None, ext, "ghi chú ô giữ (B2)")
        rep(ws["B3"].hyperlink is not None, ext, "liên kết giữ (B3)")
        rep("E6:F6" in [str(r) for r in ws.merged_cells.ranges], ext, "ô gộp E6:F6 giữ")
        w = ws.column_dimensions["B"].width
        rep(w is not None and w >= 15, ext, "độ rộng cột B giữ", str(w))

for ext in ("xls", "xlsm"):
    p = os.path.join(out, f"sang-xlsx-{ext}.xlsx")
    if g(ext, "xlsx_có_xác_nhận").startswith("ok"): check_xlsx(p, ext)
p = os.path.join(out, "sang-xlsx-xlsb.xlsx")
if g("xlsb", "xlsx_có_xác_nhận").startswith("ok"):
    # tệp .xlsb tổng hợp tối thiểu chỉ có 3 cột/3 dòng, không công thức/định dạng
    if os.path.exists(p):
        ws = openpyxl.load_workbook(p).worksheets[0]
        rep(ws["A2"].value == "00123", "xlsb", "số 0 đầu giữ (A2)", repr(ws["A2"].value))
        rep(ws["C2"].value == 1500.5, "xlsb", "số giữ (C2)", repr(ws["C2"].value))
p = os.path.join(out, "sang-xlsx-csv.xlsx")
if g("csv", "xlsx_có_xác_nhận").startswith("ok") and os.path.exists(p):
    ws = openpyxl.load_workbook(p).worksheets[0]
    rep(ws["A2"].value == "00123", "csv", "số 0 đầu giữ khi mở CSV (A2='00123')", repr(ws["A2"].value))
    rep(str(ws["A4"].value) == "12345678901234567890", "csv", "số dài 20 chữ số giữ khi mở CSV (A4)", repr(ws["A4"].value))
    rep(isinstance(ws["D2"].value, dt.datetime) and ws["D2"].value.date() == dt.date(2026, 3, 1), "csv", "ngày dd/mm/yyyy hiểu đúng (01/03/2026 là 1 tháng 3)", repr(ws["D2"].value))
    rep(ws["B3"].value == "Vở, kẻ ngang", "csv", "dấu phẩy trong ngoặc kép giữ", repr(ws["B3"].value))
    rep(ws["B2"].value == "Bút bi", "csv", "UTF-8 tiếng Việt đọc đúng", repr(ws["B2"].value))
    c2 = ws["C2"].value
    rep(c2 == "1.500,5" or (isinstance(c2, (int, float)) and abs(c2 - 1500.5) < 1e-9), "csv", "số kiểu VN '1.500,5' không bị hiểu sai", repr(c2))
# xlsx gốc: sửa ô rồi lưu
p = os.path.join(out, "cung-duoi.xlsx")
if os.path.exists(p):
    ws = openpyxl.load_workbook(p).worksheets[0]
    rep(ws["H2"].value == "SUA", "xlsx", "ô đã sửa (H2) có trong bản lưu", repr(ws["H2"].value))
    check_xlsx(p, "xlsx")
# macro: bản .xlsx sinh ra từ .xlsm không còn macro (đúng kỳ vọng, phải được cảnh báo ở app)
p = os.path.join(out, "sang-xlsx-xlsm.xlsx")
if os.path.exists(p):
    has = any("vbaProject" in n for n in zipfile.ZipFile(p).namelist())
    print(f"[TIN]  xlsm  bản .xlsx sinh ra {'CÒN' if has else 'KHÔNG còn'} vbaProject.bin (vbaProject.bin trong tệp mẫu là GIẢ; chưa thử macro VBA thật)")
bad = res.count(False)
print(f"\nTỔNG: {len(res) - bad} đạt / {bad} lệch (cần phân loại)")
print("KẾT QUẢ ĐỐI CHIẾU: " + ("CÓ LỆCH" if bad else "KHÔNG LỆCH"))
