# Ghi đè module khi build engine

Áp dụng lên mã nguồn LibreOffice `libreoffice-26.8.3.1` trước khi build (xem `apply-overrides.sh`).

## external/fontconfig

- **Lý do:** LibreOffice 26.8 dùng fontconfig 2.18.3, nhưng các tệp cấu hình Windows cho chế độ headless
  (`external/fontconfig/configs/wnt_fontconfig_config.h`, ...) vẫn viết cho 2.16. Khi build
  `--enable-headless` trên Windows thì lỗi: `fontconfig.h(79): error C2018: unknown character '0x40'`,
  `fcint.h: #error no vsnprintf function implemented` (log lượt 1, nhánh `ci-report-engine`).
  Bản LibreOffice chính thức cho Windows không build nhánh mã này nên không lộ lỗi.
- **Cách sửa:** dùng nguyên thư mục `external/fontconfig` và dòng `FONTCONFIG_*` trong `download.lst` từ nhánh
  `distro/collabora/co-26.04` của chính kho LibreOffice core (commit `c1013fad0d38683630b48c01b6d5208b8d81181e`), dùng fontconfig 2.17.1.
  Collabora build headless trên Windows bằng cấu hình này.
- **Giấy phép:** MPL-2.0 (giống phần còn lại của LibreOffice).
- **Việc tiếp theo:** báo lỗi lên upstream (cập nhật cấu hình wnt cho fontconfig 2.18) rồi bỏ ghi đè này.

## external/freetype (sửa 1 dòng, giữ phiên bản upstream)

- **Lý do:** LibreOffice 26.8 nâng freetype lên 2.15.0, bản này có thêm driver `hvf`
  (`FT_USE_MODULE(... hvf_driver_class)` trong `ftmodule.h`). Danh sách tệp build Windows
  (`external/freetype/StaticLibrary_freetype.mk`) chưa thêm `src/hvf/hvf`, nên khi link `cairo.dll` bị lỗi
  `unresolved external symbol hvf_driver_class` (log lần build 2, nhánh `ci-report-engine`).
- **Cách sửa:** `apply-overrides.sh` thêm dòng `src/hvf/hvf` (tệp gộp một nguồn của freetype, cùng kiểu với
  `src/cff/cff`) vào danh sách. Không đổi phiên bản freetype.
