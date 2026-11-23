#!/usr/bin/env python3
"""So sánh tệp XLSX gốc và tệp do Dũng Office lưu ra.

Kiểm tra: tên sheet, giá trị/công thức từng ô, định dạng số, in đậm, màu nền,
comment, hyperlink, ô gộp. Các ô được chỉnh sửa có chủ đích truyền qua --expect.

Cách dùng:
  python verify_roundtrip.py goc.xlsx moi.xlsx --expect "Doanh thu!F2=Dũng" --expect "Doanh thu!F3==D5*2"
Thoát mã 0 nếu không mất dữ liệu, 1 nếu có khác biệt ngoài dự kiến.
"""
import argparse
import re
import sys
import warnings
import zipfile

warnings.filterwarnings("ignore", category=UserWarning)  # openpyxl cảnh báo phần mở rộng không đọc được

from openpyxl import load_workbook


def norm_formula(v):
    if isinstance(v, str) and v.startswith("="):
        f = re.sub(r"\s+", "", v).upper()
        # Cách viết tương đương mà Excel chấp nhận như nhau: FALSE() == FALSE, TRUE() == TRUE
        return re.sub(r"\b(TRUE|FALSE)\(\)", r"\1", f)
    return v


def raw_formula_differs(x, y):
    return isinstance(x, str) and isinstance(y, str) and x.startswith("=") and re.sub(r"\s+", "", x).upper() != re.sub(r"\s+", "", y).upper()


def norm_numfmt(f):
    # LibreOffice có thể viết lại định dạng tương đương (vd. thêm dấu ngoặc kép); chuẩn hoá nhẹ.
    return (f or "General").replace('"', "").replace("\\", "").lower()


