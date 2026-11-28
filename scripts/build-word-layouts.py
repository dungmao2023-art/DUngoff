# SPDX-License-Identifier: MPL-2.0
"""Generate the original Dũng Office editable layout pack; no external artwork."""
import argparse
import csv
import json
import struct
import zlib
from pathlib import Path
from docx import Document
from docx.enum.section import WD_SECTION_START
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_ROW_HEIGHT_RULE
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

CAT = "00_HUONG_DAN_VA_DANH_MUC/DANH_MUC_TAP_TIN.csv"
NAMES = ["Báo giấy cổ", "Tạp chí", "Bản tin", "Báo cáo học tập", "Biên bản"]
COLS = [3, 2, 2, 1, 1]
COLORS = ["242424", "244F58", "245680", "243A60", "333333"]

def set_cols(section, count):
    cols = section._sectPr.find(qn("w:cols"))
    if cols is None:
        cols = OxmlElement("w:cols")
        section._sectPr.append(cols)
    cols.set(qn("w:num"), str(count))
    cols.set(qn("w:space"), "360")

def text(doc, value, size, color, bold=False, align=None):
    p = doc.add_paragraph()
    if align is not None:
        p.alignment = align
    r = p.add_run(value)
    r.bold = bold
    r.font.size = Pt(size)
    r.font.color.rgb = RGBColor.from_string(color)
    return p

def png(path, layout, color):
    # Schematic preview (not a screenshot), 100 x 140, RGB PNG.
    w, h = 100, 140
    rows = [bytearray([255, 255, 250] * w) for _ in range(h)]
    def box(x, y, ww, hh, rgb):
        for row in rows[y:y+hh]:
            row[x*3:(x+ww)*3] = bytes(rgb) * ww
    ink = tuple(bytes.fromhex(color))
    box(9, 9, 82, 8, ink)
    box(9, 21, 82, 2, ink)
    if layout in (1, 3):
        box(9, 29, 82, 37, (209, 222, 220))
    start = 73 if layout in (1, 3) else 30
    n = COLS[layout]
    width = (82-(n-1)*4)//n
    for col in range(n):
        for y in range(start, 127, 6):
            box(9+col*(width+4), y, width-(3 if y%4 else 0), 2, (153, 159, 160))
    def chunk(tag, data):
        return struct.pack(">I", len(data))+tag+data+struct.pack(">I", zlib.crc32(tag+data)&0xffffffff)
    data = b"".join(b"\0"+r for r in rows)
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(data))+chunk(b"IEND",b""))

def frame(doc, width, height, style="Normal", color="CBD5E1"):
    # A real editable Word table cell, with minimum height (grows with content).
    table = doc.add_table(rows=1, cols=1)
    table.autofit = False
    table.columns[0].width = Cm(width)
    cell = table.cell(0,0)
    cell.width = Cm(width)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.TOP
    table.rows[0].height = Cm(height)
    table.rows[0].height_rule = WD_ROW_HEIGHT_RULE.AT_LEAST
    pr = cell._tc.get_or_add_tcPr()
    margins = OxmlElement("w:tcMar")
    for side in ("top", "left", "bottom", "right"):
        edge = OxmlElement("w:"+side)
        edge.set(qn("w:w"), "120")
        edge.set(qn("w:type"), "dxa")
        margins.append(edge)
    pr.append(margins)
    borders = OxmlElement("w:tcBorders")
    for side in ("top", "left", "bottom", "right"):
        edge = OxmlElement("w:"+side)
        edge.set(qn("w:val"), "single")
        edge.set(qn("w:sz"), "4")
        edge.set(qn("w:color"), color)
        borders.append(edge)
    pr.append(borders)
    cell.paragraphs[0].style = doc.styles[style]
    return cell

