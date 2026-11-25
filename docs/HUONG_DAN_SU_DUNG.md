# Hướng dẫn sử dụng Dũng Office

Tài liệu này chỉ mô tả những gì **có trong mã nguồn và bộ kiểm thử hiện tại**. Phần nào chưa làm, chưa kiểm trên máy thật hoặc có giới hạn đều được ghi rõ. Dũng Office là bản **thử nghiệm sớm (v0.1)**: hãy dùng trên bản sao tài liệu quan trọng.

Cách kiểm trên máy thật: xem [KICH_BAN_KIEM_TRA_THIET_BI_THAT.md](KICH_BAN_KIEM_TRA_THIET_BI_THAT.md).

## 1. Cài đặt

1. Tải bộ cài `DungOffice-Setup-<phiên bản>-x64.exe` ở trang phát hành của dự án trên GitHub (kho `dungmao2023-art/DUngoff`).
2. **Bộ cài chưa được ký số.** Windows SmartScreen có thể cảnh báo "Windows protected your PC". Đây là hành vi bình thường với phần mềm chưa mua chứng thư ký số; nó không có nghĩa tệp bị hỏng, nhưng bạn nên kiểm tra tệp như mục 1.1 trước khi chạy.
3. Chạy bộ cài. Mặc định cài vào `%LOCALAPPDATA%\Programs\DungOffice` (không cần quyền quản trị).
4. Loại cài đặt:
   - **Cài đầy đủ** (mặc định): gồm thành phần "Từ điển chính tả tiếng Việt (GPL v2)".
   - **Cài tối thiểu**: không có từ điển.
   - **Tuỳ chọn**: tự chọn thành phần.
5. Tuỳ chọn lối tắt (Start Menu, màn hình nền) và **liên kết tệp** `.docx`, `.xlsx`, `.pptx`, `.pdf` (đều **không chọn sẵn**). Dù bạn chọn, Windows 10/11 vẫn để **bạn** quyết định phần mềm mặc định: bộ cài chỉ thêm Dũng Office vào danh sách "Mở bằng". Muốn đặt mặc định: chuột phải tệp → Mở bằng → Chọn ứng dụng khác → Luôn dùng ứng dụng này.

> Lưu ý về từ điển: từ điển chính tả tiếng Việt được cài vào thư mục riêng `dictionaries\vi` và hiện **chưa được nối vào giao diện soạn thảo** (chưa có gạch đỏ lỗi chính tả dựa trên nó). Giấy phép của từ điển ghi trong `THIRD_PARTY_NOTICES.md` và có chỗ cần luật sư xác nhận trước khi phát hành rộng.

### 1.1 Kiểm tra SHA-256 của bộ cài

Mỗi bản phát hành dự kiến đính kèm tệp `SHA256SUMS.txt`. Mở PowerShell trong thư mục tải về:

```powershell
Get-FileHash .\DungOffice-Setup-0.1.0-x64.exe -Algorithm SHA256
# hoặc: certutil -hashfile DungOffice-Setup-0.1.0-x64.exe SHA256
```

So chuỗi kết quả với dòng tương ứng trong `SHA256SUMS.txt`. Khớp nghĩa là tệp đúng như người phát hành đã đăng; **không khớp thì đừng chạy**. SHA-256 chỉ phát hiện tệp hỏng hoặc bị thay giữa đường; nó không chứng minh người đăng là ai (việc đó cần chữ ký số, dự án chưa có). Chi tiết: [RELEASE.md](RELEASE.md).

### 1.2 Cập nhật

Mở **Dự án › Update**. Ứng dụng chỉ kết nối mạng khi bạn bấm **Kiểm tra bản mới** (hoặc khi bạn tự tích kiểm tra hằng ngày, mặc định **tắt**). Bản tải về được kiểm SHA-256; nếu bản phát hành không có tệp SHA-256, ứng dụng **từ chối tự tải** và bảo bạn tải thủ công. Bộ cài chạy riêng sau khi bạn xác nhận và nên đóng tài liệu trước.

## 2. Nguyên tắc an toàn dữ liệu (đọc kỹ)

Những quy tắc này nằm trong engine, không chỉ ở giao diện (chi tiết và kiểm thử: [DATA_SAFETY.md](DATA_SAFETY.md)):

