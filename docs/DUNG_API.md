# Dũng API — cầu nối script Python của Dũng Office

Engine Windows (headless) không phát hộp thoại JSON, nên các hộp thoại LibreOffice (Page Style, Format Cells,
Data Validity…) không dùng được. Thay vào đó, ứng dụng đóng gói sẵn **một** thư viện script Python
(`engine-build/dung-scripts/dung_api.py`) và gọi các hàm trong danh sách trắng qua `lok::Office::runMacro`.

## Nguyên tắc an toàn
- Chỉ chạy script của ứng dụng (cài vào `<hồ sơ>/user/Scripts/python/dung_api.py` mỗi lần khởi động, đè bản cũ).
  **Không bao giờ chạy macro nằm trong tệp của người dùng** (xlsm/docm giữ nguyên VBA, không thực thi).
- Hồ sơ engine đặt `MacroSecurityLevel=3`.
- Danh sách trắng: tên hàm lạ bị từ chối; mỗi đối số được kiểm tra kiểu/khoảng; đối số thừa bị từ chối.
  Không `eval`, không nhận đường dẫn tệp.
- Tài liệu chỉ đọc: chỉ hàm `readOnlySafe` (ping, read_*). Ghi/sửa bị từ chối.
- Tài liệu mở từ tệp: ứng dụng gửi `expect_url`; script từ chối nếu tài liệu hiện hành của engine khác (tránh sửa nhầm).
- Hết giờ (mặc định 30 s): engine bị coi là kẹt, các lần gọi sau bị từ chối; người dùng nên lưu bản sao và mở lại.
- Hàm làm đổi tài liệu → `Document::m_modified = true` và phát sự kiện làm mới (SYNTH). Tệp gốc vẫn không bị ghi đè (luật "Lưu thành").

## Giao thức
`runMacro` không truyền đối số và không trả giá trị, nên dùng hai tệp trong `<hồ sơ>/user/dung-api/`:

1. C++ ghi `args.json` (nguyên tử): `{"nonce","fn","args":{...},"expect_url"}`.
2. C++ gọi `runMacro("macro:///Standard.DungApi.Run")` trên luồng engine, sau `setView(getView())`.
   (Đã đo trên engine thật: `runMacro` với URL `vnd.sun.star.script:` trả false; chỉ macro Basic chạy được. Mô-đun Basic
   `DungApi.Run` do ứng dụng sinh ra chỉ gọi script Python qua `MasterScriptProviderFactory`; lỗi khởi chạy ghi ra `basic-error.txt`.
   LibreOffice bỏ qua thư viện Basic ghi trước lần khởi động đầu, nên ứng dụng ghi lại thư viện sau khi engine khởi động.)
3. `call_api()` đọc rồi xoá `args.json`, kiểm tra, chạy, ghi `result.json` (nguyên tử):
   `{"nonce","ok","message","data","mutated"}`.
4. C++ kiểm tra `nonce` khớp rồi trả `ScriptResult{ok, mutated, message, json}`.

Dùng từ C++: `auto r = doc->callScript("page_margins", R"({"left_mm":30,"right_mm":15,"top_mm":20,"bottom_mm":20})");`

## Hàm hiện có
Đơn vị chiều dài: mm. Màu: `#RRGGBB`. Vùng: `A1` hoặc `A1:C10`. `sheet` bỏ trống = sheet đang chọn.

| Hàm | Loại | Đối số |
|---|---|---|
| `ping` | đọc | — (trả `api_version`, `kind`) |
| `read_page_margins` | Writer, đọc | — (trả lề từng kiểu trang đang dùng) |
| `read_doc_properties` | chung, đọc | — |
| `page_margins` | Writer | `left_mm,right_mm,top_mm,bottom_mm` (0–200; tổng lề < kích thước trang) |
| `page_setup` | Writer | `paper` (A3/A4/A5/Letter/Legal), `orientation` (portrait/landscape) |
| `header_footer` | Writer | `header,footer` (bool), `header_text,footer_text`, `footer_page_number` |
| `toc_insert` | Writer | `title`, `levels` (1–10) |
| `toc_update` | Writer | — |
| `paragraph_format` | Writer | `align`, `left_indent_mm,right_indent_mm,first_line_mm,space_before_mm,space_after_mm`, `line_spacing_percent` |
| `outline_numbering` | Writer | `levels` |
| `list_apply` | Writer | `kind` (bullet/numbered/none) |
| `cell_format` | Calc | `range`, `number_format` (general, number_0, number_2, percent, date_dmy, datetime, vnd, text), `font_name,font_size,bold,italic,bg_color,font_color,halign,wrap`, `border` (none/outline/all) |
| `data_validation` | Calc | `range`, `kind` (list/whole/decimal/textlength) + `items` hoặc `min,max`, `error_message`, `clear` |
| `conditional_format` | Calc | `range`, `rule`, `value[,value2]`, `bg_color,font_color,bold`, `clear` |
| `protect_sheet` | Calc | `protect` (bool), `password` |
| `sort_range` | Calc | `range`, `keys` (≤3: `{column,ascending}`), `has_header` |
| `text_to_columns` | Calc | `range` (một cột), `delimiter` (comma/semicolon/tab/space/pipe), `overwrite` — giữ giá trị dạng chữ, không đụng ô có công thức, không ghi đè dữ liệu nếu chưa cho phép |
| `doc_properties` | chung | `title,subject,author,description,keywords` |

## Thêm hàm mới
1. Viết `def ten_ham(doc, a)` trong `dung_api.py`: gọi `_only(a, (...))` rồi kiểm từng đối số bằng `_num/_int/_bool/_enum/_str/_color`;
   ném `ApiError("thông báo tiếng Việt")` khi sai; trả `{"message","data","mutated"}`.
2. Thêm vào `WHITELIST` (Python) **và** `Engine::scriptFunctions()` (C++, kèm cờ `readOnlySafe`).
   `check_whitelist.py` (chạy trong CI) báo lệch.
3. Thêm ca thử vào `test_dung_api_local.py` (pyuno trực tiếp) và `tools/smoketest/api_tests.cpp` (qua engine).
4. Cập nhật bảng ở trên.

## Kiểm thử
- `engine-build/dung-scripts/test_dung_api_local.py`: pyuno + soffice, gọi `run_call` trực tiếp (không qua LOK).
- `dung-smoketest --program <program> --api 1 --out <dir>`: qua engine thật và `runMacro` (Linux CI).
- `check_whitelist.py`: đối chiếu hai danh sách.
- Windows: bài thử chưa chạy tự động — xem phần "Chưa kiểm chứng" trong báo cáo nhánh `feat/dung-api`.
