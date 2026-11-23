# LibreOfficeKit headers (vendored, unmodified)

- Nguồn: https://github.com/LibreOffice/core (mirror chính thức của git.libreoffice.org/core)
- Tag: `libreoffice-26.8.2.2`
- Commit: `9dea406ab7695df6325b11b0e19fdca8db4b7276`
- Thư mục: `include/LibreOfficeKit/` (bỏ `LibreOfficeKitGtk.h` vì không dùng GTK)
- Giấy phép: Mozilla Public License 2.0 (xem đầu mỗi file)

Các header này là header-only (C API + wrapper C++ inline). Dũng Office chỉ gọi
những hàm có từ LibreOffice 6.x trở đi, nên chạy được với runtime LibreOffice 24.2+.
Không gọi các hàm mới hơn runtime đang dùng (struct LOK không tự kiểm tra kích thước).
