# Dũng Office — checklist nghiệm thu duy nhất

Mốc rà soát 26/11/2026: main 487a62b31a7cf645dc8bfaa45d2afe2518cae1b3. Đây là backlog sống, không phải tuyên bố hoàn thiện. Đọc file này trước mỗi lượt làm; đối chiếu HEAD, PR và CI mới nhất, không sao chép báo cáo cũ. Mỗi mục chỉ đánh dấu [x] khi đạt tiêu chí nghiệm thu ghi ngay bên cạnh (có link PR, SHA, CI, hoặc bằng chứng test máy thật). "Có mã" / "CI xanh" không đồng nghĩa "GUI đã nghiệm thu". P0 = bắt buộc trước bản beta ổn định, P1 = mở rộng sau P0 hoặc triển khai độc lập; không bắt buộc tương đương 100% MS Office. Không phát hành công khai khi chưa được chủ dự án đồng ý.

## Cổng nghiệm thu công sở — ưu tiên cơ bản (27/11/2026, chủ dự án xác nhận)
Phần này là các ca nghiệm thu con của 48 ID hiện có, KHÔNG phải bảng điểm thứ hai. Không đổi trọng số hoặc stage chỉ vì tạo checklist. Cổng B0/B1 là yêu cầu sử dụng hằng ngày; B2 làm sau hoặc khi có blocker độc lập. Không bỏ P0 cũ khỏi phạm vi dự án; chức năng nâng cao chưa đạt phải ghi rõ giới hạn, không quảng cáo. Mốc 50% chỉ là checkpoint, không phải đủ phát hành hoặc lý do dừng nếu cổng cơ bản chưa đạt.

### B0 — an toàn dữ liệu, thao tác và bộ cài (ưu tiên tuyệt đối)
- [ ] A01.1 Tạo DOCX/XLSX/PPTX từ trống; nhập nội dung Unicode, lưu, đóng, mở lại; đối chiếu nội dung/định dạng trên MS Office.
- [ ] A01.2 Mở file có sẵn, sửa một phần, Save As sang tên mới; hash nguồn không đổi; Cancel không tạo file.
- [ ] A04.1 Undo/Redo liên tiếp sau gõ, dán, xóa, định dạng; không thay đổi dữ liệu ngoài vùng.
- [ ] A04.2 Tắt bất thường khi đang chỉnh; khôi phục bản tự lưu; chọn giữ/bỏ; không ghi đè nguồn.
- [ ] A04.3 File khóa/read-only/đầy đĩa/đích đã tồn tại: báo lỗi rõ, giữ bản gốc và nội dung chưa lưu; không báo thành công giả.
- [ ] A02.1 Windows thật: Telex/VNI/IME, dấu tiếng Việt, caret nhấp nháy, click đúng vị trí, kéo chọn, Backspace/Delete, Ctrl+A/C/X/V/Z/Y, Enter, Tab.
- [ ] A02.2 DPI 100/125/150/200%; zoom, cuộn, chuột và bàn phím không lệch vị trí.
- [ ] A03.1 Mở đồng thời 2 file mỗi loại; tab phân loại; đổi tab/sheet; lưu/đóng đúng file, hủy đóng khi chưa lưu.
- [ ] A05.1 In/xem trước từ Word/Excel/PPT/PDF; chọn phạm vi, A4, hướng giấy, lề, chữ Việt; kiểm PDF xuất và máy in thật.
- [ ] A06.1 Ribbon/panel vừa 800×600, 1366×768, 1920×1080; không che thao tác bắt buộc; DPI kiểm thật.
- [ ] A07.1 Thu nhỏ/restore FloatMenu, mở đúng công cụ/tab, không mất file chưa lưu.
- [ ] A08.1 Bộ cài đúng SHA main, SHA256; Windows sạch cài/mở đủ bốn công cụ, nâng cấp/gỡ không mất dữ liệu.
- [ ] A09.1 Inventory dependency/license/notice, font và từ điển; kiểm offline, không tự tải binary hoặc thực thi macro không được phép.
- [ ] A10.1 Đo thời gian mở/lưu, RAM và độ phản hồi trên máy thật với 5.000 dòng, 30 slide, 100 trang PDF; ghi cấu hình và kết quả.

### B1 — Word: ca làm việc hành chính
- [ ] W01.1 Soạn văn bản 3 trang tiếng Việt: chọn đoạn, font/cỡ/màu, đậm/nghiêng/gạch chân, căn trái/phải/giữa/đều, spacing, indent; Undo/Redo và roundtrip.
- [ ] W01.2 Bullet/numbering nhiều cấp, tiếp tục và khởi động lại danh sách.
- [ ] W02.1 Tạo bảng 4×5; chèn/xóa hàng cột, gộp/tách, chỉnh rộng/cao; giữ nội dung khi lưu.
- [ ] W02.2 Chèn ảnh, resize giữ tỷ lệ, wrap, kéo vị trí; link, ký hiệu, ngắt trang.
- [ ] W03.1 A4, lề tùy chỉnh theo nhu cầu hành chính, portrait/landscape, header/footer, số trang; kiểm preview/in.
- [ ] W03.2 Heading 1–3, tạo TOC thật, sửa heading và cập nhật mục lục; mở lại DOCX.
- [ ] W03.3 Footnote/endnote, số trang và section; không mất nội dung.
- [ ] W04.1 Tìm/thay một và tất cả, phân biệt hoa thường, đếm từ; test dấu Việt, hủy thao tác.
- [ ] W04.2 Kiểm chính tả tiếng Việt bằng từ điển có license xác minh; phân biệt kiểm âm tiết.
- [ ] W05.1 Mẫu công văn, đơn, hợp đồng; số tiền thành chữ, ngày, chuyển mã; mail merge từ Excel/CSV có Unicode.
- [ ] W06.1 Comment thêm/sửa/xóa; Track Changes bật/tắt, chấp nhận/từ chối; giữ review cũ và mở lại MS Word.

