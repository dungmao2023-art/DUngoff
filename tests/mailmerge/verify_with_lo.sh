#!/usr/bin/env bash
# Xác minh độc lập: mở các tệp XUẤT bằng LibreOffice (không dùng mã của dự án) và tìm đúng chữ đã điền.
# Cách dùng: verify_with_lo.sh <thư mục tệp xuất> ; mẫu gốc cũng được mở để đối chứng (nếu mẫu không mở được thì không đánh giá tệp xuất cùng loại).
set -u
OUT="$1"; FX="${2:-tests/fixtures/mailmerge}"; W=$(mktemp -d); rc=0
conv() { # <tệp> <định dạng> <thư mục>
  timeout 120 soffice -env:UserInstallation=file://$W/prof --headless --convert-to "$2" --outdir "$3" "$1" >/dev/null 2>&1; }
check() { # <tên> <thư mục> <đuôi> <chuỗi cần thấy>
  local f="$2/$1.$3"
  [ -s "$f" ] || { echo "[LỖI] $1: LibreOffice không tạo được $3"; rc=1; return; }
  if [ "$3" = odp ]; then unzip -p "$f" content.xml | grep -q "$4" && echo "[ĐẠT] $1: thấy «$4»" || { echo "[LỖI] $1: không thấy «$4»"; rc=1; }
  else grep -q "$4" "$f" && echo "[ĐẠT] $1: thấy «$4»" || { echo "[LỖI] $1: không thấy «$4»"; rc=1; }; fi
}
mkdir -p $W/o
conv "$OUT/docx-1.docx" txt:Text $W/o;            check docx-1 $W/o txt "Kính mời Nguyễn Văn <An> & Co đến dự lễ."
conv "$OUT/docx-gop.docx" pdf $W/o;               [ -s $W/o/docx-gop.pdf ] && echo "[ĐẠT] docx-gop: LibreOffice mở và xuất PDF được" || { echo "[LỖI] docx-gop: không xuất PDF"; rc=1; }
conv "$OUT/xlsx-1.xlsx" csv $W/o;                 check xlsx-1 $W/o csv "Kính gửi Lê Văn Cường"
conv "$FX/giay-moi.pptx" odp $W/c
if [ -s $W/c/giay-moi.odp ]; then
  conv "$OUT/pptx-1.pptx" odp $W/o;               check pptx-1 $W/o odp "Kính mời Trần Thị Bình"
else echo "[BỎ QUA] mẫu pptx tối giản không mở được bằng LibreOffice → không đánh giá tệp xuất pptx (ghi nhận, không phải đạt)"; fi
rm -rf $W; exit $rc
