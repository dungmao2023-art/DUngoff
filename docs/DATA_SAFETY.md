# An toàn dữ liệu

## Quy tắc (được thực thi trong `engine/src/Engine.cpp`, không phải chỉ ở giao diện)

| # | Quy tắc | Cơ chế | Kiểm thử |
|---|---|---|---|
| 1 | Không mở trực tiếp tệp gốc | Sao chép sang bản làm việc tạm, chỉ nạp bản tạm | "Engine nạp bản làm việc tạm" |
| 2 | Không ghi đè tệp gốc mặc định | `saveAs(gốc)` bị từ chối nếu thiếu `allowOverwriteOriginal` | "Từ chối ghi đè tệp gốc…" |
| 3 | Sao lưu trước khi thay thế bất kỳ tệp nào đã tồn tại | `tên.dung-backup-YYYYMMDD-HHMMSS.ext` | "Thay tệp đã tồn tại có xác nhận -> có bản sao lưu" |
| 4 | Ghi nguyên tử | Ghi `~dung-xxxx-tên.ext` cùng thư mục → kiểm tra ZIP/PDF → `rename` | "Không để lại tệp tạm" |
| 5 | Tệp gốc giữ nguyên từng byte | So sánh byte trước/sau | "Tệp gốc giữ nguyên từng byte" |
| 6 | Định dạng chưa bảo toàn được thì chỉ đọc | `FormatPolicy`: `ReadOnly` chặn phím sửa, lệnh sửa, dán, nhấp đúp vào ô | "CSV mở ở chế độ chỉ đọc" |
| 7 | Xuất có mất mát phải xác nhận | PDF cần `allowLossyExport` | "Xuất PDF cần xác nhận" |
| 8 | Không ép kiểu khi nhập CSV | Mọi cột kiểu Văn bản, tắt nhận dạng số đặc biệt | thủ công: "00123" giữ nguyên |
| 9 | Không treo vì hộp thoại cũ | `Batch=true` khi nạp mọi tài liệu | CSV, password (dự kiến) |

Định dạng ghi được (Native): **XLSX, ODS, DOCX, ODT, PPTX, ODP**.
Chỉ đọc: XLS, XLSM (macro), XLSB, CSV, DOC, RTF, TXT, PPT, PDF.

## Vấn đề đã biết

Các vấn đề dưới đây do module lọc (filter) của LibreOffice gây ra, không phải do lớp bọc của
Dũng Office. Chúng xuất hiện y hệt khi chạy `soffice --headless --convert-to xlsx`
(đã kiểm chứng trên LibreOffice 24.2.7). Kiểm thử in chúng ra ở mục "KHÁC BIỆT ĐÃ BIẾT" chứ không giấu.

| Mã | Mô tả | Ảnh hưởng | Kế hoạch |
|---|---|---|---|
| DS-001 | Ô công thức tham chiếu sheet khác, định dạng *General*, được gán định dạng số của ô nguồn (`#,##0`) khi ghi XLSX | Chỉ đổi cách hiển thị, không đổi giá trị hay công thức | Kiểm tra lại trên 26.8.3 (CI Windows). Nếu vẫn còn: vá `sc/source/filter/excel` (xuất `numFmtId=0` khi ô chưa từng được người dùng đặt định dạng), gửi bản vá lên upstream |
| DS-002 | Tác giả comment thành "Unknown Author" | Mất siêu dữ liệu tác giả; nội dung comment giữ nguyên | Kiểm tra trên 26.8.3; nếu còn thì vá phần nhập `<authors>` trong `sc/source/filter/oox/commentsbuffer.cxx` |

**Quy định:** không được thêm dòng vào `tests/known_differences.txt` mà không có mục tương ứng ở đây.

## Phạm vi kiểm thử round-trip hiện có

Đã kiểm: tên sheet tiếng Việt, giá trị, công thức (cùng sheet, khác sheet, chuỗi có ngoặc kép),
định dạng số (`#,##0`, `0.00%`, `dd/mm/yyyy`, `@`), chuỗi "00123", in đậm, màu chữ, màu nền,
comment (nội dung và tác giả), hyperlink, ô gộp, độ rộng cột.

Chưa kiểm (cần thêm trước khi gọi là "bảo toàn" cho các thành phần này): biểu đồ, pivot,
định dạng có điều kiện, data validation, tên định nghĩa (defined names), ảnh, nhóm hàng/cột,
bảo vệ sheet, công thức mảng/động (FILTER, XLOOKUP…), slicer, chú thích dạng thread (Excel 365).