- **Tệp gốc không bao giờ bị mở trực tiếp.** Ứng dụng làm việc trên một bản sao tạm; tệp gốc không bị khoá hay sửa.
- **Mặc định là "Lưu thành" tệp mới** (ví dụ `Tên - Dũng Office.docx`). Muốn ghi đè tệp gốc phải xác nhận rõ, và bản sao lưu `tên.dung-backup-YYYYMMDD-HHMMSS.ext` được tạo *trước khi* thay.
- **Ghi an toàn:** ghi ra tệp tạm, kiểm tra cấu trúc, rồi mới thay thế. Lỗi giữa chừng không làm hỏng tệp đích.
- **Định dạng chưa bảo toàn được thì chỉ đọc:** `.xls .xlsm .xlsb .csv .doc .ppt .rtf .txt .pdf` mở chỉ đọc kèm cảnh báo. Ghi được: `.xlsx .ods .docx .odt .pptx .odp`.
- **Xuất PDF có mất mát nên phải xác nhận.**
- **Nhập CSV không ép kiểu:** mọi cột là văn bản, "00123" giữ nguyên.
- **Tự lưu bản khôi phục** mỗi 2 phút vào thư mục dữ liệu của app (không bao giờ vào tệp của bạn). Xem mục 8.

### Macro (VBA) — quan trọng

- **Dũng Office không bao giờ chạy macro** trong tệp của bạn (`.xlsm`, `.docm`…). Nút bấm gắn macro vẫn có thể hiện trong tệp nhưng **không hoạt động**.
- Tệp `.xlsm` mở **chỉ đọc**.
- **Lưu sang `.xlsx` làm MẤT macro** (đã đo với một tệp `.xlsm` thật: bản `.xlsx` không còn `xl/vbaProject.bin`). Tệp gốc không bị đổi nên macro vẫn còn trong tệp gốc.
- Nếu bạn cần macro hoạt động, hãy tiếp tục dùng Excel/Word của Microsoft cho tệp đó.

### Cảnh báo khi tệp có thành phần chưa bảo toàn được

Khi mở tệp có thành phần mà bản lưu có thể làm mất hoặc đổi (ví dụ macro, một số đối tượng nhúng), app hiện cảnh báo và để bạn quyết định, thay vì âm thầm lưu. Hiệu ứng chuyển cảnh/hoạt ảnh của PowerPoint **chưa được hỗ trợ chỉnh sửa và chưa bảo đảm giữ nguyên** (app có thông báo riêng).

## 3. Giao diện chung

Một cửa sổ, thanh bên gồm **Trang chủ**, **Mẫu**, **Dự án**, **Công cụ**; mỗi tài liệu mở thành một thẻ. Mở tệp bằng kéo thả, từ Trang chủ (gần đây) hoặc nhấp đúp tệp trong Explorer: nếu app đang chạy, tệp được chuyển sang cửa sổ cũ (chỉ một phiên chạy).

- **Mẫu**: thư viện mẫu có sẵn để tạo tài liệu mới.
- **In**: hộp thoại in có xem trước đúng tệp sẽ gửi tới máy in; chọn máy in, khoảng trang (ví dụ `1-3, 5, 8-`), số bản, khổ giấy, hướng giấy, in hai mặt, đen trắng. Nếu bản dựng không có xem trước tích hợp, app mở bản in (PDF) bằng trình xem PDF của máy để bạn in từ đó.

## 4. Word (.docx, .odt)

- Soạn thảo, phông chữ, cỡ chữ, màu chữ/nền, kiểu đoạn (Heading…), chèn bảng, ảnh, liên kết, tìm/thay thế.
- **Tiện ích Việt** (thẻ ribbon): *Số tiền → chữ*, *Chèn ngày Việt*, *TCVN3 → Unicode*. Mỗi thao tác hiện **xem trước** và hỏi trước khi chèn; nếu không đổi được thì không chèn gì.
- **Lề trang**: *Lề chuẩn VN* (thể thức văn bản hành chính, Nghị định 30/2020/NĐ-CP), *Lề thường*, *Lề hẹp*.
  - Trên Windows, engine hiện tại không nhận lệnh đổi lề trực tiếp. Vì vậy app làm bằng cách **tạo bản sao** tài liệu với lề mới (sửa `w:pgMar` trong XML) rồi **mở bản sao trong thẻ mới**, sau khi bạn xác nhận. Thẻ cũ và tệp gốc không bị đổi.
  - Thẻ mới **không giữ lịch sử hoàn tác (Undo)**, mở ở đầu tài liệu (không giữ vị trí cuộn) và đánh dấu "chưa lưu" nên Lưu sẽ hỏi tên mới.
  - Đã kiểm tự động trên Windows: lề trong tệp lưu khớp lề đặt, chữ giữ nguyên, 156/156 tệp mẫu. **Phần giao diện (hộp thoại, thẻ mới) chưa được người nào bấm thử.** Chỉ làm cho `.docx`, chưa làm cho `.odt`.
