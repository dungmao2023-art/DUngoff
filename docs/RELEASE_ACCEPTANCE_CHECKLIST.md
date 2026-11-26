# Dũng Office — checklist nghiệm thu duy nhất

Mốc rà soát 26/11/2026: main 58ee8c5384cef3a274de4352effe730f7f3d9436. Đây là backlog sống, không phải tuyên bố hoàn thiện. Đọc file này trước mỗi lượt làm; đối chiếu HEAD, PR và CI mới nhất, không sao chép báo cáo cũ. Mỗi mục chỉ đánh dấu [x] khi đạt tiêu chí nghiệm thu ghi ngay bên cạnh (có link PR, SHA, CI, hoặc bằng chứng test máy thật). "Có mã" / "CI xanh" không đồng nghĩa "GUI đã nghiệm thu". P0 = bắt buộc trước bản beta ổn định, P1 = mở rộng sau P0 hoặc triển khai độc lập; không bắt buộc tương đương 100% MS Office. Không phát hành công khai khi chưa được chủ dự án đồng ý.

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
- [ ] X06 Biểu đồ thật lưu XLSX, chỉnh nguồn/loại/tiêu đề; chưa xác minh XML và GUI. Không coi tổng hợp bảng là chart.
- [ ] X07 PivotTable thật tạo/sửa/refresh, lưu và mở lại XLSX; #36 merged, chỉ tổng hợp đọc dữ liệu, không phải PivotTable.
- [ ] X08 CSV, làm sạch/trùng/đối soát, mã dài/0 đầu, chuyển số thành chữ, xuất theo mẫu. #23/#32/#34 merged, cần test dữ liệu thật.
- [ ] X09 MISA/KiotViet (ưu tiên), FAST/Sapo sau: import file xuất, chọn mẫu chính thức đúng phiên bản, mapping tái sử dụng, cảnh báo thiếu/thừa, xuất mới và nhập thử vào phần mềm đích; không tuyên bố tương thích nếu chưa thử.

## P1 — Excel nâng cao (không quên)
- [ ] X10 Power Query cơ bản: nguồn CSV/XLSX, chọn sheet, tiêu đề, đổi tên/chọn cột, lọc/sort, làm sạch, kiểu dữ liệu, join/group, xem trước, lưu quy tắc và chạy lại. Không gọi là Power Query M đầy đủ.
- [ ] X11 Power Query M/connection/query OOXML: bảo toàn phần chưa hỗ trợ; kiểm refresh và tương thích MS Excel, không xóa query. #41 đang CI, chỉ cảnh báo nguy cơ mất connections/queryTable, chưa bảo toàn query.
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
- [ ] D01 Mở/đọc, zoom, trang, chọn chữ, tìm chữ, copy, preview, file có mật khẩu; cần kiểm chữ Việt, file scan. #39 đã push tìm lớp chữ, phạm vi trang, đoạn trích, đọc/copy; đã vá lỗi icon/fixture, chờ CI mới, chưa highlight canvas.
- [ ] D02 Gộp/tách/xoay/nén/sắp xếp/xóa trang/ảnh PDF, số trang, watermark, xem trước; qpdf kiểm output và giữ nguồn.
- [ ] D03 Chỉnh realtime qua bản sao ODG, chữ/ảnh/hình, undo/redo, lưu ODG, xuất PDF, đối chiếu bản gốc. #15/#26/#27 merged, cần GUI, fidelity; không phải sửa stream PDF gốc.
- [ ] D04 Chỉnh sửa PDF gốc có cấu trúc: chọn đoạn/chữ, sửa trực tiếp, giữ font/layout và vector, không mất chữ ký/biểu mẫu/link ngoài ý muốn. Chưa hoàn thiện; không giả lập bằng lớp phủ rồi gọi là sửa gốc. Tách khỏi redaction bảo mật.
- [ ] D05 Lớp phủ chữ Việt Unicode và ảnh theo vị trí, preview thật; kiểm font nhúng/không mất nét. ASCII/ảnh đã có; Unicode chưa đầy đủ.
- [ ] D06 OCR tiếng Việt/Anh đa trang, dừng, TXT, searchable PDF (#22/#31 merged), kiểm layer chữ, chất lượng OCR, cài Tesseract/gói ngôn ngữ hợp pháp; bản raster không giữ vector/link, cảnh báo. #40 đang CI (runtime discovery), chưa đóng gói Tesseract; chưa test Windows GUI.
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

Mốc dự toán đầu tiên: 48 nhóm, chỉ số triển khai 29%, P0 32%, P1 13%; chưa có nhóm nào được ghi nhận nghiệm thu đủ toàn bộ tiêu chí. 391–1188 giờ công còn lại (P0 315–956, P1 76–232). Cập nhật CSV khi mã/CI/GUI thay đổi, giảm giờ chỉ khi có bằng chứng; nếu phát hiện blocker thì tăng giờ và ghi lý do. Không dùng số PR/commit làm phần trăm.

## Quy trình mỗi lượt
1. Đọc checklist và HEAD main + PR mở. Phân biệt chưa làm / có mã / CI đang chạy / CI đạt / merge / GUI đạt.
2. Ưu tiên blocker P0, nếu CI còn chạy thì chọn mục độc lập; không lặp lại báo cáo mà không hành động nếu còn việc khả thi. Chia nhánh theo nhóm, không force push.
3. Cập nhật bằng chứng tại mục tương ứng, link commit/PR/CI đúng SHA. Không đánh [x] chỉ vì merge.
4. Kiểm CI Windows/Linux trên head mới và bản tích hợp main, kiểm bộ cài đúng commit, test máy thật. Không rebuild engine trừ blocker thực, không phát hành khi chưa được phép.
5. Khi không có GUI/mẫu thật/email/engine phù hợp, ghi blocker cụ thể, không tự suy đoán. 
