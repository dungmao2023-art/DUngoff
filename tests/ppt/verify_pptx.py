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

# Independent two-shape fixture: exported shape order, geometry and text.
from pathlib import Path
layer_dir = Path(path).parent
def layer_shapes(filename):
    deck = Presentation(layer_dir / filename)
    check(len(deck.slides) == 1, filename + ": slide identity")
    shapes = list(deck.slides[0].shapes)
    order = [s.text for s in shapes if s.has_text_frame]
    info = {s.text: (s.left, s.top, s.width, s.height) for s in shapes if s.has_text_frame}
    print(filename, order, info)
    check(len(shapes) == 2 and len(info) == 2, filename + ": both objects retained")
    return order, info
baseline, geometry = layer_shapes("ppt-layer-baseline.pptx")
check(baseline == ["LAYER_A", "LAYER_B"], "original object order")
for filename, expected in (
    ("ppt-layer-forward.pptx", ["LAYER_B", "LAYER_A"]),
    ("ppt-layer-forward-undone.pptx", baseline),
    ("ppt-layer-backward.pptx", ["LAYER_B", "LAYER_A"]),
    ("ppt-layer-backward-undone.pptx", baseline),
):
    order, info = layer_shapes(filename)
    check(order == expected, filename + ": native layer order")
    check(info == geometry, filename + ": object geometry and text preserved")
# Group geometry is in a local coordinate system; transform into slide
# coordinates before checking against the ungrouped baseline.
from pptx.enum.shapes import MSO_SHAPE_TYPE
from pptx.oxml.ns import qn
for filename, expected_group in (
    ("ppt-objects-grouped.pptx", True),
    ("ppt-objects-ungrouped.pptx", False),
    ("ppt-objects-ungroup-undone.pptx", True),
    ("ppt-objects-group-undone.pptx", False),
):
    deck=Presentation(layer_dir / filename)
    shapes=list(deck.slides[0].shapes)
    check(len(deck.slides)==1, filename+": single slide preserved")
    is_group=len(shapes)==1 and shapes[0].shape_type==MSO_SHAPE_TYPE.GROUP
    check(is_group==expected_group, filename+": group topology")
    if is_group:
        g=shapes[0]
        xf=g._element.find(qn("p:grpSpPr")).find(qn("a:xfrm"))
        off=xf.find(qn("a:off")); ext=xf.find(qn("a:ext"))
        co=xf.find(qn("a:chOff")); ce=xf.find(qn("a:chExt"))
        sx=int(ext.get("cx"))/int(ce.get("cx"))
        sy=int(ext.get("cy"))/int(ce.get("cy"))
        pos=lambda s:(round(int(off.get("x"))+(s.left-int(co.get("x")))*sx),
                      round(int(off.get("y"))+(s.top-int(co.get("y")))*sy),
                      round(s.width*sx),round(s.height*sy))
        children=list(g.shapes)
    else:
        children=shapes
        pos=lambda s:(s.left,s.top,s.width,s.height)
    order=[s.text for s in children if s.has_text_frame]
    positions={s.text:pos(s) for s in children if s.has_text_frame}
    print(filename, order, positions)
    check(len(children)==2 and order==baseline, filename+": original objects and order retained")
    check(positions.keys()==geometry.keys() and all(
        abs(a-b)<=360 for k in positions for a,b in zip(positions[k],geometry[k])),
        filename+": absolute geometry preserved within 0.01 mm")
print("\nKẾT QUẢ XÁC MINH PPTX:", "CÓ LỖI" if fail else "TẤT CẢ ĐẠT")
sys.exit(1 if fail else 0)
