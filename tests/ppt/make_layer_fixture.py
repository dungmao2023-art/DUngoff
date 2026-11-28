# SPDX-License-Identifier: MPL-2.0
"""Synthetic overlapping rectangles for native z-order smoke tests."""
from pathlib import Path
import sys
from pptx import Presentation
from pptx.enum.shapes import MSO_SHAPE
from pptx.util import Inches
from pptx.dml.color import RGBColor

path = Path(sys.argv[1])
path.parent.mkdir(parents=True, exist_ok=True)
deck = Presentation()
slide = deck.slides.add_slide(deck.slide_layouts[6])
for x, marker, rgb in ((1, "LAYER_A", (35, 90, 180)), (2, "LAYER_B", (180, 60, 30))):
    shape = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(1), Inches(2), Inches(2))
    shape.text = marker
    shape.fill.solid()
    shape.fill.fore_color.rgb = RGBColor(*rgb)
deck.save(path)
# Different x/y and sizes, so all six alignments require a real geometry change.
deck = Presentation()
slide = deck.slides.add_slide(deck.slide_layouts[6])
for x,y,w,h,marker in ((1,1,2,2,"LAYER_A"),(4,3,3,1,"LAYER_B")):
    shape = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(y), Inches(w), Inches(h))
    shape.text = marker
deck.save(path.with_name("ppt-align-input.pptx"))

# Two slides share the same master; editing it must not insert objects into either slide XML.
deck = Presentation()
for marker in ("MASTER_SLIDE_A", "MASTER_SLIDE_B"):
    slide = deck.slides.add_slide(deck.slide_layouts[6])
    slide.shapes.add_textbox(Inches(1), Inches(1), Inches(3), Inches(1)).text = marker
# Use a real text box in the shared blank layout, not an invisible
# date/footer placeholder. Copy the generated OOXML into the layout tree.
from copy import deepcopy
from pptx.oxml.ns import qn
source = deck.slides[0].shapes.add_textbox(Inches(3), Inches(3), Inches(4), Inches(1))
source.text = "MASTER_EDIT_ME"
shape_xml = deepcopy(source._element)
source._element.getparent().remove(source._element)
layout_tree = deck.slide_layouts[6]._element.cSld.spTree
ids = [int(n.get("id")) for n in layout_tree.iter(qn("p:cNvPr"))]
shape_xml.nvSpPr.cNvPr.set("id", str(max(ids,default=0)+1))
layout_tree.insert_element_before(shape_xml,"p:extLst")
deck.save(path.with_name("ppt-master-input.pptx"))

# Playback requires a non-overlapping neighbour so pixel counts isolate
# animation from correct occlusion changes. Keep z-order fixture unchanged.
deck = Presentation()
slide = deck.slides.add_slide(deck.slide_layouts[6])
for x, marker, rgb in ((1, "LAYER_A", (35, 90, 180)), (5, "LAYER_B", (180, 60, 30))):
    shape = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(1), Inches(2), Inches(2))
    shape.text = marker
    shape.fill.solid()
    shape.fill.fore_color.rgb = RGBColor(*rgb)
deck.save(path.with_name("ppt-playback-input.pptx"))

# Two independent slides with ordinary editable objects, plus a title placeholder.
deck = Presentation()
slide = deck.slides.add_slide(deck.slide_layouts[5])
slide.shapes.title.text = "LAYOUT_TITLE_ORIGINAL"
slide.shapes.add_textbox(Inches(1), Inches(2), Inches(4), Inches(1)).text = "LAYOUT_BODY_ORIGINAL"
other = deck.slides.add_slide(deck.slide_layouts[6])
other.shapes.add_textbox(Inches(1), Inches(2), Inches(4), Inches(1)).text = "LAYOUT_OTHER_SLIDE"
deck.save(path.with_name("ppt-layout-input.pptx"))
