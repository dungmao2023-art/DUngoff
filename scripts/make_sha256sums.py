#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Tạo SHA256SUMS.txt cho các tệp phát hành (bộ cài, bản portable nếu có).

Định dạng giống `sha256sum`: "<64 ký tự hex thường>  <tên tệp>" (hai dấu cách, chỉ tên tệp, không đường dẫn).
Bộ cập nhật trong ứng dụng (UpdateCore::parseChecksums) đọc đúng định dạng này.

Dùng:  python scripts/make_sha256sums.py -o SHA256SUMS.txt tep1.exe tep2.zip
Không ký số, không mua gì: đây chỉ là mã băm để kiểm tra tệp tải về không bị lỗi/bị thay.
"""
import argparse, hashlib, os, sys


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", default="SHA256SUMS.txt")
    ap.add_argument("files", nargs="+")
    a = ap.parse_args()
    names = set()
    lines = []
    for p in a.files:
        if not os.path.isfile(p):
            print(f"LỖI: không có tệp {p}", file=sys.stderr)
            return 1
        n = os.path.basename(p)
        if n in names:
            print(f"LỖI: trùng tên tệp {n} (mã băm sẽ mơ hồ)", file=sys.stderr)
            return 1
        names.add(n)
        lines.append(f"{sha256_file(p)}  {n}")
    out = os.path.abspath(a.out)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
