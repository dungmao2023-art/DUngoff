# Đối chiếu chức năng: bản thiết kế HTML ↔ ứng dụng Qt

Nguồn đối chiếu: tệp HTML thiết kế do chính chủ dự án cung cấp (`Dung_Office_Complete_FIXED.html`, bản lưu nội bộ ngoài repo).
Quy ước cột "Trạng thái": **Có** = có trong mã và có kiểm thử/tuyên bố rõ; **Một phần** = có nhưng thiếu so với HTML; **Không** = chưa có.
"Bộ tính" ghi rõ ai làm việc nặng: **engine** = LibreOffice qua LibreOfficeKit, **lõi riêng** = mã Qt của dự án (kiểm thử được không cần engine).
Không dòng nào dưới đây dựa vào giả định chưa kiểm: chỗ chưa chạy thật trên thiết bị được ghi "chưa thử trên máy thật".

## Lưu ý về bản HTML
Bản HTML tự đánh dấu nhiều nút ribbon là "chưa triển khai" và coi một số hàm (FILTER, SORT, UNIQUE, TEXT, DATE, NETWORKDAYS, PMT, NPV, IRR, ROUNDUP, ROUNDDOWN) là `unsupported` — **chỉ vì bộ tính công thức viết bằng JS của HTML không có**. Ứng dụng Qt dùng bộ tính của LibreOffice nên có sẵn hầu hết các hàm này. **Kết quả thăm dò thật** (CI run 45611673917, engine Linux của CI): chạy đúng SUM/IF (cả dấu `,` lẫn `;`), ROUNDUP, ROUNDDOWN, TEXT, DATE, NETWORKDAYS, DATEDIF, PMT, NPV, IRR, COUNTIF, SUMPRODUCT, VLOOKUP; **XLOOKUP, FILTER, SORT, UNIQUE trả `#NAME?`** (engine đó chưa có). **Thăm dò trên engine Windows 26.8.3.1 (bản cài, CI run 45618633022): XLOOKUP, FILTER, SORT, UNIQUE chạy ĐÚNG** (trả 20; 20/30; 30/20/10; a/b). Engine Linux của CI vẫn trả `#NAME?`. Vì vậy ứng dụng không ẩn 4 hàm này; khi engine trả lỗi, giao diện hiện gợi ý hàm thay thế (INDEX/MATCH, VLOOKUP, SUMIF, bộ lọc, Sắp xếp, Tìm dòng trùng). Danh mục đánh dấu 4 hàm này là "mới" và cảnh báo trong giao diện.

## Bảng đối chiếu

