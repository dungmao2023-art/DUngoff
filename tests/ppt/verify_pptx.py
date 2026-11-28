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
# Verify six native alignments and exact undo against independent source.
align_order, original = layer_shapes("ppt-align-baseline.pptx")
check(align_order==["LAYER_A","LAYER_B"],"alignment source order")
for mode, axis, factor in (
    ("left",0,0),("center",0,0.5),("right",0,1),
    ("top",1,0),("middle",1,0.5),("bottom",1,1),
):
    filename="ppt-align-"+mode+".pptx"
    order, actual=layer_shapes(filename)
    check(order==align_order and actual.keys()==original.keys(),filename+": object identity/order")
    if actual.keys()==original.keys() and len(actual)==2:
        edges=[v[axis]+factor*v[axis+2] for v in actual.values()]
        check(abs(edges[0]-edges[1])<=720,filename+": requested edges aligned")
        check(any(abs(actual[k][axis]-original[k][axis])>720 for k in actual),filename+": actual position changed")
        check(all(abs(actual[k][j]-original[k][j])<=360 for k in actual
                  for j in (1-axis,2,3)),filename+": other axis and dimensions preserved")
    undo_order, undo=layer_shapes("ppt-align-"+mode+"-undone.pptx")
    check(undo_order==align_order and undo==original,filename+": Undo restores original geometry")
# Real native resize: dimensions in PPTX EMU, independent baseline and Undo.
resize_order, resize_base=layer_shapes("ppt-resize-baseline.pptx")
order, resized=layer_shapes("ppt-resize-edited.pptx")
check(order==resize_order and resized.keys()==resize_base.keys(), "resize preserves text and object order")
if resized.keys()==resize_base.keys() and "LAYER_A" in resized and "LAYER_B" in resized:
    check(abs(resized["LAYER_A"][2]-2743200)<=720 and abs(resized["LAYER_A"][3]-1371600)<=720,
          "resize applies requested 3 x 1.5 inch dimensions")
    check(resized["LAYER_B"]==resize_base["LAYER_B"], "resize preserves neighbouring object")
check(layer_shapes("ppt-resize-undone.pptx")==(resize_order,resize_base), "resize Undo restores exact geometry")
def rotation_snapshot(filename):
    order, geom=layer_shapes(filename)
    slide=Presentation(layer_dir/filename).slides[0]
    return order,geom,{s.text:s.rotation for s in slide.shapes if s.has_text_frame}
rotbase,rotgeom,angles=rotation_snapshot("ppt-rotate-baseline.pptx")
for mode,expected in (("positive",270),("negative",45)):
    filename="ppt-rotate-"+mode+".pptx"
    order,geom,angle=rotation_snapshot(filename)
    check(order==rotbase and geom.keys()==rotgeom.keys(),filename+": original objects and order")
    if geom.keys()==rotgeom.keys() and "LAYER_A" in angle and "LAYER_B" in angle:
        actual=(angle["LAYER_A"]-angles["LAYER_A"])%360
        print("Actual rotation",mode,actual,"expected",expected,"geometry",geom["LAYER_A"])
        check(abs(actual-expected)<0.1,filename+": native rotation angle")
        check(angle["LAYER_B"]==angles["LAYER_B"] and geom["LAYER_B"]==rotgeom["LAYER_B"],filename+": neighbour untouched")
        check(all(abs(a-b)<=720 for a,b in zip(geom["LAYER_A"][2:],rotgeom["LAYER_A"][2:])),filename+": dimensions preserved")
        before=rotgeom["LAYER_A"]; after=geom["LAYER_A"]
        check(all(abs((before[i]+before[i+2]/2)-(after[i]+after[i+2]/2))<=1440 for i in (0,1)),filename+": rotation around original center")
    else:
        check(False,filename+": both objects must exist")
    check(rotation_snapshot("ppt-rotate-"+mode+"-undone.pptx")== (rotbase,rotgeom,angles),filename+": exact Undo")
import zipfile
import posixpath
import xml.etree.ElementTree as ET
master_file=layer_dir/"ppt-master-edited.pptx"
if master_file.exists():
    with zipfile.ZipFile(master_file) as z:
        names=set(z.namelist())
        masters=[n for n in names if n.startswith("ppt/slideMasters/slideMaster") and n.endswith(".xml")]
        layouts=[n for n in names if n.startswith("ppt/slideLayouts/slideLayout") and n.endswith(".xml")]
        slides_xml=sorted(n for n in names if n.startswith("ppt/slides/slide") and n.endswith(".xml"))
        marker=b"DUNG_MASTER_MARKER"
        marked={n for n in masters+layouts if marker in z.read(n)}
        print("Native master/layout XML containing marker:", sorted(marked))
        def related(part, kind):
            relfile=posixpath.join(posixpath.dirname(part),"_rels",posixpath.basename(part)+".rels")
            if relfile not in names:
                return set()
            root=ET.fromstring(z.read(relfile))
            targets=set()
            for rel in root:
                if rel.get("Type","").endswith("/"+kind) and rel.get("TargetMode")!="External":
                    target=rel.get("Target","")
                    resolved=posixpath.normpath(target.lstrip("/") if target.startswith("/") else posixpath.join(posixpath.dirname(part),target))
                    if resolved in names:
                        targets.add(resolved)
            return targets
        chains=[]
        for slide in slides_xml:
            layout_parts=related(slide,"slideLayout")
            master_parts=set().union(*(related(layout,"slideMaster") for layout in layout_parts))
            chains.append(layout_parts|master_parts)
            print("Native slide inheritance:",slide,sorted(layout_parts),sorted(master_parts))
        shared=set.intersection(*chains) if len(chains)==2 else set()
        check(bool(marked & shared),"Native edit saved in master or layout inherited by both original slides")
        check(any(b"MASTER_EDIT_ME" in z.read(n) for n in marked & shared),"Original shared master text preserved")
        check(len(slides_xml)==2 and all(marker not in z.read(n) for n in slides_xml),"No master text copied into ordinary slide XML")
        check(any(b"MASTER_SLIDE_A" in z.read(n) for n in slides_xml) and any(b"MASTER_SLIDE_B" in z.read(n) for n in slides_xml),"Both original slides preserved")
else:
    check(False,"Native master edited PPTX missing")
print("\nKẾT QUẢ XÁC MINH PPTX:", "CÓ LỖI" if fail else "TẤT CẢ ĐẠT")
sys.exit(1 if fail else 0)