- Round-trip DOCX đã kiểm ở mức engine; chưa có bộ kiểm riêng cho mọi tính năng Word (theo dõi thay đổi, mục lục, trường tự động…): hãy kiểm tra bản lưu trước khi gửi đi.

## 5. Excel (.xlsx, .ods)

- Nhập/sửa ô, công thức, định dạng ô (phông, cỡ, đậm/nghiêng, màu, căn lề, xuống dòng), định dạng số (`%`, `#,##0`…), sắp xếp, chèn hàng (công thức tham chiếu dịch theo), tìm & thay thế (không sửa bên trong công thức), xoá sheet luôn hỏi xác nhận.
- Đã kiểm với một tệp thử 5000 dòng, 25.130 công thức: mở 0,4 giây (Linux), giá trị và bảng, biểu đồ, định dạng có điều kiện, xác thực dữ liệu, cố định ô giữ nguyên. Có các khác biệt đã biết (DS-001…DS-004) liệt kê trong [DATA_SAFETY.md](DATA_SAFETY.md); ví dụ ô `TRUE` nhập tay có thể thành công thức `=TRUE()`, thanh dữ liệu (data bar) chưa kiểm bằng Excel thật.
- **Chưa có / chưa kiểm:** viền ô, tiền tệ VND (engine đang ở locale en-US), pivot, slicer, công thức mảng/động (FILTER, XLOOKUP…), chú thích dạng thread của Excel 365, tên định nghĩa, nhóm hàng/cột, bảo vệ sheet. Excel xử lý các thứ này thế nào sau khi lưu bằng Dũng Office **chưa được chứng minh**.
- Công thức được **tính lại** khi mở tệp không do Excel lưu: nếu giá trị cache trong tệp gốc sai thì kết quả sẽ khác.
- Macro: xem mục 2.

## 6. PowerPoint (.pptx, .odp)

- Mở, xem hình thu nhỏ slide, thêm/xoá/sắp xếp slide, chèn ảnh, trình chiếu toàn màn hình.
- Giới hạn đã biết ([PPTX_LIMITS.md](PPTX_LIMITS.md)): khi lưu, một slide master có nhiều bố cục có thể bị tách thành nhiều master (hiển thị không đổi, nhưng sửa "trang cái" trong PowerPoint sau đó phải sửa nhiều master). Hiệu ứng chuyển cảnh và hoạt ảnh chưa bảo đảm giữ nguyên.
- Phông Aptos (không có sẵn trên máy không cài Office mới) được thay bằng phông tương đương (Carlito) để chữ không tràn khung; hai mẫu thử đạt 100% chữ khớp. App **chưa báo** cho bạn khi có thay phông.
- Mới thử với số ít mẫu; chưa thử với tệp nhiều master, slide ẩn hay hiệu ứng phức tạp.

## 7. PDF

- Mở PDF để **xem** (chỉ đọc). Không sửa nội dung PDF.
- **Công cụ PDF** (qpdf đi kèm): *Gộp*, *Tách*, *Xoay*, *Nén*, *Thông tin*, *Đánh số trang*, *Đóng dấu*, *Ảnh → PDF*, *Trang trắng*. Tệp gốc không bao giờ bị sửa: kết quả luôn là tệp mới; nếu thay thế tệp đã có, bản cũ được sao lưu. Hỏi mật khẩu khi PDF có mật khẩu.

## 8. Mở lại sau sự cố (khôi phục)

Nếu Dũng Office tắt bất thường (mất điện, treo, bị đóng cưỡng bức), lần mở sau hiện hộp thoại **"Khôi phục phiên trước"** liệt kê các bản tự lưu (tên, giờ tự lưu, tệp gốc). Bạn chọn **Mở các bản đã chọn**, **Bỏ các bản đã chọn** hoặc **Để sau**. Thoát bình thường thì bản khôi phục của phiên đó bị xoá. Bản khôi phục nằm trong thư mục dữ liệu của app (`%APPDATA%\DungOffice\DungOffice\recovery`), tự lưu 2 phút một lần: bạn có thể mất tối đa khoảng 2 phút gõ.

