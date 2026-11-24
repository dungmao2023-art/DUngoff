#!/usr/bin/env python3
"""In ma trận (CSV) + bằng chứng hộp thoại/IME ra log CI (không tải được artifact nên đọc từ log)."""
import sys, csv, glob, os, json, collections, re
out = sys.argv[1]; plat = sys.argv[2]
print(f"===== MATRIX {plat} =====")
rows = list(csv.DictReader(open(os.path.join(out, f"matrix-{plat}.csv"), encoding="utf-8-sig")))
print(collections.Counter(r["ket_qua"] for r in rows))
for r in rows:
    print(f"MX|{plat}|{r['ung_dung']}|{r['ma_ca']}|{r['ket_qua']}|{r['giay']}|{r['ghi_chu'][:150]}")
print(f"===== DIALOG EVIDENCE {plat} =====")
d = os.path.join(out, "cases", "dialogs")
by = collections.defaultdict(list)
for f in sorted(glob.glob(os.path.join(d, "*.json"))):
    m = re.match(r"(.+)-(\d+)-(\d+)\.json$", os.path.basename(f)); by[m.group(1)].append((int(m.group(2)), f))
for case, lst in sorted(by.items()):
    w = [open(f, encoding="utf-8", errors="replace").read() for t, f in lst if t == 36]
    j = [open(f, encoding="utf-8", errors="replace").read() for t, f in lst if t == 46]
    kinds = collections.Counter()
    for s in j:
        try:
            o = json.loads(s); kinds[f"{o.get('jsontype')}/{o.get('action')}/{o.get('type','')}"] += 1
        except Exception:
            kinds["unparsed"] += 1
    full = [s for s in j if '"children"' in s and ('"jsontype": "dialog"' in s or '"jsontype":"dialog"' in s) and '"action": "close"' not in s]
    print(f"DLG|{plat}|{case}|WINDOW={w[:2]}|JSDIALOG={len(j)} kinds={dict(kinds)}|fullTree={len(full)}")
    if full: print("DLGFULL|", case, full[0][:1500])
for f in sorted(glob.glob(os.path.join(out, "cases", "*.log"))):
    for l in open(f, encoding="utf-8", errors="replace"):
        if " TEXT [" in l or " RESULT .uno:" in l and ("Dialog" in l or "Print" in l or "PageSetup" in l):
            print("LOGLINE|", l.strip()[:240])
