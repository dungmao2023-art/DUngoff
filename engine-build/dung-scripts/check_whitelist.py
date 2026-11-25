#!/usr/bin/env python3
# Đối chiếu danh sách trắng hàm script: dung_api.py (WHITELIST) và Engine.cpp (Engine::scriptFunctions).
import re, sys, os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
py = open(os.path.join(root, "engine-build/dung-scripts/dung_api.py"), encoding="utf-8").read()
cpp = open(os.path.join(root, "engine/src/Engine.cpp"), encoding="utf-8").read()
block = py[py.index("WHITELIST = {"):py.index("# ---", py.index("WHITELIST = {"))]
a = set(re.findall(r'^\s+"(\w+)":\s*\w+,', block, re.M))
body = cpp[cpp.index("Engine::scriptFunctions()"):]
body = body[:body.index("return v;")]
b = set(re.findall(r'\{"(\w+)",\s*(?:true|false)\}', body))
if a != b or not a:
    print("LỆCH danh sách trắng. Chỉ có ở Python:", sorted(a - b), "| chỉ có ở C++:", sorted(b - a)); sys.exit(1)
print("Danh sách trắng khớp: %d hàm" % len(a))
