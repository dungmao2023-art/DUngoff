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
from docx.enum.text import WD_ALIGN_PARAGRAPH
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

def make(root, index):
    name, color = NAMES[index], COLORS[index]
    doc = Document()
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21), Cm(29.7)
    sec.top_margin = sec.bottom_margin = Cm(1.8)
    sec.left_margin = sec.right_margin = Cm(2)
    normal = doc.styles["Normal"]
    normal.font.name = "Times New Roman" if index in (0,4) else "Arial"
    normal.font.size = Pt(11)
    normal.paragraph_format.space_after = Pt(6)
    for level in (1,2,3):
        style = doc.styles[f"Heading {level}"]
        style.font.name = normal.font.name
        style.font.color.rgb = RGBColor.from_string(color)
    footer = sec.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    footer.add_run("Dũng Office • Trang ")
    field = OxmlElement("w:fldSimple")
    field.set(qn("w:instr"), "PAGE")
    footer._p.append(field)
    if index <= 2:
        text(doc, ["BẢN TIN CUỐI TUẦN", "TẠP CHÍ", "BẢN TIN NỘI BỘ"][index], 30, color, True, WD_ALIGN_PARAGRAPH.CENTER)
        text(doc, "Số [01]  •  [Tháng / Năm]  •  [Tên đơn vị]", 10, color, False, WD_ALIGN_PARAGRAPH.CENTER)
        doc.add_heading("Tiêu đề bài viết chính", 1)
        text(doc, "Thay dòng này bằng lời dẫn ngắn của bạn.", 14, color)
        if index == 1:
            t = doc.add_table(rows=1, cols=1)
            t.style = "Table Grid"
            cell = t.cell(0,0)
            cell.text = "[Chèn ảnh bìa tại đây]"
            cell.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
            t.rows[0].height = Cm(5)
        sec = doc.add_section(WD_SECTION_START.CONTINUOUS)
        set_cols(sec,COLS[index])
        for title in ("Bài viết chính", "Góc nhìn", "Tin ngắn"):
            doc.add_heading(title,2)
            doc.add_paragraph("Nhập nội dung tại đây. Văn bản tự chảy qua các cột, không cần tự kẻ bảng.")
        text(doc, "“Thêm một câu trích dẫn đáng nhớ.”", 14, color, True)
    elif index == 3:
        text(doc,"[TÊN TRƯỜNG / ĐƠN VỊ]",14,color,True,WD_ALIGN_PARAGRAPH.CENTER)
        text(doc,"BÁO CÁO HỌC TẬP",30,color,True,WD_ALIGN_PARAGRAPH.CENTER)
        text(doc,"[Tên đề tài] • [Họ tên] • [Lớp]",14,color,False,WD_ALIGN_PARAGRAPH.CENTER)
        for title in ("Mở đầu", "Nội dung", "Kết luận", "Tài liệu tham khảo"):
            doc.add_heading(title,1)
            doc.add_paragraph("[Nhập nội dung]")
    else:
        text(doc,"[TÊN ĐƠN VỊ]",12,color,True,WD_ALIGN_PARAGRAPH.CENTER)
        text(doc,"BIÊN BẢN CUỘC HỌP",24,color,True,WD_ALIGN_PARAGRAPH.CENTER)
        for label in ("Thời gian", "Địa điểm", "Thành phần"):
            doc.add_paragraph(label+": [Điền thông tin]")
        doc.add_heading("Nội dung và kết luận",1)
        table=doc.add_table(rows=4,cols=3)
        table.style="Table Grid"
        for cell,value in zip(table.rows[0].cells,("Nội dung","Phụ trách","Thời hạn")):
            cell.text=value
        doc.add_paragraph("Người lập biên bản                         Người chủ trì")
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
        assert len(doc.paragraphs)>3 and doc.sections[0].page_width==Cm(21)
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
    (root/"manifest.json").write_text(json.dumps({"id":"word-layouts","name":"Bố cục Word","version":"2026.1"},ensure_ascii=False),encoding="utf-8")
    with (root/CAT).open("w",encoding="utf-8",newline="") as f:
        writer=csv.writer(f)
        writer.writerow(["Duong_dan","Tinh_trang","Nguon_goc","URL","Dinh_dang"])
        for i in range(5):
            writer.writerow([make(root,i),"Bố cục sáng tạo • chỉnh sửa tự do","Dũng Office","","DOCX"])
    verify(root)

if __name__=="__main__":
    main()