## 9. Gõ tiếng Việt

- App nhận chữ từ bộ gõ qua cơ chế nhập văn bản chuẩn của Windows/Qt (kiểu Telex hiển thị chữ đang soạn ngay trong tài liệu). Đã có kiểm thử tự động cho kiểu gõ này.
- **Chưa kiểm trên máy thật với Unikey/EVKey** (Telex và VNI, trong Word/Excel/PowerPoint). Nếu bạn gặp lỗi (mất chữ, nhân đôi chữ, dấu sai vị trí), hãy gửi lỗi theo mục 12: kèm tên bộ gõ, kiểu gõ, và chế độ (có bật "gõ bằng clipboard/ghi đè" hay không).
- Phím bấm đặc biệt: tổ hợp AltGr được hiểu là gõ chữ chứ không phải phím tắt.

## 10. Hộp công cụ

Mở **Công cụ** ở thanh bên: lưới thẻ có ô tìm (không phân biệt dấu), nút ★ để ghim công cụ lên đầu trang. Hiện có:

### 10.1 Điền tên hàng loạt (giấy mời, thư mời, giấy khen)

1. **Chọn mẫu** (`.docx`, `.pptx` hoặc `.xlsx`) bằng "Chọn tệp…".
2. Đánh dấu chỗ cần điền: gõ `{{Tên}}` (hoặc `«Tên»`) vào mẫu — một hoặc nhiều trường (`{{Tên}}`, `{{Chức vụ}}`…). Hoặc dán đoạn chữ mẫu vào ô "Đoạn chữ cần biến thành trường", đặt tên trường rồi bấm "Biến đoạn này thành trường": app **tạo bản mẫu mới** có trường; mẫu gốc không bị sửa. (Các nút "Dùng tài liệu đang mở" và "Lấy đoạn đang bôi chọn" chỉ hiện khi công cụ được nhúng vào tài liệu đang mở; trong Hộp công cụ hiện tại chúng **ẩn**.)
3. **Danh sách**: dán từ Excel (Tab), CSV/`.xlsx` hoặc gõ tay; một cột hay nhiều cột; ghép trường với cột.
4. App kiểm trước và báo: dòng trống, thiếu cột, quá dài, trùng. Trường thiếu dữ liệu **giữ nguyên chỗ giữ chữ và có cảnh báo**, không bị xoá âm thầm.
5. Xuất: **mỗi người một tệp** (đặt tên theo trường; ký tự cấm/tên dành riêng của Windows được xử lý) hoặc **gộp một tệp** (chỉ cho Word). Không bao giờ ghi đè tệp có sẵn hay sửa mẫu gốc.
- Giới hạn: xuất PDF chưa được nối (chọn xuất cùng loại với mẫu); trường trong biểu đồ, SmartArt, trang cái, công thức Excel không thay được (có cảnh báo); không hỗ trợ `.docm/.xlsm/.pptm`, `.doc/.xls` cũ hoặc tệp có mật khẩu; giao diện chưa ai bấm thử, chưa thử với tệp Word/Excel thật của bạn.

### 10.2 Chụp màn hình và chú thích

- Chụp vùng / cửa sổ (chỉ Windows) / toàn màn hình, hỗ trợ nhiều màn hình, hẹn giờ.
- Chú thích: khung, mũi tên, bút, tô sáng, chữ, đánh số bước, **mosaic và làm mờ** (ghi đè điểm ảnh thật để che thông tin nhạy cảm), cắt, hoàn tác/làm lại.
- Sao chép vào clipboard; lưu PNG/JPG/PDF theo tên ngày giờ trong thư mục Ảnh, không ghi đè tệp có sẵn.
- **Phím tắt toàn cục** (chọn một trong Ctrl+Alt+S, Ctrl+Shift+X, Ctrl+Alt+PrtSc) mặc định **tắt**, chỉ hoạt động khi Dũng Office đang chạy; nếu tổ hợp bị ứng dụng khác chiếm, app báo và bạn chọn tổ hợp khác.
- Chưa kiểm trên máy thật: chụp thật, DPI/đa màn hình, phím tắt, clipboard. Nút "Chèn vào tài liệu đang mở" **ẩn** (chưa nối vào tài liệu).

### 10.3 Tìm trong thư mục

