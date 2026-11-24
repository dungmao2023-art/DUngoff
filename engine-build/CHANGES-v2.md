# Engine v2 (engine-26.8.3.2-headless): thay đổi so với v1

Người dùng đã chốt build lại engine một lần (25/11/2026). Tag mới để KHÔNG ghi đè engine cũ đang dùng được
(`engine-26.8.3.1-headless`); ứng dụng chỉ chuyển sang v2 khi v2 qua bộ kiểm thử.

## Cờ configure (autogen.input)

| Dòng | Thay đổi | Căn cứ / lý do |
|---|---|---|
| `--without-myspell-dicts` | **bỏ** | Cần từ điển Hunspell tiếng Việt cho chính tả. Từ điển là phần "dictionaries" của LibreOffice (submodule). |
| `--with-myspell-dicts` | **thêm** | configure.ac (26.8.3.1): "Adds myspell dictionaries to the LibreOffice installation set". |
| `--with-lang=vi` | **thêm** | configure.ac: "build LibreOffice with additional UI language support. English (US) is always included". Cho nhãn tiếng Việt ở hộp thoại/thông báo của engine. |
| `--enable-release-build` | **thêm** | configure.ac: nếu không bật, tên sản phẩm có hậu tố "Dev" (PRODUCTNAME="...Dev"). Bản phát hành nên là release build. Tác dụng phụ đã biết: đổi tên sản phẩm; ứng dụng dùng thư mục hồ sơ riêng nên không ảnh hưởng. |
| `--enable-python=fully-internal` | giữ | Cung cấp Python nội bộ + pyuno + script provider; cần cho `dung_api.py`. |
| Basic | giữ (mặc định bật) | `--disable-scripting` (tắt Basic/Python) KHÔNG được dùng. |

Không cắt module trong lượt này: chưa kiểm chứng được cờ `--disable-*` nào an toàn mà có lợi rõ về dung lượng
(không đoán cờ; có thể làm ở lượt sau khi đã đọc configure.ac đầy đủ).

## Bước thêm trong workflow (engine-windows.yml)
1. **Lấy submodule** `translations` và `dictionaries` nếu bản mã nguồn dùng submodule (kiểm `.gitmodules`; ghi log).
2. **Sau build, trước đóng gói** (không chặn phát hành nếu lỗi — người dùng sẽ thấy trong báo cáo):
   - Lọc từ điển: chỉ giữ `dict-vi` và `dict-en` (tránh phình dung lượng), ghi danh sách đã bỏ.
   - Sao chép README/LICENSE của từ điển tiếng Việt vào `LICENSES-dung/` (giấy phép từ điển cần xác minh theo tệp đó).
   - Cài `share/Scripts/python/dung_api.py` (đặt lề trang, bật/tắt đầu-chân trang, tiêu đề tài liệu qua API UNO).
   - Tự kiểm: chạy engine, kết nối UNO, gọi `dung_api` trực tiếp VÀ qua Python script provider theo URL
     `vnd.sun.star.script:dung_api.py$margins_vn?language=Python&location=share`, lưu .docx và đọc `w:pgMar`
     (trái 30 mm = 1701 twip). Kết quả: `dung-api-test.txt` trong báo cáo `ci-report-engine`.

## An toàn
- App KHÔNG chạy macro trong tệp người dùng. `dung_api.py` nằm ngoài tài liệu, chỉ app gọi.
- Quy tắc build giữ nguyên: tối đa 3 lượt tự động, checkpoint SHA-256, dừng khi lỗi tài nguyên/biên dịch,
  log từng lượt (nhánh `ci-report-engine`), không chạy thêm lượt âm thầm.

## Chưa biết (sẽ rõ sau lượt build)
- Từ điển tiếng Việt có trong bản `dictionaries` của LO 26.8.3.1 và giấy phép đúng của nó (xem `dict-vi-files.txt`, `LICENSES-dung/`).
- `--with-lang=vi` có buộc tải `translations` làm tăng thời gian build đáng kể không.
- Script provider Python chạy được từ engine headless Windows không (bài tự kiểm trả lời).
