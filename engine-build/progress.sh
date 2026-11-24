#!/bin/bash
# Đo tiến độ build engine từ log make và thống kê ccache.
# Cách dùng: progress.sh <make.log> <ccache.txt> <thư mục core> <số lượt> <thư mục report>
# Chỉ báo những gì đo được; KHÔNG suy ra phần trăm từ một tổng số tự đặt.
log=$1; cc=$2; core=$3; round=$4; rep=${5:-.}
out="$rep/progress.txt"
{
echo "== Tiến độ lượt $round =="
if [ -f "$rep/make-start.txt" ] && [ -f "$rep/make-end.txt" ]; then
  secs=$(( $(cat "$rep/make-end.txt") - $(cat "$rep/make-start.txt") ))
  echo "Thời gian make: $((secs/60)) phút"
fi
echo "Đơn vị build theo loại (trong log lượt này):"
grep -oE '^\[build [A-Za-z0-9_]+\]' "$log" | sort | uniq -c | sort -rn | head -10 | sed 's/^/  /'
objs=$(grep -cE '^\[build (CXX|CC|OBJC|OBJCXX)\]' "$log")
echo "Tệp C/C++ đã đi qua trình biên dịch (kể cả trúng cache): $objs"
touched=0
for m in $(grep -E '^\[build [A-Za-z0-9_]+\] ' "$log" | sed -E 's/^\[build [A-Za-z0-9_]+\] //' | cut -d/ -f1 | sort -u); do
  [ -f "$core/$m/Module_$m.mk" ] && touched=$((touched+1))
done
total=$(ls "$core"/*/Module_*.mk 2>/dev/null | wc -l)
echo "Module đã chạm tới: $touched / $total (module có kích thước rất khác nhau, chỉ là chỉ báo thô)"
last=$(grep -E '^\[build [A-Za-z0-9_]+\] ' "$log" | tail -1)
echo "Dòng build cuối: $last"
if [ -f "$cc" ]; then
  echo "ccache (tích lũy qua các lượt, nếu thư mục cache được khôi phục):"
  grep -E "Hits:|Misses:|Cache size" "$cc" | sed 's/^ */  /' | head -6
  echo "  (số 'Misses' tích lũy ~ số tệp đã biên dịch thật cho tới nay)"
fi
} > "$out" 2>&1
cat "$out"
