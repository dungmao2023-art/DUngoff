# Tạo tệp lớn tổng hợp để kiểm kê hiệu năng: xlsx 100k dòng, docx dài, pptx 100 slide.
import sys, os
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
from openpyxl import Workbook
wb = Workbook(write_only=True)
ws = wb.create_sheet("Dữ liệu")
ws.append(["STT", "Mã", "Họ tên", "Số tiền", "Ngày", "Ghi chú", "Thành tiền", "Tỷ lệ"])
for i in range(1, 100001):
    ws.append([i, "MA%06d" % i, "Nguyễn Văn %d" % (i % 977), i * 1000.5, "2026-01-%02d" % (i % 28 + 1), "Ghi chú dòng %d" % i, i * 1000.5 * 1.1, (i % 100) / 100])
wb.save(os.path.join(out, "xlsx-100k-dong.xlsx"))
from docx import Document
d = Document()
for i in range(1, 3001):
    d.add_paragraph("Điều %d. Nội dung đoạn văn số %d của văn bản dài dùng để kiểm kê hiệu năng, có tiếng Việt có dấu." % (i, i))
    if i % 300 == 0:
        t = d.add_table(rows=20, cols=5)
        for r in range(20):
            for c in range(5):
                t.cell(r, c).text = "Ô %d-%d-%d" % (i, r, c)
d.save(os.path.join(out, "docx-3000-doan.docx"))
from pptx import Presentation
from pptx.util import Inches
p = Presentation()
for i in range(1, 101):
    s = p.slides.add_slide(p.slide_layouts[1])
    s.shapes.title.text = "Slide %d — Tiêu đề tiếng Việt" % i
    s.placeholders[1].text = "Nội dung slide %d\nDòng hai\nDòng ba" % i
p.save(os.path.join(out, "pptx-100-slide.pptx"))
for f in sorted(os.listdir(out)):
    print(f, os.path.getsize(os.path.join(out, f)))
