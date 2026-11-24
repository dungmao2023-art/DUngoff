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

## Quy trình quyết định
1. Chờ ba thí nghiệm (cầu nối macro, ma trận lệnh, toàn vẹn tính năng).
2. Điền bảng trên; chỉ các mục "Chỉ build lại nếu" đã thành sự thật mới đưa vào patch/cấu hình.
3. Chuẩn bị patch trong `engine-build/overrides/`, kiểm tra cú pháp ở CI nhẹ trước khi bấm build.
4. Build tối đa 3 lượt, mỗi lượt có log; kiểm lại bằng bộ kiểm thử engine hiện có (word/excel/ppt/pdf, fidelity 158 mẫu, input).
