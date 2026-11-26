"""Synthetic DOCX fixture containing a real TOC field with a stale result."""
from pathlib import Path
import sys
from docx import Document
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
doc = Document()
p = doc.add_paragraph()
def field(kind):
    el = OxmlElement("w:fldChar")
    el.set(qn("w:fldCharType"), kind)
    p.add_run()._r.append(el)
field("begin")
el = OxmlElement("w:instrText")
el.set(qn("xml:space"), "preserve")
el.text = r' TOC \o "1-3" \h \z \u '
p.add_run()._r.append(el)
field("separate")
p.add_run("STALE_TOC")
field("end")
doc.add_heading("HEADING_BEFORE", level=1)
doc.add_paragraph("BODY_REMAINS")
dst = Path(sys.argv[1])
dst.parent.mkdir(parents=True, exist_ok=True)
doc.save(dst)
