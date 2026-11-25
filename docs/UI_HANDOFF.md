# Bàn giao giao diện Dũng Office (UI hand-off)

Tài liệu tự đủ cho AI/lập trình viên khác (ví dụ ChatGPT) dựng lại / làm đẹp giao diện **mà không phá chức năng**. Phần chức năng (engine, lưu an toàn, lệnh UNO) do người phụ trách chức năng giữ; bạn chỉ sửa lớp Qt Widgets.

Đọc kèm: `docs/ARCHITECTURE.md`, `docs/DATA_SAFETY.md`, `docs/ICONS.md`, `docs/TOOLBOX.md`, mẫu thiết kế `docs/design/DungOffice_UI_reference.html`.

---

## 1. Kiến trúc UI hiện tại

### 1.1 Cây thư mục `app/src`
| Thư mục | Vai trò |
|---|---|
| `MainWindow.*` | Cửa sổ duy nhất: ngăn xếp trang Home / tài liệu / Projects / Templates / Toolbox, sidebar, chế độ gọn, điều phối lệnh (`runCommand`, `activateKind`, `onAppAction`, `captureShots`) |
| `shell/` | `TopBar` (tab Home/Word/Excel/PPT/PDF, ô tìm Ctrl+K, trạng thái lưu), `QuickPalette` (bảng lệnh nhanh, `foldVietnamese` bỏ dấu), `ShellIcons` |
| `DocumentTab.*` | Một tài liệu mở: ribbon + `TileView` + thanh trạng thái; lưu/Lưu thành/PDF/in/đóng, phục hồi |
| `TileView.*` | Vẽ ô ảnh (tile) do engine render; con trỏ, vùng chọn, IME, cuộn, thu phóng |
| `Ribbon.*`, `word/WordRibbon.cpp` | Định nghĩa tab/nhóm/nút (`Btn{id,label,mdl2,fallback,checkable}`) |
| `word/`, `excel/`, `ppt/`, `pdf/` | Hành động riêng từng loại (`WordCommands.h`, `ExcelCommands.h`, `PptActions.h`…) |
| `toolbox/` (+`tools/`) | Hộp công cụ: `ToolRegistry`, các công cụ tích hợp, công cụ ngoài qua `tool.json` |
| `extras/` | `ProjectsPage`, `SidePanel`, `WorkspaceStore` |
| `templates/`, `print/`, `recovery/`, `update/`, `vntext/`, `tableutil/`, `tools/{filesearch,filetools,mailmerge,screenshot}` | Mẫu, in, phục hồi, cập nhật, xử lý chữ Việt, tiện ích |
| `ui/` | `Icons.h` + `IconData` + `CommandIcons.cpp` — bộ icon vector tự vẽ (142 tên) |
| `Theme.h` | Màu/stylesheet toàn cục |

### 1.2 UI ↔ engine
- Engine = LibreOfficeKit chạy headless (svp), **không có cửa sổ LO**. Singleton `EngineHost` (`open`, `createNew`) trả về `Document`.
- UI gửi lệnh: `Document::postUnoCommand(".uno:...", jsonArgs)`. Nút ribbon có id bắt đầu bằng `.uno:` đi thẳng engine; id khác (`word:*`, `xl:*`, `ppt.*`, `pdfTools:*`, `save`, `open`…) là **app action** xử lý trong `DocumentTab`/`MainWindow::onAppAction`/`handle*Action`.
- Engine báo ngược bằng callback LOK (con trỏ, vùng ô, vùng chọn text, `INVALIDATE_TILES`, tiêu đề cột/hàng `.uno:ViewRowColumnHeaders`…). Callback chạy thread engine → chuyển về UI thread bằng `QueuedConnection`. **Không gọi engine trực tiếp từ UI thread khác.**
- Hiển thị: `TileView` xin tile 256 px ARGB32_Premultiplied, đổi twips↔px, vẽ lại khi INVALIDATE.
- Clipboard: `getTextSelection` / paste qua engine.

