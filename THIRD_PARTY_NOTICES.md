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


## Từ điển chính tả tiếng Việt (thành phần tuỳ chọn, GPL)

**Đã xác minh giấy phép** (thay cho ghi chú "chưa xác minh" trước đây). Từ điển KHÔNG phải mã của Dũng Office và KHÔNG nằm trong tệp chạy hay engine.

- **Là gì:** từ điển Hunspell tiếng Việt `vi-DauMoi` (đặt dấu kiểu mới: hoà, thuỷ) và `vi-DauCu` (kiểu cũ: hòa, thủy) — các tệp `.dic` + `.aff`, gồm các âm tiết đơn (tiếng Việt viết tách từng âm tiết).
- **Tác giả:** danh sách từ gốc của Hồ Ngọc Đức ("Dự án Từ điển tiếng Việt miễn phí", gói GNU Aspell 0.60 Vietnamese Dictionary Package 0.01.1-1, 2004); chuyển sang Hunspell bởi László Németh; sửa luật và đóng gói bởi Ivan Garcia (2007) và Minh Nguyen / Nguyễn Xuân Minh (kho hunspell-vi).
- **Nguồn:** https://github.com/1ec5/hunspell-vi, commit `39cc647f0cf3bdfc579980a2c3aac3f6f4203fe6`. Các tệp được tải về lúc đóng gói bằng `scripts/fetch-dict-vi.ps1`, kiểm SHA-256 từng tệp, **không sửa đổi**.
- **Giấy phép:** GNU GPL. README của kho gốc ghi danh sách từ gốc theo "GPLv2" (không ghi "hoặc bản mới hơn"); phần đóng gói của hunspell-vi ghi GPL phiên bản 3 hoặc mới hơn (tệp `firefox_thunderbird/LICENSE`) hoặc GPLv3 (`openoffice/3.x/LICENSES-en.txt`). Các nguồn công bố khác nhau về phiên bản; mình chưa đọc được tệp `Copyright` gốc của gói Aspell (máy build không truy cập được ftp.gnu.org). Vì vậy bộ cài giữ nguyên văn cả hai giấy phép (`GPL-2.0.txt`, `GPL-3.0.txt`) và các tệp thông báo gốc, và chỉ phân phối từ điển như dữ liệu riêng.
- **Cách phân phối (để tuân thủ GPL):** thành phần riêng "Từ điển chính tả tiếng Việt (GPL v2)" trong bộ cài, chọn sẵn nhưng bỏ chọn được; cài vào `dictionaries\vi` (thư mục riêng cạnh ứng dụng); có trang giấy phép + liên kết nguồn khi cài; chính các tệp `.dic/.aff` là mã nguồn kèm theo; văn bản GPL và `NOTICE-vi.txt` nằm cùng thư mục. Dũng Office (MPL-2.0) không liên kết vào từ điển này — chỉ được phân phối chung trong một bộ cài ("tập hợp đơn thuần").
- **Thay hoặc gỡ:** bỏ chọn khi cài, hoặc xoá thư mục `dictionaries\vi`; ứng dụng vẫn chạy.
- **Engine:** bản dựng engine có thể kèm sẵn từ điển tiếng Việt của LibreOffice (cấu hình `--with-myspell-dicts`, mục `dict-vi`); bước đóng gói `scripts/package-windows.ps1` **gỡ nó khỏi thư mục engine** trước khi tạo bộ cài (ghi danh sách trong `dict-vi-removed-from-engine.txt`), để dữ liệu GPL chỉ có mặt ở thành phần riêng nói trên.
- **Hiện trạng:** từ điển chưa được nối vào giao diện kiểm tra chính tả. Mozilla đã gỡ từ điển tiếng Việt khỏi cây mã của họ vì giấy phép không tương thích với chính sách giấy phép của họ; không tìm thấy từ điển tiếng Việt nào có giấy phép dễ dãi hơn.

## Kiểm tra chính tả theo âm tiết (mã của dự án)

`app/src/vntext/SyllableCheck.*` do dự án tự viết (MPL-2.0), dùng luật âm vị tiếng Việt, **không** dùng từ điển hay bảng từ của bên thứ ba. Chỉ bắt âm tiết không tồn tại (ví dụ "tòan", "ngĩa", "viêc"); **không** bắt sai từ (ví dụ "xử dụng" khác "sử dụng", vì cả hai đều gồm âm tiết hợp lệ).

## Qt PDF (xem trước in và in)
- Mô-đun Qt PDF của Qt Project / The Qt Company, giấy phép GNU LGPL phiên bản 3 (hoặc GPL phiên bản 2, hoặc giấy phép thương mại) — theo trang giấy phép chính thức https://doc.qt.io/qt-6/qtpdf-index.html. Dũng Office dùng bản LGPL, liên kết động (DLL kèm theo bộ cài), không sửa mã Qt.
- Mô-đun này đi kèm bộ vẽ PDF PDFium cùng các thành phần bên thứ ba của nó; văn bản giấy phép của các thành phần đó nằm trong tài liệu giấy phép của Qt (https://doc.qt.io/qt-6/qtpdf-attribution.html). Mình chưa đối chiếu từng thành phần: cần rà lại trước khi phát hành chính thức.

## PDFium Editing SDK (tùy chọn)
- PDFium upstream: https://pdfium.googlesource.com/pdfium/ (BSD-style và các giấy phép thành phần đi kèm).
- SDK nhị phân: https://github.com/bblanchon/pdfium-binaries, bản không V8 `chromium/8179`, build bên thứ ba. CI kiểm tra release bất biến, SHA-256 asset từ GitHub trước khi giải nén. DLL liên kết động, không sửa mã thư viện.
- Gói Windows có SDK phải kèm `pdfium-notices/verified-source.json` và toàn bộ các tệp giấy phép/notice phát hiện trong archive. Cần rà giấy phép thành phần trước phát hành công khai.
