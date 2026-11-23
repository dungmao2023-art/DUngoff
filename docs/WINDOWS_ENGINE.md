# Nhúng engine trên Windows: kết quả kiểm chứng và hướng đi

Tài liệu này ghi lại những gì đã **chạy thật trên CI Windows** (windows-2022, LibreOffice 26.8.3 bản
chính thức), không phải suy đoán. Log và dump ngăn xếp nằm ở nhánh `ci-report`.

## Đã chứng minh chạy được trên Windows

| Bước | Kết quả |
|---|---|
| Tải MSI chính thức, kiểm tra SHA-256, giải nén administrative install | ✅ |
| Nạp `mergedlo.dll` (bản Windows dùng merged libs) | ✅ sau khi thêm thư mục engine vào đường tìm DLL |
| `lok_init` + `getVersionInfo` (26.8.3.1) | ✅ |
| Mở XLSX (tên tệp/thư mục tiếng Việt), nhận loại, đếm sheet, tên sheet | ✅ |

## Các vấn đề đã gặp và cách xử lý

1. **Treo deadlock khi gọi `documentLoad` từ luồng ứng dụng.** Dump cho thấy luồng gọi giữ SolarMutex
   rồi `SetWindowPos` (SendMessage) vào cửa sổ ẩn của luồng "VCL Main", trong khi VCL Main chờ SolarMutex.
   → **Đã sửa:** mọi lệnh gọi LOK được chạy *trên luồng VCL Main* qua hook `WH_GETMESSAGE`
   (`engine/src/Engine.cpp`, `getMsgHook`).
2. **Chế độ unipoll (`SAL_LOK_OPTIONS=unipoll`) không dùng được với bản chính thức:** `runLoop` trả về
   sau 0–1 ms (Desktop::Main thoát ngay), sau đó `documentLoad` lỗi truy cập bộ nhớ (0xC0000005).
   Vẫn giữ trong mã (`DUNG_ENGINE_MODE=unipoll`) để thử nghiệm; trên Linux chế độ này đạt 46/46.
3. **Render tile trả về bộ đệm trống (đen hoàn toàn) và không có callback (con trỏ ô…).**
   Nguyên nhân: bản Windows chính thức dùng backend VCL Win32 (`vclplug_winlo`). Việc render tile của LOK
   vào bộ đệm của ứng dụng cần backend **headless (`svp`)**, chỉ có khi LibreOffice được build với
   `--enable-headless` (`vcl/source/app/salplug.cxx`: `#if ENABLE_HEADLESS`). `configure.ac` có hỗ trợ
   tuỳ chọn này cho Windows (tự build fontconfig/freetype), nhưng bản phát hành của TDF không bật.
   Trên Linux, bản LibreOffice của Ubuntu có headless nên mọi thứ chạy.

## Kết luận

Với **bản LibreOffice Windows chính thức**, Dũng Office mở, đọc, chuyển đổi và lưu được tài liệu,
nhưng **không hiển thị/chỉnh sửa trực tiếp** bằng giao diện riêng được. Muốn có giao diện riêng trên
Windows (mục tiêu của dự án), cần **tự build engine LibreOffice cho Windows với `--enable-headless`**,
đúng như Collabora làm cho ứng dụng desktop Windows của họ. Đây vốn là "Giai đoạn 3: Engine gọn"
trong lộ trình, nay phải làm sớm hơn, và cũng là dịp bỏ các module không cần (Base, Math, Java, trợ giúp…).

### Phương án build engine tuỳ biến

| Phương án | Ưu | Nhược |
|---|---|---|
| GitHub Actions (runner Windows miễn phí, 4 nhân, giới hạn 6 giờ/job) | Không tốn tiền, tự động | Build lần đầu LibreOffice mất ~3–6 giờ, dễ chạm giới hạn 6 giờ; cần chia nhỏ/cache |
| Self-hosted runner trên máy Windows của bạn (khuyên ≥ 8 nhân, 16 GB RAM, 100 GB trống) | Nhanh, lần sau build tăng dần | Cần máy bật khi build |
| Runner lớn trả phí của GitHub | Nhanh, tự động | Tốn phí |

Cấu hình dự kiến (chưa chạy thử): `--enable-headless --disable-java --disable-database-connectivity
--disable-extension-integration --without-help --without-myspell-dicts --disable-firebird-sdbc
--enable-release-build --enable-mergelibs` trên Cygwin + Visual Studio 2022, theo hướng dẫn chính thức
https://wiki.documentfoundation.org/Development/BuildingOnWindows .