### 1.3 Vòng đời tài liệu
1. **Mở**: sao chép bản gốc vào `%LOCALAPPDATA%\DungOffice\work\<pid>\doc-N-xxxx\`, mở bản sao với `Batch=true`. Bản gốc không bị đụng.
2. **Tab**: `MainWindow` tạo `DocumentTab` (ribbon theo loại: Word/Excel/PPT/PDF/chỉ-đọc).
3. **Lưu / Lưu thành**: mặc định *Lưu thành bản sao*; ghi nguyên tử `~dung-xxxx-tên.ext` rồi đổi tên; ghi đè gốc chỉ khi `allowOverwriteOriginal`; có sao lưu `tên.dung-backup-YYYYMMDD-HHMMSS.ext`. Định dạng chỉ-đọc (XLS/XLSM/XLSB/CSV/DOC/RTF/TXT/PPT/PDF) → luôn "lưu bản sao" hoặc cảnh báo (`FormatPolicy`).
4. **Phục hồi**: thư mục work + `recovery/` cho phép nhận lại sau sập (`DocumentTab` adoption).
5. **Đóng**: `maybeClose` hỏi lưu; thoát cứng (TerminateProcess) **chỉ khi** `dung::liveDocumentCount()==0` trên Windows.

---

## 2. Hợp đồng bất khả xâm phạm (đừng phá)

1. **An toàn dữ liệu**: không ghi đè gốc, không mất âm thầm, tính năng chưa hỗ trợ phải cảnh báo/chỉ-đọc. Mọi hộp thoại liên quan lưu/xuất giữ nguyên ngữ nghĩa (xem DATA_SAFETY.md).
2. **Con trỏ nhập liệu bề rộng 0**: engine gửi `x, y, 0, h`. **Không dùng `QRect::isEmpty()`** để quyết định vẽ; dùng cờ `m_hasCaret`/`m_hasCell` và vẽ vạch 1–2 px nhấp nháy (`freezeBlink` cho test). Phải có caret nhấp nháy ở ô Excel và vị trí nhập Word/PPT.
3. **Không dùng icon font** (Segoe MDL2…): dùng `dung::ui::icon(name, size, color)` / `iconNameForCommand`. Không thêm Qt6::Svg. Icon mới → thêm vào `IconData`, `CommandIcons.cpp`, cập nhật `docs/ICONS.md`.
4. **Thoát cứng** chỉ khi không còn tài liệu mở; `DUNG_EXIT_CLEAN=1` tắt hẳn.
5. **DPI**: toàn bộ kích thước theo px logic Qt (không nhân tay); phải đúng ở 100–200%, đa màn hình.
6. **Đường dẫn dài/Unicode**: dùng `QString`/`std::filesystem::path`, không `toLatin1`/`toLocal8Bit` cho đường dẫn tệp người dùng.
7. **Không thêm DLL/phụ thuộc** ngoài Qt6 Widgets/Pdf đã có (installer có kiểm tra). Không gói Python/AutoHotkey.
8. **Macro** trong tệp người dùng không bao giờ chạy.
9. Không đưa dữ liệu người dùng/xlsx kiểm thử vào repo. Văn bản UI: tiếng Việt, có dấu.
10. Không đổi id lệnh/`.uno:` khi tút giao diện; chỉ đổi cách trình bày.

---

## 3. Danh mục chức năng UI và nơi nối

Quy ước: **id** là chuỗi trong `Btn`; `.uno:` → engine; còn lại → app action.

### 3.1 Chung (mọi loại, `Ribbon.cpp`)
- Tệp: `open`, `save`, `saveAs`, `exportPdf`, `print` (In / Xem trước).
- Bộ nhớ tạm: `paste`, `cut`, `copy`, `.uno:SelectAll`, `.uno:FormatPaintbrush`.
- Hoàn tác: `.uno:Undo`, `.uno:Redo`. Phông: `.uno:Bold/Italic/Underline`.
- Thu phóng: `zoomOut`, `zoom100`, `zoomIn`.

### 3.2 Word (`word/WordRibbon.cpp`, `WordCommands.h`)
- Phông chữ: `word:font`, `word:size`, `.uno:Bold/Italic/Underline/Strikeout/SuperScript/SubScript/ResetAttributes`, `word:color`, `word:back`.
- Đoạn văn: `.uno:LeftPara/CenterPara/RightPara/JustifyPara`, `SpacePara1/SpacePara15`, `DefaultBullet`, `DefaultNumbering`, `DecrementIndent/IncrementIndent`.
- Kiểu: `word:style0..3` (Thường, Tiêu đề 1–3) → `paragraphStyle`.
- Tìm: `word:find`, `word:replace`, `word:count`.
- Chèn: `word:table`, `word:image`, `word:link`, `.uno:InsertPagebreak`, `.uno:InsertPageNumberField`.
- Bố cục: `word:a4`, `word:landscape`; lề `word:marginVN/marginNormal/marginNarrow`.
  - **Lưu ý Windows**: engine Windows không áp lề qua UNO; app sửa `w:pgMar` trong `word/document.xml` của **bản sao tạm** rồi mở lại ở tab mới (mất lịch sử hoàn tác, mất vị trí cuộn) → luôn có hộp thoại xác nhận. Giữ nguyên hộp thoại này.
- Tiện ích Việt: `word:vnMoney`, `word:vnDate`, `word:vnTcvn3`, `word:vnSyllable`.

### 3.3 Excel (`excel/ExcelCommands.h`, chỉ lệnh đã kiểm chứng)
- Phông & màu: `xl:fontName`, `xl:fontSize`, `.uno:Strikeout`, `xl:textColor`, `xl:fillColor`.
- Căn lề: `.uno:AlignLeft/HorizontalCenter/Right/Top/VCenter/Bottom`, `ToggleMergeCells`, `WrapText`.
- Số: `.uno:NumberFormatStandard/Thousands/Percent/Date/IncDecimals/DecDecimals`.
- Hàng & cột: `.uno:InsertRowsBefore/InsertColumnsBefore/DeleteRows/DeleteColumns/SetOptimalColumnWidth`, `xl:colWidth`, `xl:rowHeight`.
- Sheet: `xl:sheetAdd/Rename/Dup/Delete`.
- Dữ liệu: `.uno:AutoSum`, `SortAscending/Descending`, `DataFilterAutoFilter`.
- Tìm: `xl:find`, `xl:replace`. Xem: `.uno:FreezePanes/FreezePanesRow/FreezePanesColumn`.
- Chưa có (đừng vẽ nút giả): viền ô, tiền tệ VND.

### 3.4 PowerPoint (`ppt/PptActions.h` → `handlePptAction`)
- Trang chiếu: `.uno:InsertPage/DuplicatePage/DeletePage/MovePageUp/MovePageDown/HideSlide/ShowSlide`.
- Đoạn: `.uno:LeftPara/CenterPara/RightPara/DefaultBullet/DefaultNumbering`.
- Đối tượng: `ppt.textbox`, `ppt.rect`, `ppt.image`.
- Trình chiếu: `ppt.show`, `ppt.showHere`, `ppt.transitionNote` (hiệu ứng chưa chỉnh được → chỉ ghi chú).
- Giới hạn PPTX/slide master: xem `docs/PPTX_LIMITS.md`.

### 3.5 PDF (ribbon dạng Drawing)
- Trang: `pdfPrev`, `pdfNext`. Công cụ: `pdfTools:merge|split|rotate|compress|number|stamp|images|blank|info`.
- PDF mở chỉ-đọc; "Lưu" luôn là bản sao.

### 3.6 Chế độ chỉ-đọc
Chỉ `copy` và nhóm xem/thu phóng. Không hiện nút sửa.

### 3.7 Toolbox (`toolbox/`)
Đăng ký bằng macro `DUNG_REGISTER_TOOL(id, "Tên", "Mô tả", "Nhóm", "icon", createXxxWidget)`; hàm `createXxxWidget()` trả `QWidget*`. Có thể ghim (QSettings). Công cụ ngoài qua `tool.json` (không shell, xác nhận SHA-256).

| id | Tên | Hàm tạo |
|---|---|---|
| dong_ho | Đồng hồ | createTimerWidget |
| doi_ten_hang_loat | Đổi tên hàng loạt | createBatchRenameWidget |
| tim_file_trung | Tìm tệp trùng lặp | createDuplicateFinderWidget |
| so_sanh_thu_muc | So sánh hai thư mục | createFolderCompareWidget |
| dung_luong_thu_muc | Dung lượng thư mục | createDiskUsageWidget |
| lam_sach_link | Làm sạch liên kết | createLinkToolsWidget |
| sap_xep_tep | Sắp xếp tệp | createOrganizeWidget |
| duong_dan_tep | Đường dẫn tệp | createPathToolsWidget |
| quet_lien_ket | Quét liên kết hỏng | createLinkScanWidget |
| dien_ten_hang_loat | Điền tên hàng loạt | createMailMergeWidget |
| chup_man_hinh | Chụp màn hình | createScreenshotWidget |
| tim_trong_thu_muc | Tìm trong thư mục | createFileSearchWidget |

Hộp thoại quan trọng giữ nguyên hành vi: xác nhận lưu/ghi đè, cảnh báo mất macro/định dạng (`featureLossConfirmed`), xác nhận lề Windows, hỏi lưu khi đóng, xem trước in, cập nhật (`update/`), phục hồi.

---

## 4. Tham chiếu thiết kế

File: `docs/design/DungOffice_UI_reference.html` (mở bằng trình duyệt; đây là **mẫu hình**, không phải code để nhúng).

### 4.1 Token
- Nền `#f6f9fe`; viền `#dee7f5`; chữ `#20365a`; xanh nhấn `#285fc9`/`#3069dc`; hover `#f0f5ff`; active `#eaf1ff` + vạch trái 3 px.
- Màu theo ứng dụng: Word `#2864ba`, Excel `#16804e`, PowerPoint `#c45530`, PDF `#b94449`.
- Bo góc: nút/ô nhập 9 px; thẻ tính năng 17 px; banner 20 px. Focus: viền `#8aaff4` + vòng `#dfebff`.
- Hiện tại `Theme.h` dùng bảng gần giống (kBlue `#2864BA`, kBorder `#DCE5F2`, kText `#17294D`, kMuted `#6B7A99`) — nên đồng bộ về token mẫu.

