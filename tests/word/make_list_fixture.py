# SPDX-License-Identifier: MPL-2.0
"""Explicit numbered outline fixture; not a single-level default list."""
import sys
from pathlib import Path
from docx import Document
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

def elem(tag, **attrs):
    node = OxmlElement("w:" + tag)
    for key, value in attrs.items():
        node.set(qn("w:" + key), str(value))
    return node

doc = Document()
numbering = doc.part.numbering_part.element
abstract_ids = [int(n.get(qn("w:abstractNumId"))) for n in numbering.findall(qn("w:abstractNum"))]
num_ids = [int(n.get(qn("w:numId"))) for n in numbering.findall(qn("w:num"))]
aid = max(abstract_ids, default=-1) + 1
nid = max(num_ids, default=0) + 1
abstract = elem("abstractNum", abstractNumId=aid)
abstract.append(elem("multiLevelType", val="multilevel"))
for i in range(3):
    lvl = elem("lvl", ilvl=i)
    lvl.append(elem("start", val=1))
    lvl.append(elem("numFmt", val="decimal"))
    lvl.append(elem("lvlText", val=".".join("%" + str(j + 1) for j in range(i + 1)) + "."))
    lvl.append(elem("lvlJc", val="left"))
    props = elem("pPr")
    props.append(elem("ind", left=720 * (i + 1), hanging=360))
    lvl.append(props)
    abstract.append(lvl)
first_num = numbering.find(qn("w:num"))
if first_num is not None:
    first_num.addprevious(abstract)
else:
    numbering.append(abstract)
num = elem("num", numId=nid)
num.append(elem("abstractNumId", val=aid))
numbering.append(num)
for marker in ("LIST_FIRST", "LIST_SECOND", "LIST_THIRD"):
    p = doc.add_paragraph(marker)
    props = p._p.get_or_add_pPr()
    np = elem("numPr")
    np.append(elem("ilvl", val=0))
    np.append(elem("numId", val=nid))
    props.append(np)
dest = Path(sys.argv[1])
dest.parent.mkdir(parents=True, exist_ok=True)
doc.save(dest)
