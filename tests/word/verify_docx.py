# -*- coding: utf-8 -*-
"""Kiểm tra tệp DOCX do `dung-smoketest --word 1` lưu: định dạng phải còn nguyên sau khi lưu.
Chạy: python3 tests/word/verify_docx.py <word-dinh-dang.docx>   (thoát 1 nếu sai)"""
import sys
import zipfile

import docx

path = sys.argv[1]
d = docx.Document(path)
fails = []


def check(ok, what):
    print(("[ĐẠT]  " if ok else "[LỖI]  ") + what)
    if not ok:
        fails.append(what)


paras = d.paragraphs
check(any(p.style.name == "Heading 1" and p.text == "Tiêu đề thử" for p in paras), "Đoạn 'Tiêu đề thử' giữ kiểu Heading 1")
bold_ok = any(r.bold and r.text == "Chữ đậm" for p in paras for r in p.runs)
check(bold_ok, "Chữ 'Chữ đậm' giữ định dạng đậm")
normal_ok = any((not r.bold) and "thường" in r.text for p in paras for r in p.runs)
check(normal_ok, "Chữ ' thường' không bị đậm")
check(any("Mục một" in p.text for p in paras) and any("Mục hai" in p.text for p in paras), "Hai mục danh sách còn nguyên nội dung")
check([(len(t.rows), len(t.columns)) for t in d.tables] == [(2, 3)], "Bảng 3 cột x 2 hàng được lưu")
xml = zipfile.ZipFile(path).read("word/document.xml").decode("utf-8")
check(xml.count("<w:numPr>") >= 2, "Hai đoạn có danh sách (w:numPr)")
check("<w:hyperlink" in xml, "Liên kết được lưu (w:hyperlink)")
rels = zipfile.ZipFile(path).read("word/_rels/document.xml.rels").decode("utf-8")
check("https://example.com/" in rels, "Đích liên kết https://example.com/ còn nguyên")
sec = d.sections[0]
def mm(x): return round(x / 36000.0, 1) if x is not None else None
print("       lề thực tế (mm): trái", mm(sec.left_margin), "phải", mm(sec.right_margin), "trên", mm(sec.top_margin), "dưới", mm(sec.bottom_margin))
if "--margin-tested-separately" in sys.argv[2:]:
    print("[LƯU Ý] Lề kiểm bằng dung-margintest (vá DOCX theo đúng luồng app), không dùng UNO PageLRMargin cũ.")
else:
    check(all(v is not None for v in (mm(sec.left_margin), mm(sec.right_margin), mm(sec.top_margin), mm(sec.bottom_margin))) and
          abs(mm(sec.left_margin) - 30) < 1 and abs(mm(sec.right_margin) - 15) < 1 and abs(mm(sec.top_margin) - 20) < 1 and abs(mm(sec.bottom_margin) - 20) < 1,
          "Lề chuẩn VN (trái 30, phải 15, trên 20, dưới 20 mm) được lưu vào DOCX")
# Independent check of actual anchored picture wrapping and embedded image data.
from pathlib import Path
from xml.etree import ElementTree as ET
import hashlib
WP = "{http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing}"
W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
def picture_snapshot(filename):
    with zipfile.ZipFile(Path(path).parent / filename) as package:
        root = ET.fromstring(package.read("word/document.xml"))
        media = sorted(hashlib.sha256(package.read(n)).hexdigest()
                       for n in package.namelist() if n.startswith("word/media/") and not n.endswith("/"))
        content = "".join(n.text or "" for n in root.iter(W+"t"))
        return root, media, content
_, original_media, original_text = picture_snapshot("word-image-wrap-before.docx")
check(bool(original_text.strip()) and bool(original_media), "wrap baseline contains real text and embedded image")
images = []
for filename, wrap in (("word-image-square.docx", "wrapSquare"),
                       ("word-image-topbottom.docx", "wrapTopAndBottom"),
                       ("word-image-wrap-undone.docx", "wrapSquare")):
    with zipfile.ZipFile(Path(path).parent / filename) as package:
        root = ET.fromstring(package.read("word/document.xml"))
        anchors = list(root.iter(WP+"anchor"))
        check(len(anchors) == 1 and anchors[0].find(WP+wrap) is not None,
              filename + ": actual native wrap " + wrap)
        media = sorted(hashlib.sha256(package.read(n)).hexdigest()
                       for n in package.namelist() if n.startswith("word/media/") and not n.endswith("/"))
        check(bool(media), filename + ": embedded image retained")
        images.append(media)
        content = "".join(n.text or "" for n in root.iter(W+"t"))
        check(content == original_text,
              filename + ": body retained")
check(original_media == images[0] == images[1] == images[2], "wrap and Undo preserve original image bytes")
# Anchor modes must change the actual drawing representation, not its pixels.
for filename, mode in (
    ("word-image-anchor-inline.docx","inline"),
    ("word-image-anchor-inline-undone.docx","anchor"),
    ("word-image-anchor-paragraph.docx","anchor"),
    ("word-image-anchor-paragraph-undone.docx","inline"),
):
    root, media, content=picture_snapshot(filename)
    check(len(list(root.iter(WP+mode)))==1 and not list(root.iter(WP+("anchor" if mode=="inline" else "inline"))),
          filename+": native "+mode+" topology")
    check(media==original_media,filename+": original embedded image bytes")
    check(content==original_text,filename+": full body preserved")