### 4.2 Bố cục theo màn
- **Sidebar** rộng 241 px, nền trắng: logo, nhóm điều hướng, thẻ "Chế độ riêng tư" cuối.
- **Topbar** cao 66 px: tab ở giữa, ô tìm (Ctrl+K), trạng thái lưu bên phải.
- **Home**: banner chào gradient (bo 20), lưới 4 cột tính năng, 6 thẻ nhanh, ngăn kéo phải 425 px.
- **Ribbon**: nhấn màu theo ứng dụng, nhóm có nhãn dưới, nút icon vector + chữ.
- **Thanh trạng thái** cao 32 px.

### 4.3 Hành vi
Hover/active/focus theo token; tab chuyển ứng dụng đổi màu nhấn; Ctrl+K mở QuickPalette (tìm không dấu); sidebar thu gọn ở cửa sổ hẹp (chế độ gọn).

### 4.4 Khác biệt đã biết so với bản Qt
(Ảnh chụp: nhánh `feat/ui-shell` trong repo local / CI artifact của job ui-shot; chạy lại bằng mục 5.)
- Nhãn ribbon bị cắt ("Chọ…cả").
- Sidebar bị tràn ở chiều cao 768 px.
- Cửa sổ không thu nhỏ được dưới ~877 px (minimumSize quá lớn).
- Trang Projects / Templates / Tasks chưa dàn trang đúng mẫu.
- Một số icon còn mơ hồ ở 16 px; cần kiểm lại ở DPI 150–200%.