### B1 — Excel: ca làm việc bảng tính
- [ ] X01.1 Nhập số/chữ/công thức, F2/double click, thanh công thức, chọn ô/vùng và chuyển sheet; IME và Undo.
- [ ] X01.2 Copy/cut/paste vùng có công thức, số, ngày, mã 001; giữ dữ liệu ngoài vùng; paste values không đổi style; Undo khôi phục chính xác. #110 là probe, chưa bật GUI.
- [ ] X02.1 Thêm/xóa/đổi tên/nhân đôi/di chuyển sheet; hàng/cột; merge/wrap/resize; định dạng số, ngày, tiền, %, custom; lưu XLSX.
- [ ] X03.1 SUM/AVERAGE/COUNT/IF/SUMIF(S)/COUNTIF(S), VLOOKUP/XLOOKUP/INDEX/MATCH, tham chiếu tương đối/tuyệt đối/liên sheet; lỗi công thức đúng.
- [ ] X03.2 Tôi muốn tự nhận ô/vùng đang chọn, đủ đối số từng hàm, chọn ô kết quả, đổi sheet không ghi nhầm.
- [ ] X04.1 FillDown/Right, chuỗi ngày/số, kéo fill handle; công thức và tham chiếu cập nhật đúng.
- [ ] X05.1 Filter, sort một/nhiều khóa, freeze; dữ liệu nguyên hàng, header và công thức không lệch.
- [ ] X05.2 Conditional formatting, data validation/dropdown, bảo vệ sheet; kiểm XLSX thật.
- [ ] X06.1 Tạo biểu đồ cột/đường/tròn từ vùng; sửa tiêu đề, nguồn, loại; thay dữ liệu cập nhật chart; lưu và mở lại Excel.
- [ ] X07.1 Tạo PivotTable thật từ vùng có tiêu đề, chọn row/column/value, SUM/COUNT; sửa cấu hình, refresh sau đổi số và nhóm; kiểm cache và output XLSX.
- [ ] X08.1 CSV nhập/xuất Unicode, dấu phân cách/quote/newline, mã dài/0 đầu; dedup/đối soát; số thành chữ không phá định dạng.
- [ ] X09.1 Import/export mẫu MISA/KiotViet chính thức; nếu thiếu fixture thì ghi blocker, không bịa tương thích.
- [ ] X10.1 Power Query cơ bản: chọn nguồn CSV/XLSX/sheet, lọc/sort, kiểu dữ liệu, join/group, lưu recipe, chạy lại; preview, xuất bản sao; không gọi là M đầy đủ.

### B1 — PowerPoint: ca làm bài thuyết trình
- [ ] P01.1 Chèn/sửa textbox, ảnh/hình, kéo/resize/rotate; căn chỉnh, thứ tự lớp, nhóm/bỏ nhóm; nhiều đối tượng, Undo/Redo.
- [ ] P02.1 Tạo/xóa/nhân đôi/sắp xếp slide, layout, thumbnail, notes; mở lại PPTX.
- [ ] P03.1 F5/Shift+F5, chuyển slide/phím ESC, quay đúng slide; toàn màn hình/cửa sổ, màn hình ngoài.
- [ ] P04.1 Transition cơ bản, thời gian và advance; áp dụng cho một/nhiều slide; theme/master/layout cơ bản.
- [ ] P05.1 Appear/Fade, nhiều đối tượng, click, duration/delay/order; phát thật PowerPoint/Impress, không ghi đè timing cũ.

### B1 — PDF: ca xử lý tài liệu
- [ ] D01.1 Đọc PDF văn bản/scan/mật khẩu, trang/zoom, tìm tiếng Việt, chọn/copy chữ, hiển thị kết quả; lỗi mật khẩu rõ.
- [ ] D02.1 Gộp/tách/xoay/xóa/sắp xếp/nén; preview, page count, hash nguồn, đích không ghi đè; xác nhận trước khi bỏ mã hóa.
- [ ] D03.1 Chỉnh bản sao ODG chữ/ảnh/hình, Undo, xuất PDF, đối chiếu layout; thông báo giới hạn, không gọi là sửa stream gốc.
- [ ] D05.1 Watermark, số trang, chữ Unicode và ảnh; xem trước, font nhúng, đúng vị trí/trang.
- [ ] D06.1 OCR vie/eng trên Windows thật: đa trang, hủy, TXT/searchable PDF; kiểm chất lượng chữ và lớp text; cảnh báo raster, không trộn nguồn đổi.
- [ ] D07.1 Chú thích/điền form cơ bản nếu khả thi; lưu bản sao và xác minh, không coi overlay là redaction.

### B1 — Tiện ích cần cho công sở
- [ ] E01.1 Mẫu Word/Excel/PPT thường dùng mở/lưu thật, phân loại, nguồn và giấy phép rõ; không đếm mẫu trùng.
- [ ] E03.1 Notes/Calendar/tác vụ lưu local, nhắc lịch, backup/restore.
- [ ] E06.1 Hướng dẫn tiếng Việt: cài đặt, tạo/lưu/in, giới hạn tính năng, lỗi thường gặp và cập nhật.
- [ ] E02.1 Kho luật có link chính thức, ngày kiểm tra, hiệu lực; không tự tạo nội dung luật.
- [ ] E04.1 Góp ý, email đích do chủ xác nhận; E05.1 kiểm tool thực tế, license/checksum và báo thiếu dependency.

### B2 — mở rộng sau khi chức năng cơ bản ổn định
X11/X12/X13 (M, VBA, macro), P06 (emphasis, motion path, sửa timing nâng cao), D04 (sửa PDF gốc), D07 (redaction thật/chữ ký nâng cao), E02/E05 nâng cao. Không xóa các ID hoặc giảm tiêu chí cũ; chỉ đổi thứ tự thực hiện.

### Quy trình chống làm lại và quên việc
1. Đọc HEAD main, file này, CSV, PR mở và CI theo đúng SHA. Tìm ID/ca con trước khi sửa; không tạo PR trùng chức năng đã có. Nếu PR tồn tại, tiếp tục nhánh đó hoặc ghi rõ PR thay thế khi xung đột.
2. Chọn ca B0 chưa đạt trước, rồi B1; nếu bị blocker thật (GUI/fixture/quyền/engine/CI), ghi blocker và chuyển ca độc lập. Ưu tiên vá regression, không chạy probe vô hạn hoặc thêm tính năng nâng cao chỉ để tăng số commit.
3. Mỗi PR ghi ID/ca con, mã và test thực tế, SHA, CI Windows/Linux, kết quả mở/lưu file; nếu chưa test GUI thì ghi rõ. Merge khi CI đúng HEAD PASS, mergeable và các test bắt buộc đã chạy; kiểm lại xung đột sau mỗi merge.
4. Chỉ đánh dấu [x] từng ca con khi có bằng chứng theo tiêu chí; lưu bằng chứng cạnh ca (PR, commit, CI, máy thật/phiên bản Office nếu cần). Stage CSV chỉ tăng khi tiêu chí nhóm cũ đạt mức tương ứng; không cộng hai lần.
5. Khi đã đạt một ca, không làm lại trừ regression hoặc thay đổi phụ thuộc ảnh hưởng. Ghi ca hoàn thành, ca tiếp theo, blocker, điều kiện nghiệm thu máy thật trong mỗi lượt. Không force push, không ghi đè người khác, không rebuild engine vô cớ, không public release.
6. Chỉ số 50/60/70% không thay cho cổng: báo riêng số ca B0/B1 đã nghiệm thu, ca fail/blocked, 48 nhóm có trọng số. Chỉ đề xuất dùng công sở khi tất cả ca B0 và B1 thiết yếu PASS trên bản tích hợp Windows thật, không có lỗi mất dữ liệu; các mục thiếu phải được chủ dự án chấp nhận rõ ràng.