| # | Chức năng trong HTML | Trạng thái | Bộ tính | Tệp chính | Ưu tiên | Ghi chú |
|---|---|---|---|---|---|---|
| **Word** |||||||
| 1 | Định dạng ký tự (đậm/nghiêng/gạch chân/gạch ngang/chỉ số), phông, cỡ, màu chữ/nền | Có | engine | `word/WordRibbon.cpp`, `WordCommands.cpp` | — | |
| 2 | Căn lề đoạn, thụt lề, gạch đầu dòng/đánh số, kiểu đoạn (Tiêu đề 1–3) | Có | engine | `word/WordRibbon.cpp` | — | |
| 3 | Tìm / thay thế | Có | engine | `WordCommands.cpp` (findNext/replaceAll) | — | |
| 4 | Chèn bảng, ảnh, liên kết, ngắt trang, số trang | Có | engine | `WordCommands.cpp`, `WordActions.cpp` | — | |
| 5 | Chèn ký hiệu đặc biệt | Không | engine | — | Thấp | Hộp ký tự riêng chưa làm |
| 6 | Lề trang, khổ giấy | Có (lề qua ghi lại tệp) | lõi riêng + engine | `word/PageMargins.*` | — | Lề trên Windows phải vá `w:pgMar` rồi mở lại (mất undo, mở tab mới) — đã nói rõ trong giao diện |
| 7 | Hướng giấy (dọc/ngang) | Một phần | engine | `print/PrintDialog.cpp` | Trung bình | Có khi in; chưa có nút ribbon đổi hướng trang tài liệu |
| 8 | Giãn dòng | Một phần | engine | `.uno:SpacePara` | Trung bình | Giãn đoạn có; chọn hệ số giãn dòng chưa có hộp riêng |
| 9 | Đếm từ | Có | lõi riêng | `WordCommands.cpp countWords` | — | |
| 10 | Kiểm tra tài liệu, đổi kiểu chữ hoa/thường, dàn ý (outline) | Không | engine | — | Thấp | Chưa làm |
| 11 | Kiểm tra chính tả tiếng Việt | Một phần | lõi riêng + engine | `vntext/*` | Cao (đang theo dõi) | Bộ kiểm âm tiết riêng; hiệu quả của từ điển Hunspell trong engine **chưa chứng minh** |
| 12 | In (hộp thoại in, xem trước) | Có (chưa thử máy in thật) | Qt | `print/*` | — | Cần kiểm tra trên máy in thật |
| **Excel** |||||||
| 13 | Nhiều sheet (thêm/đổi tên/nhân đôi/xoá) | Có | engine | `excel/ExcelUi.cpp` | — | Kiểm bằng `--excel` |
| 14 | Định dạng ô: phông, màu, nền, cỡ, độ rộng cột/chiều cao dòng | Có | engine | `ExcelCommands.h` | — | |
| 15 | Định dạng ₫ / % | Có | engine | `Ribbon.cpp` | — | |
| 16 | Sắp xếp, bộ lọc, cố định hàng, chèn hàng | Có | engine | `Ribbon.cpp` | — | Kiểm bằng `--excel` kể cả tệp 5000 dòng |
| 17 | Bộ tính công thức | Có | **engine** | — | — | Không viết lại bằng JS |
| 18 | **ƒx · Tôi muốn** (6 nhóm hàm, tìm theo ý định, điền đối số) | **Có (mới)** | lõi riêng dựng chuỗi, **engine tính** | `tools/dataassist/FormulaCatalog.*`, `DataAssistWidgets.cpp` | Cao | 49 hàm, tìm có/không dấu. Nút "Chèn vào ô" đã nối vào tab Excel đang mở (`MainWindow.cpp` + `ExcelBridge.h`): hỏi xác nhận khi ô đang có dữ liệu, hoàn tác bằng Ctrl+Z, đọc lại kết quả engine tính. Kiểm bằng sweep (`wire:*`, ĐẠT trên Linux và Windows) |
| 19 | Chọn vùng + thống kê (tổng/TB/min/max/đếm) | **Có (mới, dán/CSV/XLSX)** | lõi riêng | `DataChecks.*` | Cao | Chưa tự lấy vùng đang chọn trong tab (cần `Hooks::selectedRangeText`) |
| 20 | Biểu đồ nhanh | **Một phần (mới)** | lõi riêng | `DataChecks.cpp groupForChart` | Trung bình | Chỉ biểu đồ cột dạng chữ trong hộp kết quả; chưa chèn biểu đồ vào bảng tính |
| 21 | Làm sạch dữ liệu (ngày, tiền) có xem trước/hoàn tác | Có (công cụ riêng) | lõi riêng | `tableutil/*` (đã có) | — | Không tự ép kiểu; bảo vệ mã/số điện thoại |
| 22 | Tìm dữ liệu trùng | Có | lõi riêng | `tableutil/TableOps` | — | Hộp "Công cụ vùng" liệt kê trùng, **không xoá** |
| 23 | Kiểm tra lỗi công thức | **Có (mới)** | lõi riêng đọc giá trị engine trả | `DataChecks.cpp findFormulaErrors` | Cao | Dựa vào giá trị hiển thị (#DIV/0!, #N/A, Err:5xx…) kèm giải thích tiếng Việt |
| 24 | Tay nắm điền (fill handle) | Không | engine | — | Thấp | Engine có `.uno:FillDown/FillRight`; chưa gắn nút/kéo |
| 25 | Nhập/xuất CSV | Có | lõi riêng | `tableutil/TableOps` (nhận diện bảng mã, không đoán âm thầm) | — | |
| **PowerPoint** |||||||
| 26 | Danh sách slide, thêm/xoá/nhân đôi/đổi thứ tự | Có | engine | `ppt/SlideThumbs.*`, `PptActions.*` | — | |
| 27 | Chèn hình/chữ/khối | Có | engine | `PptActions.cpp` | — | |
| 28 | Trình chiếu | Có (chưa thử màn hình ngoài) | Qt + engine | `ppt/SlideShow.*` | — | |
| 29 | Chủ đề, chuyển cảnh, ghi chú diễn giả | Không | engine | — | Trung bình | Chưa có nút; chuyển cảnh/ghi chú đọc-ghi vào tệp nguyên vẹn nhưng không chỉnh được |
| **PDF** |||||||
| 30 | Ghép, tách, xoay, đánh số trang, sắp xếp/xoá trang | Có | lõi riêng (qpdf) | `pdf/PdfOps.*`, `PdfToolsDialog.*` | — | Ghi tệp mới, không ghi đè |
| 31 | Xem trước PDF | Có | Qt PDF | — | — | |
| **Tiện ích** |||||||
| 32 | Ghi chú (Notes) | Có | lõi riêng | `extras/SidePanel.*` | — | |
| 33 | Lịch + ghi chú/việc theo ngày | **Một phần** | lõi riêng | `extras/SidePanel.*` | Trung bình | Lịch có sự kiện theo ngày; "ghi chú + danh sách việc cho từng ngày" kiểu HTML **chưa** làm trọn |
| 34 | **Kho luật** (4 liên kết chính thức) | **Có (mới, chỉ liên kết)** | — | `DataAssistWidgets.cpp LawLinks` | Cao | **Chưa xác minh** 4 liên kết còn sống và hiệu lực văn bản (lệnh tải trang bị từ chối trong phiên làm việc). Giao diện nói rõ "chưa xác minh", không lưu nội dung luật |
| 35 | Kho mẫu Việt (cây Word/Excel/PowerPoint) | Một phần | lõi riêng | `templates/*` | Trung bình | Có thư viện + cài gói mẫu; danh mục theo HTML chưa đối chiếu hết |
| 36 | **Xuất theo mẫu** MISA/FAST/KiotViet/Sapo | **Có (mới)** | lõi riêng | `TemplateExport.*` | Cao | Giống HTML: người dùng đưa mẫu .xlsx chính thức. Ghép cột, ghi **bản mới**; cảnh báo cột chưa ghép/không dùng/ô số không đọc chắc chắn. **Không** hứa phần mềm đích chấp nhận; **chưa thử** với mẫu thật của MISA/FAST/KiotViet/Sapo (không có tệp) |
| 37 | Ctrl+K bảng lệnh | Có | Qt | `shell/QuickPalette.*` | — | |
| 38 | Chế độ gọn/nổi, nhật ký thao tác, lưu/mở dự án | Một phần | Qt | `extras/ProjectsPage`, `shell/*` | Thấp | Nhật ký thao tác chưa có |

## Còn thiếu (sau nhánh này)
- ~~Nối `Hooks::insertFormula` / `selectedRangeText`~~ — ĐÃ NỐI (có test engine thật). Chưa kiểm tay trên cửa sổ Qt thật (hộp thoại xác nhận).
- Chèn biểu đồ thật vào bảng tính; tay nắm điền; ký hiệu đặc biệt; đổi hoa/thường; dàn ý; hướng giấy trang; chủ đề/chuyển cảnh/ghi chú PowerPoint.
- Ghi chú + việc theo ngày trong Lịch; đối chiếu đủ danh mục Kho mẫu Việt.
- Thử "Xuất theo mẫu" với mẫu thật; xác minh liên kết Kho luật; mọi mục "chưa thử trên máy thật".


## Bổ sung (nhánh feat/parity-wire) — kết quả sweep trên engine thật
| Mục trong mẫu HTML | Lệnh / cách làm | Kết quả CI (Linux + Windows) | Trên giao diện |
|---|---|---|---|
| Word: đổi hoa/thường | `.uno:ChangeCaseTo{Upper,Lower,SentenceCase,TitleCase,ToggleCase}` | ĐẠT cả 5 (ToggleCase đảo theo từ: "hELLO WORLD") | Nhóm "Hoa/thường" tab Trang đầu |
| Word: ký hiệu đặc biệt | `.uno:InsertSymbol` + `typeText` | ĐẠT (€, ©) | Nhóm "Ký hiệu" tab Chèn (8 ký hiệu); chưa có bảng ký hiệu đầy đủ |
| Word: hướng giấy | "A4 ngang" đã có từ trước (vá `w:pgSz` trên bản sao) | `.uno:AttributePageSize` thăm dò: số đo không đủ tin cậy → không dùng | giữ nút cũ |
| Excel: tay nắm điền | `.uno:FillDown/FillRight/FillSeries` | ĐẠT; kéo bằng chuột chưa kiểm | Nhóm "Điền" (xuống / sang phải) |
| Excel: chèn biểu đồ | `.uno:InsertObjectChart` | chỉ thấy tài liệu đổi (modified), **chưa xác nhận biểu đồ thật** | KHÔNG thêm nút; "Biểu đồ nhanh" trong Công cụ vùng vẫn dùng |
| PowerPoint: đổi chủ đề/bố cục chủ, chuyển cảnh, ghi chú diễn giả | — | CHƯA KIỂM (cần lưu .pptx rồi đọc XML) | chưa làm |