### 4.5 Lỗi UI đã biết cần sửa
Các mục ở 4.4 cộng: chưa kiểm caret ở DPI 125–200%; công cụ Toolbox chưa đồng nhất kiểu (padding, nút chính/phụ); chưa có chế độ tối.

---

## 5. Build / test / chụp ảnh

- **Không có Qt cục bộ** trong môi trường phát triển. Mọi build chạy ở GitHub Actions (`.github/workflows/ci.yml`), Windows + Linux.
- Chạy CI: push nhánh hoặc `gh workflow run ci.yml --ref <nhánh>`.
- Phát hành bản cài thử không cần đẩy tag: `gh workflow run ci.yml --ref main -f release_tag=v0.1.0-testN` (tag có dấu `-` ⇒ pre-release).
- Chụp ảnh giao diện (không cần engine): `DUNG_UI_SHOT_DIR=<thư_mục>` (ngầm bật `DUNG_NO_ENGINE=1`) → `MainWindow::captureShots` ghi PNG rồi thoát (mã 0 = ok, 3 = lỗi). CI đã gọi bước này; xem artifact.
- Gỡ lỗi: `DUNG_TRACE_CARET=1` (log toạ độ caret/ô), `DUNG_TRACE_EXIT=1` (log thoát), `DUNG_EXIT_CLEAN=1` (không thoát cứng), `DUNG_NO_ENGINE=1`, `DUNG_LO_PROGRAM=<đường dẫn engine>`, `DUNG_QPDF=<qpdf>`.
- Test: `tools/smoketest` (excel_tests, word_tests, ppt_tests, preedit_tests, recover_tests, template_fidelity). Sửa UI xong phải xanh toàn bộ.
- Quy tắc nhánh: làm trên nhánh riêng, không merge main khi chưa xanh CI; không commit xlsx người dùng.

