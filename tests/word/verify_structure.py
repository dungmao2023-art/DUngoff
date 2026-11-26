# SPDX-License-Identifier: MPL-2.0
"""Fail unless the markers really reside in DOCX header/footer/foot/endnotes."""
import sys
import zipfile
from xml.etree import ElementTree as ET
W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print("PASS", message)
def text(root):
    return "".join(n.text or "" for n in root.iter(W+"t"))
with zipfile.ZipFile(sys.argv[1]) as z:
    names = z.namelist()
    body = ET.fromstring(z.read("word/document.xml"))
    check("BODY_MARKER" in text(body), "body retained")
    for prefix, marker in (("header", "HEADER_MARKER"), ("footer", "FOOTER_MARKER"), ("footnotes", "FOOTNOTE_MARKER"), ("endnotes", "ENDNOTE_MARKER")):
        parts = [n for n in names if n.startswith("word/"+prefix) and n.endswith(".xml")]
        check(bool(parts) and any(marker in text(ET.fromstring(z.read(n))) for n in parts), marker+" in correct XML part")
        check(marker not in text(body), marker+" not accidentally inserted in body")
    for tag in ("headerReference", "footerReference", "footnoteReference", "endnoteReference"):
        check(any(True for _ in body.iter(W+tag)), "document has "+tag)
