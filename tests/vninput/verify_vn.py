#!/usr/bin/env python3
# Đọc lại tệp đã lưu (docx/xlsx/pptx) từ dung-smoketest --vninput và so với đáp án.
# Mỗi ca in: ĐẠT/LỖI, và nếu khác thì in cả dạng NFC lẫn byte-ký-tự để phân loại (A/B/C).
import sys, unicodedata, pathlib

d = pathlib.Path(sys.argv[1])
fails = 0

def nfc(s): return unicodedata.normalize("NFC", s)
def cps(s): return " ".join("U+%04X" % ord(c) for c in s)

def load_expected(name):
    rows = []
    p = d / name
    if not p.exists(): return rows
    for line in p.read_text(encoding="utf-8").splitlines():
        if "\t" in line:
            k, v = line.split("\t", 1); rows.append((k, v))
    return rows

def report(app, got_list, expected):
    global fails
    for i, (k, exp) in enumerate(expected):
        got = got_list[i] if i < len(got_list) else None
        info = k.startswith("info") or "info-" in k
        if got is None:
            print(("[GHI NHẬN] " if info else "[LỖI]  ") + f"{app} {k}: không có dòng đọc lại")
            if not info: fails += 1
            continue
        if got == exp:
            print(f"[ĐẠT]  {app} {k}: đúng từng ký tự")
        elif nfc(got) == nfc(exp):
            print(f"[ĐẠT*] {app} {k}: đúng sau chuẩn hoá NFC; thực lưu: {cps(got)}  (không ép NFC → C)")
        else:
            print(("[GHI NHẬN] " if info else "[LỖI]  ") + f"{app} {k}: mong {exp!r} nhưng đọc {got!r}  mong={cps(exp)}  đọc={cps(got)}")
            if not info: fails += 1

# --- Word ---
f = d / "vn-writer.docx"
if f.exists():
    import docx
    paras = [p.text for p in docx.Document(str(f)).paragraphs]
    while paras and paras[-1] == "": paras.pop()
    report("Word", paras, load_expected("expected-writer.txt"))
else:
    print("[LỖI]  Word: không có vn-writer.docx"); fails += 1

# --- Excel ---
f = d / "vn-calc.xlsx"
if f.exists():
    import openpyxl
    ws = openpyxl.load_workbook(str(f)).active
    vals = []
    for r in range(1, 6):
        v = ws.cell(r, 1).value
        vals.append("" if v is None else (str(v) if not isinstance(v, str) else v))
    exp = load_expected("expected-calc.txt")
    # ca A5: số bắt đầu bằng 0 — Excel thường coi là số 12345; ghi nhận kiểu (cảnh báo, không phải lỗi gõ)
    report("Excel", vals, [(k, v) for k, v in exp if not k.startswith("A5")])
    a5 = ws.cell(5, 1)
    print(f"[GHI NHẬN] Excel A5: nhập '0012345' → lưu {a5.value!r} kiểu {a5.data_type} (hành vi mặc định của bảng tính, cần cảnh báo/định dạng văn bản khi nhập mã)")
else:
    print("[LỖI]  Excel: không có vn-calc.xlsx"); fails += 1

# --- PowerPoint ---
f = d / "vn-impress.pptx"
if f.exists():
    from pptx import Presentation
    texts = []
    for sl in Presentation(str(f)).slides:
        for sh in sl.shapes:
            if sh.has_text_frame and sh.text_frame.text.strip():
                texts.append(sh.text_frame.text.replace("\n", " "))
    report("PowerPoint", texts, load_expected("expected-impress.txt"))
else:
    print("[LỖI]  PowerPoint: không có vn-impress.pptx"); fails += 1

print("\nKẾT QUẢ NHẬP TIẾNG VIỆT:", "CÓ LỖI (%d ca)" % fails if fails else "TẤT CẢ ĐẠT")
sys.exit(1 if fails else 0)
