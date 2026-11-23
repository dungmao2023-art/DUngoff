# Dũng Office

Bộ ứng dụng văn phòng **miễn phí, ngoại tuyến, không cần tài khoản** cho Windows: một cửa sổ,
bốn công cụ **Word / Excel / PowerPoint / PDF**, giao diện mới xanh–trắng.

Dũng Office **không tự viết lại engine tài liệu**. Phần đọc, tính toán, render và ghi
DOCX/XLSX/PPTX/ODF/PDF dùng lại các module lõi của **LibreOffice** (Calc, Writer, Impress, Draw,
filter OOXML/ODF/PDF) thông qua **LibreOfficeKit**, tức giao diện nhúng chính thức của LibreOffice.
Ở chế độ này không có cửa sổ, menu, toolbar hay hộp thoại nào của LibreOffice: toàn bộ giao diện do
Dũng Office (Qt 6) tự vẽ.

```
┌──────────── Dũng Office (Qt 6, mã của dự án này) ────────────┐
│ Thanh bên • Trang chủ • Ribbon ngữ cảnh • Thẻ tài liệu       │
│ Thanh công thức • Sheet/Trang chiếu • Thanh trạng thái       │
│ TileView: tự vẽ tile, con trỏ ô, vùng chọn, tiêu đề hàng/cột │
└───────────────┬──────────────────────────────────────────────┘
                │ dungengine (C++): bản làm việc tạm, Lưu an toàn,
                │ chính sách định dạng, cầu nối clipboard
┌───────────────▼──────────────────────────────────────────────┐
│ LibreOfficeKit (C API) → module lõi LibreOffice (MPL-2.0)    │
│ sc (Calc) • sw (Writer) • sd (Impress/Draw) • oox • filter   │
└──────────────────────────────────────────────────────────────┘
```

## Nguyên tắc dữ liệu (bất di bất dịch)

1. **Tệp gốc không bao giờ bị mở trực tiếp.** Engine chỉ nạp một bản sao tạm, nên tệp gốc không thể bị khoá hay ghi đè.
2. **Mặc định là "Lưu thành" sang tệp mới** (`Tên - Dũng Office.xlsx`). Muốn ghi đè tệp gốc phải xác nhận rõ ràng, và bản sao lưu được tạo *trước khi* ghi.
3. **Ghi an toàn:** ghi ra tệp tạm, kiểm tra cấu trúc (ZIP/PDF hợp lệ), rồi mới thay thế nguyên tử. Lỗi giữa chừng không làm hỏng tệp đích.
4. **Định dạng chưa bảo toàn được thì chỉ đọc:** `.xls .xlsm .xlsb .csv .doc .ppt .rtf .txt .pdf` mở ở chế độ chỉ đọc kèm cảnh báo. Xuất PDF phải xác nhận vì có mất mát.
5. **Không im lặng ép kiểu:** CSV được nhập với mọi cột kiểu Văn bản ("00123" giữ nguyên). Mọi khác biệt round-trip đã biết đều được liệt kê công khai trong [docs/DATA_SAFETY.md](docs/DATA_SAFETY.md).

## Trạng thái hiện tại (v0.1, giai đoạn chứng minh kiến trúc)

| Hạng mục | Trạng thái | Bằng chứng |
|---|---|---|
| Engine qua LibreOfficeKit: mở/render/sửa/lưu XLSX | ✅ chạy thật | `dung-smoketest`: 46 kiểm tra |
| Round-trip XLSX: công thức, comment, hyperlink, ô gộp, định dạng số, "00123" | ✅ có 2 khác biệt đã biết (DS-001, DS-002) | `tests/verify_roundtrip.py` |
| DOCX / PPTX: mở, render, Lưu thành; tạo mới | ✅ mức engine | smoke test |
| PDF: mở xem (chỉ đọc), xuất PDF có xác nhận | ✅ mức engine | smoke test |
| Giao diện Qt 6 (Home, thanh bên, ribbon, thẻ, thanh công thức, sheet, status bar) | ✅ chạy thật trên Linux, có ảnh chụp | nhánh `ci-report` |
| Build trên Windows (MSVC + Qt 6.8) | ✅ biên dịch được | job `windows` |
| Engine trên Windows với LibreOffice 26.8.3 **chính thức**: khởi động, mở XLSX | ✅ | job `windows` |
| Engine trên Windows: hiển thị/chỉnh sửa trong giao diện riêng | ❌ bản chính thức thiếu backend headless → cần tự build engine `--enable-headless` | [docs/WINDOWS_ENGINE.md](docs/WINDOWS_ENGINE.md) |
| Bộ cài OneClick | ⏸ chưa tạo: chỉ tạo khi kiểm thử Windows đạt | |

Kết quả CI mới nhất (log và ảnh chụp giao diện) được đẩy lên nhánh
[`ci-report`](../../tree/ci-report). **Bộ cài chỉ được tạo khi mọi kiểm thử Windows đã đạt.**

## Build

**Windows** (Visual Studio 2022, CMake, Ninja, Qt 6.8 msvc2022_64; Python + openpyxl chỉ để kiểm thử):

```powershell
.\scripts\build-windows.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64
```

Script tải LibreOffice chính thức từ download.documentfoundation.org (có kiểm tra SHA-256), build,
chạy kiểm thử, rồi đóng gói vào `build\dist\DungOffice`. Bộ cài: `installer\DungOffice.iss` (Inno Setup 6).

**Linux** (phát triển nhanh, dùng LibreOffice của hệ thống):

```bash
sudo apt install libreoffice-calc libreoffice-writer libreoffice-impress qt6-base-dev cmake ninja-build python3-openpyxl
cmake -S . -B build -G Ninja && cmake --build build
python3 tests/fixtures/make_fixtures.py fx
./build/dung-smoketest --program /usr/lib/libreoffice/program --xlsx fx/mau-doanh-thu.xlsx --csv fx/mau.csv --out smoke
./build/app/DungOffice
```

## Cấu trúc

```
engine/            dungengine: bọc LibreOfficeKit + chính sách an toàn dữ liệu (không phụ thuộc giao diện)
app/               giao diện Dũng Office (Qt 6 Widgets)
tools/smoketest/   kiểm thử engine trên runtime thật
tests/             tạo tệp mẫu, so sánh round-trip, danh sách khác biệt đã biết
third_party/       header LibreOfficeKit (nguyên bản, tag libreoffice-26.8.2.2)
scripts/           tải runtime chính thức, build, đóng gói Windows
installer/         bộ cài Inno Setup (ngoại tuyến, không cần quyền admin)
docs/              kiến trúc, an toàn dữ liệu, lộ trình
```

## Giấy phép

Mã của Dũng Office: [MPL-2.0](LICENSE), cùng giấy phép với LibreOffice. Xem
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Dũng Office không liên kết với The Document
Foundation; "LibreOffice" là nhãn hiệu của The Document Foundation và không được dùng làm tên
hay logo sản phẩm này.
