# Tool mẫu cho Dũng Office: đọc đối số, in lời chào. Không dùng mạng, không ghi tệp.
import argparse
import sys

p = argparse.ArgumentParser()
p.add_argument("--ten", required=True)
p.add_argument("--lap", type=int, default=1)
p.add_argument("--kieu", default="Trang trọng")
p.add_argument("ghichu", nargs="?", default="")
a = p.parse_args()
for i in range(a.lap):
    loi = "Kính chào" if a.kieu == "Trang trọng" else "Chào"
    print(f"{loi} {a.ten}! ({i + 1}/{a.lap})")
if a.ghichu:
    print("Ghi chú:", a.ghichu)
sys.exit(0)
