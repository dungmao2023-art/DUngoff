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

# Verify native tracked insertions and comments in both initial save and reopen/save.
from pathlib import Path
for filename in ("word-review.docx", "word-review-roundtrip.docx"):
    with zipfile.ZipFile(Path(sys.argv[1]).parent / filename) as review:
        root = ET.fromstring(review.read("word/document.xml"))
        check("ORIGINAL_MARKER" in text(root), filename + ": original text retained")
        check(any("INSERTED_MARKER" in text(node) for node in root.iter(W+"ins")),
              filename + ": inserted text is actual tracked insertion")
        comments = ET.fromstring(review.read("word/comments.xml"))
        comment_ids = {node.get(W+"id") for node in comments.iter(W+"comment")
                       if "COMMENT_MARKER" in text(node)}
        refs = {node.get(W+"id") for node in root.iter(W+"commentReference")}
        check(bool(comment_ids & refs), filename + ": native comment linked to document")

with zipfile.ZipFile(Path(sys.argv[1]).parent / "word-toc-updated.docx") as z:
    root = ET.fromstring(z.read("word/document.xml"))
    # Collect the field result only: the heading itself is insufficient proof.
    stack = []
    found = []
    for node in root.iter():
        if node.tag == W+"fldChar":
            kind = node.get(W+"fldCharType")
            if kind == "begin":
                stack.append({"code": "", "result": "", "separate": False})
            elif kind == "separate" and stack:
                stack[-1]["separate"] = True
            elif kind == "end" and stack:
                item = stack.pop()
                if item["code"].strip().startswith("TOC"):
                    found.append(item["result"])
        elif node.tag == W+"instrText" and stack and not stack[-1]["separate"]:
            stack[-1]["code"] += node.text or ""
        elif node.tag == W+"t":
            for item in stack:
                if item["separate"]:
                    item["result"] += node.text or ""
    check(any("HEADING_AFTER" in value and "STALE_TOC" not in value for value in found),
          "real TOC field result updated to new heading")
    check("HEADING_BEFORE" not in text(root), "old heading absent")
