# Dũng Office — checklist nghiệm thu duy nhất

Mốc rà soát 26/11/2026: main 212e757d78e3a36e3afcabeb159a7ee68f5ced0a. Đây là backlog sống, không phải tuyên bố hoàn thiện. Đọc file này trước mỗi lượt làm; đối chiếu HEAD, PR và CI mới nhất, không sao chép báo cáo cũ. Mỗi mục chỉ đánh dấu [x] khi đạt tiêu chí nghiệm thu ghi ngay bên cạnh (có link PR, SHA, CI, hoặc bằng chứng test máy thật). "Có mã" / "CI xanh" không đồng nghĩa "GUI đã nghiệm thu". P0 = bắt buộc trước bản beta ổn định, P1 = mở rộng sau P0 hoặc triển khai độc lập; không bắt buộc tương đương 100% MS Office. Không phát hành công khai khi chưa được chủ dự án đồng ý.

## P0 — dùng chung cả bốn công cụ
- [ ] A01 Mở, tạo, sửa, lưu, Save As, mở lại DOCX/XLSX/PPTX/PDF; kiểm nội dung, ảnh, công thức và định dạng, không ghi đè nguồn ngoài ý muốn. CI #27 là cơ sở, cần test bản tổng hợp.
- [ ] A02 Nhập tiếng Việt (Telex/VNI/IME), caret nhấp nháy, click, kéo chọn, Ctrl+A/C/X/V/Z/Y, double click, phím tắt, cuộn/zoom trên từng loại. Test Windows thật, kể cả DPI 125/150/200%.
- [ ] A03 Đa tab theo loại, đổi tab, đóng/đóng tất cả, cảnh báo chưa lưu, không nhầm tài liệu/sheet; thử với nhiều file cùng lúc.
- [ ] A04 Tự lưu, khôi phục crash, không mất dữ liệu; test tắt bất thường, dung lượng đĩa thấp, file khóa, nguồn read-only; kiểm lưu bản sao và không mất bản gốc.
- [ ] A05 In/xem trước/PDF export: trang đúng, lề đúng, chọn trang, PDF chữ Việt, máy in thật; test từ từng công cụ.
- [ ] A06 UI desktop: 800x600, 1100x700, 1366x768, 1920x1080, DPI; panel trái/phải, menu ribbon, tương phản, không tràn chữ, bàn phím. Ảnh CI không thay test tay.
- [ ] A07 FloatMenu 4 công cụ holo xếp dọc, thu nhỏ/ẩn/restore, mở đúng tab, nhiều màn hình, taskbar native. #25 merged; #37 đang CI, chưa test native.
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
- [ ] X06 Biểu đồ thật lưu XLSX, chỉnh nguồn/loại/tiêu đề; chưa xác minh XML và GUI. Không coi tổng hợp bảng là chart.
- [ ] X07 PivotTable thật tạo/sửa/refresh, lưu và mở lại XLSX; #36 chỉ tổng hợp đọc dữ liệu, không phải PivotTable.
- [ ] X08 CSV, làm sạch/trùng/đối soát, mã dài/0 đầu, chuyển số thành chữ, xuất theo mẫu. #23/#32/#34 merged, cần test dữ liệu thật.
- [ ] X09 MISA/KiotViet (ưu tiên), FAST/Sapo sau: import file xuất, chọn mẫu chính thức đúng phiên bản, mapping tái sử dụng, cảnh báo thiếu/thừa, xuất mới và nhập thử vào phần mềm đích; không tuyên bố tương thích nếu chưa thử.

## P1 — Excel nâng cao (không quên)
- [ ] X10 Power Query cơ bản: nguồn CSV/XLSX, chọn sheet, tiêu đề, đổi tên/chọn cột, lọc/sort, làm sạch, kiểu dữ liệu, join/group, xem trước, lưu quy tắc và chạy lại. Không gọi là Power Query M đầy đủ.
- [ ] X11 Power Query M/connection/query OOXML: bảo toàn phần chưa hỗ trợ; kiểm refresh và tương thích MS Excel, không xóa query.
- [ ] X12 VBA: kiểm mở/lưu XLSM giữ vbaProject.bin, chạy macro fixture vô hại khi có sự đồng ý, không bật macro tự động; VBA object model tương thích từng phần, không hứa 100%. Chưa có bằng chứng đầy đủ.
- [ ] X13 Macro ghi thao tác, quản lý macro, cảnh báo bảo mật và test tương thích; triển khai sau X12.

