# Giới hạn đã đo khi mở–lưu PPTX

Đo bằng LibreOffice desktop (cùng lõi với engine) trên 3 mẫu: 2 mẫu thư viện (phông Aptos) và `mau-trinh-chieu.pptx`.

| Mục | Kết quả đo |
|---|---|
| Số slide | giữ nguyên (6→6, 6→6, 2→2) |
| Chữ trên từng slide | giữ nguyên (so sánh theo từng slide) |
| Slide master | **1 master có 11 bố cục → 11 master, mỗi master 1 bố cục** (LibreOffice gộp mỗi bố cục thành master riêng) |
| Phông Aptos | không có trong engine; thay bằng Carlito (số đo gần giống, ngắt dòng có thể lệch) |

Hệ quả: hiển thị slide không đổi, nhưng nếu sửa "trang cái" trong PowerPoint sau khi lưu từ Dũng Office thì phải sửa
nhiều master thay vì một. Đây là hành vi của engine, chưa có cách sửa ở ứng dụng. Chưa đo mẫu có nhiều master gốc,
slide ẩn, hay hiệu ứng chuyển cảnh; thêm mẫu thật trước khi kết luận cho các trường hợp đó.

Về dòng "phủ chữ 46,8%" trong log Windows: đó là biến thể đối chứng v5 (Always=false), cố ý đo để chứng minh cấu hình đó
KHÔNG thay được phông. Cấu hình đang dùng (Always=true, Carlito) đạt 100% ở cả 2 mẫu.
