# Build engine LibreOffice headless cho Windows

`autogen.input` là cấu hình build engine dùng bởi `.github/workflows/engine-windows.yml`.

- `--enable-headless`: bật backend VCL headless (svp) để LibreOfficeKit render tile vào giao diện Dũng Office.
- Tắt những phần không dùng: Java, trợ giúp, từ điển, Skia (headless dùng cairo), cập nhật trực tuyến, kiểm tra CVE…
- Không sửa mã nguồn LibreOffice. Mọi bản vá sau này (nếu có) đặt trong thư mục `patches/` và được công bố
  theo MPL-2.0.

Kết quả: Release `engine-<phiên bản>-headless` trong repo, tệp `dung-engine-headless-win64.7z`.
