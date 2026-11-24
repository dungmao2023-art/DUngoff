# Thông báo bên thứ ba

## LibreOffice (runtime engine + header LibreOfficeKit)
- Bản quyền: The Document Foundation và các cộng tác viên LibreOffice
- Giấy phép: Mozilla Public License 2.0 (một số thành phần: LGPLv3+, Apache-2.0 và các giấy phép tương thích khác, liệt kê trong `engine/readmes/` và `engine/license/` của bộ cài)
- Nguồn: https://www.libreoffice.org/ — mã nguồn: https://download.documentfoundation.org/libreoffice/src/ và https://git.libreoffice.org/core
- Dũng Office phân phối **nguyên bản** bản phát hành chính thức (chưa sửa đổi mã). Header LibreOfficeKit
  trong `third_party/libreofficekit/` được sao chép nguyên vẹn từ tag `libreoffice-26.8.2.2`.
- "LibreOffice" là nhãn hiệu đã đăng ký của The Document Foundation. Dũng Office là dự án độc lập,
  không liên kết hay được bảo trợ bởi The Document Foundation.

## Qt 6
- Bản quyền: The Qt Company Ltd. và các cộng tác viên
- Giấy phép: GNU LGPL v3 (liên kết động; các DLL Qt có thể được người dùng thay thế)
- Nguồn: https://www.qt.io/ — mã nguồn: https://download.qt.io/official_releases/qt/

## Microsoft Visual C++ Runtime
- Phân phối lại theo điều khoản Visual Studio (redistributable files).

## Inno Setup (chỉ dùng khi tạo bộ cài)
- Giấy phép Inno Setup: https://jrsoftware.org/files/is/license.txt

## qpdf (Công cụ PDF: gộp, tách, xoay, nén)
- Bản quyền: Jay Berkenbilt và các cộng tác viên
- Giấy phép: Apache License 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
- Nguồn: https://github.com/qpdf/qpdf — Dũng Office chỉ gọi chương trình qpdf nguyên bản qua dòng lệnh,
  đặt trong thư mục `qpdf\` cạnh DungOffice.exe.

## Phông chữ đi kèm engine (thay thế Aptos và các phông Microsoft không có trong engine)
- Dũng Office KHÔNG tự phân phối thêm phông nào ngoài các phông đã nằm trong bản phát hành chính thức của LibreOffice
  (thư mục `engine\share\fonts\truetype`). Hai phông được dùng để thay thế khi máy không có phông gốc:
  - **Carlito** (tương thích số đo với Calibri) — giấy phép SIL Open Font License 1.1, Bản quyền Łukasz Dziedzic / Google (dựa trên Lato).
  - **Liberation Sans** — giấy phép SIL Open Font License 1.1, Bản quyền Red Hat, Inc.
  - Nguồn: bản phân phối LibreOffice (https://www.libreoffice.org/) — danh sách và văn bản giấy phép nằm trong `engine\license` của bộ cài.
- Bảng thay thế (Aptos, Aptos Display, Aptos Narrow, Aptos SemiBold, Aptos Light, Aptos Black → Carlito; Liberation Sans không được dùng làm phông dự phòng vì engine bỏ qua bảng khi ghi danh sách "A;B") được ghi vào hồ sơ engine khi chạy và áp dụng cho mọi máy (kể cả máy có Aptos): CI Windows cho thấy chế độ "chỉ khi thiếu phông" không có tác dụng trên engine này.
- "Aptos" là tên phông của Microsoft; Dũng Office không phân phối phông Aptos.

## Qt PDF (xem trước in và in)
- Mô-đun Qt PDF của Qt Project / The Qt Company, giấy phép GNU LGPL phiên bản 3 (hoặc GPL phiên bản 2, hoặc giấy phép thương mại) — theo trang giấy phép chính thức https://doc.qt.io/qt-6/qtpdf-index.html. Dũng Office dùng bản LGPL, liên kết động (DLL kèm theo bộ cài), không sửa mã Qt.
- Mô-đun này đi kèm bộ vẽ PDF PDFium cùng các thành phần bên thứ ba của nó; văn bản giấy phép của các thành phần đó nằm trong tài liệu giấy phép của Qt (https://doc.qt.io/qt-6/qtpdf-attribution.html). Mình chưa đối chiếu từng thành phần: cần rà lại trước khi phát hành chính thức.