- Tìm theo **tên** và **nội dung** trong thư mục bạn chọn: văn bản/mã, `docx/xlsx/pptx/odt`, **PDF** (đọc bằng thành phần PDF của Qt nếu bản dựng có; nếu không và máy không có `pdftotext` thì PDF bị bỏ qua và app báo).
- Tìm không phân biệt dấu ("nguyen" ra "Nguyễn"), lọc theo loại/ngày/cỡ, có thể huỷ, giới hạn bộ nhớ, liệt kê tệp bị bỏ qua kèm lý do. Chỉ mục tên tệp là **tuỳ chọn** do bạn bật và xoá được.
- Hiệu năng đo trên máy chủ CI: **không nhanh hơn** một cách duyệt thư mục đơn giản trong bộ thử 6000 tệp nhỏ (tìm nội dung 1360 ms so với 389 ms; có chỉ mục tên: 58 ms). Chưa so với Windows Search thật. Đừng kỳ vọng nhanh hơn Windows Search; chưa có số đo chứng minh.
- Không tìm nội dung `.doc/.xls/.ppt` cũ. Giao diện chưa ai bấm thử.

### 10.4 Công cụ tệp, thư mục, liên kết

Đổi tên hàng loạt (xem trước, hoàn tác bằng nhật ký) · Tìm tệp trùng lặp (chỉ liệt kê; xoá chuyển vào thư mục `_to_delete`, **không** vào Thùng rác của Windows) · So sánh hai thư mục · Dung lượng thư mục · Làm sạch liên kết · Sắp xếp tệp vào thư mục (theo loại/đuôi/năm/tháng) · Đường dẫn tệp (sao chép nhiều dạng, cảnh báo ≥ 260 ký tự) · Quét liên kết hỏng (`.url`, `.lnk`, liên kết trong docx/xlsx/pptx; kiểm liên kết web **chỉ khi bạn bấm riêng và xác nhận**, vì dùng mạng).
- Mọi thao tác phá huỷ có xem trước, xác nhận và nhật ký hoàn tác; không ghi đè tệp.
- Chưa kiểm: giao diện, `.lnk` thật do Windows tạo, công thức `HYPERLINK()` của Excel, `.doc/.xls/.ppt` cũ.

### 10.5 Đồng hồ (công cụ mẫu) và công cụ ngoài

Có công cụ **Đồng hồ** (bấm giờ, đếm ngược). Bạn có thể thêm công cụ ngoài bằng một thư mục có `tool.json` + script (do **bạn** tự cài Python/AutoHotkey nếu cần; dự án **không đóng gói** chúng). App hiện đúng lệnh sẽ chạy, **hỏi xác nhận lần đầu**, và hỏi lại nếu script bị sửa. Chi tiết: [TOOLBOX.md](TOOLBOX.md). Chưa có: ghim công cụ lên ribbon của từng ứng dụng.

## 11. Giới hạn đã biết (tóm tắt)

- Phiên bản thử nghiệm; **chưa ký số**; chưa có macOS/di động.
- Macro không chạy; lưu sang `.xlsx` mất macro (mục 2).
- Từ điển chính tả chưa nối vào soạn thảo; bộ kiểm tra chính tả theo âm tiết đã có trong mã nhưng chưa nối giao diện.
- Giao diện của các công cụ trong Hộp công cụ chưa được kiểm trên máy thật.
- Đặt lề trên Windows tạo thẻ mới, mất Undo (mục 4).
- PDF chỉ xem (có Công cụ PDF để gộp/tách…).
- Xem thêm: [DATA_SAFETY.md](DATA_SAFETY.md), [PPTX_LIMITS.md](PPTX_LIMITS.md), [ROADMAP.md](ROADMAP.md).

## 12. Báo lỗi

Gửi email tới **suvietdc@gmail.com** theo mẫu trong [KICH_BAN_KIEM_TRA_THIET_BI_THAT.md](KICH_BAN_KIEM_TRA_THIET_BI_THAT.md#gửi-lỗi). Ứng dụng **chưa có tệp nhật ký chung** để đính kèm (chỉ công cụ ngoài `tool.json` ghi `tools.log` trong `%APPDATA%\DungOffice\DungOffice\logs`); hãy gửi ảnh chụp màn hình, phiên bản (Dự án › Update hiện "Phiên bản hiện tại"), các bước tái hiện và, nếu được, tệp mẫu **không chứa dữ liệu nhạy cảm**.