---

## 6. Mẫu prompt giao cho ChatGPT

```
Bạn là lập trình viên Qt6 Widgets (C++17). Repo: github.com/dungmao2023-art/DUngoff.
Đọc: docs/UI_HANDOFF.md, docs/ICONS.md, docs/DATA_SAFETY.md và mở docs/design/DungOffice_UI_reference.html.
Nhiệm vụ: làm giao diện app/src khớp mẫu thiết kế (sidebar 241px, topbar 66px, ribbon theo màu từng ứng dụng,
Home banner + lưới thẻ, status bar 32px), sửa các lỗi ở mục 4.4–4.5.

Ràng buộc cứng:
- Chỉ sửa lớp trình bày; KHÔNG đổi id lệnh, luồng mở/lưu/đóng, FormatPolicy, hộp thoại xác nhận.
- Vẽ caret nhấp nháy kể cả khi rộng 0 (không dùng QRect::isEmpty).
- Không icon font, không Qt6::Svg, dùng dung::ui::icon; không thêm DLL/phụ thuộc mới.
- Kích thước theo px logic; đúng ở 100–200% DPI; cửa sổ thu nhỏ được xuống 1024x640 không tràn.
- Văn bản tiếng Việt có dấu; đường dẫn Unicode/dài an toàn.
- Không động vào tools/smoketest trừ khi thêm test; không commit dữ liệu người dùng.

Tiêu chí nghiệm thu:
1. CI (ci.yml) xanh Windows + Linux, smoketest qua hết.
2. DUNG_UI_SHOT_DIR cho ảnh Home/Word/Excel/PPT/PDF/Toolbox khớp mẫu; không nhãn bị cắt ở 1366x768.
3. Caret thấy nhấp nháy ở ô Excel và đoạn Word; mọi nút ribbon vẫn gửi đúng lệnh như bảng mục 3.
4. Không có icon lỗi/ô vuông; mọi nút có icon vector.
Giao: một nhánh riêng + mô tả thay đổi; không merge main.
```
