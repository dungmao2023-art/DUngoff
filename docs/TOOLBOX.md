# Hộp công cụ — cách thêm công cụ mới

Trang **Công cụ** (thanh bên) gom các tiện ích cho dân văn phòng: điền thư mời hàng loạt, chụp màn hình, tìm trong thư mục, làm việc với tệp/liên kết… Có **hai cách** thêm công cụ.

## Cách 1 — Công cụ tích hợp (C++/Qt, có giao diện riêng)

Mỗi công cụ là một thư mục `app/src/toolbox/tools/<Ten>/` (hoặc một tệp `tools/<Ten>Tool.cpp`) cung cấp **một hàm**:

```cpp
QWidget* createMailMergeWidget(QWidget* parent);   // trả về giao diện của công cụ, con của parent
```

Rồi đăng ký ở cuối tệp `.cpp` bằng **một dòng**:

```cpp
#include "ToolRegistry.h"
DUNG_REGISTER_TOOL(tron_thu, "Trộn thư / giấy mời", "Điền tên hàng loạt vào giấy mời",
                   "Văn phòng", "✉", createMailMergeWidget)
```

và thêm **một dòng** vào danh sách trong `app/src/toolbox/toolbox.cmake` (đã có sẵn các dòng chú thích cho mailmerge, screenshot, filesearch, filetools — bỏ `#` khi tệp đã có). Không phải sửa lõi.

API chính thức (`#include "ToolRegistry.h"`, không gian tên `dung::toolbox`, bí danh của `toolbox`):

| Hàm | Ý nghĩa |
|---|---|
| `registerTool(ToolInfo)` | Đăng ký. Từ chối (trả `false`) nếu mã sai định dạng, trùng mã hoặc thiếu hàm tạo. Không ghi đè âm thầm. |
| `ToolInfo{id, name, description, category, iconText, create}` | `id`: chữ thường/số/`_`/`-`, duy nhất. `category` trống → nhóm "Khác". |
| `tools()`, `categories()`, `find(id)`, `search(q)` | Tra cứu; `search` không phân biệt hoa-thường và dấu tiếng Việt. |
| `pinnedIds()`, `isPinned()`, `setPinned()` | Ghim công cụ (lưu bằng `QSettings`, khoá `toolbox/pinned`). |

Quy tắc cho công cụ tích hợp: phần xử lý (không giao diện) tách thành mô-đun chỉ cần QtCore để kiểm thử được; không làm mất dữ liệu gốc của người dùng (ghi ra tệp mới, không ghi đè); không chạy macro trong tệp người dùng.

## Cách 2 — Công cụ ngoài, không cần biên dịch (`tool.json`)

Người dùng (hoặc bạn) thả một thư mục vào `<thư mục dữ liệu DungOffice>/tools/<tên>/` gồm `tool.json` + script. Mẫu: `samples/tools/xin-chao/`.

```json
{
  "schema": 1, "id": "xin-chao", "name": "Lời chào", "description": "…", "category": "Tool ngoài",
  "command": "python", "args": ["xin_chao.py", "--ten", "{ten}", "{ghichu}"],
  "workingDir": ".", "timeoutSec": 30,
  "fields": [ { "id": "ten", "label": "Tên", "type": "text", "required": true } ]
}
```

Kiểu trường: `text`, `multiline`, `number` (min/max), `file`, `folder`, `choice`. Trong `args` dùng `{mã_trường}` và `{tooldir}`.

An toàn (đã có kiểm thử):
- **Dự án không đóng gói Python hay AutoHotkey.** Tool gọi chương trình người dùng tự cài; thiếu thì báo rõ.
- Không qua shell: đối số truyền riêng từng cái, giá trị nhập không "chèn lệnh" được.
- `command` chỉ là tên trần (tìm trong PATH) hoặc đường dẫn **tương đối** trong thư mục tool. Từ chối đường dẫn tuyệt đối, `..`, ổ đĩa, liên kết tượng trưng thoát thư mục, `cmd`/`powershell`/`wscript`…, `.bat`/`.cmd`/`.lnk`.
- Hiển thị đúng lệnh sẽ chạy và **hỏi xác nhận lần đầu**; xác nhận gắn với dấu vân tay SHA-256 của cả thư mục, sửa script là phải xác nhận lại. Không chạy gì ngầm.
- Có giới hạn thời gian, nút Dừng và nhật ký.
- Tool ngoài hỏng (JSON sai, thiếu trường) bị bỏ qua kèm lý do hiển thị trên trang.

## Kiểm thử

`dung-toolboxtest` (chỉ QtCore): đăng ký, tìm không dấu, ghim, đọc/kiểm tra `tool.json` (schema sai, thoát thư mục), xác nhận, ghép đối số, chạy/hết giờ/dừng. Chạy trong CI Linux và Windows.

## Chưa làm

- Ghim **lên thanh ribbon** của từng ứng dụng: hiện chỉ ghim ở đầu trang Công cụ.
- Chưa kiểm tra giao diện trên máy thật; CI chỉ chạy bộ kiểm thử không giao diện và biên dịch.
