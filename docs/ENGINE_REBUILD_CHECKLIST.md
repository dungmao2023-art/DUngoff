# V2 — kiểm lại bản phát hành đã có (28/11/2026)

- Main hiện ghim `engine-26.8.3.2-headless`, SHA256 `cb7664001dfb51a31467f2b1849e294e6564d607a91baea5c4efa215010ad492`. V1 giữ nguyên bản phát hành và checksum riêng.
- Port công cụ độc lập từ #160: [preflight PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/46090130724), [CI PASS](https://github.com/dungmao2023-art/DUngoff/actions/runs/46090130758). Chỉ nhận V2 đúng checksum đã ghim, không tải bản thay thế không xác minh. Backup V1 trên runner là artifact 30 ngày, không thay thế bản phát hành gốc hoặc backup dài hạn của tác giả.
- Probe Python UNO chạy trong tiến trình/profile riêng, KHÔNG chứng minh chỉnh sửa tài liệu đang mở qua LOK. Không dùng kết quả đó để bật tính năng UI chưa kiểm chứng. Native transform trên Windows vẫn cần kiểm thử riêng (#183/#184). Không quyết định rebuild từ kết quả Linux.
- Word #182 Windows/Linux CI PASS (46112460256), đã merge. Không thay đổi mức nghiệm thu GUI.

---

# Danh sách kiểm trước khi build lại engine (một lần)

Quyết định của tác giả (25/11/2026): build lại engine **một lần**, và lần này phải gỡ được các vấn đề
đã biết. Tài liệu này là danh sách chốt trước khi bấm build. Mỗi mục ghi rõ: đã có bằng chứng hay chưa.

Quy tắc build (không đổi): tối đa 3 lượt tự động, checkpoint có SHA-256, dừng khi lỗi tài nguyên/biên dịch,
log từng lượt, không chạy thêm lượt âm thầm. Thời gian tham khảo từ các lần trước: 1,5–3 giờ nếu thuận lợi.

## Cấu hình hiện tại (engine-build/autogen.input)
`--enable-headless --enable-python=fully-internal --enable-mergelibs`, tắt: Java, Help, myspell dicts,
Base/Firebird, report builder, BeanShell/JavaScript scripting, galleries.

## Mục cần quyết (điền kết quả thí nghiệm vào cột "Bằng chứng")

| # | Nhu cầu | Cách giải không build lại | Chỉ build lại nếu | Bằng chứng |
|---|---|---|---|---|
| 1 | Đặt lề trang, đầu/chân trang, kiểu trang, thuộc tính tài liệu | Cầu nối macro Basic/Python (thí nghiệm exp/basic-bridge) | Không cầu nối nào chạy: vá LOK để lộ API UNO | chờ |
| 2 | Lệnh cần hộp thoại (Pivot, định dạng ô, đoạn văn, biểu đồ…) | Dựng hộp thoại Qt từ JSON dialog của LOK (nếu engine đã phát) | Engine không phát JSON dialog; bật/vá | chờ (exp/command-matrix) |
| 3 | Nhãn tiếng Việt trong hộp thoại/thông báo engine | — | Cần `--with-lang=vi` (chỉ khi dùng JSON dialog hoặc hiện thông báo engine) | chờ |
| 4 | Chữ đang soạn Telex (preedit) | `postExtTextInputEvent` của LOK | API không dùng được | chờ |
| 5 | Engine không phát INVALIDATE_TILES / CURSOR_VISIBLE (Windows, Writer) | Đã có cách tránh SYNTH ở app | Muốn gốc rễ: vá backend svp (không bắt buộc) | đã vá ở app |
| 6 | Chính tả tiếng Việt | Thêm từ điển Hunspell vi làm dữ liệu (kiểm giấy phép) | Không cần | chưa làm |
| 7 | Đường dẫn dài ≥250 ký tự | Vá ở app (thư mục tạm ngắn, tiền tố `\\?\`) | Cách vá app không chạy: manifest longPathAware cho engine | đang làm (feat/recover) |
| 8 | Tính năng mất khi lưu (track changes, bình luận, Pivot, SmartArt…) | Cảnh báo/chỉ đọc ở app | Mất tính năng quan trọng do lỗi engine | chờ (exp/feature-fidelity) |
| 9 | Dung lượng bộ cài (184 MB) | Bỏ module không dùng khi build (Math, Base, Draw?) | — (tuỳ chọn) | chưa đo |

## Cập nhật 25/11/2026 (người dùng chốt build, không chờ hết thí nghiệm)
Build v2 (tag `engine-26.8.3.2-headless`) gồm: `--with-lang=vi`, từ điển Hunspell vi, `--enable-release-build`,
giữ Python nội bộ, thư viện script `dung_api.py` (đặt lề/đầu-chân trang qua UNO) kèm bài tự kiểm. Chi tiết: `engine-build/CHANGES-v2.md`.
Không cắt module trong lượt này (chưa kiểm chứng cờ an toàn). Mục 2 (JSON dialog) và 4 (preedit) giải ở app nếu engine đã hỗ trợ.

## Quy trình quyết định
1. Chờ ba thí nghiệm (cầu nối macro, ma trận lệnh, toàn vẹn tính năng).
2. Điền bảng trên; chỉ các mục "Chỉ build lại nếu" đã thành sự thật mới đưa vào patch/cấu hình.
3. Chuẩn bị patch trong `engine-build/overrides/`, kiểm tra cú pháp ở CI nhẹ trước khi bấm build.
4. Build tối đa 3 lượt, mỗi lượt có log; kiểm lại bằng bộ kiểm thử engine hiện có (word/excel/ppt/pdf, fidelity 158 mẫu, input).


## V2 — bảo vệ ABI khi đọc thông tin slide (28/11/2026)
- Nhánh kế nhiệm cho phần ABI độc lập của #160, trên main b73a9c67f1f9087bf59aedc19a8ea844d44f0a47. Preflight 46090130724 head e4b6077feae0fdeefac6ea82fa45db929a4c0202: log xác nhận Writer split/TOC, Calc validation/protection/Pivot, Impress group và standalone DUNG_API PASS. Chưa chứng minh live LOK bridge; không chuyển pin ngược về v1.
- Mã/test 23703d14f7dfb61ad65656061e476ef5f8116d04: kiểm kích thước ABI bảng hàm bằng LIBREOFFICEKIT_DOCUMENT_HAS, con trỏ hàm và chỉ số part trước khi đọc hash/info; không xóa slide nếu thiếu bằng chứng nhận diện. Bỏ ca kiểm luôn true, thêm kiểm chỉ số ngoài miền và không đổi trang. Windows/Linux CI và GUI chưa xác minh. Đây không phải native UNO bridge hay animation playback; không build v3, không tăng điểm.
