# SPDX-License-Identifier: MPL-2.0
"""Create a real synthetic XLSX with openpyxl (already installed in CI)."""
from pathlib import Path
import sys
from openpyxl import Workbook

dest = Path(sys.argv[1])
dest.parent.mkdir(parents=True, exist_ok=True)
wb = Workbook()
ws = wb.active
ws.title = "Đơn hàng"
ws.append(["Mã", "Khách hàng", "Số tiền"])
ws.append(["001", "Nguyễn Văn An", 0.1])
ws.append(["001", "Nguyễn Văn An", 0.2])
ws.append(["1", "Trần Thị Bình", 12.3456])
ws.append(["002", "Lê Hà", 0])
lookup = wb.create_sheet("Danh mục")
lookup.append(["Mã", "Khu vực"])
lookup.append(["001", "An Khê"])
lookup.append(["1", "Gia Lai"])
formula = wb.create_sheet("Công thức")
formula.append(["Mã", "Số tiền"])
formula.append(["001", "=1+2"])
wb.save(dest)
print(dest)
