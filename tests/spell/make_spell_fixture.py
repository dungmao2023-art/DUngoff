#!/usr/bin/env python3
"""Tạo 2 tệp docx tổng hợp (không phụ thuộc thư viện ngoài) để thăm dò kiểm chính tả tiếng Việt:
  spell-good.docx: chữ tiếng Việt đúng (có thanh hỏi/huyền/ngã...), ngôn ngữ vi-VN
  spell-bad.docx : cùng cỡ chữ nhưng nhiều âm tiết sai (gõ lỗi), ngôn ngữ vi-VN
Dùng: make_spell_fixture.py <thư mục đích>"""
import sys, zipfile, os
good = ["Hòa bình là nguyện vọng của nhân dân Việt Nam.", "Chúng tôi đang kiểm tra chính tả tiếng Việt trong văn bản này."]
bad  = ["Hoaf binhf lafw nguyeenj vojng cura nhaan daan Vieetj Nam.", "Chuýng toií đnag kieemr tra chinhf taxw tieesng Vieetj trong vawn baanr nayf."]
def doc(lines):
    paras = "".join(
        '<w:p><w:r><w:rPr><w:sz w:val="40"/><w:lang w:val="vi-VN" w:eastAsia="vi-VN" w:bidi="vi-VN"/></w:rPr><w:t xml:space="preserve">%s</w:t></w:r></w:p>' % t for t in lines)
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body>'
            + paras + '<w:sectPr><w:pgSz w:w="11906" w:h="16838"/><w:pgMar w:top="1134" w:right="1134" w:bottom="1134" w:left="1701" w:header="709" w:footer="709" w:gutter="0"/></w:sectPr></w:body></w:document>')
CT = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/></Types>'
RELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>'
DRELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>'
STY = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="Liberation Sans" w:hAnsi="Liberation Sans" w:cs="Liberation Sans"/><w:sz w:val="40"/><w:lang w:val="vi-VN" w:eastAsia="vi-VN" w:bidi="vi-VN"/></w:rPr></w:rPrDefault></w:docDefaults></w:styles>'
def write(path, lines):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", CT); z.writestr("_rels/.rels", RELS)
        z.writestr("word/_rels/document.xml.rels", DRELS); z.writestr("word/styles.xml", STY)
        z.writestr("word/document.xml", doc(lines))
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
write(os.path.join(out, "spell-good.docx"), good); write(os.path.join(out, "spell-bad.docx"), bad)
print("OK", out)
