# Lỗi runMacro 1287 trên engine Windows — kết quả chẩn đoán (nhánh exp/macro-1287, KHÔNG merge)

Đo bằng workflow `macro1287.yml` (3 vòng CI Windows: runs 45524324426, 45524831666, 45526271150).

| Thí nghiệm | Kết quả |
|---|---|
| Engine cũ 26.8.3.1 vs mới 26.8.3.2, cùng bước `--api` | Cả hai đều lỗi 1287 → **không phải hồi quy của engine v2** |
| MacroSecurityLevel = 3 / 0 / bỏ hẳn; DisableMacrosExecution=false | Không đổi: vẫn 1287 → **không phải mức bảo mật macro** |
| Sau khi mở tài liệu: `Standard.DungApi.Noop` (sub rỗng), `Standard.Module1.Main` (mô-đun mặc định của LO), `Nope.Nope.Nope` (không tồn tại), `macro://./…` | Tất cả đều 1287 → **không do nội dung mô-đun Basic của ta, cũng không phải "không tìm thấy macro"** |
| Trước khi mở tài liệu nào: `Standard.DungApi.Noop`, `Tools.Strings.GetFileName` | `runMacro` trả **true** → thư viện Basic nạp đúng, macro chạy được |
| Trước khi mở tài liệu: `Nope.Nope.Nope` | lỗi 87560 (khác 1287: đây là "macro không tồn tại") |

Kết luận đo được: 1287 chỉ xuất hiện khi **đã có tài liệu mở**, với mọi URL macro. Nó có dạng mã IO/truy cập
(lớp 5 = Access, không phải mã Basic), khớp với cổng `ERRCODE_IO_ACCESSDENIED` của sfx2 (nghi vấn, chưa chứng minh:
không đọc được `errcode.hxx` và engine bản phát hành không có SAL_LOG).
Chưa biết vì sao Linux (LibreOffice của distro) thì qua.

Chưa thử (đề xuất): (1) mở tài liệu với MacroExecutionMode tường minh (hiện LOK không có tuỳ chọn này, cần xem
`lo_documentLoadWithOptions`); (2) bản engine có SAL_LOG (`--enable-sal-log`) để thấy nơi trả 1287; (3) gọi script qua
`postUnoCommand(".uno:RunMacro")`/dispatcher `vnd.sun.star.script:` (không qua SfxMacroLoader); (4) bỏ hẳn `runMacro`:
ghi `pgMar`/`w:pgMar` lúc lưu (phương án nhẹ, không phụ thuộc engine).
