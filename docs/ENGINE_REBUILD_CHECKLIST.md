# Tổng rà soát engine — cổng quyết định một đợt build (28/11/2026)

Bản đối chiếu main: 46c14efacd2ab87ca9914a43bf6d617023b704a9. Giữ nguyên engine đang hoạt động và checksum. Không đổi tag CI, không kích hoạt workflow build engine chỉ vì cập nhật tài liệu. Kết quả dưới đây là phân loại dựa trên mã, log và cấu hình, KHÔNG phải nghiệm thu máy thật.

## 1. Baseline và blocker quan trọng nhất

- `.github/workflows/ci.yml` đang ghim Windows `engine-26.8.3.1-headless`, SHA256 `03261237878855c27013ff81ba03d766fd794ff49f88d711a013747f7325cac1`. Linux dùng engine hệ thống; khác phiên bản/backend nên PASS một OS không thay OS kia.
- Engine v2 `engine-26.8.3.2-headless`: [run 45500311461](https://github.com/dungmao2023-art/DUngoff/actions/runs/45500311461) build và bước đóng gói/phát hành thành công. [Báo cáo cố định cd4e440](https://github.com/dungmao2023-art/DUngoff/tree/cd4e4408c903a48797c061fdc8d69487004c271b/round-1). Tự kiểm `dung-api-test.txt` thoát 1 vì `ModuleNotFoundError: No module named 'dung_api'`; workflow copy test vào /tmp trong khi module nằm share/Scripts/python, test chỉ thêm program và thư mục test vào sys.path. Không chứng minh UNO/provider không hoạt động; sửa đường tìm module, profile/endpoint riêng, kiểm checksum artifact đã có và chạy lại trước khi quyết định build. Không tự động sử dụng engine v2 cho người dùng.
- V2 đã cấu hình vi, myspell, Python nội bộ, release build; có vi_VN.aff/dic. Bộ cài app hiện phân phối từ điển GPL riêng có checksum; không cần đưa toàn bộ từ điển trở lại engine.
- #57 [CI 45901046539](https://github.com/dungmao2023-art/DUngoff/actions/runs/45901046539) thất bại giao tiếp dialog/tạo TOC; #49 là thí nghiệm cũ. #110 dán TSV không đạt, không bật GUI. #158 [CI 46079201540](https://github.com/dungmao2023-art/DUngoff/actions/runs/46079201540) Windows PASS, Linux sửa ô mở khóa và giữ range/dropdown PASS nhưng lỗi chính sách blank/stop; chưa xác định chính xác flag nào. #153 merged, [CI 46077805614](https://github.com/dungmao2023-art/DUngoff/actions/runs/46077805614) hai OS PASS. Không nâng stage từ audit này.

## 2. Font, tiếng Việt, render — không bỏ sót

| Nhóm | Bằng chứng hiện tại | Cổng quyết định |
|---|---|---|
| Font list/đổi font | #153 Windows log: commandValues CharFontName 142 họ; Word Alef, Excel Arial 16, PPT DejaVu Sans PASS; PDF xuất Times New Roman. Ribbon và Excel dialog dùng QFontDatabase, Word dùng danh sách engine. | APP: dùng danh sách engine làm chuẩn cho tài liệu, giữ font thực tại con trỏ, font mixed, chọn size/style/preview, thông báo font thiếu; không tự đổi tên font tài liệu. Test Qt/LOK font inventory và roundtrip 3 định dạng. |
| Font discovery | Windows log: Cannot load default config file, sau đó mở fc_local.conf; warning without FcInit. Fontconfig 2.17.1 override + FreeType hvf build patch, giữ Skia theo README. | RUNTIME trước: kiểm config root, font Windows user/system, thư mục engine, profile sạch, Unicode path, cache; chỉ patch SVP/fontconfig init nếu cấu hình đúng vẫn fail. |
| Font substitution | Engine.cpp seedFontSubstitution ép 6 biến thể Aptos -> Carlito Always=true; ghi cả máy có Aptos, notice xác nhận Always=false từng không đạt. Carlito tương thích số đo Calibri, không phải bảo đảm Aptos. | RUNTIME/ENGINE có điều kiện: phát hiện font thật, thay khi thiếu, giữ tên gốc, không đổi profile người dùng tùy tiện. Test có/không Aptos, Arial/Calibri/Times New Roman/Cambria, Vietnamese NFC/NFD, bold/italic, ký hiệu; so số trang, dòng, PDF font embedding và độ lệch layout. |
| Font nhúng | CI có test chặn lưu DOCX font nhúng để tránh mất. Không đồng nghĩa hỗ trợ chỉnh sửa. | FILTER có điều kiện: kiểm DOCX/PPTX có font nhúng, embedding rights, giữ relationship và byte font khi không chỉnh, không nhúng lại font không có quyền. Nếu mất, tiếp tục chặn; phân loại import/export trước patch. |
| Font mới cài / font miễn phí | Carlito, Liberation Sans kèm OFL notice; không phân phối font Microsoft. | APP/RUNTIME: test cài font user/system, restart/refresh an toàn; xem xét Liberation Serif/Mono, Noto (chỉ sau kiểm license/coverage/checksum), không cần biên dịch để thêm dữ liệu font. |
| Chính tả | script fetch-dict-vi đã pin checksum, dict_tests + spellprobe; gạch chân đỏ chưa chứng minh, bộ âm tiết khác Hunspell. | RUNTIME/APP trước: language vi-VN, kiểm lỗi đúng/sai và suggest thực, hai kiểu đặt dấu, thêm từ cá nhân; nếu spell callback/service không khả dụng mới patch engine hoặc dùng thư viện Hunspell độc lập có license phù hợp. |
| IME / DPI | TileView inputMethodEvent, imePlan và Document::postExtTextInput gọi postWindowExtTextInputEvent; preedit_tests.cpp có ca engine, chưa UniKey/EVKey thật. | APP trước; test preedit/commit/replace, Ctrl+Z, focus, DPI 100/125/150/200, nhiều tab. Chỉ patch API/backend khi tái hiện lỗi dưới engine trực tiếp. |
| Render/caret/tile | Windows SVP thiếu invalidation native được ghi trong WINDOWS_ENGINE.md; synthetic event workaround và CI PASS. | Đo latency/RAM/p95 và event native đối chiếu. Nếu vẫn chậm hoặc mất vùng, sửa callback SVP, không gỡ workaround cho tới khi đạt. |
| Đường dẫn dài, in, PDF export | App có bản làm việc và test recovery/print; chưa máy thật. | APP/RUNTIME trước; engine manifest/filter chỉ khi lỗi tái hiện độc lập; test Windows sạch, unicode path, printer A4, lỗi lưu/đầy đĩa. |

## 3. Chức năng chỉnh sửa — một bridge, không vá riêng từng nút

Ưu tiên API UNO đã có của LibreOffice. Giai đoạn 0: kiểm cả v1/v2 trên tài liệu ĐANG MỞ qua LOK (không chỉ tạo tài liệu trong soffice khác). Kiểm ABI và symbol, gọi trên engine thread/SolarMutex, xác định chính xác document/view, JSON có schema và allowlist thao tác, giá trị lỗi rõ, UndoContext, read-only, callback invalidation. Không expose macro URL tùy ý, socket public hoặc arbitrary UNO từ tài liệu. Không dùng file args chung cho hai tab. Nếu không có đường truy cập an toàn vào model sống thì mới thiết kế native bridge versioned trong engine, patch nhỏ và kiểm ABI v1/v2. Không chuyển qua process khác bằng cách save/reopen rồi gọi đó là chỉnh realtime.

| Chức năng | API / hướng | Phân loại |
|---|---|---|
| Word tách ô, chiều rộng/cao, ảnh/anchor/wrap | XTextTableCursor.splitRange, thuộc tính table/cell và graphic object | Bridge nếu LOK không đủ, UI Qt kéo/preview riêng |
| Word TOC mới / heading / styles / section / lề / page bands | DocumentIndex, XDocumentIndexesSupplier, PageStyles | #57 blocker dialog; bridge hoặc dialog transport; giữ refresh native có sẵn |
| Word sửa/xóa comment / Review | Annotation fields, native review command, selection ID, Undo | App/bridge tùy probe, #159 đã có CI; không xóa nhầm bình luận |
| Excel dán vùng/copy/cut/paste special | LOK clipboard/selection, UNO XCellRangeData + công thức/kiểu theo từng ca | #110 fail; bridge nếu clipboard không đủ, không setDataArray đè công thức/format |
| Excel fill handle | native fill series/selection/mouse; app geometry/drag | App trước; bridge nếu không thể chọn vùng/dispatch chính xác |
| Excel validation/protection/conditional formatting | TableValidation, XProtectable, conditional formats | Bridge tạo/sửa; #158 filter flag Linux fail cần diff XML và so cùng engine trước khi patch |
| Excel chart | Chart2/series/source/title/type | Bridge nếu dialog không hoạt động; chart native cột và refresh đã có |
| Excel PivotTable | XDataPilotTables.insertNewByName, descriptor, row/col/value/sum/count | Bridge nếu LOK không đủ; fixture output/cache/refresh XLSX thật |
| PPT select/move/resize/rotate/align/group/z-order | Drawing XShape/XShapes/XShapeGrouper, position/size, selection | App geometry + bridge; giữ Undo và group/animation refs |
| PPT master/theme/layout | draw pages/master interfaces, OOXML roundtrip | Bridge/filter có điều kiện, không tạo master giả |
| PPT animation playback/transition | XPresentation2/XSlideShowController/XSlideShowView, engine thực | SlideShow.cpp chỉ paintPartTile ảnh tĩnh. Probe trình chiếu native/headless và surface/callback; nếu cần patch xuất frame theo clock/click, làm chung lần rebuild; UI Qt hiện tại không tự có playback |
| VBA/macro/XLSM, Power Query | kiểm filter preserve project/connection; macro mặc định tắt | B2; chưa chứng minh, không bật macro hoặc quảng cáo M đầy đủ. Chỉ gom filter patch đã tái hiện và có fixture. |

Tham chiếu upstream: https://api.libreoffice.org/docs/idl/ref/interfacecom_1_1sun_1_1star_1_1text_1_1XTextTableCursor.html ; https://api.libreoffice.org/docs/idl/ref/interfacecom_1_1sun_1_1star_1_1sheet_1_1XDataPilotTables.html ; https://api.libreoffice.org/docs/idl/ref/interfacecom_1_1sun_1_1star_1_1presentation_1_1XPresentation2.html . API tồn tại không chứng minh có thể gọi từ runtime LOK hiện tại.

## 4. Việc độc lập không cần rebuild LibreOffice

- Qt PDF selection/search rectangle trực tiếp, zoom/DPI, copy, mật khẩu; Qt API QPdfDocument/QPdfSelection/QPdfSearchModel. https://doc.qt.io/qt-6/qpdfselection.html ; https://doc.qt.io/qt-6/qpdfsearchmodel.html . Kiểm version Qt pin trước triển khai.
- PDFium/PoDoFo: form/annotation/page object, sửa text gốc giới hạn, redaction/chữ ký riêng. Đánh giá license, ABI, checksum, bảo toàn font/vector/link/form/chữ ký, lưu bản sao; không gọi overlay là sửa gốc.
- Tesseract Windows + vie/eng: runtime độc lập, đóng gói tùy chọn offline, checksum/notice, OCR thật và searchable PDF; Linux đã PASS. Không rebuild LO.
- Power Query recipe/CSV/đối soát/MISA fixture; kho mẫu, kho luật, note/calendar, contact, UI ribbon/tab; cài đặt, license, font data. Không đụng engine nếu không có lỗi filter cụ thể.

## 5. Gate duy nhất trước khi bấm build

1. Xác minh asset v1/v2 (tag, checksum, cấu hình, ABI, license) và test lại v2 đã có, không đổi pin CI main. Sửa probe import và chứng minh model live trên đúng tài liệu/Undo/save; nếu chạy được, không cần rebuild bridge. Chạy font runtime và IME native trước.
2. Chạy ma trận Word/Excel/PPT nêu trên, ghi PASS/FAIL/NOT RUN, log, engine SHA và fixture; test font inventory/real vs fallback/embedded/print, slideshow playback, callback. Không kết luận từ việc gửi UNO không có output. Kiểm #158 cụ thể flag và khác phiên bản Linux.
3. Chỉ gom những blocker đã tái hiện thành patch thống nhất: native bridge/ABI, dialog/clipboard/render/IME, fontconfig discovery/fallback, slideshow surface, filter fidelity (nếu cần). Đọc upstream đúng tag trước sửa; giữ headless, Python, localization, Skia và module đang dùng, không đổi nền Collabora/LibreOffice đồng thời nếu không có căn cứ. Không thêm dependency chưa xác minh.
4. Gate build: mỗi patch có test thất bại trên baseline, acceptance, rollback. Nếu còn NOT RUN ảnh hưởng cấu hình, chưa chốt build. Một ứng viên tag riêng, tối đa 3 lượt checkpoint theo quy định cũ, SHA256, log, không tự public release. Không ghi đè v1. Không kích hoạt build từ PR audit.
5. Gate chuyển pin: Windows/Linux CI đầy đủ + fixture roundtrip + 158 mẫu hiện có + font/IME/Undo/playback/GUI Windows sạch; xác minh MS Office, không mất dữ liệu. Chỉ đổi pin sau kết quả. Nếu thất bại, giữ v1 và ghi blocker, không đánh dấu accepted.

## 6. Kế hoạch bảo toàn và build một đợt — quyết định 28/11

**Bảo toàn bản hoạt động trước bất kỳ build/chuyển pin:** giữ nguyên release v1, checksum ghim và bản cài đang chạy. Script `scripts/backup-engine.ps1` tải đúng archive v1, đối chiếu SHA256 ghim, kiểm 7z, ghi manifest và lưu sang thư mục riêng theo tag, từ chối ghi đè. Ví dụ lưu ổ khác: `./scripts/backup-engine.ps1 -BackupRoot "D:\\DungOffice-Engine-Backup"` (thay đường dẫn bằng ổ thật có đủ dung lượng; GitHub CLI đã xác thực repo riêng). Workflow `engine-preflight.yml` lưu bản sao archive/manifest thành artifact độc lập 30 ngày; đây không phải backup vĩnh viễn. Chỉ coi backup đã có khi job kiểm checksum + upload PASS; tải artifact về ổ riêng hoặc lưu archive qua script để bảo quản lâu dài. Không xóa release v1/v2. Không truy cập hay sao lưu dữ liệu cá nhân của người dùng.

**Preflight, không biên dịch:** kiểm digest v2 từ metadata asset phát hành; thiếu digest hoặc sai hash thì dừng, không tự tin cậy hash mới tính. Giải nén v2 riêng, không vào build/lo của v1. Probe `test_dung_api.py` nạp đúng module shipped trong share/Scripts/python, dùng pipe ngẫu nhiên + profile/temp riêng, kiểm cả 4 lề trong DOCX của gọi UNO trực tiếp và provider, không đóng process soffice khác. PASS standalone UNO chỉ chứng minh API ngoài LOK; chưa chứng minh thao tác model sống. PR audit không thay engine-windows.yml/autogen.input/CI engine pin. Bằng chứng backup và probe hiện đang chờ workflow; chưa chạy build.

| Gate | Điều kiện đóng | Nếu không đạt |
|---|---|---|
| G0 Bảo toàn | Backup archive v1 khớp SHA256, 7z test, manifest và nơi lưu độc lập; xác nhận có thể giải nén/mở thử từ backup | Dừng trước build mới; không ghi đè bản hoạt động |
| G1 V2 sẵn có | Xác minh SHA, runtime và standalone provider; thử lại live LOK trên v1/v2 | Phân biệt lỗi cài đặt/bridge/ABI với lỗi engine; không build vì lỗi setup |
| G2 Đóng scope | Tất cả ma trận mục 2–4 có PASS/FAIL/NOT RUN + SHA/fixture; blocker mỗi mục có nguyên nhân, test tái hiện và phương án app/runtime/engine | NOT RUN ảnh hưởng cấu hình thì chưa chốt; vấn đề độc lập chuyển về app |
| G3 Patch freeze | Gom patch bridge, dialog, font, render, IME, playback, filter nếu chứng minh cần; kiểm upstream đúng tag, compile nhỏ, license/manifest/checksum; không thay nền/cắt module tùy tiện | Sửa patch trước build tốn kém |
| G4 Build | Một candidate tag mới không trùng v1/v2; thủ công khi G0–G3 đạt; tối đa 3 lượt có checkpoint, không tự phát hành; test bắt buộc fail thì không promote | Giữ v1; giữ log và ứng viên lỗi để phân tích, không vòng lặp build vô hạn |
| G5 Chuyển engine | CI đúng SHA, DOCX/XLSX/PPTX fixtures, 158 mẫu, font fallback/IME, UNO/Undo, animation, không mất dữ liệu, thử Windows sạch; bản cài rollback còn sẵn | Không đổi pin, không gọi là hoàn thành; khôi phục v1 từ archive độc lập |

**Yêu cầu trước build mới:** sửa workflow engine để chỉ chạy thủ công khi freeze, tách package candidate khỏi phát hành và chặn promote nếu test fail (hiện workflow cũ continue-on-error với UNO probe; chưa thay trong PR audit để tránh kích hoạt push build). Không tự động nâng version ứng dụng hoặc xóa archive cũ. Các chức năng chưa có UI vẫn cần lập trình sau khi bridge đạt, và PDF/OCR không chờ LibreOffice.

## 7. Quyết định thử v2 trước (28/11, chủ dự án đồng ý)

- [Preflight 46084197395](https://github.com/dungmao2023-art/DUngoff/actions/runs/46084197395): tải và kiểm SHA256/7z v1 PASS, [artifact rollback 15237803289](https://github.com/dungmao2023-art/DUngoff/actions/runs/46084197395/artifacts/15237803289) 170667831 bytes, retention 30 ngày, chưa chứng minh mở ứng dụng từ bản sao độc lập. Release v1 giữ nguyên.
- V2 asset 789258177, digest `cb7664001dfb51a31467f2b1849e294e6564d607a91baea5c4efa215010ad492` khớp khi tải. UNO trực tiếp PASS 4 lề DOCX, provider FAIL `AttributeError: 'NoneType' object has no attribute 'StyleFamilies'` (đối tượng tài liệu không có trong script context); không còn lỗi import. Không suy diễn lỗi này thành yêu cầu rebuild.
- PR #160 chuyển pin CI Windows sang v2 trên nhánh thử nghiệm (commit 6ed1001022b0137d1125458572637614ee90f2c8), cache tách theo tag/checksum, khóa bước release. Main vẫn v1. Linux vẫn dùng LibreOffice hệ thống; không gọi kết quả Linux là v2.
- Thêm ma trận standalone UNO (d376110bd30b0302c309cc95ee4ae48d7d97089c, workflow 0eb55ef43c87cf8315ad8ce112072276b9a6e317): split bảng Word, tạo TOC, validation + bảo vệ Calc, tạo PivotTable, nhóm hình Impress. Đọc DOCX/XLSX/PPTX thực; lỗi riêng từng case, không che fail. Kết quả đang chờ. Đây không phải live LOK, chưa có giao diện tính năng; cần kiểm Undo/selection/document identity sau khi chọn bridge.
- Header LOK hiện có getPresentationInfo/createSlideRenderer/renderNextSlideLayer/postSlideshowCleanup, chưa chứng minh ABI/runtime hoặc playback. Ưu tiên probe v2 trước khi viết patch slideshow. Kiểm lại font/IME/clipboard/dialog theo ma trận; không thay workflow biên dịch engine, không build v3 hoặc phát hành installer.

## 8. V2 CI và hệ thống font — lượt tiếp theo

- [CI 46085029776](https://github.com/dungmao2023-art/DUngoff/actions/runs/46085029776) head def4ab3a5c1d341700fc2aa6b4154097e451dde1: Windows v2 PASS, Linux engine hệ thống PASS. [Preflight 46085029856](https://github.com/dungmao2023-art/DUngoff/actions/runs/46085029856): split Word, validation/protection Calc, PivotTable thật, group Impress và direct margins PASS; TOC FAIL (log cũ thiếu exception), provider context FAIL. Không chuyển pin main.
- App đã dùng danh sách .uno:CharFontName JSON theo tài liệu, cache theo tab, Ribbon Word/Excel/PPT, dialog Word/Excel refresh; fallback Qt có tooltip chưa xác minh; giữ tên font đang dùng nếu thiếu, không gửi lệnh chọn font thiếu như font mới. Test UI regression kiểm không lẫn font tab, không tự phát lệnh khi refresh, chặn font thiếu. Code đến 95fb70c9230f9048b51af97d32b239053112be37. Chờ CI mới; chưa xử lý fontconfig, Aptos ép thay thế, font nhúng hoặc font install hot refresh toàn bộ.
- Probe TOC thay thứ tự gắn index trước cấu hình, ghi từng bước và traceback để xác định lỗi thật; chưa coi TOC thành công. Live LOK bridge vẫn chưa đạt; không rebuild v3.
 
## 9. Quyết định chuyển mặc định sang v2 (28/11/2026)

Chủ dự án yêu cầu dùng v2 cho toàn bộ bản build tiếp theo, chỉ cần giữ v1. Main commit [5d37239](https://github.com/dungmao2023-art/DUngoff/commit/5d3723973bebe32f5d408992bc7e322552d8db89) đổi duy nhất ENGINE_TAG/SHA256 trong CI sang v2. Đối chiếu mã ứng dụng tại main cũ với head def4ab3: không có khác biệt nguồn ứng dụng, CI Windows v2/Linux hệ thống 46085029776 PASS; CI main mới vẫn cần xác nhận. Không phát hành bộ cài, không chạy build LibreOffice. Release v1 và archive rollback 15237803289 vẫn giữ; backup artifact 30 ngày, cần lưu riêng dài hạn. Các điều kiện chuyển pin cũ trong tài liệu là lịch sử và được thay bằng quyết định này; điều kiện nghiệm thu tính năng/merge PR không thay đổi. PR #160 vẫn chưa đạt toàn bộ kiểm thử, TOC lỗi tại gán ParaStyleName Text Body (run 46087396879), provider context vẫn FAIL. Không coi các API standalone là tính năng UI đã hoàn thành. Linux tiếp tục dùng engine hệ thống, v2 là gói Windows. Không đổi các tag release lịch sử hoặc tự sửa bản cài trên máy người dùng. 

## 10. Kết luận audit

Có blocker thực ở giao tiếp model/dialog và font fallback; chưa có đủ bằng chứng bắt buộc rebuild ngay. Engine v2 đã có, probe import đã sửa; direct UNO PASS, provider context FAIL; đang thử ma trận và CI ứng dụng trên v2. Danh sách này là scope gom một đợt nếu cần, không phải tuyên bố đã sửa hoặc hoàn tất kiểm thử. Các mục engine-independent phát triển song song. Không tăng trọng số/stage, không public release.

---

## Lịch sử kế hoạch cũ (giữ nguyên để đối chiếu)

# Danh sách kiểm trước khi build lại engine (một lần)

Quyết định của tác giả (25/11/2026): build lại engine **một lần**, và lần này phải gỡ được các vấn đề
đã biết. Tài liệu này là danh sách chốt trước khi bấm build. Mỗi mục ghi rõ: đã có bằng chứng hay chưa.

Quy tắc build (không đổi): tối đa 3 lượt tự động, checkpoint có SHA-256, dừng khi lỗi tài nguyên/biên dịch,
log từng lượt, không chạy thêm lượt âm thầm. Thời gian tham khảo từ các lần trước: 1,5–3 giờ nếu thuận lợi.

## Cấu hình hiện tại (engine-build/autogen.input)
`--enable-headless --enable-python=fully-internal --enable-mergelibs`, tắt: Java, Help, myspell dicts,
Base/Firebird, report builder, BeanShell/JavaScript scripting, galleries.

## Mục cần quyết (điền kết quả thí nghiệm vào cột "Bằng chứng")

| # | Nhu cầu | Cách giải không build lại | Chỉ build lại nếu | Bằng chứng |
|---|---|---|---|---|
| 1 | Đặt lề trang, đầu/chân trang, kiểu trang, thuộc tính tài liệu | Cầu nối macro Basic/Python (thí nghiệm exp/basic-bridge) | Không cầu nối nào chạy: vá LOK để lộ API UNO | chờ |
| 2 | Lệnh cần hộp thoại (Pivot, định dạng ô, đoạn văn, biểu đồ…) | Dựng hộp thoại Qt từ JSON dialog của LOK (nếu engine đã phát) | Engine không phát JSON dialog; bật/vá | chờ (exp/command-matrix) |
| 3 | Nhãn tiếng Việt trong hộp thoại/thông báo engine | — | Cần `--with-lang=vi` (chỉ khi dùng JSON dialog hoặc hiện thông báo engine) | chờ |
| 4 | Chữ đang soạn Telex (preedit) | `postExtTextInputEvent` của LOK | API không dùng được | chờ |
| 5 | Engine không phát INVALIDATE_TILES / CURSOR_VISIBLE (Windows, Writer) | Đã có cách tránh SYNTH ở app | Muốn gốc rễ: vá backend svp (không bắt buộc) | đã vá ở app |
| 6 | Chính tả tiếng Việt | Thêm từ điển Hunspell vi làm dữ liệu (kiểm giấy phép) | Không cần | chưa làm |
| 7 | Đường dẫn dài ≥250 ký tự | Vá ở app (thư mục tạm ngắn, tiền tố `\\?\`) | Cách vá app không chạy: manifest longPathAware cho engine | đang làm (feat/recover) |
| 8 | Tính năng mất khi lưu (track changes, bình luận, Pivot, SmartArt…) | Cảnh báo/chỉ đọc ở app | Mất tính năng quan trọng do lỗi engine | chờ (exp/feature-fidelity) |
| 9 | Dung lượng bộ cài (184 MB) | Bỏ module không dùng khi build (Math, Base, Draw?) | — (tuỳ chọn) | chưa đo |

## Cập nhật 25/11/2026 (người dùng chốt build, không chờ hết thí nghiệm)
Build v2 (tag `engine-26.8.3.2-headless`) gồm: `--with-lang=vi`, từ điển Hunspell vi, `--enable-release-build`,
giữ Python nội bộ, thư viện script `dung_api.py` (đặt lề/đầu-chân trang qua UNO) kèm bài tự kiểm. Chi tiết: `engine-build/CHANGES-v2.md`.
Không cắt module trong lượt này (chưa kiểm chứng cờ an toàn). Mục 2 (JSON dialog) và 4 (preedit) giải ở app nếu engine đã hỗ trợ.

## Quy trình quyết định
1. Chờ ba thí nghiệm (cầu nối macro, ma trận lệnh, toàn vẹn tính năng).
2. Điền bảng trên; chỉ các mục "Chỉ build lại nếu" đã thành sự thật mới đưa vào patch/cấu hình.
3. Chuẩn bị patch trong `engine-build/overrides/`, kiểm tra cú pháp ở CI nhẹ trước khi bấm build.
4. Build tối đa 3 lượt, mỗi lượt có log; kiểm lại bằng bộ kiểm thử engine hiện có (word/excel/ppt/pdf, fidelity 158 mẫu, input).
