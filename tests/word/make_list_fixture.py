# SPDX-License-Identifier: MPL-2.0
"""Explicit numbered outline fixture; not a single-level default list."""
import sys
import zipfile
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
# python-docx's default template ships an unrelated bibliography customXml
# part. Strip that relationship ONLY from this synthetic fixture before save.
# Never bypass the application's real-document feature-loss guard.
for rid, rel in list(doc.part.rels.items()):
    if rel.reltype.endswith("/customXml"):
        doc.part.drop_rel(rid)
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
with zipfile.ZipFile(dest) as package:
    if any(n.lower().startswith("customxml/") for n in package.namelist()):
        raise RuntimeError("Unexpected unrelated custom XML in synthetic fixture")

# Independent numbered lists with distinct abstract styles. Some Writer
# versions coalesce identical abstract definitions even with two numIds.
# The third item begins alphabetic; Continue adopts the preceding decimal list.
from copy import deepcopy
separate_abstract = deepcopy(abstract)
separate_abstract.set(qn("w:abstractNumId"), str(aid + 1))
separate_abstract.find(qn("w:lvl")).find(qn("w:numFmt")).set(qn("w:val"), "lowerLetter")
numbering.find(qn("w:num")).addprevious(separate_abstract)
separate_num = elem("num", numId=nid + 1)
separate_num.append(elem("abstractNumId", val=aid + 1))
numbering.append(separate_num)
third = doc.paragraphs[-1]
third._p.pPr.find(qn("w:numPr")).find(qn("w:numId")).set(qn("w:val"), str(nid + 1))
separator = third.insert_paragraph_before("LIST_SEPARATOR")
doc.save(dest.with_name("word-list-separate-input.docx"))
