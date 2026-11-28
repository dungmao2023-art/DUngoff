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
# Verify native list levels and restart through real DOCX numbering metadata.
def list_snapshot(filename):
    with zipfile.ZipFile(Path(path).parent / filename) as package:
        root = ET.fromstring(package.read("word/document.xml"))
        if "word/numbering.xml" not in package.namelist():
            check(False, filename + ": missing numbering metadata")
            return {}, None
        numbering = ET.fromstring(package.read("word/numbering.xml"))
        entries = {}
        for para in root.iter(W+"p"):
            text = "".join(n.text or "" for n in para.iter(W+"t"))
            if text not in ("LIST_FIRST","LIST_SECOND","LIST_THIRD"):
                continue
            props = para.find(W+"pPr")
            num = props.find(W+"numPr") if props is not None else None
            if num is None or num.find(W+"numId") is None:
                continue
            level = num.find(W+"ilvl")
            entries[text] = (num.find(W+"numId").get(W+"val"), int(level.get(W+"val")) if level is not None else 0)
        return entries, numbering
base, definitions = list_snapshot("word-list-baseline.docx")
check(set(base)=={"LIST_FIRST","LIST_SECOND","LIST_THIRD"}, "list baseline contains all three numbered paragraphs")
if len(base)==3 and definitions is not None:
    numid=base["LIST_FIRST"][0]
    nums=[n for n in definitions.findall(W+"num") if n.get(W+"numId")==numid]
    abstract_id=nums[0].find(W+"abstractNumId").get(W+"val") if len(nums)==1 else None
    abstract=[n for n in definitions.findall(W+"abstractNum") if n.get(W+"abstractNumId")==abstract_id]
    levels={int(n.get(W+"ilvl")) for n in abstract[0].findall(W+"lvl")} if len(abstract)==1 else set()
    check({0,1,2}.issubset(levels),"baseline has explicit multilevel numbering definitions")
    check(all(v==(numid,0) for v in base.values()),"all markers share one initial level-zero sequence")
deeper, _ = list_snapshot("word-list-deeper.docx")
if set(base)==set(deeper)=={"LIST_FIRST","LIST_SECOND","LIST_THIRD"}:
    check(deeper["LIST_SECOND"][1]==base["LIST_SECOND"][1]+1, "list level increased only for selected paragraph")
    check(all(deeper[k][1]==base[k][1] for k in ("LIST_FIRST","LIST_THIRD")), "other list levels unchanged")
else:
    check(False,"all numbered paragraphs survive level change")
undone, _ = list_snapshot("word-list-level-undone.docx")
def topology(entries):
    return [(entries[k][1], entries[k][0]==entries["LIST_FIRST"][0]) for k in ("LIST_FIRST","LIST_SECOND","LIST_THIRD")] if len(entries)==3 else []
check(bool(topology(base)) and topology(undone)==topology(base), "Undo restores original list structure")
restarted, numbering = list_snapshot("word-list-restarted.docx")
if len(restarted)==3:
    first, third=restarted["LIST_FIRST"],restarted["LIST_THIRD"]
    check(first[0]!=third[0] and first[1]==third[1], "restart creates independent numbering sequence at same level")
else:
    check(False,"restart retains all numbered paragraphs")
undone, _ = list_snapshot("word-list-restart-undone.docx")
check(bool(topology(base)) and topology(undone)==topology(base), "Undo restores numbering sequence")
print("\nKẾT QUẢ DOCX:", "CÓ LỖI" if fails else "TẤT CẢ ĐẠT")
sys.exit(1 if fails else 0)
