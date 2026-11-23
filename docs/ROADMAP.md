# Lộ trình

Nguyên tắc: Excel đã dùng để chứng minh kiến trúc. **Ưu tiên do người dùng chốt (24/11/2026):**
1. Có bộ cài Windows đầu tiên (engine headless).
2. Dồn sức hoàn thiện **một** công cụ đến mức đầy đủ (Word, PowerPoint, hoặc PDF xem + sửa),
   chạy song song nhiều tác vụ trên các nhánh riêng, mỗi nhánh qua CI trước khi gộp.
3. Lần lượt các công cụ còn lại.
4. Excel: giữ mức cơ bản trong bộ cài đầu, nâng lên mức tốt ở giai đoạn cuối.

Một hạng mục chỉ được đánh dấu xong khi có kiểm thử tự động chạy trên Windows.

## Giai đoạn 0: Chứng minh kiến trúc (v0.1, hiện tại)
- [x] Engine LOK: mở/render/gõ/công thức/lưu, tên tệp tiếng Việt
- [x] Lưu an toàn: bản tạm, Lưu thành mặc định, sao lưu, ghi nguyên tử, chỉ đọc theo định dạng
- [x] Smoke test + so sánh round-trip XLSX, khác biệt đã biết được công khai
- [x] Giao diện Qt: Home, thanh bên, ribbon ngữ cảnh, thẻ tài liệu, thanh công thức, sheet, status bar, kéo thả, gần đây
- [x] Build Windows (MSVC, Qt 6.8); engine 26.8.3 chính thức khởi động và mở XLSX trên Windows
- [ ] **Engine Windows tự build với `--enable-headless`** (bản chính thức không render tile được — xem WINDOWS_ENGINE.md)
- [ ] CI Windows xanh với engine tự build → bộ cài OneClick đầu tiên
- [ ] Ảnh chụp UI trên Windows được xem xét, sửa lỗi hiển thị

## Giai đoạn 1: Excel dùng được hằng ngày
- [ ] Chọn cả hàng/cột bằng tiêu đề; kéo đổi độ rộng cột/chiều cao hàng
- [ ] Menu chuột phải (cắt/sao chép/dán, chèn/xoá, định dạng ô)
- [ ] Hộp thoại Định dạng ô của Dũng Office (số, căn lề, viền, tô màu), không dùng hộp thoại LibreOffice
- [ ] Gợi ý hàm khi gõ công thức (`completeFunction`), tô màu tham chiếu
- [ ] Thêm/đổi tên/xoá/di chuyển sheet
- [ ] Tìm & thay thế
- [ ] Render tile bất đồng bộ (luồng nền) để cuộn mượt với bảng lớn
- [ ] Mở rộng round-trip: biểu đồ, định dạng có điều kiện, validation, defined names, ảnh, pivot (chỉ đọc nếu chưa đạt)
- [ ] Sửa DS-001, DS-002 (bản vá module `sc` hoặc xác nhận đã hết trên 26.8.x)
- [ ] Tệp có mật khẩu: hỏi mật khẩu bằng hộp thoại Dũng Office (`LOK_FEATURE_DOCUMENT_PASSWORD`)
- [ ] Tự lưu bản khôi phục (vào thư mục khôi phục, không bao giờ vào tệp người dùng)

## Giai đoạn 2: Word, PowerPoint, PDF
- [ ] Word: kiểu đoạn (Heading 1–3), phông/cỡ chữ, bảng, ảnh, đếm từ/trang, round-trip DOCX
- [ ] PowerPoint: hình thu nhỏ slide thật (render part), thêm/xoá/sắp xếp slide, trình chiếu toàn màn hình
- [ ] PDF: xem nhiều trang mượt, tìm kiếm, in. Chỉnh sửa PDF chỉ khi có kiểm chứng bố cục, mặc định luôn là bản sao

## Giai đoạn 3: Engine gọn (tổ chức lại module)
- [ ] Build LibreOffice tuỳ biến trên CI tự host: `--disable-java --disable-database-connectivity --disable-extensions
      --disable-scripting --without-help --without-myspell-dicts --disable-gui`(bỏ VCL Win32/GTK, giữ `svp` headless) …
- [ ] Bỏ module không dùng: Base (`dbaccess`), Math (`starmath`), `basctl`, `wizards`, `avmedia`, `extensions`
- [ ] Mục tiêu: runtime < 200 MB, khởi động < 2 s; mọi lần thu gọn phải giữ nguyên kết quả round-trip
- [ ] Công bố mã nguồn/bản vá tương ứng (nghĩa vụ MPL-2.0 với tệp đã sửa)
