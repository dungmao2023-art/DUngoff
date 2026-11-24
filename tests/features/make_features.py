#!/usr/bin/env python3
"""Tạo tệp tổng hợp có nhiều tính năng Office thường gặp để thử mở→sửa→lưu (không dùng tệp thật của người dùng).

Cách chạy:  python3 tests/features/make_features.py <thư mục ra>
Tệp tạo ra: features.docx, features.xlsx, features.pptx (+ features-pivot.xlsx nếu có LibreOffice + python-uno).
Cái nào không tạo được thì ghi vào NOT_CREATED.txt (không giả vờ).
"""
import copy, datetime, io, os, struct, subprocess, sys, uuid, zipfile, zlib
from lxml import etree

OUT = sys.argv[1] if len(sys.argv) > 1 else "features-out"
os.makedirs(OUT, exist_ok=True)
NOT_CREATED = []

W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
NS = {"w": W, "r": R}


def png_bytes(w=40, h=30, rgb=(200, 60, 60)):
    raw = b"".join(b"\x00" + bytes(rgb) * w for _ in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")


# ------------------------------------------------------------------ DOCX
def make_docx(path):
    import docx
    from docx.shared import Pt, RGBColor, Cm, Emu
    from docx.enum.section import WD_ORIENT, WD_SECTION
    from docx.enum.style import WD_STYLE_TYPE
    from docx.oxml.ns import qn
    from docx.oxml import parse_xml, OxmlElement

    d = docx.Document()
    # style tuỳ biến
    st = d.styles.add_style("DungKieuRieng", WD_STYLE_TYPE.PARAGRAPH)
    st.font.name = "Arial"; st.font.size = Pt(13); st.font.bold = True
    st.font.color.rgb = RGBColor(0x12, 0x6B, 0x3A)

    d.add_heading("Mục lục", level=1)
    # trường mục lục (TOC) có kết quả đệm
    p = d.add_paragraph()
    for typ, txt in (("begin", None), ("instr", ' TOC \\o "1-3" \\h \\z \\u '), ("separate", None), ("text", "1. Giới thiệu\t1"), ("end", None)):
        r = p.add_run()
        if typ == "instr":
            e = OxmlElement("w:instrText"); e.set(qn("xml:space"), "preserve"); e.text = txt; r._r.append(e)
        elif typ == "text":
            r.text = txt
        else:
            e = OxmlElement("w:fldChar"); e.set(qn("w:fldCharType"), typ); r._r.append(e)
    d.add_heading("1. Giới thiệu", level=1)
    d.add_paragraph("Đoạn mở đầu để sửa. ", style="DungKieuRieng")

    # theo dõi thay đổi: chèn + xoá
    p = d.add_paragraph("Văn bản gốc ")
    ins = parse_xml(f'<w:ins xmlns:w="{W}" w:id="901" w:author="Nguyễn Văn A" w:date="2026-11-01T09:00:00Z"><w:r><w:t xml:space="preserve">đoạn được chèn</w:t></w:r></w:ins>')
    dele = parse_xml(f'<w:del xmlns:w="{W}" w:id="902" w:author="Trần Thị B" w:date="2026-11-02T10:00:00Z"><w:r><w:delText xml:space="preserve"> đoạn bị xoá</w:delText></w:r></w:del>')
    p._p.append(ins); p._p.append(dele)

    # bình luận
    p = d.add_paragraph("Câu có bình luận.")
    try:
        d.add_comment(p.runs, text="Bình luận thử: kiểm tra số liệu", author="Lê C", initials="LC")
    except Exception as e:  # noqa
        NOT_CREATED.append(f"docx: bình luận (python-docx lỗi: {e})")

    # hyperlink
    p = d.add_paragraph("Liên kết: ")
    rid = d.part.relate_to("https://example.com/dung-office", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink", is_external=True)
    h = parse_xml(f'<w:hyperlink xmlns:w="{W}" xmlns:r="{R}" r:id="{rid}"><w:r><w:rPr><w:color w:val="0563C1"/><w:u w:val="single"/></w:rPr><w:t>Trang Dũng Office</w:t></w:r></w:hyperlink>')
    p._p.append(h)

    # bảng lồng
    t = d.add_table(rows=1, cols=2); t.style = "Table Grid"
    t.cell(0, 0).text = "Ô ngoài A"
    inner = t.cell(0, 1).add_table(rows=2, cols=2); inner.style = "Table Grid"
    inner.cell(0, 0).text = "Trong 1"; inner.cell(1, 1).text = "Trong 4"

    # ảnh nổi (anchor)
    buf = io.BytesIO(png_bytes()); d.add_paragraph("Đoạn có ảnh nổi.")
    run = d.paragraphs[-1].add_run()
    run.add_picture(buf, width=Cm(2))
    inline = run._r.find(".//" + qn("wp:inline"))
    anchor = parse_xml(
        '<wp:anchor xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing" distT="0" distB="0" distL="114300" distR="114300" simplePos="0" relativeHeight="251659264" behindDoc="0" locked="0" layoutInCell="1" allowOverlap="1">'
        '<wp:simplePos x="0" y="0"/><wp:positionH relativeFrom="column"><wp:posOffset>3000000</wp:posOffset></wp:positionH>'
        '<wp:positionV relativeFrom="paragraph"><wp:posOffset>0</wp:posOffset></wp:positionV>'
        f'<wp:extent cx="{inline.find(qn("wp:extent")).get("cx")}" cy="{inline.find(qn("wp:extent")).get("cy")}"/><wp:effectExtent l="0" t="0" r="0" b="0"/><wp:wrapSquare wrapText="bothSides"/>'
        '<wp:docPr id="777" name="AnhNoi"/><wp:cNvGraphicFramePr/></wp:anchor>')
    anchor.append(copy.deepcopy(inline.find("{http://schemas.openxmlformats.org/drawingml/2006/main}graphic")))
    inline.getparent().replace(inline, anchor)

    # hộp văn bản (wps) trong mc:AlternateContent + dự phòng VML
    p = d.add_paragraph()
    tb = parse_xml(
        f'<w:r xmlns:w="{W}" xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing" '
        'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
        'xmlns:wps="http://schemas.microsoft.com/office/word/2010/wordprocessingShape" '
        'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"><mc:AlternateContent><mc:Choice Requires="wps"><w:drawing>'
        '<wp:anchor distT="0" distB="0" distL="0" distR="0" simplePos="0" relativeHeight="251660288" behindDoc="0" locked="0" layoutInCell="1" allowOverlap="1">'
        '<wp:simplePos x="0" y="0"/><wp:positionH relativeFrom="column"><wp:posOffset>0</wp:posOffset></wp:positionH><wp:positionV relativeFrom="paragraph"><wp:posOffset>200000</wp:posOffset></wp:positionV>'
        '<wp:extent cx="2400000" cy="600000"/><wp:effectExtent l="0" t="0" r="0" b="0"/><wp:wrapSquare wrapText="bothSides"/><wp:docPr id="778" name="HopVanBan"/><wp:cNvGraphicFramePr/>'
        '<a:graphic><a:graphicData uri="http://schemas.microsoft.com/office/word/2010/wordprocessingShape"><wps:wsp><wps:cNvSpPr txBox="1"/>'
        '<wps:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="2400000" cy="600000"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom><a:ln w="9525"><a:solidFill><a:srgbClr val="000000"/></a:solidFill></a:ln></wps:spPr>'
        '<wps:txbx><w:txbxContent><w:p><w:r><w:t>Nội dung hộp văn bản</w:t></w:r></w:p></w:txbxContent></wps:txbx><wps:bodyPr/></wps:wsp></a:graphicData></a:graphic></wp:anchor></w:drawing></mc:Choice>'
        '<mc:Fallback><w:pict><v:shape xmlns:v="urn:schemas-microsoft-com:vml" type="#_x0000_t202" style="position:absolute;width:189pt;height:47pt"><v:textbox><w:txbxContent><w:p><w:r><w:t>Nội dung hộp văn bản</w:t></w:r></w:p></w:txbxContent></v:textbox></v:shape></w:pict></mc:Fallback></mc:AlternateContent></w:r>')
    p._p.append(tb)

    # header/footer khác nhau trang đầu + trường số trang, ngày
    sec = d.sections[0]
    sec.different_first_page_header_footer = True
    sec.first_page_header.paragraphs[0].text = "Đầu trang ĐẦU TIÊN"
    sec.header.paragraphs[0].text = "Đầu trang các trang sau"
    sec.first_page_footer.paragraphs[0].text = "Chân trang đầu tiên"
    fp = sec.footer.paragraphs[0]; fp.text = "Trang "
    fp._p.append(parse_xml(f'<w:fldSimple xmlns:w="{W}" w:instr=" PAGE "><w:r><w:t>1</w:t></w:r></w:fldSimple>'))
    fp.add_run("  Ngày ")
    fp._p.append(parse_xml(f'<w:fldSimple xmlns:w="{W}" w:instr=" DATE \\@ &quot;dd/MM/yyyy&quot; "><w:r><w:t>01/01/2026</w:t></w:r></w:fldSimple>'))

    # điều khiển nội dung (sdt), bookmark, công thức OMML, checkbox cũ, hình mờ VML trong header
    p = d.add_paragraph("Điều khiển: ")
    p._p.append(parse_xml(f'<w:sdt xmlns:w="{W}"><w:sdtPr><w:alias w:val="Họ tên"/><w:tag w:val="hoten"/><w:text/></w:sdtPr><w:sdtContent><w:r><w:t>Nhập họ tên</w:t></w:r></w:sdtContent></w:sdt>'))
    p = d.add_paragraph()
    p._p.append(parse_xml(f'<w:bookmarkStart xmlns:w="{W}" w:id="50" w:name="DiemNhan"/>'))
    p.add_run("Đoạn có bookmark")
    p._p.append(parse_xml(f'<w:bookmarkEnd xmlns:w="{W}" w:id="50"/>'))
    p = d.add_paragraph("Công thức: ")
    p._p.append(parse_xml('<m:oMath xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math"><m:f><m:num><m:r><m:t>a</m:t></m:r></m:num><m:den><m:r><m:t>b</m:t></m:r></m:den></m:f></m:oMath>'))
    p = d.add_paragraph("Ô chọn: ")
    p._p.append(parse_xml(f'<w:r xmlns:w="{W}"><w:fldChar w:fldCharType="begin"><w:ffData><w:name w:val="Check1"/><w:enabled/><w:checkBox><w:sizeAuto/><w:default w:val="1"/></w:checkBox></w:ffData></w:fldChar></w:r>'))
    p._p.append(parse_xml(f'<w:r xmlns:w="{W}"><w:instrText xml:space="preserve"> FORMCHECKBOX </w:instrText></w:r>'))
    p._p.append(parse_xml(f'<w:r xmlns:w="{W}"><w:fldChar w:fldCharType="end"/></w:r>'))
    hp = d.sections[0].header.paragraphs[0]
    hp._p.append(parse_xml(f'<w:r xmlns:w="{W}"><w:pict xmlns:v="urn:schemas-microsoft-com:vml" xmlns:o="urn:schemas-microsoft-com:office:office"><v:shapetype id="_x0000_t136" coordsize="21600,21600" o:spt="136" path="m@7,l@8,m@5,21600l@6,21600e"><v:path textpathok="t" o:connecttype="custom"/><v:textpath on="t" fitshape="t"/></v:shapetype><v:shape id="WaterMarkObject" o:spid="_x0000_s2049" type="#_x0000_t136" style="position:absolute;margin-left:0;margin-top:0;width:200pt;height:50pt;rotation:315;z-index:-251658752;mso-position-horizontal:center;mso-position-vertical:center" fillcolor="silver" stroked="f"><v:textpath style="font-family:&quot;Arial&quot;;font-size:1pt" string="MẪU"/></v:shape></w:pict></w:r>'))
    # nhiều section: A4 dọc → A3 ngang
    s2 = d.add_section(WD_SECTION.NEW_PAGE)
    s2.orientation = WD_ORIENT.LANDSCAPE
    s2.page_width, s2.page_height = Cm(42.0), Cm(29.7)
    d.add_paragraph("Đoạn trong section khổ A3 ngang.")
    s2._sectPr.append(parse_xml(f'<w:cols xmlns:w="{W}" w:num="2" w:space="720"/>'))
    d.save(path)

    # chú thích cuối trang (footnote) + font nhúng: chỉnh XML trong zip
    post_docx(path)


def post_docx(path):
    zin = zipfile.ZipFile(path); items = {n: zin.read(n) for n in zin.namelist()}; zin.close()
    ct = items["[Content_Types].xml"].decode("utf-8")
    rels = items["word/_rels/document.xml.rels"].decode("utf-8")
    doc = items["word/document.xml"].decode("utf-8")

    # footnote
    fn = (f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:footnotes xmlns:w="{W}">'
          '<w:footnote w:type="separator" w:id="-1"><w:p><w:r><w:separator/></w:r></w:p></w:footnote>'
          '<w:footnote w:type="continuationSeparator" w:id="0"><w:p><w:r><w:continuationSeparator/></w:r></w:p></w:footnote>'
          '<w:footnote w:id="1"><w:p><w:r><w:footnoteRef/></w:r><w:r><w:t xml:space="preserve"> Nội dung chú thích cuối trang.</w:t></w:r></w:p></w:footnote></w:footnotes>')
    items["word/footnotes.xml"] = fn.encode("utf-8")
    ct = ct.replace("</Types>", '<Override PartName="/word/footnotes.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.footnotes+xml"/></Types>')
    rels = rels.replace("</Relationships>", '<Relationship Id="rIdFn1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/footnotes" Target="footnotes.xml"/></Relationships>')
    doc = doc.replace("Câu có bình luận.", "Câu có bình luận.", 1)
    marker = "Đoạn mở đầu để sửa. "
    doc = doc.replace(f"<w:t xml:space=\"preserve\">{marker}</w:t></w:r>", f"<w:t xml:space=\"preserve\">{marker}</w:t></w:r><w:r><w:rPr><w:vertAlign w:val=\"superscript\"/></w:rPr><w:footnoteReference w:id=\"1\"/></w:r>", 1)
    if "footnoteReference" not in doc:
        doc = doc.replace(f"<w:t>{marker.strip()}</w:t></w:r>", f"<w:t>{marker.strip()}</w:t></w:r><w:r><w:footnoteReference w:id=\"1\"/></w:r>", 1)
    if "footnoteReference" not in doc:
        NOT_CREATED.append("docx: footnote (không chèn được tham chiếu)")

    # font nhúng (odttf: XOR 32 byte đầu với khoá GUID)
    ttf = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    if os.path.exists(ttf):
        guid = str(uuid.uuid4()).upper()
        key = bytes.fromhex(guid.replace("-", ""))[::-1]
        data = bytearray(open(ttf, "rb").read())
        for i in range(32):
            data[i] ^= key[i % 16]
        items["word/fonts/font1.odttf"] = bytes(data)
        ft = items.get("word/fontTable.xml")
        entry = (f'<w:font w:name="DejaVu Sans"><w:embedRegular r:id="rIdFont1" w:fontKey="{{{guid}}}"/></w:font>').encode("utf-8")
        if ft:
            s = ft.decode("utf-8")
            if 'xmlns:r=' not in s.split(">", 2)[1] and 'xmlns:r=' not in s[:600]:
                s = s.replace("<w:fonts ", f'<w:fonts xmlns:r="{R}" ', 1)
            s = s.replace("</w:fonts>", entry.decode("utf-8") + "</w:fonts>")
            items["word/fontTable.xml"] = s.encode("utf-8")
            frel = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                    '<Relationship Id="rIdFont1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/font" Target="fonts/font1.odttf"/></Relationships>')
            items["word/_rels/fontTable.xml.rels"] = frel.encode("utf-8")
            if 'Extension="odttf"' not in ct:
                ct = ct.replace("<Default ", '<Default Extension="odttf" ContentType="application/vnd.openxmlformats-officedocument.obfuscatedFont"/><Default ', 1)
            # bật embedTrueTypeFonts
            st = items["word/settings.xml"].decode("utf-8")
            if "embedTrueTypeFonts" not in st:
                st = st.replace("<w:zoom", "<w:embedTrueTypeFonts/><w:zoom", 1) if "<w:zoom" in st else st
                items["word/settings.xml"] = st.encode("utf-8")
        else:
            NOT_CREATED.append("docx: font nhúng (không có fontTable.xml)")
    else:
        NOT_CREATED.append("docx: font nhúng (không có tệp ttf để nhúng)")

    items["[Content_Types].xml"] = ct.encode("utf-8")
    items["word/_rels/document.xml.rels"] = rels.encode("utf-8")
    items["word/document.xml"] = doc.encode("utf-8")
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", items.pop("[Content_Types].xml"))
        for n, b in items.items():
            z.writestr(n, b)


# ------------------------------------------------------------------ XLSX
def make_xlsx(path):
    import openpyxl
    from openpyxl.formatting.rule import CellIsRule, ColorScaleRule, DataBarRule
    from openpyxl.styles import PatternFill, Font, Protection
    from openpyxl.worksheet.datavalidation import DataValidation
    from openpyxl.workbook.defined_name import DefinedName
    from openpyxl.chart import BarChart, Reference
    from openpyxl.comments import Comment
    from openpyxl.worksheet.formula import ArrayFormula

    wb = openpyxl.Workbook(); ws = wb.active; ws.title = "TongHop"
    ws.append(["Ngày", "Hàng", "Số lượng", "Thành tiền"])
    for i in range(1, 11):
        ws.append([datetime.date(2026, 11, i), f"Mặt hàng {i % 3}", i * 3, i * 125000])
    for r in range(2, 12):
        ws.cell(r, 1).number_format = "dd/mm/yyyy"
        ws.cell(r, 4).number_format = '#,##0 "₫"'
    ws.conditional_formatting.add("C2:C11", CellIsRule(operator="greaterThan", formula=["15"], fill=PatternFill("solid", bgColor="FFC7CE", fgColor="FFC7CE"), font=Font(color="9C0006")))
    ws.conditional_formatting.add("D2:D11", ColorScaleRule(start_type="min", start_color="FFFFFF", end_type="max", end_color="63BE7B"))
    ws.conditional_formatting.add("C2:C11", DataBarRule(start_type="min", end_type="max", color="638EC6"))
    dv = DataValidation(type="list", formula1='"Đạt,Chưa đạt,Chờ"', allow_blank=True); dv.add("F2:F11"); ws.add_data_validation(dv)
    ws["F1"] = "Trạng thái"; ws["F2"] = "Đạt"
    wb.defined_names["TongTien"] = DefinedName("TongTien", attr_text="TongHop!$D$2:$D$11")
    ws.freeze_panes = "B2"
    ws["H1"] = "Liên kết"; ws["H1"].hyperlink = "https://example.com/dung"; ws["H1"].style = "Hyperlink"
    ws["H2"] = "Có ghi chú"; ws["H2"].comment = Comment("Ghi chú thử: kiểm tra", "Dũng")
    ws["J1"] = ArrayFormula("J1:J3", "=C2:C4*2")
    ws.merge_cells("L1:N2"); ws["L1"] = "Ô gộp"
    ws["P1"] = "Sổ"; ws["P2"] = 1; ws["P2"].protection = Protection(locked=False)
    ws.protection.sheet = True; ws.protection.password = ""  # bảo vệ sheet không mật khẩu
    ch = BarChart(); ch.title = "Số lượng theo ngày"
    ch.add_data(Reference(ws, min_col=3, min_row=1, max_row=11), titles_from_data=True)
    ch.set_categories(Reference(ws, min_col=1, min_row=2, max_row=11)); ws.add_chart(ch, "R2")
    from openpyxl.worksheet.table import Table, TableStyleInfo
    from openpyxl.drawing.image import Image as XLImage
    ws3 = wb.create_sheet("Bảng và nhóm")
    ws3.append(["Mã", "Tên", "Tiền"])
    for i in range(1, 6):
        ws3.append([f"00{i}", f"Tên {i}", i * 1000])
    tb = Table(displayName="BangDuLieu", ref="A1:C6"); tb.tableStyleInfo = TableStyleInfo(name="TableStyleMedium9", showRowStripes=True); ws3.add_table(tb)
    ws3.row_dimensions.group(3, 4, outline_level=1, hidden=False)
    ws3.column_dimensions["E"].hidden = True; ws3["E1"] = "cột ẩn"
    ws3.print_title_rows = "1:1"; ws3.print_area = "A1:C6"
    ws3.page_setup.orientation = "landscape"; ws3.page_setup.paperSize = 8  # A3
    ws3.oddFooter.center.text = "Trang &P"
    try:
        png = "/tmp/_dung_logo.png"; open(png, "wb").write(png_bytes(40, 30)); ws3.add_image(XLImage(png), "G2")
    except Exception as e:  # noqa
        NOT_CREATED.append(f"xlsx: ảnh trong sheet ({e})")
    ws4 = wb.create_sheet("Sheet ẩn"); ws4["A1"] = "ẩn"; ws4.sheet_state = "hidden"
    NOT_CREATED.append("xlsx: sparkline, slicer, timeline, Power Query (không tạo được bằng openpyxl/không có tệp thật)")
    ws2 = wb.create_sheet("Dữ liệu 2"); ws2["A1"] = "Sheet phụ"; ws2["A2"] = "=TongHop!C2+1"
    wb.save(path)


def make_pivot(src, dst):
    """Tạo Pivot thật bằng LibreOffice (python-uno) rồi lưu xlsx. Không có thì ghi NOT_CREATED."""
    import openpyxl
    wbp = openpyxl.Workbook(); wsp = wbp.active; wsp.title = "Nguon"
    wsp.append(["Ngày", "Hàng", "Số lượng", "Thành tiền"])
    for i in range(1, 11):
        wsp.append([f"2026-11-{i:02d}", f"Mặt hàng {i % 3}", i * 3, i * 125000])
    src = os.path.join(os.path.dirname(dst), "pivot-source.xlsx"); wbp.save(src)
    script = f'''
import subprocess, time, sys, os
import uno
from com.sun.star.beans import PropertyValue
from com.sun.star.table import CellRangeAddress, CellAddress
from com.sun.star.sheet.DataPilotFieldOrientation import ROW, DATA, COLUMN
from com.sun.star.sheet.GeneralFunction import SUM
p = subprocess.Popen(["soffice","--headless","--invisible","--norestore","--accept=socket,host=localhost,port=2099;urp;"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
ctx=None
for _ in range(60):
    try:
        local=uno.getComponentContext(); res=local.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver",local)
        ctx=res.resolve("uno:socket,host=localhost,port=2099;urp;StarOffice.ComponentContext"); break
    except Exception: time.sleep(1)
if ctx is None: sys.exit(3)
smgr=ctx.ServiceManager; desk=smgr.createInstanceWithContext("com.sun.star.frame.Desktop",ctx)
def pv(n,v):
    x=PropertyValue(); x.Name=n; x.Value=v; return x
doc=desk.loadComponentFromURL(uno.systemPathToFileUrl(os.path.abspath("{src}")),"_blank",0,(pv("Hidden",True),))
sh=doc.Sheets.getByIndex(0)
rng=CellRangeAddress(); rng.Sheet=0; rng.StartColumn=0; rng.StartRow=0; rng.EndColumn=3; rng.EndRow=10
dpt=sh.getDataPilotTables(); d=dpt.createDataPilotDescriptor(); d.setSourceRange(rng)
f=d.getDataPilotFields(); f.getByIndex(1).Orientation=ROW; f.getByIndex(3).Orientation=DATA; f.getByIndex(3).Function=SUM
ca=CellAddress(); ca.Sheet=0; ca.Column=6; ca.Row=1
dpt.insertNewByName("PivotThu",ca,d)
doc.storeToURL(uno.systemPathToFileUrl(os.path.abspath("{dst}")),(pv("FilterName","Calc MS Excel 2007 XML"),))
doc.close(True)
try: desk.terminate()
except Exception: pass
try: p.wait(timeout=30)
except Exception: p.kill()
'''
    try:
        r = subprocess.run([sys.executable, "-c", script], timeout=180, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(dst):
            NOT_CREATED.append(f"xlsx: Pivot (python-uno lỗi mã {r.returncode}: {r.stderr[-200:]})")
    except Exception as e:  # noqa
        NOT_CREATED.append(f"xlsx: Pivot ({e})")
    for junk in (src, ):
        if os.path.exists(junk): os.remove(junk)


# ------------------------------------------------------------------ PPTX
def make_pptx(path):
    from pptx import Presentation
    from pptx.util import Inches, Pt
    from pptx.chart.data import CategoryChartData
    from pptx.enum.chart import XL_CHART_TYPE
    from pptx.dml.color import RGBColor
    from lxml import etree as ET

    prs = Presentation()
    # slide master tuỳ biến: thêm khối "logo" và nền
    master = prs.slide_master
    bg = master.background.fill; bg.solid(); bg.fore_color.rgb = RGBColor(0xF4, 0xF8, 0xFF)
    P_ = "http://schemas.openxmlformats.org/presentationml/2006/main"
    A_ = "http://schemas.openxmlformats.org/drawingml/2006/main"
    logo = ET.fromstring(
        f'<p:sp xmlns:p="{P_}" xmlns:a="{A_}"><p:nvSpPr><p:cNvPr id="99" name="LogoMaster"/><p:cNvSpPr/><p:nvPr userDrawn="1"/></p:nvSpPr>'
        '<p:spPr><a:xfrm><a:off x="182880" y="91440"/><a:ext cx="1097280" cy="365760"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom>'
        '<a:solidFill><a:srgbClr val="1F4FA3"/></a:solidFill></p:spPr>'
        '<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="vi-VN"/><a:t>DŨNG</a:t></a:r></a:p></p:txBody></p:sp>')
    master.shapes._spTree.append(logo)

    s = prs.slides.add_slide(prs.slide_layouts[1])
    s.shapes.title.text = "Tiêu đề trình chiếu"
    s.placeholders[1].text = "Nội dung chính\nÝ thứ hai"
    s.notes_slide.notes_text_frame.text = "Ghi chú diễn giả: nhắc số liệu quý 3"

    # chuyển cảnh
    P = "http://schemas.openxmlformats.org/presentationml/2006/main"
    tr = ET.fromstring(f'<p:transition xmlns:p="{P}" spd="med"><p:fade/></p:transition>')
    cs = s._element.find(f"{{{P}}}clrMapOvr")
    (cs.addnext(tr) if cs is not None else s._element.append(tr))

    s2 = prs.slides.add_slide(prs.slide_layouts[5]); s2.shapes.title.text = "Ảnh, bảng, biểu đồ"
    s2.shapes.add_picture(io.BytesIO(png_bytes(80, 60)), Inches(0.5), Inches(1.5), Inches(1.5), Inches(1.1))
    tbl = s2.shapes.add_table(3, 3, Inches(2.2), Inches(1.5), Inches(4), Inches(1.2)).table
    for r in range(3):
        for c in range(3):
            tbl.cell(r, c).text = f"Ô {r+1}-{c+1}"
    cd = CategoryChartData(); cd.categories = ["Q1", "Q2", "Q3"]; cd.add_series("Doanh thu", (10, 14, 9))
    s2.shapes.add_chart(XL_CHART_TYPE.COLUMN_CLUSTERED, Inches(0.5), Inches(3.0), Inches(6), Inches(3.5), cd)
    # hyperlink trong chữ, nhóm hình, slide ẩn, hoạt ảnh tối thiểu
    s3 = prs.slides.add_slide(prs.slide_layouts[5]); s3.shapes.title.text = "Liên kết, nhóm, ẩn, hoạt ảnh"
    tb3 = s3.shapes.add_textbox(Inches(0.5), Inches(1.5), Inches(5), Inches(0.6)); r_ = tb3.text_frame.paragraphs[0].add_run()
    r_.text = "Bấm vào đây"; r_.hyperlink.address = "https://example.com/dung-office"
    grp = s3.shapes.add_group_shape()
    a_ = grp.shapes.add_shape(1, Inches(0.5), Inches(2.5), Inches(1), Inches(0.6)); a_.name = "NhomA"
    b_ = grp.shapes.add_shape(1, Inches(1.7), Inches(2.5), Inches(1), Inches(0.6)); b_.name = "NhomB"
    s3._element.set("show", "0")
    sid = tb3.shape_id
    timing = ET.fromstring(
        f'<p:timing xmlns:p="{P}"><p:tnLst><p:par><p:cTn id="1" dur="indefinite" restart="never" nodeType="tmRoot"><p:childTnLst>'
        '<p:seq concurrent="1" nextAc="seek"><p:cTn id="2" dur="indefinite" nodeType="mainSeq"><p:childTnLst>'
        '<p:par><p:cTn id="3" fill="hold"><p:stCondLst><p:cond delay="indefinite"/></p:stCondLst><p:childTnLst><p:par><p:cTn id="4" fill="hold"><p:stCondLst><p:cond delay="0"/></p:stCondLst><p:childTnLst>'
        '<p:par><p:cTn id="5" presetID="10" presetClass="entr" presetSubtype="0" fill="hold" nodeType="clickEffect"><p:stCondLst><p:cond delay="0"/></p:stCondLst><p:childTnLst>'
        f'<p:set><p:cBhvr><p:cTn id="6" dur="1" fill="hold"><p:stCondLst><p:cond delay="0"/></p:stCondLst></p:cTn><p:tgtEl><p:spTgt spid="{sid}"/></p:tgtEl><p:attrNameLst><p:attrName>style.visibility</p:attrName></p:attrNameLst></p:cBhvr><p:to><p:strVal val="visible"/></p:to></p:set>'
        f'<p:animEffect transition="in" filter="fade"><p:cBhvr><p:cTn id="7" dur="500"/><p:tgtEl><p:spTgt spid="{sid}"/></p:tgtEl></p:cBhvr></p:animEffect>'
        '</p:childTnLst></p:cTn></p:par></p:childTnLst></p:cTn></p:par></p:childTnLst></p:cTn></p:par></p:childTnLst></p:cTn>'
        '<p:prevCondLst><p:cond evt="onPrev" delay="0"><p:tgtEl><p:sldTgt/></p:tgtEl></p:cond></p:prevCondLst><p:nextCondLst><p:cond evt="onNext" delay="0"><p:tgtEl><p:sldTgt/></p:tgtEl></p:cond></p:nextCondLst></p:seq>'
        '</p:childTnLst></p:cTn></p:par></p:tnLst></p:timing>')
    s3._element.append(timing)
    prs.save(path)
    NOT_CREATED.append("pptx: SmartArt (python-pptx không tạo được SmartArt; không có tệp SmartArt thật để thử)")


make_docx(os.path.join(OUT, "features.docx"))
make_xlsx(os.path.join(OUT, "features.xlsx"))
make_pptx(os.path.join(OUT, "features.pptx"))
make_pivot(os.path.join(OUT, "features.xlsx"), os.path.join(OUT, "features-pivot.xlsx"))
open(os.path.join(OUT, "NOT_CREATED.txt"), "w", encoding="utf-8").write("\n".join(NOT_CREATED) + ("\n" if NOT_CREATED else ""))
print("Đã tạo:", sorted(os.listdir(OUT)))
if NOT_CREATED:
    print("KHÔNG TẠO ĐƯỢC:"); [print(" -", x) for x in NOT_CREATED]
