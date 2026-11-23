# Kiến trúc Dũng Office

## 1. Vì sao dùng LibreOfficeKit thay vì "bóc" từng thư mục mã nguồn

Mã nguồn LibreOffice (~10 triệu dòng) có các module xử lý tài liệu (`sc`, `sw`, `sd`, `oox`,
`filter`, `editeng`, `svx`, `vcl`…) gắn chặt với nhau qua UNO, SFX2 và VCL. Ngay cả "Calc core"
cũng phụ thuộc VCL để đo chữ và render. Tách thủ công `sc/` ra khỏi phần còn lại là việc nhiều
người-năm và sẽ phải tự bảo trì một nhánh rẽ khổng lồ.

**LibreOfficeKit (LOK)** là lớp nhúng chính thức do chính dự án LibreOffice duy trì (Collabora
Online và các ứng dụng di động dùng nó). Ở chế độ LOK:

- LibreOffice chạy **headless** (backend VCL `svp`): không cửa sổ, menu, toolbar hay StartCenter.
- Ứng dụng nhúng gọi API C: `documentLoad`, `paintTile`, `postKeyEvent`, `postMouseEvent`,
  `postUnoCommand`, `getCommandValues`, `saveAs`… và nhận sự kiện qua callback (con trỏ ô,
  vùng chọn, nội dung ô, trạng thái nút, vùng cần vẽ lại…).
- Mọi phần nhìn thấy được (khung, ribbon, thanh công thức, tiêu đề hàng/cột, con trỏ, vùng chọn,
  danh sách sheet/slide) **do ứng dụng nhúng tự vẽ**.

Vì vậy "bỏ phụ thuộc UI cũ" được thực hiện ở ranh giới LOK. Việc thu gọn/tổ chức lại module
diễn ra theo giai đoạn (xem ROADMAP): build tuỳ biến với `--disable-*` để bỏ Base, Math, Java,
help, giao diện GTK/Win32…, nhưng chỉ khi kiểm thử round-trip chứng minh không ảnh hưởng.

## 2. Các tầng

| Tầng | Thư mục | Vai trò |
|---|---|---|
| Giao diện | `app/` | Qt 6 Widgets. `MainWindow` (thanh bên, stack Home/Tài liệu, status bar), `Ribbon` (tab/nhóm/nút theo loại tài liệu), `DocumentTab` (băng cảnh báo, thanh công thức, sheet/slide, Lưu thành), `TileView` (vẽ tile 256px, lớp phủ, tiêu đề hàng/cột, chuột/phím/IME). |
| Engine | `engine/` | `dung::Engine` (khởi tạo LOK một lần, hồ sơ riêng, thư mục tạm), `dung::Document` (bản làm việc tạm, phần/sheet, render, nhập liệu, lệnh, sự kiện, clipboard, `saveAs` an toàn), `FormatPolicy` (định dạng nào ghi được / chỉ đọc / chỉ xuất). Không phụ thuộc Qt nên kiểm thử được bằng CLI. |
| Runtime | `engine/program` khi đóng gói | Bản phát hành LibreOffice **chính thức, không sửa đổi** (Windows x64 MSI, giải nén bằng administrative install). |

## 3. Luồng chính

**Mở tệp:** sao chép tệp gốc sang `%LOCALAPPDATA%\DungOffice\work\<pid>\doc-N-xxxx\` → `documentLoad(bản tạm, "Batch=true[,tuỳ chọn CSV]")`.
`Batch=true` bắt engine chạy chế độ im lặng: không hộp thoại cũ nào được hiện (nếu hiện sẽ treo vì không có giao diện LibreOffice).
Điều này đã được kiểm chứng: nhập CSV không có `Batch=true` làm treo tiến trình ở hộp thoại "Text Import".

**Hiển thị:** `TileView` tính các tile nhìn thấy → `paintTile` (BGRA premultiplied = `QImage::Format_ARGB32_Premultiplied`)
→ bộ đệm tile; callback `INVALIDATE_TILES` xoá đúng tile bị ảnh hưởng. Với Excel, tiêu đề hàng/cột lấy từ
`.uno:ViewRowColumnHeaders` (vị trí tính bằng pixel ở zoom hiện tại, đã kiểm chứng), con trỏ ô từ `CELL_CURSOR` (twip).

**Nhập liệu:** phím Qt → mã phím VCL (`KeyMap.cpp`); chuột → twip; IME tiếng Việt → chuỗi đã chốt.
Thanh công thức gửi `.uno:EnterString`; ô địa chỉ gửi `.uno:GoToCell`.

**Clipboard:** ở chế độ LOK, engine dùng clipboard nội bộ. `DocumentTab::copy/paste` chuyển
`text/html` + `text/plain` giữa engine (`getTextSelection` / `paste`) và clipboard Windows.

**Lưu:** xem [DATA_SAFETY.md](DATA_SAFETY.md).

## 4. Luồng và đồng bộ

LOK chạy vòng lặp chính của LibreOffice trên luồng riêng (`lo_startmain`). Callback đến từ luồng đó;
`DocumentTab` chuyển chúng sang luồng giao diện bằng `QMetaObject::invokeMethod(..., QueuedConnection)`.
Các lệnh gọi API LOK từ luồng giao diện được LibreOffice tự khoá bằng SolarMutex.

## 5. Kiểm chứng

- `tools/smoketest` chạy trên runtime thật: khởi động, mở XLSX ở thư mục/tên tiếng Việt, render,
  gõ nội dung và công thức, `EnterString`, clipboard, các quy tắc từ chối ghi đè/định dạng rủi ro,
  Lưu thành, tệp gốc giữ nguyên từng byte, không sót tệp tạm, xuất PDF, sao lưu khi thay thế,
  CSV chỉ đọc, DOCX/PPTX/PDF, tạo mới 3 loại.
- `tests/verify_roundtrip.py` so sánh từng ô giữa tệp gốc và tệp đã lưu.
- CI chạy cả hai trên Linux (LibreOffice của Ubuntu) và Windows (LibreOffice 26.8.3 chính thức),
  chụp ảnh giao diện thật, rồi mới đóng gói bộ cài.
