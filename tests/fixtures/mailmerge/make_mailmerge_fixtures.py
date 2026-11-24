#!/usr/bin/env python3
"""Tạo tệp mẫu cho kiểm thử trộn thư (viết XML thô để kiểm soát việc tách run).
Chỉ dùng trong CI / máy phát triển. Cách dùng: python make_mailmerge_fixtures.py <thư_mục_đích>"""
import sys, zipfile, datetime
from pathlib import Path

CT_BASE = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/>%s</Types>'
RELS_ROOT = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="%s"/></Relationships>'
HDR = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'

def zipit(path, files):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        for k, v in files.items():
            z.writestr(k, v)

def make_docx(p):
    W = 'xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"'
    doc = HDR + f'''<w:document {W}><w:body>
<w:p><w:r><w:rPr><w:b/></w:rPr><w:t xml:space="preserve">Kính mời </w:t></w:r><w:r><w:t>{{{{</w:t></w:r><w:r><w:rPr><w:b/><w:i/></w:rPr><w:t>Họ</w:t></w:r><w:r><w:rPr><w:b/><w:i/></w:rPr><w:t xml:space="preserve"> tên</w:t></w:r><w:r><w:t>}}}}</w:t></w:r><w:r><w:t xml:space="preserve"> đến dự lễ.</w:t></w:r></w:p>
<w:p><w:r><w:t xml:space="preserve">Chức vụ: {{{{Chức vụ}}}}</w:t></w:r></w:p>
<w:p><w:r><w:t>Cặp ký tự đặc biệt: A &amp; B &lt;C&gt;</w:t></w:r></w:p>
<w:tbl><w:tblPr/><w:tblGrid><w:gridCol w:w="4000"/></w:tblGrid><w:tr><w:tc><w:tcPr/><w:p><w:r><w:t>Đơn vị: «Đơn vị»</w:t></w:r></w:p></w:tc></w:tr></w:tbl>
<w:p><w:r><w:t>Trường chưa có dữ liệu: {{{{Thiếu}}}}</w:t></w:r></w:p>
<w:p><w:r><w:t>Ghi chú:</w:t></w:r><w:r><w:tab/></w:r><w:r><w:t>{{{{Ghi chú}}}}</w:t></w:r></w:p>
<w:sectPr><w:headerReference w:type="default" r:id="rId1"/><w:pgSz w:w="11906" w:h="16838"/></w:sectPr>
</w:body></w:document>'''
    hdr = HDR + f'<w:hdr {W}><w:p><w:r><w:t>Giấy mời – {{{{Họ tên}}}}</w:t></w:r></w:p></w:hdr>'
    zipit(p, {
        '[Content_Types].xml': CT_BASE % '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/header1.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml"/>',
        '_rels/.rels': RELS_ROOT % 'word/document.xml',
        'word/document.xml': doc,
        'word/_rels/document.xml.rels': HDR + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/header" Target="header1.xml"/></Relationships>',
        'word/header1.xml': hdr,
    })

def make_pptx(p):
    """Dùng python-pptx để có cấu trúc đầy đủ (bản tối giản viết tay không mở được bằng LibreOffice)."""
    from pptx import Presentation
    from pptx.util import Inches
    prs = Presentation()
    blank = prs.slide_layouts[6]
    s1 = prs.slides.add_slide(blank)
    tf = s1.shapes.add_textbox(Inches(1), Inches(1), Inches(6), Inches(2)).text_frame
    para = tf.paragraphs[0]
    r1 = para.add_run(); r1.text = "Kính mời {{Họ"; r1.font.bold = True
    r2 = para.add_run(); r2.text = " tên}}"; r2.font.italic = True
    p2 = tf.add_paragraph()
    r = p2.add_run(); r.text = "Dòng 1"
    p2.add_line_break()
    r = p2.add_run(); r.text = "{{Ghi chú}}"
    s2 = prs.slides.add_slide(blank)
    tf2 = s2.shapes.add_textbox(Inches(1), Inches(1), Inches(6), Inches(2)).text_frame
    tf2.paragraphs[0].add_run().text = "Chức vụ: {{Chức vụ}} & «Đơn vị»"
    s1.notes_slide.notes_text_frame.text = "Ghi chú cho {{Họ tên}}"
    prs.save(p)

def make_xlsx_template(p):
    M = 'xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"'
    sst = HDR + f'<sst {M} count="3" uniqueCount="2"><si><r><t xml:space="preserve">Kính gửi {{{{Họ</t></r><r><rPr><b/></rPr><t xml:space="preserve"> tên}}}}</t></r></si><si><t>Tiêu đề cố định</t></si></sst>'
    sheet = HDR + f'''<worksheet {M}><sheetData>
<row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>0</v></c></row>
<row r="2"><c r="A2" t="inlineStr"><is><t>{{{{Chức vụ}}}}</t></is></c><c r="B2" t="s"><v>1</v></c></row>
<row r="3"><c r="A3" t="str"><f>"{{{{CôngThức}}}}"</f><v>x</v></c><c r="B3"><v>42</v></c></row>
</sheetData></worksheet>'''
    wb = HDR + f'<workbook {M}><sheets><sheet name="Mẫu" sheetId="1" r:id="rId1"/></sheets></workbook>'
    wrels = HDR + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/></Relationships>'
    ct = CT_BASE % ('<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
                    '<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
                    '<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>')
    zipit(p, {'[Content_Types].xml': ct, '_rels/.rels': RELS_ROOT % 'xl/workbook.xml', 'xl/workbook.xml': wb,
              'xl/_rels/workbook.xml.rels': wrels, 'xl/sharedStrings.xml': sst, 'xl/worksheets/sheet1.xml': sheet})

def make_data_xlsx(p):
    from openpyxl import Workbook
    wb = Workbook(); ws = wb.active; ws.title = "Danh sách"
    ws.append(["Họ tên", "Chức vụ", "Đơn vị", "Ngày", "Ghi chú"])
    ws.append(["Nguyễn Văn An", "Giám đốc", "Phòng A & B", datetime.date(2026, 11, 20), "Dòng 1\nDòng 2"])
    ws.append(["Trần Thị Bình", "", "Phòng C", datetime.date(2026, 12, 5), ""])
    ws.append([None, None, None, None, None])
    ws.append(["Lê <Cường>", "Phó phòng", "Phòng D", datetime.date(2027, 1, 9), "CON"])
    ws["A6"] = "Phạm Thị Dung"; ws["B6"] = "Kế toán"; ws["C6"] = "Phòng E"; ws["D6"] = datetime.date(2027, 2, 1)
    wb.save(p)

if __name__ == '__main__':
    out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
    make_docx(out / 'giay-moi.docx'); make_pptx(out / 'giay-moi.pptx')
    make_xlsx_template(out / 'mau-trang-tinh.xlsx'); make_data_xlsx(out / 'danh-sach.xlsx')
    (out / 'danh-sach.csv').write_bytes(b'\xef\xbb\xbfH\xe1\xbb\x8d t\xc3\xaan;Ch\xe1\xbb\xa9c v\xe1\xbb\xa5\r\n"Nguy\xe1\xbb\x85n, V\xc4\x83n An";Gi\xc3\xa1m \xc4\x91\xe1\xbb\x91c\r\n"Tr\xe1\xba\xa7n ""B""";"Ph\xc3\xb3\r\nph\xc3\xb2ng"\r\n')
    print('đã tạo', *sorted(p.name for p in out.iterdir()))