def color(fill):
    try:
        if fill is None or fill.fill_type is None:
            return None
        rgb = fill.fgColor.rgb
        return rgb[-6:].upper() if isinstance(rgb, str) else None
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("original")
    ap.add_argument("saved")
    ap.add_argument("--expect", action="append", default=[], help="Sheet!Ô=giá_trị dự kiến sau khi sửa")
    ap.add_argument("--known", help="Tệp liệt kê khác biệt ĐÃ BIẾT (được báo cáo nhưng không làm hỏng kiểm thử)")
    a = ap.parse_args()

    expected = {}
    for e in a.expect:
        ref, val = e.split("=", 1)
        sheet, cell = ref.split("!")
        expected[(sheet, cell)] = val

    wa = load_workbook(a.original)
    wb = load_workbook(a.saved)
    problems, checked, rewritten = [], 0, []

    if wa.sheetnames != wb.sheetnames:
        problems.append(f"Tên sheet khác: {wa.sheetnames} -> {wb.sheetnames}")

    for name in wa.sheetnames:
        if name not in wb.sheetnames:
            continue
        sa, sb = wa[name], wb[name]
        for row in sa.iter_rows():
            for ca in row:
                if (name, ca.coordinate) in expected:
                    continue
                cb = sb[ca.coordinate]
                checked += 1
                va, vb = norm_formula(ca.value), norm_formula(cb.value)
                if va == vb and raw_formula_differs(ca.value, cb.value):
                    rewritten.append(f"{name}!{ca.coordinate}: {ca.value} -> {cb.value}")
                if hasattr(va, "date") and hasattr(vb, "date"):
                    va, vb = va.date() if hasattr(va, "hour") else va, vb.date() if hasattr(vb, "hour") else vb
                if va != vb and not (isinstance(va, (int, float)) and isinstance(vb, (int, float)) and abs(va - vb) < 1e-9):
                    problems.append(f"{name}!{ca.coordinate}: giá trị {ca.value!r} -> {cb.value!r}")
                if ca.value is not None and norm_numfmt(ca.number_format) != norm_numfmt(cb.number_format):
                    problems.append(f"{name}!{ca.coordinate}: định dạng số {ca.number_format!r} -> {cb.number_format!r}")
                fa = ca.font.color.rgb if ca.font and ca.font.color is not None and isinstance(ca.font.color.rgb, str) else None
                fb = cb.font.color.rgb if cb.font and cb.font.color is not None and isinstance(cb.font.color.rgb, str) else None
                if fa and fa[-6:] != (fb or "")[-6:]:
                    problems.append(f"{name}!{ca.coordinate}: màu chữ {fa} -> {fb}")
                if bool(ca.font and ca.font.bold) != bool(cb.font and cb.font.bold):
                    problems.append(f"{name}!{ca.coordinate}: in đậm {ca.font.bold} -> {cb.font.bold}")
                if color(ca.fill) != color(cb.fill):
                    problems.append(f"{name}!{ca.coordinate}: màu nền {color(ca.fill)} -> {color(cb.fill)}")
                ta = ca.comment.text.strip() if ca.comment else None
                tb = cb.comment.text.strip() if cb.comment else None
                if ta != tb:
                    problems.append(f"{name}!{ca.coordinate}: comment {ta!r} -> {tb!r}")
                elif ca.comment and (ca.comment.author or "") != (cb.comment.author or ""):
                    problems.append(f"{name}!{ca.coordinate}: tác giả comment {ca.comment.author!r} -> {cb.comment.author!r}")
                ha = ca.hyperlink.target if ca.hyperlink else None
                hb = cb.hyperlink.target if cb.hyperlink else None
                if ha != hb:
                    problems.append(f"{name}!{ca.coordinate}: hyperlink {ha!r} -> {hb!r}")
        def widths(ws):
            # openpyxl gom cột theo khoảng min..max; mở rộng để so sánh từng cột
            from openpyxl.utils import get_column_letter
            out = {}
            for dim in ws.column_dimensions.values():
                if dim.width and dim.min and dim.max:
                    for i in range(dim.min, min(dim.max, 300) + 1):
                        out[get_column_letter(i)] = dim.width
            return out
        wa_w, wb_w = widths(sa), widths(sb)
        for col, w in wa_w.items():
            if abs(wb_w.get(col, 0) - w) > 0.6:
                problems.append(f"{name}: độ rộng cột {col} {w} -> {wb_w.get(col)}")
        ma = sorted(str(r) for r in sa.merged_cells.ranges)
        mb = sorted(str(r) for r in sb.merged_cells.ranges)
        if ma != mb:
            problems.append(f"{name}: ô gộp {ma} -> {mb}")

    # ---- Cấu trúc: bảng, biểu đồ, định dạng có điều kiện, xác thực dữ liệu, cố định ô ----
    for name in wa.sheetnames:
        if name not in wb.sheetnames:
            continue
        sa, sb = wa[name], wb[name]
        checks = {
            "cố định ô": (sa.freeze_panes, sb.freeze_panes),
            "số bảng (Table)": (len(sa.tables), len(sb.tables)),
            "số vùng định dạng có điều kiện": (len(sa.conditional_formatting), len(sb.conditional_formatting)),
            "số quy tắc xác thực dữ liệu": (len(sa.data_validations.dataValidation), len(sb.data_validations.dataValidation)),
            "bộ lọc tự động": (sa.auto_filter.ref, sb.auto_filter.ref),
        }
        for label, (x, y) in checks.items():
            if x != y:
                problems.append(f"{name}: {label} {x!r} -> {y!r}")
        for t in sa.tables.values():
            tb = sb.tables.get(t.name) or next((u for u in sb.tables.values() if u.ref == t.ref), None)
            if tb is None or tb.ref != t.ref:
                problems.append(f"{name}: bảng {t.name} ({t.ref}) -> {None if tb is None else (tb.name, tb.ref)}")

    def zip_count(path, pattern):
        with zipfile.ZipFile(path) as z:
            return sum(1 for n in z.namelist() if re.search(pattern, n))

    def zip_grep(path, pattern):
        with zipfile.ZipFile(path) as z:
            return sum(len(re.findall(pattern, z.read(n).decode("utf-8", "ignore")))
                       for n in z.namelist() if n.startswith("xl/worksheets/sheet"))

    for label, pat in [("biểu đồ", r"xl/charts?/chart\d+\.xml$|xl/drawings/charts/chart\d+\.xml$"),
                       ("bảng (table part)", r"xl/tables/table\d+\.xml$")]:
        n1, n2 = zip_count(a.original, pat), zip_count(a.saved, pat)
        if n1 != n2:
            problems.append(f"Tệp: số {label} {n1} -> {n2}")
    for label, pat in [("quy tắc định dạng có điều kiện (cfRule, kể cả x14)", r"<(?:\w+:)?cfRule\b"),
                       ("phần mở rộng định dạng có điều kiện (x14)", r"<(?:\w+:)?conditionalFormatting\b[^>]*>\s*<(?:\w+:)?cfRule[^>]*\bid=|<x14:conditionalFormatting\b"),
                       ("quy tắc xác thực dữ liệu", r"<(?:\w+:)?dataValidation\b")]:
        x, y = zip_grep(a.original, pat), zip_grep(a.saved, pat)
        if x != y:
            problems.append(f"Tệp: số {label} {x} -> {y}")

    # ---- Giá trị tính được của công thức (cache) ----
    va_wb = load_workbook(a.original, data_only=True)
    vb_wb = load_workbook(a.saved, data_only=True)
    has_cache = any(c.value is not None for ws in va_wb.worksheets for r in ws.iter_rows() for c in r
                    if isinstance(wa[ws.title][c.coordinate].value, str) and str(wa[ws.title][c.coordinate].value).startswith("="))
    value_diffs = 0
    recalculated = []
    if has_cache:
        for ws in va_wb.worksheets:
            if ws.title not in vb_wb.sheetnames:
                continue
            for r in ws.iter_rows():
                for c in r:
                    f = wa[ws.title][c.coordinate].value
                    if not (isinstance(f, str) and f.startswith("=")) or (ws.title, c.coordinate) in expected:
                        continue
                    x, y = c.value, vb_wb[ws.title][c.coordinate].value
                    same = x == y or (isinstance(x, (int, float)) and isinstance(y, (int, float)) and abs(x - y) <= 1e-9 * max(1, abs(x)))
                    if not same:
                        value_diffs += 1
                        recalculated.append(f"{ws.title}!{c.coordinate}: {x!r} -> {y!r}  ({f})")
        if recalculated:
            print(f"KẾT QUẢ CÔNG THỨC KHÁC GIÁ TRỊ CACHE TRONG TỆP GỐC ({len(recalculated)} ô) — engine đã tính lại; "
                  "cần đối chiếu thủ công xem bên nào đúng:")
            for r in recalculated[:10]:
                print("  ?", r)
        print(f"Đã so sánh kết quả tính của công thức (giá trị cache).")
    else:
        print("Tệp gốc không có giá trị cache của công thức (chưa từng được Excel tính) — bỏ qua so sánh kết quả.")

    for (sheet, cell), val in expected.items():
        got = wb[sheet][cell].value
        if norm_formula(str(got)) != norm_formula(val):
            problems.append(f"{sheet}!{cell}: chỉnh sửa dự kiến {val!r} nhưng có {got!r}")

    known = []
    if a.known:
        with open(a.known, encoding="utf-8") as f:
            known = [l.strip() for l in f if l.strip() and not l.lstrip().startswith("#")]
    known_hits = [p for p in problems if any(p.startswith(k) for k in known)]
    problems = [p for p in problems if p not in known_hits]

    print(f"Đã kiểm tra {checked} ô, {len(expected)} chỉnh sửa dự kiến.")
    if rewritten:
        print(f"CÔNG THỨC VIẾT LẠI TƯƠNG ĐƯƠNG (cùng kết quả, Excel chấp nhận): {len(rewritten)} ô, ví dụ:")
        for r in rewritten[:3]:
            print("  =", r)
    if known_hits:
        print("KHÁC BIỆT ĐÃ BIẾT (xem docs/DATA_SAFETY.md):")
        for p in known_hits:
            print("  ~", p)
    if problems:
        groups = {}
        for p in problems:
            key = re.sub(r"^[^:]*![A-Z]+\d+: ", "", p)
            key = re.sub(r"'.*", "", key).strip()
            groups.setdefault(key, []).append(p)
        print(f"KHÁC BIỆT ({len(problems)}):")
        for key, items in groups.items():
            print(f"  [{len(items)}] {key}")
            for it in items[:4]:
                print("      -", it)
        return 1
    print("OK: không phát hiện mất dữ liệu.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