## P0 — PowerPoint
- [ ] P01 Chữ, ảnh, hình khối, kéo thả/resize/rotate, căn chỉnh, thứ tự lớp, chọn nhiều đối tượng, nhóm; kiểm PPTX roundtrip.
- [ ] P02 Thêm/xóa/nhân đôi/đổi thứ tự slide, layout, thumbnail, notes; test lưu.
- [ ] P03 Trình chiếu toàn màn hình/cửa sổ, phím tới/lui, thoát và về đúng slide, màn hình ngoài; chưa test native.
- [ ] P04 Transition + Slide Master: #12/#17 có cơ bản; cần kiểm timing khi chiếu, nhiều master/layout, theme, font và đối tượng master.
- [ ] P05 Animation: #28/#33 có Appear/Fade theo click, nhiều đối tượng, duration/delay/order. Nghiệm thu PPTX trong PowerPoint, kéo thả thật; không sửa timing có sẵn.
- [ ] P06 Animation nâng cao: With Previous/After Previous, exit/emphasis/motion path, timeline, chỉnh hiệu ứng cũ, preview thực sự. Chưa triển khai đầy đủ; phải có test XML + trình chiếu.

## P0 — PDF
- [ ] D01 Mở/đọc, zoom, trang, chọn chữ, tìm chữ, copy, preview, file có mật khẩu; cần kiểm chữ Việt, file scan. #39 đã push tìm lớp chữ, phạm vi trang, đoạn trích, đọc/copy; chờ CI, chưa highlight canvas.
- [ ] D02 Gộp/tách/xoay/nén/sắp xếp/xóa trang/ảnh PDF, số trang, watermark, xem trước; qpdf kiểm output và giữ nguồn.
- [ ] D03 Chỉnh realtime qua bản sao ODG, chữ/ảnh/hình, undo/redo, lưu ODG, xuất PDF, đối chiếu bản gốc. #15/#26/#27 merged, cần GUI, fidelity; không phải sửa stream PDF gốc.
- [ ] D04 Chỉnh sửa PDF gốc có cấu trúc: chọn đoạn/chữ, sửa trực tiếp, giữ font/layout và vector, không mất chữ ký/biểu mẫu/link ngoài ý muốn. Chưa hoàn thiện; không giả lập bằng lớp phủ rồi gọi là sửa gốc. Tách khỏi redaction bảo mật.
- [ ] D05 Lớp phủ chữ Việt Unicode và ảnh theo vị trí, preview thật; kiểm font nhúng/không mất nét. ASCII/ảnh đã có; Unicode chưa đầy đủ.
- [ ] D06 OCR tiếng Việt/Anh đa trang, dừng, TXT, searchable PDF (#22/#31 merged), kiểm layer chữ, chất lượng OCR, cài Tesseract/gói ngôn ngữ hợp pháp; bản raster không giữ vector/link, cảnh báo. Chưa test Windows GUI.
- [ ] D07 Form/annotation/highlight, chữ ký tài liệu, redaction đúng nghĩa; đánh giá khả thi, không quảng cáo đã có nếu chưa test.

## P0/P1 — hệ sinh thái miễn phí
- [ ] E01 Kho mẫu Word/Excel/PPT: kiểm từng mẫu thực mở/lưu, lọc đúng loại, nguồn/giấy phép, số lượng thực tế; phân nhóm doanh nghiệp/kế toán/nhân sự/cá nhân/học sinh/sinh viên. Không giả định đủ 100/170 mẫu.
- [ ] E02 Kho luật: nguồn chính thức, hiệu lực và văn bản thay thế, thời điểm cập nhật; không tự đưa nội dung pháp lý chưa xác minh.
- [ ] E03 Notes/Calendar/việc/liên hệ: lưu cục bộ, nhắc lịch, lọc, backup/restore, không mất JSON; panel phải #19 merged, chưa nghiệm thu.
- [ ] E04 Góp ý: form lớn, cam kết miễn phí/cập nhật trọn đời, email do chủ xác nhận, gửi qua mail client hoặc backend được phép. #29 merged; chưa có email nhận xác nhận, chưa kiểm gửi thật.
- [ ] E05 Hộp công cụ ngoài: kiểm từng tính năng, dependency, checksum, giấy phép, khi thiếu báo rõ, không chạy lệnh ngoài trái phép.
- [ ] E06 Hướng dẫn tiếng Việt, onboarding, lỗi rõ, hỗ trợ update, bộ cài ổn định; macOS sau Windows.

## Quy trình mỗi lượt
1. Đọc checklist và HEAD main + PR mở. Phân biệt chưa làm / có mã / CI đang chạy / CI đạt / merge / GUI đạt.
2. Ưu tiên blocker P0, nếu CI còn chạy thì chọn mục độc lập; không lặp lại báo cáo mà không hành động nếu còn việc khả thi. Chia nhánh theo nhóm, không force push.
3. Cập nhật bằng chứng tại mục tương ứng, link commit/PR/CI đúng SHA. Không đánh [x] chỉ vì merge.
4. Kiểm CI Windows/Linux trên head mới và bản tích hợp main, kiểm bộ cài đúng commit, test máy thật. Không rebuild engine trừ blocker thực, không phát hành khi chưa được phép.
5. Khi không có GUI/mẫu thật/email/engine phù hợp, ghi blocker cụ thể, không tự suy đoán. 
