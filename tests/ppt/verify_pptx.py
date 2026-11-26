# -*- coding: utf-8 -*-
"""Kiểm tra NỘI DUNG tệp PPTX do ppt_tests.cpp lưu ra (không tin vào trạng thái engine báo).
Chạy: python3 -I tests/ppt/verify_pptx.py <tệp đã lưu>
Mọi mục "BẮT BUỘC" sai thì thoát mã 1.
"""
import sys
from pptx import Presentation
from pptx.util import Pt

path = sys.argv[1]
prs = Presentation(path)
fail = 0

def check(ok, what):
    global fail
    print(("[ĐẠT]  " if ok else "[LỖI]  ") + what)
    if not ok:
        fail += 1

slides = list(prs.slides)
check(len(slides) == 3, f"Số trang chiếu = 3 sau (thêm, nhân bản, xoá) (thực tế {len(slides)})")

# Trang 1: khung đầu tiên đã sửa chữ + định dạng
def runs(slide):
    for sh in slide.shapes:
        if sh.has_text_frame:
            for p in sh.text_frame.paragraphs:
                for r in p.runs:
                    yield sh, p, r

first = [(sh, p, r) for sh, p, r in runs(slides[0]) if "Dung Office" in r.text]
check(bool(first), "Trang 1 có chữ 'Dung Office' đã nhập")
if first:
    sh, p, r = first[0]
    check(bool(r.font.bold), "Chữ đã nhập: đậm")
    check(bool(r.font.italic), "Chữ đã nhập: nghiêng")
    check(bool(r.font.underline), "Chữ đã nhập: gạch chân")
    check(r.font.size is not None and abs(r.font.size.pt - 32) < 0.6, f"Chữ đã nhập: cỡ 32 (thực tế {r.font.size.pt if r.font.size else None})")
    check(r.font.name == "DejaVu Sans", f"Chữ đã nhập: phông DejaVu Sans (thực tế {r.font.name})")
    rgb = None
    try:
        rgb = str(r.font.color.rgb)
    except Exception:
        pass
    check(rgb == "FF0000", f"Chữ đã nhập: màu đỏ FF0000 (thực tế {rgb})")

# Trang 2: bảng, ảnh, danh sách
kinds = [sh.shape_type for sh in slides[1].shapes]
check(any(getattr(sh, "has_table", False) and sh.has_table for sh in slides[1].shapes), "Trang 2 có bảng")
check(any(sh.shape_type == 13 for sh in slides[1].shapes), "Trang 2 có ảnh (PICTURE)")

# Hộp văn bản và hình khối trên trang 1
texts = [sh.text_frame.text for sh in slides[0].shapes if sh.has_text_frame]
check(any("Hop van ban" in t for t in texts), f"Trang 1 có hộp văn bản 'Hop van ban' (thực tế: {texts!r})")
check(any(sh.shape_type == 1 and not sh.has_text_frame or sh.shape_type == 1 for sh in slides[0].shapes) or len(slides[0].shapes) >= 4,
      f"Trang 1 có thêm hình khối/hộp (số đối tượng {len(slides[0].shapes)})")

# Ghi chú người nói của trang 1
note = slides[0].notes_slide.notes_text_frame.text if slides[0].has_notes_slide else ""
check("Ghi chu nguoi noi" in note, f"Ghi chú người nói của trang 1 đã lưu (thực tế: {note!r})")

print("\nKẾT QUẢ XÁC MINH PPTX:", "CÓ LỖI" if fail else "TẤT CẢ ĐẠT")
sys.exit(1 if fail else 0)