## P0 — dùng chung cả bốn công cụ
- [ ] A01 Mở, tạo, sửa, lưu, Save As, mở lại DOCX/XLSX/PPTX/PDF; kiểm nội dung, ảnh, công thức và định dạng, không ghi đè nguồn ngoài ý muốn. CI #27 là cơ sở, cần test bản tổng hợp.
- [ ] A02 Nhập tiếng Việt (Telex/VNI/IME), caret nhấp nháy, click, kéo chọn, Ctrl+A/C/X/V/Z/Y, double click, phím tắt, cuộn/zoom trên từng loại. Test Windows thật, kể cả DPI 125/150/200%.
- [ ] A03 Đa tab theo loại, đổi tab, đóng/đóng tất cả, cảnh báo chưa lưu, không nhầm tài liệu/sheet; thử với nhiều file cùng lúc.
- [ ] A04 Tự lưu, khôi phục crash, không mất dữ liệu; test tắt bất thường, dung lượng đĩa thấp, file khóa, nguồn read-only; kiểm lưu bản sao và không mất bản gốc.
- [ ] A05 In/xem trước/PDF export: trang đúng, lề đúng, chọn trang, PDF chữ Việt, máy in thật; test từ từng công cụ.
- [ ] A06 UI desktop: 800x600, 1100x700, 1366x768, 1920x1080, DPI; panel trái/phải, menu ribbon, tương phản, không tràn chữ, bàn phím. Ảnh CI không thay test tay.
- [ ] A07 FloatMenu 4 công cụ holo xếp dọc, thu nhỏ/ẩn/restore, mở đúng tab, nhiều màn hình, taskbar native. #25 merged; #37 merged, CI Windows/Linux PASS, chưa test native.
- [ ] A08 Cài/gỡ/nâng cấp OneClick trên Windows sạch, mở cả 4 công cụ, không cần admin nếu có thể, không ghi đè dữ liệu cá nhân; xác minh artifact đúng SHA main tích hợp, hash SHA256. Chưa public.
- [ ] A09 Audit giấy phép LibreOffice/Qt/qpdf/Tesseract/font/từ điển/template/công cụ ngoài; ghi nguồn, notice, không bắt trả phí, không tự tải code không xác minh. Kiểm quyền, offline, privacy và update an toàn.
- [ ] A10 Độ mượt và RAM: file lớn, 5000+ hàng, nhiều slide/trang, không treo, kiểm log và timeout; đo trên máy thật.

## P0 — Word
- [ ] W01 Gõ/sửa/chọn đoạn, font, size, màu, đậm/nghiêng/gạch/chỉ số, căn lề đoạn, bullets/numbering, thụt lề, giãn dòng; kiểm DOCX roundtrip.
- [ ] W02 Bảng (chèn/xóa hàng/cột, gộp, chỉnh kích thước), ảnh (chèn/resize/wrap), liên kết, ký hiệu, ngắt trang; test GUI.
- [ ] W03 Khổ A4, ngang/dọc, lề Việt tùy chỉnh, header/footer, số trang, footnote/endnote, dàn ý, TOC tự cập nhật. #16 có test header/footer/chú thích; TOC chưa xác minh.
- [ ] W04 Tìm/thay, đếm chữ, chọn/đổi hoa thường, kiểm chính tả Việt thực tế; phân biệt bộ kiểm âm tiết với từ điển.
- [ ] W05 Trợ lý Tôi muốn, số tiền thành chữ, mẫu hành chính, ngày tháng, TCVN3, mail merge. #30 merged; cần test thực tế và kiểm mẫu chuẩn.
- [ ] W06 Track Changes, comment, lưu lại không mất review hiện có; kiểm roundtrip, chưa xác nhận đầy đủ.

## P0 — Excel
- [ ] X01 Nhập ô, F2, double click, IME, chọn ô/vùng, nhiều sheet, con trỏ, thanh công thức; test Windows.
- [ ] X02 Thêm/xóa/đổi tên/nhân đôi/di chuyển sheet, hàng/cột; merge, wrap, width/height, định dạng số/ngày/tiền/%/custom; lưu XLSX.
- [ ] X03 Hàm cơ bản + tra cứu/điều kiện/mảng, lỗi công thức, trợ lý Tôi muốn tự điền tham chiếu và ô kết quả, đổi sheet an toàn. #35 merged; GUI chưa xác minh. XLOOKUP/FILTER/SORT/UNIQUE đã test engine Windows, Linux cũ có giới hạn.
- [ ] X04 FillDown/Right/Series và tay nắm kéo điền thật; nút có, kéo handle chưa xác minh.
- [ ] X05 Lọc/sort/freeze, định dạng điều kiện, validation, bảo vệ sheet; kiểm XLSX thật.
- [ ] X06 Biểu đồ thật lưu XLSX, chỉnh nguồn/loại/tiêu đề; chưa xác minh XML và GUI. Không coi tổng hợp bảng là chart. #44 xuất chart OOXML trong báo cáo mới, Linux PASS, chờ Windows và merge.
- [ ] X07 PivotTable thật tạo/sửa/refresh, lưu và mở lại XLSX; #36 merged, chỉ tổng hợp đọc dữ liệu, không phải PivotTable.
- [ ] X08 CSV, làm sạch/trùng/đối soát, mã dài/0 đầu, chuyển số thành chữ, xuất theo mẫu. #23/#32/#34 merged, cần test dữ liệu thật.
- [ ] X09 MISA/KiotViet (ưu tiên), FAST/Sapo sau: import file xuất, chọn mẫu chính thức đúng phiên bản, mapping tái sử dụng, cảnh báo thiếu/thừa, xuất mới và nhập thử vào phần mềm đích; không tuyên bố tương thích nếu chưa thử.

