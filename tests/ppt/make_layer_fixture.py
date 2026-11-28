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
deck.save(path.with_name("ppt-master-input.pptx"))
