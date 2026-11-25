# Phát hành Dũng Office

## Tệp trong mỗi bản phát hành
- `DungOffice-Setup-<phiên bản>-x64.exe` — bộ cài.
- `*.zip` — bản portable (chỉ có khi được làm ra).
- `SHA256SUMS.txt` — mã SHA-256 của các tệp trên, định dạng giống `sha256sum` (`<mã hex>  <tên tệp>`).

`SHA256SUMS.txt` do `scripts/make_sha256sums.py` tạo trong CI (job Windows) sau khi dựng bộ cài, được kiểm lại bằng `Get-FileHash` rồi đính vào bản phát hành khi đẩy tag `v*`.

## Bộ cập nhật trong ứng dụng
Bộ cập nhật (`app/src/update/`) chỉ tải bộ cài khi bản phát hành có `SHA256SUMS.txt` (hoặc `<bộ cài>.sha256`), tính SHA-256 của tệp đã tải, và loại bỏ tệp nếu không khớp. Không bao giờ tự cài: bộ cài chỉ chạy khi người dùng bấm. Bản phát hành thiếu tệp băm thì ứng dụng không tải để cài và hướng dẫn tải thủ công. Logic này có kiểm thử trong `tests/update/` (`dung-updatetest`).

Giới hạn thật: mã băm nằm cùng nơi (GitHub Release) với bộ cài. Nó phát hiện tải lỗi hoặc tệp bị đổi lẻ, nhưng KHÔNG chống được kẻ kiểm soát được cả trang phát hành. Chống việc đó cần chữ ký số (xem bên dưới), hiện chưa có.

## Chưa ký số
Bộ cài **chưa được ký bằng chứng thư số** (Authenticode). Hệ quả với người dùng:
- Windows SmartScreen có thể hiện "Windows protected your PC / Publisher: Unknown" khi chạy bộ cài. Chọn **More info → Run anyway** nếu bạn đã kiểm mã băm bên dưới và tải từ trang phát hành chính thức.
- Mức cảnh báo giảm dần khi có nhiều người tải, nhưng chỉ chữ ký số (kèm uy tín) mới bỏ hẳn cảnh báo.
- Chữ ký số Authenticode này khác với chữ ký số kế toán/thuế (token nộp thuế điện tử).

Việc ký số (mua chứng thư, cấu hình ký trong CI) để làm sau cùng; chưa làm, chưa mua gì.

## Cách người dùng tự kiểm SHA-256
1. Tải cả bộ cài và `SHA256SUMS.txt` từ cùng một bản phát hành.
2. Tính mã của bộ cài:
   - PowerShell: `Get-FileHash .\DungOffice-Setup-0.1.0-x64.exe -Algorithm SHA256`
   - Dòng lệnh: `certutil -hashfile DungOffice-Setup-0.1.0-x64.exe SHA256`
3. So với dòng cùng tên trong `SHA256SUMS.txt` (so không phân biệt hoa/thường). Khác dù một ký tự thì **không chạy** tệp, hãy tải lại.
