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

from openpyxl import load_workbook


def norm_formula(v):
    if isinstance(v, str) and v.startswith("="):
        return re.sub(r"\s+", "", v).upper()
    return v


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
    problems, checked = [], 0

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
        for col, dim in sa.column_dimensions.items():
            if dim.width and abs((sb.column_dimensions[col].width or 0) - dim.width) > 0.6:
                problems.append(f"{name}: độ rộng cột {col} {dim.width} -> {sb.column_dimensions[col].width}")
        ma = sorted(str(r) for r in sa.merged_cells.ranges)
        mb = sorted(str(r) for r in sb.merged_cells.ranges)
        if ma != mb:
            problems.append(f"{name}: ô gộp {ma} -> {mb}")

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
    if known_hits:
        print("KHÁC BIỆT ĐÃ BIẾT (xem docs/DATA_SAFETY.md):")
        for p in known_hits:
            print("  ~", p)
    if problems:
        print("KHÁC BIỆT:")
        for p in problems:
            print("  -", p)
        return 1
    print("OK: không phát hiện mất dữ liệu.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
