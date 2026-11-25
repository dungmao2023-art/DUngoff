#!/usr/bin/env python3
"""Kiểm bằng TỆP: sau khi bật/tắt "Cố định ô", xlsx lưu ra có/không có <pane state="frozen"> trong XML sheet.
Dùng: check_freeze.py <thư mục chứa freeze-bat.xlsx và freeze-tat.xlsx>
Thoát 0 nếu bật -> có pane đóng băng và tắt -> không còn; ngược lại thoát 1 và in rõ từng tệp."""
import re, sys, zipfile, os

def frozen(path):
    with zipfile.ZipFile(path) as z:
        names = sorted(n for n in z.namelist() if re.match(r"xl/worksheets/sheet\d+\.xml$", n))
        for n in names:
            xml = z.read(n).decode("utf-8", "replace")
            m = re.search(r"<pane\b[^>]*>", xml)
            if m and 'state="frozen' in m.group(0):
                return True, n, m.group(0)
        return False, (names[0] if names else "?"), ""

d = sys.argv[1] if len(sys.argv) > 1 else "."
rc = 0
for name, want in (("freeze-bat.xlsx", True), ("freeze-tat.xlsx", False)):
    p = os.path.join(d, name)
    if not os.path.exists(p):
        print(f"[LỖI] thiếu {p}"); rc = 1; continue
    got, sheet, tag = frozen(p)
    ok = got == want
    print(f"[{'ĐẠT' if ok else 'LỖI'}] {name}: đóng băng={got} (mong đợi {want}) {sheet} {tag}")
    rc |= 0 if ok else 1
sys.exit(rc)