def make(root, index):
    name, color = NAMES[index], COLORS[index]
    doc = Document()
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21), Cm(29.7)
    sec.top_margin = sec.bottom_margin = Cm(2)
    sec.left_margin = sec.right_margin = Cm(2)
    normal = doc.styles["Normal"]
    normal.font.name = "Times New Roman" if index in (0,4) else "Arial"
    normal.font.size = Pt(11)
    normal.paragraph_format.space_after = Pt(6)
    normal.paragraph_format.line_spacing = 1.15
    normal.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    for level in (1,2,3):
        style = doc.styles[f"Heading {level}"]
        style.font.name = normal.font.name
        style.font.size = Pt({1:24,2:14,3:12}[level])
        style.font.color.rgb = RGBColor.from_string(color)
        style.paragraph_format.space_before = Pt(4)
        style.paragraph_format.space_after = Pt(6)
        style.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.LEFT
    title = doc.styles["Title"]
    title.font.name = normal.font.name
    title.font.size = Pt(28)
    title.font.bold = True
    title.font.color.rgb = RGBColor.from_string(color)
    title.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
    footer = sec.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    field = OxmlElement("w:fldSimple")
    field.set(qn("w:instr"), "PAGE")
    footer._p.append(field)
    # No demo article, sample names or fake dates: every body cell starts empty.
    frame(doc,17,1.7,"Title",color)
    frame(doc,17,0.8,"Subtitle")
    if index in (0,1,2):
        if index==1:
            frame(doc,17,4.7)
        sec = doc.add_section(WD_SECTION_START.CONTINUOUS)
        set_cols(sec,COLS[index])
        width=(17-(COLS[index]-1)*0.635)/COLS[index]-0.2
        for col in range(COLS[index]):
            frame(doc,width,1.2,"Heading 2",color)
            frame(doc,width,5.5 if index==1 else 7.5)
            if col+1<COLS[index]:
                doc.add_paragraph().add_run().add_break(WD_BREAK.COLUMN)
        doc.add_paragraph()
    elif index == 3:
        frame(doc,17,4.7)
        for _ in range(2):
            frame(doc,17,1.2,"Heading 2",color)
            frame(doc,17,4)
        doc.add_paragraph()
    else:
        frame(doc,17,2.5)
        table=doc.add_table(rows=4,cols=3)
        table.style="Table Grid"
        table.autofit=False
        for column in table.columns:
            column.width=Cm(17/3)
        for row in table.rows:
            row.height=Cm(1.2)
            row.height_rule=WD_ROW_HEIGHT_RULE.AT_LEAST
            for cell in row.cells:
                cell.width=Cm(17/3)
        frame(doc,17,3)
        doc.add_paragraph()
    path=root/"layouts"/(name+".docx")
    doc.save(path)
    png(Path(str(path)+".png"),index,color)
    return path.relative_to(root).as_posix()

def verify(root):
    with (root/CAT).open(encoding="utf-8",newline="") as f:
        rows=list(csv.reader(f))[1:]
    assert len(rows)==5
    for i,row in enumerate(rows):
        path=root/row[0]
        doc=Document(path)
        assert len(doc.tables)>=4 and doc.sections[0].page_width==Cm(21)
        assert all(not p.text.strip() for p in doc.paragraphs)
        assert all(not cell.text.strip() for table in doc.tables for row in table.rows for cell in row.cells)
        assert all(s.left_margin==Cm(2) and s.right_margin==Cm(2) for s in doc.sections)
        assert doc.styles["Normal"].font.size==Pt(11)
        assert (Path(str(path)+".png")).read_bytes().startswith(b"\x89PNG")
        assert int(doc.sections[-1]._sectPr.find(qn("w:cols")).get(qn("w:num"),"1"))==COLS[i]
        assert row[4]=="DOCX"
    print("PASS: five editable Word layouts, previews and native columns")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("root",type=Path)
    p.add_argument("--verify",action="store_true")
    a=p.parse_args()
    if a.verify:
        verify(a.root)
        return
    root=a.root
    root.mkdir(parents=True,exist_ok=False)
    (root/"layouts").mkdir()
    (root/CAT).parent.mkdir()
    (root/"manifest.json").write_text(json.dumps({"id":"word-layouts","name":"Bố cục Word","version":"2026.2"},ensure_ascii=False),encoding="utf-8")
    with (root/CAT).open("w",encoding="utf-8",newline="") as f:
        writer=csv.writer(f)
        writer.writerow(["Duong_dan","Tinh_trang","Nguon_goc","URL","Dinh_dang"])
        for i in range(5):
            writer.writerow([make(root,i),"Bố cục sáng tạo • chỉnh sửa tự do","Dũng Office","","DOCX"])
    verify(root)

if __name__=="__main__":
    main()
