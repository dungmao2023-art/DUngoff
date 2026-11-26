"""Independent XLSX consumer checks for the native chart report (openpyxl, MIT)."""
import sys
from openpyxl import load_workbook
from openpyxl.chart import BarChart

wb = load_workbook(sys.argv[1])
assert wb.sheetnames == ["TongHop"]
ws = wb.active
assert ws["A2"].value == "A & B"
assert ws["D2"].value == 7
assert ws["B2"].value == 2
assert ws["A3"].value == "=không phải công thức"
assert ws["A3"].data_type == "s"
assert ws["D3"].value == 20
assert len(ws._charts) == 1
chart = ws._charts[0]
assert isinstance(chart, BarChart)
assert chart.type == "col"
assert chart.series[0].cat.strRef.f == "TongHop!$A$2:$A$3"
assert chart.series[0].val.numRef.f == "TongHop!$D$2:$D$3"
print("PASS: native editable column chart and source cell references")