## P1 — Excel nâng cao (không quên)
- [ ] X10 Power Query cơ bản: nguồn CSV/XLSX, chọn sheet, tiêu đề, đổi tên/chọn cột, lọc/sort, làm sạch, kiểu dữ liệu, join/group, xem trước, lưu quy tắc và chạy lại. Không gọi là Power Query M đầy đủ.
- [ ] X11 Power Query M/connection/query OOXML: bảo toàn phần chưa hỗ trợ; kiểm refresh và tương thích MS Excel, không xóa query. #41 merged (CI Windows/Linux PASS), chỉ cảnh báo nguy cơ mất connections/queryTable, chưa bảo toàn query.
- [ ] X12 VBA: kiểm mở/lưu XLSM giữ vbaProject.bin, chạy macro fixture vô hại khi có sự đồng ý, không bật macro tự động; VBA object model tương thích từng phần, không hứa 100%. Chưa có bằng chứng đầy đủ.
- [ ] X13 Macro ghi thao tác, quản lý macro, cảnh báo bảo mật và test tương thích; triển khai sau X12.

## P0 — PowerPoint
- [ ] P01 Chữ, ảnh, hình khối, kéo thả/resize/rotate, căn chỉnh, thứ tự lớp, chọn nhiều đối tượng, nhóm; kiểm PPTX roundtrip.
- [ ] P02 Thêm/xóa/nhân đôi/đổi thứ tự slide, layout, thumbnail, notes; test lưu.
- [ ] P03 Trình chiếu toàn màn hình/cửa sổ, phím tới/lui, thoát và về đúng slide, màn hình ngoài; chưa test native.
- [ ] P04 Transition + Slide Master: #12/#17 có cơ bản; cần kiểm timing khi chiếu, nhiều master/layout, theme, font và đối tượng master.
- [ ] P05 Animation: #28/#33 có Appear/Fade theo click, nhiều đối tượng, duration/delay/order. Nghiệm thu PPTX trong PowerPoint, kéo thả thật; không sửa timing có sẵn.
- [ ] P06 Animation nâng cao: With Previous/After Previous, exit/emphasis/motion path, timeline, chỉnh hiệu ứng cũ, preview thực sự. #43 bổ sung 3 trigger và timeline dự kiến, vá thứ tự drag, đang CI; phải có test XML + trình chiếu.