# Actual OOXML drawing extent, source media, text, anchor and Undo.
def resize_snapshot(filename):
    root, media, content = picture_snapshot(filename)
    anchors = list(root.iter(WP+"anchor"))
    check(len(anchors)==1 and len(list(root.iter(WP+"inline")))==0, filename+": one floating picture")
    extent = anchors[0].find(WP+"extent") if len(anchors)==1 else None
    dims = tuple(int(extent.get(k,"0")) for k in ("cx","cy")) if extent is not None else (0,0)
    return dims, media, content
base_dims, base_media, base_text = resize_snapshot("word-image-resize-baseline.docx")
dims, media, content = resize_snapshot("word-image-resized.docx")
check(bool(base_media) and "IMAGE_RESIZE_KEEP" in base_text and min(base_dims)>0, "resize fixture contains real image and marker")
check(abs(dims[0]-1828800)<=1270 and abs(dims[1]-914400)<=1270 and dims!=base_dims, "native resize changes actual extent to 2 x 1 inch")
check(media==base_media and content==base_text, "resize retains embedded image bytes and document text")
check(resize_snapshot("word-image-resize-undone.docx")== (base_dims,base_media,base_text), "Undo restores exact picture dimensions and content")
# Restart is independently supported; multilevel promote/demote stays blocked in #186.
def list_snapshot(filename):
    with zipfile.ZipFile(Path(path).parent / filename) as package:
        root=ET.fromstring(package.read("word/document.xml"))
        entries={}
        for para in root.iter(W+"p"):
            text="".join(n.text or "" for n in para.iter(W+"t"))
            if text not in ("LIST_FIRST","LIST_SECOND","LIST_THIRD"):
                continue
            props=para.find(W+"pPr")
            num=props.find(W+"numPr") if props is not None else None
            if num is None or num.find(W+"numId") is None:
                continue
            level=num.find(W+"ilvl")
            entries[text]=(num.find(W+"numId").get(W+"val"),int(level.get(W+"val")) if level is not None else 0)
        return entries
def list_topology(entries):
    keys=("LIST_FIRST","LIST_SECOND","LIST_THIRD")
    if set(entries)!=set(keys):
        return []
    return [(entries[k][1],entries[k][0]==entries[keys[0]][0]) for k in keys]
baseline=list_topology(list_snapshot("word-list-baseline.docx"))
check(baseline==[(0,True),(0,True),(0,True)],"all three list items start in one sequence")
check(list_topology(list_snapshot("word-list-restarted.docx"))==[(0,True),(0,True),(0,False)],"restart only third item, other items and levels unchanged")
check(bool(baseline) and list_topology(list_snapshot("word-list-restart-undone.docx"))==baseline,"Undo restores list sequence")
deeper=list_topology(list_snapshot("word-list-deeper.docx"))
print("Native list level topology:", deeper)
check(deeper==[(0,True),(1,True),(0,True)],"Demote only middle item without changing sequence")
check(list_topology(list_snapshot("word-list-level-undone.docx"))==baseline,"Undo demote restores original levels")
check(list_topology(list_snapshot("word-list-promoted.docx"))==baseline,"Promote restores middle item")
check(list_topology(list_snapshot("word-list-promote-undone.docx"))==deeper,"Undo promote restores level one")
check(list_topology(list_snapshot("word-list-separate-baseline.docx"))==[(0,True),(0,True),(0,False)],"two independent lists before Continue")
check(list_topology(list_snapshot("word-list-continued.docx"))==[(0,True),(0,True),(0,True)],"Continue numbering rejoins third item to original sequence without changing levels")
check(list_topology(list_snapshot("word-list-continue-undone.docx"))==[(0,True),(0,True),(0,False)],"Undo continue restores independent third sequence")
for filename in ("word-list-separate-baseline.docx","word-list-continued.docx","word-list-continue-undone.docx"):
    with zipfile.ZipFile(Path(path).parent / filename) as package:
        root=ET.fromstring(package.read("word/document.xml"))
        content=["".join(n.text or "" for n in para.iter(W+"t")) for para in root.iter(W+"p")]
        check(content==["LIST_FIRST","LIST_SECOND","LIST_SEPARATOR","LIST_THIRD"],filename+": preserve all text and list order")
        numbering=ET.fromstring(package.read("word/numbering.xml"))
        ids={node.get(W+"numId"):node.find(W+"abstractNumId").get(W+"val") for node in numbering.findall(W+"num")}
        formats={node.get(W+"abstractNumId"):node.find(W+"lvl").find(W+"numFmt").get(W+"val")
                 for node in numbering.findall(W+"abstractNum") if node.find(W+"lvl") is not None and node.find(W+"lvl").find(W+"numFmt") is not None}
        items=list_snapshot(filename)
        expected=["decimal","decimal","decimal" if filename=="word-list-continued.docx" else "lowerLetter"]
        actual=[formats.get(ids.get(items.get(key, ("",0))[0])) for key in ("LIST_FIRST","LIST_SECOND","LIST_THIRD")]
        check(actual==expected,filename+": actual list formats "+str(actual))
print("\nKẾT QUẢ DOCX:", "CÓ LỖI" if fails else "TẤT CẢ ĐẠT")
sys.exit(1 if fails else 0)
