#!/usr/bin/env python3
"""Ma trận lệnh UNO: chạy từng ca trong MỘT tiến trình dung-smoketest riêng (timeout), rồi kiểm hiệu ứng
THẬT trong tệp đã lưu (python-docx / openpyxl / python-pptx / đọc zip).

Phân loại mỗi lệnh:
  HOẠT ĐỘNG          — tệp lưu ra có hiệu ứng mong đợi (khác so với tệp mẫu)
  KHÔNG HIỆU LỰC     — lệnh trả về, không treo, tệp không đổi như mong đợi (hộp thoại không hiện được cũng rơi vào đây)
  TREO-CẦN HỘP THOẠI — tiến trình quá thời gian (lệnh mở hộp thoại/vòng lặp mà headless không hiển thị)
  LỖI                — engine/tiến trình lỗi, không lưu được

Cách dùng: run_matrix.py --exe <dung-smoketest> --program <thư mục program> --out <thư mục> --platform <linux|windows>
"""
import argparse, csv, os, re, shutil, struct, subprocess, sys, zlib, zipfile, time
from pathlib import Path

PER_CASE_TIMEOUT = 75  # giây (tiến trình tự có watchdog 40 s; đây là chốt chặn cuối)


# ---------------------------------------------------------------- tệp mẫu
def make_png(path: Path):
    w = h = 64
    raw = b"".join(b"\x00" + bytes([220, 40, 40]) * w for _ in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + \
          chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def make_fixtures(d: Path):
    d.mkdir(parents=True, exist_ok=True)
    make_png(d / "img.png")
    from docx import Document
    doc = Document()
    doc.add_paragraph("Tieu de tai lieu")
    doc.add_paragraph("Doan van mot ABC")
    doc.add_paragraph("Doan van hai ABC")
    doc.add_paragraph("Doan van ba")
    doc.save(d / "fx.docx")
    from openpyxl import Workbook
    wb = Workbook()
    ws = wb.active
    ws.title = "Data"
    ws.append(["Ten", "SoLuong", "Gia"])
    names = ["Mango", "Apple", "Cherry", "Banana", "Elder", "Date", "Grape", "Fig", "Honey", "Iris"]
    for i, n in enumerate(names):
        ws.append([n, (i * 7) % 10 + 1, 1000 + i * 100])
    wb.save(d / "fx.xlsx")
    from pptx import Presentation
    p = Presentation()
    for t in ("Slide mot", "Slide hai"):
        s = p.slides.add_slide(p.slide_layouts[1])
        s.shapes.title.text = t
        s.placeholders[1].text = "Noi dung " + t
    p.save(d / "fx.pptx")


# ---------------------------------------------------------------- tiện ích đọc tệp
def zread(path, name):
    with zipfile.ZipFile(path) as z:
        try:
            return z.read(name).decode("utf-8", "replace")
        except KeyError:
            return None

def znames(path):
    with zipfile.ZipFile(path) as z:
        return z.namelist()

def docxml(p): return zread(p, "word/document.xml") or ""


# ---------------------------------------------------------------- kiểm hiệu ứng: trả (ok|None, ghi chú). None = không có hiệu ứng tệp để kiểm
def v_w_table(o, f):
    from docx import Document
    t = Document(o).tables
    return (len(t) == 1 and len(t[0].rows) == 2 and len(t[0].columns) == 3), f"bảng={len(t)}"
def v_w_table_row(o, f):
    from docx import Document
    t = Document(o).tables
    return (len(t) == 1 and len(t[0].rows) == 3), f"hàng={len(t[0].rows) if t else 0}"
def v_w_image(o, f):
    m = [n for n in znames(o) if n.startswith("word/media/")]
    return len(m) >= 1, f"media={len(m)}"
def v_w_pagebreak(o, f):
    x = docxml(o)
    return ('w:type="page"' in x or "pageBreakBefore" in x), "br/page" if 'w:type="page"' in x else ("pageBreakBefore" if "pageBreakBefore" in x else "không có")
def _hf(o, kind):
    x = docxml(o)
    return f"w:{kind}Reference" in x and any(re.match(rf"word/{kind}\d*\.xml", n) for n in znames(o))
def v_w_header(o, f): return _hf(o, "header"), "headerReference" if _hf(o, "header") else "không có header"
def v_w_footer(o, f): return _hf(o, "footer"), "footerReference" if _hf(o, "footer") else "không có footer"
def v_w_toc(o, f):
    x = docxml(o)
    ok = ("TOC " in x and "instrText" in x) or "docPartGallery" in x or "Table of Contents" in x
    return ok, "có TOC" if ok else "không có TOC"
def v_w_numbering(o, f):
    x = docxml(o)
    return "<w:numPr>" in x, "numPr" if "<w:numPr>" in x else "không có numPr"
def v_w_list_level(o, f):
    x = docxml(o)
    m = re.findall(r'<w:ilvl w:val="(\d+)"', x)
    return any(int(v) >= 1 for v in m), f"ilvl={m}"
def v_w_list_outline(o, f):
    return v_w_list_level(o, f)
def v_w_footnote(o, f):
    t = zread(o, "word/footnotes.xml") or ""
    return "Chu thich chan trang" in t, "có chữ chú thích" if "Chu thich chan trang" in t else f"footnotes.xml={'có' if t else 'không'}"
def v_w_endnote(o, f):
    t = zread(o, "word/endnotes.xml") or ""
    return "Chu thich cuoi van ban" in t, "có chữ chú thích" if "Chu thich cuoi van ban" in t else f"endnotes.xml={'có' if t else 'không'}"
def v_w_replace_all(o, f):
    from docx import Document
    txt = "\n".join(p.text for p in Document(o).paragraphs)
    return ("XYZ" in txt and "ABC" not in txt), f"XYZ={txt.count('XYZ')} ABC={txt.count('ABC')}"
def v_w_track(o, f):
    x = docxml(o)
    return "<w:ins " in x, "w:ins" if "<w:ins " in x else "không có w:ins"
def v_w_accept_all(o, f):
    x = docxml(o)
    from docx import Document
    txt = "\n".join(p.text for p in Document(o).paragraphs)
    return ("<w:ins " not in x and "Them moi" in txt), f"ins={'<w:ins ' in x} có chữ={'Them moi' in txt}"
def v_w_comment(o, f):
    t = zread(o, "word/comments.xml") or ""
    return "Binh luan thu" in t, "có bình luận" if "Binh luan thu" in t else "không có bình luận"
def v_w_heading(o, f):
    from docx import Document
    s = Document(o).paragraphs[0].style.name
    return s == "Heading 1", f"kiểu={s}"
def v_w_hyperlink(o, f):
    return "<w:hyperlink" in docxml(o), "hyperlink" if "<w:hyperlink" in docxml(o) else "không có"
def v_w_bookmark(o, f):
    return 'w:name="BM1"' in docxml(o), "BM1" if 'w:name="BM1"' in docxml(o) else "không có"
def v_w_docprops(o, f):
    t = zread(o, "docProps/core.xml") or ""
    return "Tieu de thu nghiem" in t, "title đã đặt" if "Tieu de thu nghiem" in t else "title không đổi"
def v_w_date_field(o, f):
    x = docxml(o)
    return bool(re.search(r"instrText[^>]*>\s*DATE|w:fldSimple[^>]*DATE", x)), "trường DATE" if re.search(r"DATE", x) else "không có"
def v_w_page_number(o, f):
    x = docxml(o)
    return bool(re.search(r"PAGE", x)), "trường PAGE" if "PAGE" in x else "không có"
def v_w_pagesize_a5(o, f):
    m = re.search(r'<w:pgSz[^>]*w:w="(\d+)"[^>]*w:h="(\d+)"', docxml(o))
    return bool(m and abs(int(m.group(1)) - 8392) < 40), f"pgSz={m.groups() if m else None}"
def v_w_margin(o, f):
    m = re.search(r"<w:pgMar([^>]*)/>", docxml(o))
    if not m: return False, "không có pgMar"
    a = dict(re.findall(r'w:(\w+)="(-?\d+)"', m.group(1)))
    L, R, T, B = (int(a.get(k, 0)) for k in ("left", "right", "top", "bottom"))
    ok = abs(L - 1701) < 30 and abs(R - 850) < 30 and abs(T - 1134) < 30 and abs(B - 1134) < 30
    return ok, f"pgMar L{L} R{R} T{T} B{B} (đích 1701/850/1134/1134 twip)"

def _nochange_docx(o, f):
    return None, "không có hiệu ứng trên tệp (lệnh mở hộp thoại / chỉ đổi trạng thái)"

# ---- Calc
def _ws(o):
    import openpyxl
    return openpyxl.load_workbook(o).worksheets[0]
def v_c_sort_asc(o, f):
    ws = _ws(o); col = [ws.cell(r, 1).value for r in range(2, 12)]
    return col == sorted(col), f"A2:A11={col[:4]}…"
def v_c_sort_desc(o, f):
    ws = _ws(o); col = [ws.cell(r, 1).value for r in range(2, 12)]
    return col == sorted(col, reverse=True), f"A2:A11={col[:4]}…"
def v_c_autofilter(o, f):
    ws = _ws(o); return bool(ws.auto_filter.ref), f"ref={ws.auto_filter.ref}"
def v_c_freeze(o, f):
    ws = _ws(o); return ws.freeze_panes == "B2", f"freeze={ws.freeze_panes}"
def v_c_chart(o, f):
    c = [n for n in znames(o) if n.startswith("xl/charts/chart")]
    return len(c) >= 1, f"chart={len(c)}"
def v_c_merge(o, f):
    ws = _ws(o); r = [str(x) for x in ws.merged_cells.ranges]
    return "E1:F2" in r, f"merged={r}"
def v_c_print_area(o, f):
    ws = _ws(o); return bool(ws.print_area), f"print_area={ws.print_area}"
def v_c_protect(o, f):
    ws = _ws(o); return bool(ws.protection.sheet), f"sheet_protect={ws.protection.sheet}"
def v_c_insert_row(o, f):
    ws = _ws(o); return ws.max_row == 12, f"max_row={ws.max_row} (mẫu 11)"
def v_c_insert_col(o, f):
    ws = _ws(o); return ws.max_column == 4, f"max_col={ws.max_column} (mẫu 3)"
def v_c_delete_row(o, f):
    ws = _ws(o); return ws.max_row == 10, f"max_row={ws.max_row} (mẫu 11)"
def v_c_comment(o, f):
    ws = _ws(o); c = ws["C3"].comment
    return bool(c and "Ghi chu o" in c.text), f"comment={c.text if c else None}"
def v_c_hyperlink(o, f):
    ws = _ws(o); h = ws["E3"].hyperlink
    return bool(h), f"hyperlink={h.target if h else None}"
def v_c_autosum(o, f):
    ws = _ws(o); vals = [str(ws.cell(r, c).value) for r in (11, 12) for c in (2,)]
    ok = any("SUM" in str(ws.cell(r, 2).value).upper() for r in range(11, 14))
    return ok, f"B11..B13={[ws.cell(r,2).value for r in range(11,14)]}"
def v_c_group(o, f):
    ws = _ws(o)
    lv = [ws.row_dimensions[r].outlineLevel for r in range(2, 5)]
    return any(l >= 1 for l in lv), f"outline={lv}"
def v_c_remove_dups(o, f):
    return None, "không có trùng trong mẫu — chỉ biết lệnh có chạy hay không"
def v_c_rename_sheet(o, f):
    import openpyxl
    n = openpyxl.load_workbook(o).sheetnames
    return "DoiTen" in n, f"sheets={n}"
def v_c_percent(o, f):
    ws = _ws(o); nf = ws["B2"].number_format
    return "%" in nf, f"B2.format={nf}"
def v_c_wrap(o, f):
    ws = _ws(o); return bool(ws["A2"].alignment.wrap_text), f"wrap={ws['A2'].alignment.wrap_text}"
def v_c_formula(o, f):
    ws = _ws(o); v = ws["D2"].value
    return str(v).startswith("=B2*2"), f"D2={v}"
def v_c_bgcolor(o, f):
    ws = _ws(o); fg = ws["A1"].fill.fgColor.rgb
    return ws["A1"].fill.fill_type == "solid", f"A1.fill={ws['A1'].fill.fill_type}/{fg}"

# ---- Impress
def _slides(o):
    from pptx import Presentation
    return Presentation(o).slides
def v_i_new_slide(o, f): n = len(_slides(o)); return n == 3, f"slides={n}"
def v_i_dup_slide(o, f): n = len(_slides(o)); return n == 3, f"slides={n}"
def v_i_delete_slide(o, f): n = len(_slides(o)); return n == 1, f"slides={n}"
def v_i_layout(o, f):
    s = _slides(o)[0]
    return (s.slide_layout.name != f.get("layout0", ""), f"layout={s.slide_layout.name} (mẫu={f.get('layout0')})")
def v_i_transition(o, f):
    x = zread(o, "ppt/slides/slide1.xml") or ""
    return "<p:transition" in x or "p14:" in x and "transition" in x, "p:transition" if "transition" in x else "không có transition"
def v_i_image(o, f):
    x = zread(o, "ppt/slides/slide1.xml") or ""
    return "<p:pic>" in x or "<p:pic " in x, "p:pic" if "<p:pic" in x else "không có ảnh"
def v_i_table(o, f):
    x = zread(o, "ppt/slides/slide1.xml") or ""
    return "<a:tbl>" in x, "a:tbl" if "<a:tbl>" in x else "không có bảng"
def v_i_chart(o, f):
    c = [n for n in znames(o) if n.startswith("ppt/charts/chart")]
    return len(c) >= 1, f"chart={len(c)}"
def v_i_hide(o, f):
    x = zread(o, "ppt/slides/slide1.xml") or ""
    return 'show="0"' in x, 'show="0"' if 'show="0"' in x else "slide vẫn hiện"
def v_i_notes(o, f):
    txt = ""
    for n in znames(o):
        if n.startswith("ppt/notesSlides/notesSlide"):
            txt += zread(o, n) or ""
    return "Ghi chu dien gia" in txt, "có chữ ghi chú" if "Ghi chu dien gia" in txt else "không có ghi chú"

VERIFY = {k[2:]: v for k, v in list(globals().items()) if k.startswith("v_") and callable(v)}
# ca chỉ mở hộp thoại / đổi trạng thái: không có hiệu ứng tệp
NOEFFECT = {"w_wordcount", "w_spell_online", "w_spell_dialog", "w_columns", "w_symbol", "w_print", "w_print_preview",
            "c_sort_dialog", "c_cond_dialog", "c_cond_manager", "c_validation", "c_name_define", "c_pivot",
            "c_text_to_cols", "c_print", "i_slideshow", "i_print",
            "w_page_dialog", "w_para_dialog", "w_font_dialog", "w_table_dialog", "w_lang_vi", "c_cell_dialog", "c_page_dialog",
            "i_slide_dialog", "x_ime_commit", "x_ime_nocommit", "x_ime_two_words", "x_ime_calc"}


def run_case(exe, program, fixtures, out, case, platform):
    cid = case["id"]
    outdir = out / "cases"
    outdir.mkdir(parents=True, exist_ok=True)
    cmd = [str(exe), "--program", str(program), "--cmdcase", cid, "--fixtures", str(fixtures), "--out", str(outdir)]
    t0 = time.time()
    status, log = None, ""
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=PER_CASE_TIMEOUT, encoding="utf-8", errors="replace")
        log = (p.stdout or "") + (p.stderr or "")
        rc = p.returncode
    except subprocess.TimeoutExpired as e:
        log = ((e.stdout or b"").decode("utf-8", "replace") if isinstance(e.stdout, bytes) else (e.stdout or "")) + "\nTIMEOUT tiến trình"
        rc = -9
    dt = time.time() - t0
    saved = outdir / f"{cid}.{case['ext']}"
    note = ""
    if rc == 3 or "TREO" in log or rc == -9:
        m = re.search(r"TREO bước=(.*?) sau", log)
        return "TREO-CẦN HỘP THOẠI", f"treo ở: {m.group(1) if m else 'không rõ'}", dt, log
    if "SAVED=yes" not in log or not saved.exists():
        m = re.search(r"(LỖI.*|SAVED=no.*)", log)
        return "LỖI", (m.group(1)[:160] if m else f"rc={rc}"), dt, log
    dlg = re.search(r"DIALOGHINT (.*)", log)
    if cid in NOEFFECT:
        extra = f"; có sự kiện hộp thoại: {dlg.group(1)[:60]}" if dlg else ""
        states = [l for l in log.splitlines() if " STATE " in l]
        if cid == "w_spell_online" and any("SpellOnline" in s for s in states):
            return "HOẠT ĐỘNG", "engine báo đổi trạng thái SpellOnline (chưa có tệp xác nhận)", dt, log
        return "KHÔNG HIỆU LỰC", "trả về ngay, không treo, không hiện hộp thoại, tệp không đổi" + extra, dt, log
    fn = VERIFY.get(cid)
    base = {}
    try:
        if case["fixture"] == "fx.pptx":
            from pptx import Presentation
            base["layout0"] = Presentation(str(fixtures / "fx.pptx")).slides[0].slide_layout.name
        ok, note = fn(saved, base)
    except Exception as ex:
        return "LỖI", f"kiểm tệp lỗi: {ex}", dt, log
    if ok is None:
        return "KHÔNG HIỆU LỰC", note, dt, log
    return ("HOẠT ĐỘNG" if ok else "KHÔNG HIỆU LỰC"), note, dt, log


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--program", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--platform", default="linux")
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    fixtures = out / "fixtures"
    make_fixtures(fixtures)
    lst = subprocess.run([a.exe, "--cmdlist", "1"], capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
    cases = []
    for line in lst.splitlines():
        p = line.split("|")
        if len(p) >= 5:
            cases.append(dict(id=p[0], app=p[1], fixture=p[2], ext=p[3], desc=p[4]))
    if a.only:
        cases = [c for c in cases if c["id"] in a.only.split(",")]
    rows = []
    for c in cases:
        res, note, dt, log = run_case(a.exe, a.program, fixtures, out, c, a.platform)
        print(f"[{res}] {c['id']:18s} {c['desc']} — {note} ({dt:.1f}s)", flush=True)
        rows.append([a.platform, c["app"], c["id"], c["desc"], res, note, f"{dt:.1f}"])
        (out / "cases").mkdir(exist_ok=True)
        (out / "cases" / f"{c['id']}.log").write_text(log, encoding="utf-8", errors="replace")
    csvp = out / f"matrix-{a.platform}.csv"
    with open(csvp, "w", newline="", encoding="utf-8-sig") as fh:
        w = csv.writer(fh)
        w.writerow(["nen_tang", "ung_dung", "ma_ca", "mo_ta", "ket_qua", "ghi_chu", "giay"])
        w.writerows(rows)
    from collections import Counter
    cnt = Counter(r[4] for r in rows)
    print("\nTỔNG:", dict(cnt), f"({len(rows)} ca) -> {csvp}")


if __name__ == "__main__":
    main()