## P0 — PDF
- [ ] D01 Mở/đọc, zoom, trang, chọn chữ, tìm chữ, copy, preview, file có mật khẩu; cần kiểm chữ Việt, file scan. #39 merged (CI Windows/Linux PASS) tìm lớp chữ, phạm vi trang, đoạn trích, đọc/copy; chưa highlight canvas.
- [ ] D02 Gộp/tách/xoay/nén/sắp xếp/xóa trang/ảnh PDF, số trang, watermark, xem trước; qpdf kiểm output và giữ nguồn.
- [ ] D03 Chỉnh realtime qua bản sao ODG, chữ/ảnh/hình, undo/redo, lưu ODG, xuất PDF, đối chiếu bản gốc. #15/#26/#27 merged, cần GUI, fidelity; không phải sửa stream PDF gốc.
- [ ] D04 Chỉnh sửa PDF gốc có cấu trúc: chọn đoạn/chữ, sửa trực tiếp, giữ font/layout và vector, không mất chữ ký/biểu mẫu/link ngoài ý muốn. Chưa hoàn thiện; không giả lập bằng lớp phủ rồi gọi là sửa gốc. Tách khỏi redaction bảo mật.
- [ ] D05 Lớp phủ chữ Việt Unicode và ảnh theo vị trí, preview thật; kiểm font nhúng/không mất nét. ASCII/ảnh đã có; #42 Unicode vector đã vá lỗi font Windows, chờ CI mới.
- [ ] D06 OCR tiếng Việt/Anh đa trang, dừng, TXT, searchable PDF (#22/#31 merged), kiểm layer chữ, chất lượng OCR, cài Tesseract/gói ngôn ngữ hợp pháp; bản raster không giữ vector/link, cảnh báo. #40 merged (CI Windows/Linux PASS, runtime discovery), chưa đóng gói Tesseract; chưa test Windows GUI.
- [ ] D07 Form/annotation/highlight, chữ ký tài liệu, redaction đúng nghĩa; đánh giá khả thi, không quảng cáo đã có nếu chưa test.

## P0/P1 — hệ sinh thái miễn phí
- [ ] E01 Kho mẫu Word/Excel/PPT: kiểm từng mẫu thực mở/lưu, lọc đúng loại, nguồn/giấy phép, số lượng thực tế; phân nhóm doanh nghiệp/kế toán/nhân sự/cá nhân/học sinh/sinh viên. Không giả định đủ 100/170 mẫu.
- [ ] E02 Kho luật: nguồn chính thức, hiệu lực và văn bản thay thế, thời điểm cập nhật; không tự đưa nội dung pháp lý chưa xác minh.
- [ ] E03 Notes/Calendar/việc/liên hệ: lưu cục bộ, nhắc lịch, lọc, backup/restore, không mất JSON; panel phải #19 merged, chưa nghiệm thu.
- [ ] E04 Góp ý: form lớn, cam kết miễn phí/cập nhật trọn đời, email do chủ xác nhận, gửi qua mail client hoặc backend được phép. #29 merged; chưa có email nhận xác nhận, chưa kiểm gửi thật.
- [ ] E05 Hộp công cụ ngoài: kiểm từng tính năng, dependency, checksum, giấy phép, khi thiếu báo rõ, không chạy lệnh ngoài trái phép.
- [ ] E06 Hướng dẫn tiếng Việt, onboarding, lỗi rõ, hỗ trợ update, bộ cài ổn định; macOS sau Windows.

## Chiến lược tận dụng mã nguồn mở (chủ dự án đồng ý 26/11/2026)
Ưu tiên thư viện miễn phí, giấy phép phù hợp, hoạt động offline, thay vì viết lại engine. Không có nghĩa được phép bỏ qua LICENSE, NOTICE, quyền phân phối font/dữ liệu hay dependency bắc cầu. Chỉ tích hợp bản release/commit ghim phiên bản và checksum; có inventory/SBOM, kiểm thử bảo toàn file, đo RAM/kích thước, phương án bỏ module nếu không đạt. Không tự tải/chạy binary khi người dùng mở tài liệu. Không dùng SDK trial, license thương mại bắt buộc hay API tính phí. Dùng engine LibreOffice hiện tại trước khi thêm engine khác.

| Mục | Ứng viên / nguồn chính thức | Hướng kỹ thuật và giới hạn |
|---|---|---|
| D04/D05 | [PoDoFo](https://github.com/podofo/podofo) (library LGPL-2.0-or-later hoặc MPL-2.0), [PDFium](https://pdfium.googlesource.com/pdfium/) (BSD-style, kiểm third-party notices) | Prototype thao tác PDF page objects trên bản sao, text Unicode/font nhúng, giữ vector và kiểm qpdf; không cam kết sửa tùy ý mọi PDF. PoDoFo tools có thể khác license library. Chưa tích hợp. |
| D06 | [Tesseract](https://github.com/tesseract-ocr/tesseract) (Apache-2.0), [tessdata](https://github.com/tesseract-ocr/tessdata) | Engine OCR đã dùng; kiểm license/nguồn cụ thể runtime Windows và vie/eng, đóng gói offline hoặc gói tùy chọn, kiểm PDF searchable. |
| X10/X11 | [DuckDB](https://duckdb.org/) (MIT), [C API](https://duckdb.org/docs/current/clients/c/overview.html) | Ưu tiên thử xử lý dữ liệu lớn và join/group; giữ reader XLSX hiện tại để không tự ép kiểu ID, không tải extension tự động, không thực thi SQL tùy ý; chưa hỗ trợ M. Đo kích thước/tốc độ trước quyết định. |
| X12/X13 | [LibreOffice VBA support](https://help.libreoffice.org/latest/en-US/text/sbasic/shared/vbasupport.html) | Tận dụng hỗ trợ VBA giới hạn, test giữ vbaProject.bin và macro vô hại; mặc định không thực thi macro. Không rebuild engine nếu không cần. |
| W04 | [Hunspell](https://github.com/hunspell/hunspell) (LGPL/GPL/MPL) | Dùng tích hợp engine hiện có nếu khả thi; kiểm riêng license từ điển tiếng Việt và hiệu quả gợi ý. |
| P04/P06 | LibreOffice Impress + OOXML hiện có | Ưu tiên hỗ trợ sẵn của engine; không dựng engine trình chiếu thứ hai nếu chưa có lý do. |

Các ứng viên chưa tích hợp phải ghi rõ là nghiên cứu/đánh giá, không đánh dấu hoàn thành. Khi chọn module mới, lập PR độc lập kèm license, nguồn, version, phạm vi và test.
 
## Đo tiến độ và công sức (26/11/2026)

Nguồn số liệu duy nhất: [RELEASE_PROGRESS.csv](RELEASE_PROGRESS.csv), 48 ID trùng checklist. Đây là mốc ước lượng do trợ lý lập, không phải giờ đã log hay cam kết ngày phát hành. Mỗi nhóm có trọng số 2–13 (độ phức tạp tương đối), stage, giờ công chủ động còn lại thấp/cao và bằng chứng/blocker. Stage scoped=0%, partial=25%, integrated=60% (có phần cốt lõi tích hợp, chưa nhất thiết đủ toàn nhóm), verified=85% (đã kiểm đủ phạm vi mã/CI, thiếu nghiệm thu cuối), accepted=100% (đạt toàn bộ tiêu chí nhóm). Chỉ số triển khai = tổng(weight × stage)/tổng(weight). Không gọi đây là tỷ lệ tính năng hoàn thành thực tế. Tỷ lệ nghiệm thu = số nhóm accepted / 48. Không hạ P0 âm thầm; 44 P0 và 4 P1 theo cách phân loại kế hoạch hiện tại, E01–E06 tính P0 Windows; macOS ngoài phạm vi. Cần chủ dự án xác nhận nếu tách D04/D07/P06 khỏi beta.

Chạy từ gốc repo: `python tools/release_progress.py --capacity 8`. Thêm `--priority P0` hoặc `--priority P1` để lọc. Capacity là số giờ công hữu hiệu mỗi ngày, không phải số AI chạy nền. Không có lịch chạy tự động. Ước lượng không bao gồm thời gian chờ CI, thử nghiệm phụ thuộc ngoài, tương thích 100% MS Office, VBA/M đầy đủ, sửa mọi PDF gốc hay macOS; các hạng mục chưa xác định khả thi cần prototype rồi lập lại ước lượng. Các khoảng giờ là tổng công sức chủ động, không phải đường găng; thực tế có phụ thuộc và thời gian review.

Mốc hiện tại sau #39–41: chỉ số triển khai 29.9%, chưa có nhóm accepted; D01 lên integrated vì phần tìm/đọc/copy đã merge, vẫn thiếu highlight và GUI. Không giảm dự toán giờ khi chưa đo. Mốc dự toán đầu tiên: 48 nhóm, chỉ số triển khai 29%, P0 32%, P1 13%; chưa có nhóm nào được ghi nhận nghiệm thu đủ toàn bộ tiêu chí. 391–1188 giờ công còn lại (P0 315–956, P1 76–232). Cập nhật CSV khi mã/CI/GUI thay đổi, giảm giờ chỉ khi có bằng chứng; nếu phát hiện blocker thì tăng giờ và ghi lý do. Không dùng số PR/commit làm phần trăm.

## Quy trình mỗi lượt
1. Đọc checklist và HEAD main + PR mở. Phân biệt chưa làm / có mã / CI đang chạy / CI đạt / merge / GUI đạt.
2. Ưu tiên blocker P0, nếu CI còn chạy thì chọn mục độc lập; không lặp lại báo cáo mà không hành động nếu còn việc khả thi. Chia nhánh theo nhóm, không force push.
3. Cập nhật bằng chứng tại mục tương ứng, link commit/PR/CI đúng SHA. Không đánh [x] chỉ vì merge.
4. Kiểm CI Windows/Linux trên head mới và bản tích hợp main, kiểm bộ cài đúng commit, test máy thật. Không rebuild engine trừ blocker thực, không phát hành khi chưa được phép.
5. Khi không có GUI/mẫu thật/email/engine phù hợp, ghi blocker cụ thể, không tự suy đoán. 

## Đối chiếu 27/11/2026 — main f46af9828c3f93caee40f07d8a00b6d0243ad2ac
- W06: #46 và #56 merged; #56 head ee0113c704772d2bcb8652532c55fccb7d3b33ed, [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45900243975). Còn nghiệm thu GUI.
- X06: #44/#47 merged, biểu đồ native có nguồn sheet. X07: #55 merged, refresh PivotTable thật; chưa tạo mới.
- W03: #51 merged, cập nhật TOC; #57 head df0835f6c15a60266df2f129fc3bb7e0420b1e3a [CI FAIL](https://github.com/dungmao2023-art/DUngoff/actions/runs/45901046539), Linux không nhận bitmap/JSDialog. Không coi tạo TOC đã đạt.
- D05: #42 merged, Unicode vector; P06: #43 merged, chưa nghiệm thu trình chiếu. A04: #48 merged, bảo vệ nguồn.
- #52 head d32663445e4fd347c917102fa60e33e5030c4285 [CI FAIL](https://github.com/dungmao2023-art/DUngoff/actions/runs/45875575721): native paste bốn ô, kiểu text XLSX, Undo định dạng đậm. Chưa merge.
- CSV đối chiếu: 32.9% chỉ số triển khai; 0 nhóm accepted; giữ nguyên trọng số và giờ dự toán. Cần test Windows thật, DPI/IME, PowerPoint playback, PDF preview, mở/lưu file thực. Không phát hành.

## Đối chiếu bổ sung — main b3e98476f64cdbfeea6bfb76dc1089af729bbd75
- #60 (65eb1cc91d2ed7f3d50727636e2f1c007abe647d) merged 6b6b71c; [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45905276261). X02 còn GUI.
- #61 (86a00c01b594f7ff6231a314094e7d8c2c0dd4e0) merged d240443; [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45906798085), kiểm tiện ích bảng cả hai OS. X10 vẫn partial vì thiếu nguồn XLSX, join/group và nhiều bước.
- #63 (4e7ac577f666ea09ae12bb79a6aa46e53ef4f604) merged ac01e1d; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45907636579) Windows/Linux PASS, OCR thật Linux; chưa kiểm OCR Windows và gói runtime.
- #64 (28b2a0099e6a4a4663218cec189281608b566524) merged b3e9847; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45908111680) Windows/Linux PASS; exit XML/timeline, chưa kiểm trình chiếu.
- #59 PASS nhưng mergeable=false; #62 CI chung PASS nhưng chưa chạy riêng Python pdfcore, giữ lại. #52/#57 FAIL như trên.
- Chỉ số triển khai 32.94%, không đổi stage/weight/giờ, 0 nhóm accepted. Main tích hợp mới cần CI và GUI máy thật; không phát hành.

## Đối chiếu bổ sung — main b59450067749673cfdd48e8b6f630ae3b1de781d
- #59 (18fd914a9ae7031dbdad4b9c309bfb3df75dee2c) merged 4b277e6; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45905297697) Windows/Linux PASS; Word xác nhận thay thế hàng loạt, đếm từ/âm tiết; chưa nghiệm thu GUI.
- #66 (b3e46346c4b26526524f086957c5a0cc1c25ba81) merged 2c7d153; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45911839194) Windows/Linux PASS, Left/Inner Join CSV.
- #67 (d3021fc286e3e884bcb3e2af39252e488f73723b) merged d17cc0e; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45912350596) Windows/Linux PASS, chọn/đổi tên cột, recipe v2.
- #68 (30d8c4af15bb78e9283a7fc8611e78c641845970) merged b409078; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45912848048) Windows/Linux PASS, Group By đếm/cộng thập phân.
- #70 (e8bce80876541e2073368a9d63a0142e6c44c46e) merged b594500; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45914696099) Windows/Linux PASS, chuỗi thao tác cố định, recipe v3. Chưa nối Join/Group By vào recipe.
- #69 CI PASS ở SHA f31c1d327576706f41563ddf80c68a612ac6570b, đã hòa giải xung đột trên SHA 48fe3de060093fc5953848dbb5c72e10ee5150c4; chờ CI đúng SHA mới, chưa merge.
- #62 head 0b29ba10c910529d41253863c3691d2e06418b42: đã thêm 10 kiểm tra an toàn lưu file vào CI; chưa có kết quả ở SHA này, chưa kiểm PDF thật với pikepdf.
- X10 giữ partial, chỉ số 32.94% không đổi stage/weight/giờ; 0 nhóm accepted. Cần kiểm thử main tích hợp, GUI Windows thật và dữ liệu XLSX thực. Không phát hành.

## Đối chiếu bổ sung — main 1a00e7a94d53d5a9c1cac2f21274300700d27da3
Các PR sau đã merge, CI trên head riêng Windows/Linux PASS (không đồng nghĩa nghiệm thu GUI hay main sau tích hợp):
- #69 (`48fe3de060093fc5953848dbb5c72e10ee5150c4`): XLSX reader and sheet selection; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45916841509).
- #72 (`a60062aa968cb63803e963cbfee7980414db31bb`): enter/exit pairing; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45917072946).
- #73 (`3bfdcbc264ee7bd1fe321fae2402fbf6ab942911`): qpdf safe output; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45918052528).
- #74 (`842712815239cfea247fd23a7d0746f5a0ffc61e`): native fill formulas; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45920396270).
- #75 (`4ff94f0b5412024795c2fea7a348d8d83cfed067`): real XLSX query fixture; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45921729258).
- #76 (`c9af4266c369a54d4e89aacdbf7c0f911b607265`): composite key dedup; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45920820122).
- #77 (`64e538233fb425d653b302426bea6c0203af4c08`): recipe v4 group/reorder; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45921905564).
- #78 (`c1de0b55e91b919c3455c5d8f77be0f20159ebd9`): extended OOXML fidelity scan; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45923660975).
- #79 (`9f300f301e7f93b4d65622022a1f680094f13748`): Unicode Word comments; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45922313203).
- #80 (`ee4b3b35242fe950142310e75d3ef66db854036a`): CSV import bounds; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45922965825).
- #81 (`b1f94e598ff434ebb69b42f66b964fd48cdc7bec`): recipe v5 join; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45926895754).
- #82 (`12fff6fd1441480b7ed49b2b3f216e0e6231ba58`): batch slide transition; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45927275342).
- #83 (`426e62c5e281ae01356931f75afe263c372f9c7e`): native sort and row integrity; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45927625137).
- #62 (`994a3ea45739aec4750897c18c040c424a00d028`): Python stdlib PDF safety tests only; [CI](https://github.com/dungmao2023-art/DUngoff/actions/runs/45923267715).

X10: chuyển partial → integrated vì chuỗi xử lý CSV/XLSX đã được ghép trong recipe v5 và có kiểm tra dữ liệu XLSX thật; chưa đủ kiểu dữ liệu, GUI, benchmark, không phải Power Query M. Giữ nguyên stage các nhóm khác, trọng số và dự toán giờ. Chỉ số triển khai 33.83%, 0 nhóm accepted. #62 chưa kiểm PDF thật bằng pikepdf. #84–86 chưa có kết quả CI đúng head, không tính. Không phát hành.

## Đối chiếu bổ sung — main 0f8423d134cec3bd3aa35e2d5bfcf5ad8d7e64eb
- #94 head 470c1e7f35043454cfb33747100d4f0e3fc73866 đã merge 0f8423d; [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45933356960). X07: mở lại XLSX, sửa nguồn, refresh native lần hai, kiểm tổng/cache; chưa tạo mới PivotTable và chưa nghiệm thu GUI.
- #95 head 90597a86bf13beb8eda966d72301f17132f9c88b [CI PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45933776016) nhưng xung đột sau #94. Chuyển toàn bộ hunks sang [#99](https://github.com/dungmao2023-art/DUngoff/pull/99), head 9df07b89c9e6e56139162109dbe7b5082f5f185b; cần CI mới.
- #96 head 4eb7f085bdd83f45f1541ef36facd11bb81917bf [CI PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45934166388) nhưng xung đột sau #94. Chuyển sang [#100](https://github.com/dungmao2023-art/DUngoff/pull/100), head 518860eb2c2d91a6b3b9d81f57f0daccaeb75bb4; cần CI mới.
- #97 head af4ecc1627aa686c7b5b587888d8cfffea4b9220 [CI Windows FAIL/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45934959625), test_replace_failure_preserves_complete_backup: 0 backup thay vì 1. [#98](https://github.com/dungmao2023-art/DUngoff/pull/98) head 67288c7facc1d694907fbbb23e780d7bfbff4f21 tách failure injection khỏi os.replace toàn cục và thêm test chặn thay thế khi công bố backup thất bại; chưa xác nhận nguyên nhân gốc hoặc CI mới.
- Giữ nguyên stage, trọng số, dự toán: 33.83% triển khai, 0 nhóm accepted. Cần Windows GUI, PivotTable creation, PDF pikepdf thực tế, PowerPoint playback; không phát hành.

## Đối chiếu bổ sung — main 705c45b7a89f6a0832d2ab2d76dbaeeca711560a
- #98 head 67288c7facc1d694907fbbb23e780d7bfbff4f21 merged 8a2493a; [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45947575159). Test backup publication và final replacement đã đạt, chưa đồng nghĩa mọi lỗi lưu PDF được nghiệm thu.
- #99 head 9df07b89c9e6e56139162109dbe7b5082f5f185b merged 8b09295; [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45947630925). X10 recipe v7 group average, rounding, compatibility; thay thế #95.
- #102 head 261734f1cfb6004942143f6f7f384c6124c485f5 merged 705c45b; [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45947947713). X05/A04 kiểm sheet/vùng chọn khi mở hộp thoại định dạng; GUI và trường hợp range rỗng cần nghiệm thu.
- #100 head 4b66cd51263488efde213651ce797d41a50eda1a và #97 head 0a2dc044fa7b039ef784b1c4be7816c602dc91c9 đã đồng bộ chính xác hai file PDF của #98; CI mới chưa hoàn tất. Không tính là merged.
- Giữ nguyên stage, trọng số, giờ dự toán: 33.83% triển khai có trọng số, 0 nhóm accepted. Các CI branch riêng không thay thế nghiệm thu main tích hợp và GUI thực tế.

## Bằng chứng cập nhật — main 9ac4871be570677236fdba24c73b742bda970aff (27/11/2026)
- P06: [#114](https://github.com/dungmao2023-art/DUngoff/pull/114) head 49d6c45cf57d9b0ce4c65b6dadc5ab1f70c97ac9, merged 07ddbde25b6edba2792e02da7886d43bd87987d1; [Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45971609706). Graphic frame XML target, clear roundtrip; chưa thử playback thực.
- X07.1: [#115](https://github.com/dungmao2023-art/DUngoff/pull/115) head e9d6f0f2c4ae51d63b4a46098f4602064e76c048, merged 9ac4871be570677236fdba24c73b742bda970aff; [Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45972714392). Đổi số, đổi nhóm nguồn B2, tổng từng nhóm và grand total, lưu/mở lại XLSX thật đều đạt. Tạo mới/cấu hình/GUI chưa đạt, giữ ca chưa tick.
- X01.2: #110 head 68d2a4fe00a3accafc1fcac2754f984b7ddfbb86 [CI FAIL hai OS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45970794361), giá trị/bold/Undo đạt nhưng font và alignment thay đổi; không bật GUI.
- A04.3/X10.1: #113 head 547a27719b44fc2dfab2056ec817f4da9ccdf982 [Linux PASS/Windows FAIL](https://github.com/dungmao2023-art/DUngoff/actions/runs/45971209960), test PDF save phát hiện metadata khác dù chưa thay thế. #117 head e14ace07da22d95872db4eee0458e80c7fdc040b sửa so sánh snapshot, [CI chưa xong](https://github.com/dungmao2023-art/DUngoff/actions/runs/45973869624).
- A04.2: #118 head 49f6ff8b7813d08f92b111a18f622bc8535383e4 sửa lỗi cleanup che lỗi copy recovery, [CI chưa xong](https://github.com/dungmao2023-art/DUngoff/actions/runs/45974454595).
- A04.3/A05.1: #119 head 1e9ed08eea2ee252def83a85274409107dad700e giữ dirty state khi lưu lỗi/xuất PDF, [CI chưa xong](https://github.com/dungmao2023-art/DUngoff/actions/runs/45975003462).
- D02: #112 head 0ceac8299fe64405a880cc302a5ad85d8aae8d2d [CI chung PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45964491339), chưa merge vì thiếu chạy pikepdf thực tế.
- D06: #111 merged ff4bd17, head 9b035d767e3fbe0711cfe988922b8a08b1dce95d [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45953089425), kiểm metadata nguồn OCR; Windows OCR thực/GUI còn thiếu.
- Giữ nguyên stage, trọng số, giờ dự toán: 33.83%, 0 nhóm accepted. Không đánh dấu B0/B1 đạt chỉ vì có CI từng phần.

## Batch B0 — main 11897514c48cc001e3070a0ac8b2c892d7f21681
- #117 merged 1189751; head e14ace07da22d95872db4eee0458e80c7fdc040b, [Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45973869624), kiểm thử an toàn lưu PDF đạt. Không coi đây là nghiệm thu toàn bộ A04.3.
- #118 head 49f6ff8b7813d08f92b111a18f622bc8535383e4 [Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45974454595); gom vào batch cùng #119/#120/#121/#113 và #116. Cần CI trên HEAD tích hợp; không tính là merged trước khi batch merge.
- #113: lỗi Windows trước đó là bộ kiểm thử lưu PDF; batch dùng main đã chứa #117, kiểm lại query ragged rows.
- Tiết kiệm Actions: chuẩn bị nhiều commit trên nhánh không mở PR, kiểm tra xung đột, mở đúng một PR khi đủ nhóm độc lập; không trigger từng commit. Giữ nguyên CI bắt buộc của PR và main; không build engine hoặc release. Nếu batch đang CI, chuẩn bị nhánh kế tiếp riêng thay vì liên tục push làm hủy job.
- #112 chưa gom vì chưa xác minh pikepdf runtime; #110 vẫn probe FAIL, không bật GUI. Giữ nguyên 33.83%, ca B0/B1 chưa tick.

## Bằng chứng batch 1 — 27/11/2026
- #122 HEAD 4be659dbed17a4c9d8dca09b8ec6015f224641ba, merged b61207982979aaaa0684268f27c4911491021e11. [CI Windows/Linux PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/45977512404): engine smoke, recovery, PDF save, table tools, keymap/caret offscreen và các bộ test bắt buộc thành công. #113/#116/#118/#119/#120/#121 đóng vì đã gom vào #122, không merge lặp.
- Batch 2 đang chuẩn bị: A01/A03 tên đích và tab; A04 dirty/recovery, Undo/Redo Unicode; X01 thanh công thức; A02 IME; đối chiếu đường dẫn Unicode. Chưa có CI của batch 2; không tick ca GUI hoặc nâng stage.

## Dũng Office Basic — ưu tiên 28/11/2026
- Mục tiêu cho người không chuyên: Word/Excel nhập liệu, chỉnh sửa và in; PowerPoint học sinh/sinh viên; PDF đọc/in/gộp/tách/xoay. Tập trung A01–A06/A08, W01–W04, X01–X06, P01–P05, D01–D02 (24 nhóm, 142/315 trọng số); toàn dự án ~59,63% nếu các nhóm này accepted và phần khác giữ nguyên. Không tuyên bố hoàn thành trước nghiệm thu.
- Tái sử dụng LibreOfficeKit (MPL), Qt PrintSupport/PDF (LGPL, notices của PDFium), qpdf (Apache-2.0), Hunspell (tri-license, từ điển Việt riêng GPL). Không thay engine. Nguồn: https://docs.libreoffice.org/libreofficekit.html ; https://doc.qt.io/qt-6/qtprintsupport-index.html ; https://github.com/qpdf/qpdf ; https://hunspell.github.io/ .
- Tạo mẫu PPTX bằng python-pptx (MIT) nếu cần, dùng ở công cụ phát triển, không cài Python cho người dùng: https://python-pptx.readthedocs.io/en/stable/user/intro.html . Dùng mẫu tự thiết kế hoặc có giấy phép rõ, không sao chép mẫu thương mại.
- A05.1: sửa đường dự phòng thiếu Qt PDF để người dùng lưu bản in PDF mới, không ghi đè, trước khi mở trình xem ngoài; thêm test bản sao tồn tại sau khi file tạm mất. Windows CI đã yêu cầu Qt PDF; vẫn cần test máy in thật. Không cài thêm runtime.
- Hoãn macro/VBA, PQ/Pivot nâng cao, OCR/sửa PDF gốc, tích hợp MISA/kho luật và animation phức tạp. Kiểm bộ cài và license thực tế trước phát hành.

- A08.1: batch 2 bổ sung BUILD-INFO.json trong bộ cài (checkout SHA thực tế, run id, engine tag, SHA256 DungOffice.exe), yêu cầu Qt PrintSupport/PDF, qpdf và runtime chính khi đóng gói CI; E2E đối chiếu manifest và hash sau cài. Chưa nghiệm thu batch 2. Ca đích là thư mục giờ bị từ chối trước staging, không tuyên bố đã tiêm lỗi staging.

- Batch 3 (chưa CI): P02 thumbnail tỷ lệ đúng, refresh; P03 nhảy số slide và tránh slide ẩn; A03 không ghi đè tài liệu tab khác; A05 giới hạn raster, từ chối khung lỗi, giữ tỷ lệ bản in trên DPI cao, báo lỗi kết thúc tác vụ. Các bài test được mở rộng nhưng chưa nghiệm thu máy thật.

- Batch 3 A04/X03: tự lưu giữ cờ retry tới khi dữ liệu và metadata commit thành công; sau Save As dùng tên/đường dẫn bản đang làm việc. Thanh công thức thử commit khi rời ô nhập nếu xác minh đúng ô/sheet, Esc huỷ; khi không xác minh được giữ nội dung nháp trong Chi tiết thông báo để sao chép. Test metadata mở rộng; cần kiểm tra GUI Windows trước nghiệm thu.

- Batch 3 P03: chọn màn hình/máy chiếu khi có nhiều màn hình, kiểm tra màn hình chưa ngắt kết nối, hiện cảnh báo nếu slide không render. Chưa thử máy chiếu thật; không đánh dấu accepted.

- Batch 3 A05: chọn tất cả/trang đang xem/khoảng trang, lọc chẵn/lẻ theo số trang gốc; từ chối khoảng trống hoặc không có trang phù hợp. 4 ca kiểm thử logic mới, chưa chạy CI.

## Bằng chứng batch 2 — 27/11/2026
- PR #123 HEAD d805275053a4a588bc5123e1a6c8898d3b00ae1a, merged main 389a39f56014ee787efabde50ca533774bb3be90. CI 45990409222 Windows/Linux PASS, kiểm thử cài/gỡ tự động Windows 0 lỗi; manifest và SHA256 ứng dụng sau cài khớp. Artifact bộ cài: https://github.com/dungmao2023-art/DUngoff/actions/runs/45990409222/artifacts/15194142868 . Checkout CI là merge candidate 93943a729aff203fa9cc42ee9dd9d4f073162605; không đồng nhất SHA với main sau merge. Chưa tick GUI/máy sạch ngoài CI/máy in thật. Batch 3 chưa CI.

- Batch 4 W01/W02/W04: xác minh lại vùng chữ/con trỏ trước khi chọn font/cỡ/màu, chèn bảng/ảnh; kiểm ảnh vẫn đọc được; hộp tìm/thay thống nhất có tìm trước/sau, xác nhận thay toàn bộ. Thêm test tìm ngược trên engine thật; chưa CI và GUI.

- Batch 4 X01/X05: thống nhất hộp tìm/thay Excel, tìm tiếp nhiều lần, khóa phạm vi sheet, xác nhận thay toàn bộ bằng lệnh native đã có test; kiểm tra ô khóa sắp xếp qua cả callback địa chỉ lẫn hình học. Cần CI/GUI, chưa nghiệm thu.

- Batch 4 UI theo yêu cầu: tay nắm Mẫu có nhãn; Tôi muốn chọn hàm trước, mở đối số tuần tự, ẩn tùy chọn và phép tính nhanh, chỉ hiện đích khi có công thức; ribbon icon-only và tooltip, font/cỡ chữ trực tiếp đầu Trang đầu Word/Excel/PPT; nút cửa sổ và zoom có ô màu. Giữ title bar hệ thống; cần chụp màn hình Windows 800x600/1280x800 và kiểm tra trạng thái font/selection thật trước nghiệm thu.
