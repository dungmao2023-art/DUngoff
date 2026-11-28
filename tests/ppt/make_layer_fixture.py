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
