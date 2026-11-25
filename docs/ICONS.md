# Hệ biểu tượng của Dũng Office

Mọi biểu tượng trong ứng dụng (ribbon, thanh bên, trang chủ, hộp công cụ) là **hình vector do dự án tự vẽ**,
kết xuất bằng `QPainter`. Không dùng font biểu tượng (Segoe MDL2/Fluent), không dùng emoji, không cần `Qt6::Svg`
(nên không thêm DLL vào bộ cài). Vì vậy biểu tượng hiện giống nhau trên mọi máy, kể cả máy thiếu font.

Các hình là thiết kế gốc của dự án (không sao chép từ Fluent, Material hay FontAwesome).

## Dùng trong mã

```cpp
#include "ui/Icons.h"
button->setIcon(dung::ui::icon("bold"));                 // màu mặc định (xanh đậm)
button->setIcon(dung::ui::icon("home", Qt::white, 24));  // tô màu khác, ví dụ trên thanh bên xanh
QPixmap pm = dung::ui::pixmap("search", color, 24, devicePixelRatioF());
bool ok = dung::ui::hasIcon("bold");
QStringList all = dung::ui::iconNames();
QString name = dung::ui::iconNameForCommand(".uno:Bold"); // ánh xạ mã lệnh → tên icon
```

- `icon()` trả `QIcon` dùng `QIconEngine`: mỗi lần được yêu cầu một kích thước/DPR, hình được vẽ lại ở đúng độ
  phân giải đó nên luôn sắc nét (HiDPI 125%, 150%, 200%…).
- Trạng thái `QIcon::Disabled` tự dùng màu nhạt `#A0AEC0`.
- Tên không tồn tại: vẽ một khung trống có dấu gạch chéo và `hasIcon()` trả `false` (kiểm thử sẽ báo, không sập).
- `ToolInfo.iconText` (hộp công cụ): nếu chuỗi trùng tên icon thì vẽ icon, nếu không thì hiện như chữ.

## Thêm hoặc sửa một biểu tượng

1. Thêm một dòng `I("tên", "mô tả")` vào `app/src/ui/IconData.cpp`. Tên dùng chữ thường, dấu gạch ngang.
2. Nếu là nút của ribbon: thêm `{"mã-lệnh", "tên"}` vào `app/src/ui/CommandIcons.cpp`.
3. Chạy `dung-icontest` (hoặc xem kết quả CI): mọi icon phải vẽ được, không trùng nhau, Disabled khác Normal.
   Tờ tổng hợp `icons-contact-sheet.png` và `icons-small-sizes.png` nằm trong báo cáo CI (nhánh `ci-report`).

### Cú pháp mô tả

Lưới 24×24, nét khoảng 1.6, đầu và góc bo tròn. Các phần tử cách nhau bởi `|`, mỗi phần tử có dạng `loại:dữ-liệu`:

| Loại | Ý nghĩa |
| --- | --- |
| `p:` | đường nét (đường dẫn kiểu SVG) |
| `f:` | tô đặc (đường dẫn kiểu SVG) |
| `tp:` | tô nhạt (độ mờ 30%) |
| `c:cx cy r` / `fc:` | đường tròn nét / tô đặc |
| `r:x y w h rx` / `fr:` / `tr:` | chữ nhật bo góc: nét / tô đặc / tô nhạt |

Đường dẫn hỗ trợ `M m L l H h V v C c S s Q q A a Z z`. Cờ của lệnh cung viết liền được (`a2 2 0 011 1`). Cung chỉ hỗ trợ
elip thẳng trục (góc xoay = 0), đủ cho icon.

## Kiểm thử

- `dung-icontest [thư-mục]`: đủ tên bắt buộc; mỗi icon có nét, không bị tô kín, đủ lớn ở 16/24/32 px; hai icon bất kỳ
  không giống hệt nhau (cảnh báo khi quá giống); Disabled khác Normal; màu truyền vào được áp dụng; DPR 2 và 3 cho
  ảnh đúng kích thước vật lý; `QIcon::paint`; ghi tờ tổng hợp PNG.
- `dung-ribbonicontest`: mọi mã lệnh trên mọi ribbon (Word, Excel, PowerPoint, PDF, chế độ chỉ đọc) có icon hợp lệ, và
  mọi nút dựng thật đều có icon.

Giới hạn trung thực: kiểm thử chỉ chứng minh icon vẽ được và khác nhau; **độ đẹp và độ dễ nhận ra ở cỡ nhỏ phải xem bằng mắt**
trên tờ tổng hợp và trên màn hình thật.
