#!/usr/bin/env python3
"""So sánh từng tính năng giữa tệp gốc và bản engine đã lưu (sau khi sửa một chỗ).

Cách chạy:  python3 tests/features/verify_features.py <thư mục tệp gốc> <thư mục saved> [báo cáo.md]
Mỗi tính năng được "đo" thành một dict các giá trị con (đếm, có/không). So sánh gốc↔lưu:
  GIỮ        = dict giống hệt
  MẤT        = mọi giá trị con của bản lưu đều rỗng/0/False
  BIẾN ĐỔI   = còn một phần hoặc khác
  TẠO LỖI    = tệp gốc cũng không có tính năng này (lỗi của chính tệp thử, không tính vào kết luận)
"""
import datetime, os, re, sys, zipfile
from lxml import etree

W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
WP = "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"
A = "http://schemas.openxmlformats.org/drawingml/2006/main"
MAIN = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
PML = "http://schemas.openxmlformats.org/presentationml/2006/main"
q = lambda ns, t: "{%s}%s" % (ns, t)


def zread(z, name):
    try:
        return z.read(name)
    except KeyError:
        return None


def xml(z, name):
    b = zread(z, name)
    return etree.fromstring(b) if b else None


def alltext(root):
    return "".join(root.itertext()) if root is not None else ""


# ============================================================ DOCX
def probe_docx(path):
    z = zipfile.ZipFile(path)
    names = z.namelist()
    doc = xml(z, "word/document.xml")
    out = {}
    body = doc.find(q(W, "body"))
    ins = [e for e in doc.iter(q(W, "ins")) if "chèn" in alltext(e)]
    dels = [e for e in doc.iter(q(W, "del")) if "xoá" in "".join(t.text or "" for t in e.iter(q(W, "delText")))]
    out["Theo dõi thay đổi (chèn/xoá)"] = {
        "chèn": len(ins), "xoá": len(dels),
        "tác giả chèn": any(e.get(q(W, "author")) == "Nguyễn Văn A" for e in ins),
        "tác giả xoá": any(e.get(q(W, "author")) == "Trần Thị B" for e in dels)}
    cm = xml(z, "word/comments.xml")
    ctext = alltext(cm)
    out["Bình luận"] = {"nội dung": "Bình luận thử" in ctext, "tác giả": bool(cm is not None and any(c.get(q(W, "author")) == "Lê C" for c in cm.iter(q(W, "comment")))),
                        "neo vào văn bản": len(list(doc.iter(q(W, "commentRangeStart")))) + len(list(doc.iter(q(W, "commentReference"))))}
    fn = xml(z, "word/footnotes.xml")
    out["Chú thích cuối trang (footnote)"] = {"nội dung": "Nội dung chú thích" in alltext(fn), "tham chiếu": len(list(doc.iter(q(W, "footnoteReference"))))}
    hdr = {n: alltext(xml(z, n)) for n in names if re.match(r"word/header\d*\.xml", n)}
    ftr = {n: xml(z, n) for n in names if re.match(r"word/footer\d*\.xml", n)}
    out["Header khác nhau trang đầu"] = {
        "titlePg": len(list(doc.iter(q(W, "titlePg")))),
        "đầu trang đầu": any("ĐẦU TIÊN" in t for t in hdr.values()),
        "đầu trang sau": any("các trang sau" in t for t in hdr.values())}
    def fields(root):
        s = set()
        if root is None: return s
        for e in root.iter(q(W, "fldSimple")):
            s.add((e.get(q(W, "instr")) or "").strip().split(" ")[0].upper())
        for e in root.iter(q(W, "instrText")):
            s.add((e.text or "").strip().split(" ")[0].upper())
        return s
    ff = set().union(*[fields(r) for r in ftr.values()]) if ftr else set()
    out["Footer: trường số trang + ngày"] = {"PAGE": "PAGE" in ff, "DATE": "DATE" in ff,
                                           "footer đầu tiên": any("Chân trang đầu tiên" in alltext(r) for r in ftr.values())}
    toc = any((e.text or "").strip().upper().startswith("TOC") for e in doc.iter(q(W, "instrText"))) or \
          any((e.get(q(W, "instr")) or "").strip().upper().startswith("TOC") for e in doc.iter(q(W, "fldSimple"))) or \
          any("TOC" in (e.get(q(W, "val")) or "").upper() or "Table of Contents" in (e.get(q(W, "val")) or "") for e in doc.iter(q(W, "docPartGallery")))
    out["Mục lục (trường TOC)"] = {"trường TOC": toc, "mục đệm": "Giới thiệu" in alltext(doc)}
    out["Hộp văn bản"] = {"nội dung": "Nội dung hộp văn bản" in alltext(doc), "txbxContent": len(list(doc.iter(q(W, "txbxContent"))))}
    out["Bảng lồng nhau"] = {"bảng lồng": sum(1 for tc in doc.iter(q(W, "tc")) if tc.find(q(W, "tbl")) is not None),
                             "ô trong": "Trong 4" in alltext(doc)}
    anchors = list(doc.iter(q(WP, "anchor")))
    out["Ảnh nổi (anchor)"] = {"anchor": sum(1 for a in anchors if a.find(".//" + q(A, "blip")) is not None),
                               "tệp ảnh": any(n.startswith("word/media/") for n in names)}
    rels = xml(z, "word/_rels/document.xml.rels")
    targets = [r.get("Target") for r in rels] if rels is not None else []
    out["Hyperlink"] = {"w:hyperlink": len(list(doc.iter(q(W, "hyperlink")))),
                        "đích": any("example.com/dung-office" in (t or "") for t in targets)}
    st = xml(z, "word/styles.xml")
    custom = None
    for s_ in st.iter(q(W, "style")):
        nm = s_.find(q(W, "name"))
        if (s_.get(q(W, "styleId")) == "DungKieuRieng") or (nm is not None and nm.get(q(W, "val")) in ("DungKieuRieng", "Dung Kieu Rieng")):
            custom = s_
    ctxt = etree.tostring(custom).decode() if custom is not None else ""
    out["Style tuỳ biến"] = {"có style": custom is not None, "màu 126B3A": "126B3A" in ctxt.upper(), "đậm": "<w:b" in ctxt,
                             "font Arial": "Arial" in ctxt, "dùng trong văn bản": "DungKieuRieng" in etree.tostring(doc).decode()}
    ft = xml(z, "word/fontTable.xml")
    out["Font nhúng"] = {"tệp odttf": sum(1 for n in names if n.startswith("word/fonts/")),
                         "embedRegular": len(list(ft.iter(q(W, "embedRegular")))) if ft is not None else 0}
    out["Điều khiển nội dung (sdt)"] = {"sdt": sum(1 for e in doc.iter(q(W, "sdt")) if e.find(".//" + q(W, "tag")) is not None and e.find(".//" + q(W, "tag")).get(q(W, "val")) == "hoten"),
                                       "nội dung": "Nhập họ tên" in alltext(doc)}
    out["Bookmark"] = {"DiemNhan": any(e.get(q(W, "name")) == "DiemNhan" for e in doc.iter(q(W, "bookmarkStart")))}
    M = "http://schemas.openxmlformats.org/officeDocument/2006/math"
    out["Công thức toán (OMML)"] = {"oMath": len(list(doc.iter("{%s}oMath" % M))), "phân số": len(list(doc.iter("{%s}f" % M)))}
    raw = etree.tostring(doc).decode()
    out["Ô chọn (checkbox cũ)"] = {"checkbox": "FORMCHECKBOX" in raw or "w:checkBox" in raw or "checkBox" in raw or "w14:checkbox" in raw}
    wm = any("MẪU" in (zread(z, n) or b"").decode("utf-8", "ignore") for n in names if re.match(r"word/header\d*\.xml", n))
    out["Hình mờ (watermark) trong header"] = {"chữ MẪU": wm}
    sects = list(doc.iter(q(W, "sectPr")))
    out["Hai cột (cols)"] = {"cột=2": any((c.get(q(W, "num")) == "2") for c in doc.iter(q(W, "cols")))}
    sizes = []
    for s_ in sects:
        pg = s_.find(q(W, "pgSz"))
        if pg is not None:
            sizes.append((int(pg.get(q(W, "w"), "0")) // 100, int(pg.get(q(W, "h"), "0")) // 100, pg.get(q(W, "orient"), "")))
    out["Nhiều section khổ giấy khác nhau"] = {"số section": len(sects), "khổ giấy khác nhau": len(set((a, b) for a, b, _ in sizes)),
                                              "có A3 ngang (≈420×297 mm)": any(abs(a - 238) < 6 and abs(b - 168) < 6 for a, b, _ in sizes)}
    txt = alltext(body)
    out["_edit"] = {"[SUA] có trong văn bản": "[SUA]" in txt}
    out["_text"] = {"văn bản gốc còn đủ": all(s_ in txt for s_ in ("Đoạn mở đầu để sửa", "Câu có bình luận", "Liên kết:", "Ô ngoài A", "Đoạn trong section khổ A3 ngang"))}
    return out


# ============================================================ XLSX
def probe_xlsx(path):
    import openpyxl
    z = zipfile.ZipFile(path)
    names = z.namelist()
    out = {}
    wbx = xml(z, "xl/workbook.xml")
    sheets = [(s.get("name")) for s in wbx.iter(q(MAIN, "sheet"))]
    relx = xml(z, "xl/_rels/workbook.xml.rels")
    rid2t = {r.get("Id"): r.get("Target") for r in relx}
    first = None
    for s in wbx.iter(q(MAIN, "sheet")):
        if s.get("name") == "TongHop" or first is None:
            t = rid2t.get(s.get("{%s}id" % R), "")
            t = t.lstrip("/")
            first = t if t.startswith("xl/") else "xl/" + t
            if s.get("name") == "TongHop":
                break
    sh = xml(z, first) if first and first in names else None
    has_tonghop = "TongHop" in sheets
    if has_tonghop and sh is not None:
        cf = list(sh.iter(q(MAIN, "cfRule")))
        types = sorted(c.get("type") for c in cf)
        out["Định dạng có điều kiện"] = {"cellIs": "cellIs" in types, "colorScale": "colorScale" in types, "dataBar": "dataBar" in types}
        dvs = list(sh.iter(q(MAIN, "dataValidation")))
        out["Kiểm tra dữ liệu (dropdown)"] = {"list": any(d.get("type") == "list" for d in dvs),
                                              "nguồn có 'Đạt'": any("Đạt" in alltext(d.find(q(MAIN, "formula1"))) for d in dvs if d.find(q(MAIN, "formula1")) is not None)}
        pane = next(iter(sh.iter(q(MAIN, "pane"))), None)
        out["Đóng băng ô"] = {"pane": pane is not None, "frozen": bool(pane is not None and "frozen" in (pane.get("state") or "")),
                              "topLeftCell B2": bool(pane is not None and pane.get("topLeftCell") == "B2")}
        prot = next(iter(sh.iter(q(MAIN, "sheetProtection"))), None)
        out["Bảo vệ sheet"] = {"sheetProtection": prot is not None, "bật": bool(prot is not None and prot.get("sheet") in ("1", "true"))}
        out["Ô gộp"] = {"L1:N2": any(m.get("ref") == "L1:N2" for m in sh.iter(q(MAIN, "mergeCell")))}
        fs_ = [f for f in sh.iter(q(MAIN, "f")) if f.get("t") == "array"]
        out["Công thức mảng"] = {"công thức mảng": len(fs_), "ref J1:J3": any(f.get("ref") == "J1:J3" for f in fs_)}
        out["Hyperlink"] = {"hyperlink": len(list(sh.iter(q(MAIN, "hyperlink"))))}
    dn = [d for d in wbx.iter(q(MAIN, "definedName"))]
    def sheet_xml(nm):
        for s_ in wbx.iter(q(MAIN, "sheet")):
            if s_.get("name") == nm:
                t = rid2t.get(s_.get("{%s}id" % R), "").lstrip("/")
                return xml(z, t if t.startswith("xl/") else "xl/" + t)
    s3 = sheet_xml("Bảng và nhóm")
    if s3 is not None:
        tabs = [xml(z, n) for n in names if re.match(r"xl/tables/table\d*\.xml$", n)]
        out["Bảng Excel (Table)"] = {"table": sum(1 for t in tabs if t is not None and t.get("displayName") == "BangDuLieu")}
        out["Nhóm hàng (outline)"] = {"outlineLevel": any(r.get("outlineLevel") == "1" for r in s3.iter(q(MAIN, "row")))}
        out["Cột ẩn"] = {"hidden": any((c.get("hidden") in ("1", "true")) for c in s3.iter(q(MAIN, "col")))}
        out["In: tiêu đề lặp + vùng in"] = {"Print_Titles": any("Print_Titles" in (d.get("name") or "") for d in dn),
                                           "Print_Area": any("Print_Area" in (d.get("name") or "") for d in dn)}
        ps = next(iter(s3.iter(q(MAIN, "pageSetup"))), None)
        out["In: khổ A3 ngang"] = {"landscape": bool(ps is not None and ps.get("orientation") == "landscape"), "A3 (8)": bool(ps is not None and ps.get("paperSize") == "8")}
        out["Footer in"] = {"Trang &P": any("&P" in alltext(e) for e in s3.iter(q(MAIN, "oddFooter")))}
        out["Ảnh trong sheet"] = {"drawing": s3.find(q(MAIN, "drawing")) is not None, "tệp ảnh": any(n.startswith("xl/media/") for n in names)}
        out["Mã giữ dạng chữ"] = {}
    if "Sheet ẩn" in sheets: out["Sheet ẩn"] = {"state=hidden": any(s_.get("name") == "Sheet ẩn" and s_.get("state") in ("hidden", "veryHidden") for s_ in wbx.iter(q(MAIN, "sheet")))}
    if has_tonghop:
        out["Tên vùng (defined name)"] = {"TongTien": any(d.get("name") == "TongTien" for d in dn),
                                          "đúng vùng D2:D11": any("D$2:$D$11" in (d.text or "") for d in dn)}
    if has_tonghop: out["Biểu đồ"] = {"chart part": sum(1 for n in names if re.match(r"xl/charts/chart\d*\.xml$", n)),
                      "có series": any(len(list(xml(z, n).iter("{http://schemas.openxmlformats.org/drawingml/2006/chart}ser"))) > 0
                                       for n in names if re.match(r"xl/charts/chart\d*\.xml$", n))}
    if not has_tonghop: out["Pivot"] = {"pivotTable": sum(1 for n in names if n.startswith("xl/pivotTables/pivotTable")),
                    "pivotCache": sum(1 for n in names if n.startswith("xl/pivotCache/pivotCacheDefinition"))}
    if has_tonghop: out["Ghi chú ô (comment)"] = {"nội dung": any("Ghi chú thử" in (zread(z, n) or b"").decode("utf-8", "ignore") for n in names if re.match(r"xl/comments.*\.xml$", n))}
    try:
        wb = openpyxl.load_workbook(path)  # giữ công thức
        wbv = openpyxl.load_workbook(path, data_only=False)
        if has_tonghop:
            ws = wb["TongHop"]
            a2, d2 = ws["A2"], ws["D2"]
            out["Định dạng ngày/tiền VN"] = {"ngày dd/mm/yyyy": a2.number_format.lower().replace("yyyy", "yyyy") == "dd/mm/yyyy",
                                            "tiền #,##0 \"₫\"": "#,##0" in d2.number_format and "₫" in d2.number_format,
                                            "giá trị ngày": a2.value in (datetime.datetime(2026, 11, 1), datetime.date(2026, 11, 1)),
                                            "giá trị tiền": d2.value == 125000}
            out["Ô mở khoá trong sheet bảo vệ"] = {"P2 không khoá": ws["P2"].protection.locked is False}
            out["Công thức sheet khác"] = {"công thức": str(wb["Dữ liệu 2"]["A2"].value).replace(" ", "") in ("=TongHop!C2+1", "=TongHop!C2+1.0"),
                                          "sheet phụ": "Dữ liệu 2" in wb.sheetnames}
            out["_edit"] = {"Z1 = SUA": ws["Z1"].value == "SUA"}
            if "Bảng và nhóm" in wb.sheetnames:
                w3 = wb["Bảng và nhóm"]
                out["Mã giữ dạng chữ"] = {"001 là chuỗi": w3["A2"].value == "001", "005": w3["A6"].value == "005"}
        else:
            out["_edit"] = {"Z1 = SUA": wb[wb.sheetnames[0]]["Z1"].value == "SUA"}
        out["_cells"] = {"số sheet": len(wb.sheetnames)}
    except Exception as e:  # noqa
        out["_openpyxl"] = {"đọc được bằng openpyxl": False, "lỗi": str(e)[:80]}
    return out


# ============================================================ PPTX
def probe_pptx(path):
    z = zipfile.ZipFile(path)
    names = z.namelist()
    out = {}
    slides = sorted([n for n in names if re.match(r"ppt/slides/slide\d+\.xml$", n)], key=lambda s: int(re.findall(r"\d+", s)[-1]))
    s1 = xml(z, slides[0]) if slides else None
    allsl = [xml(z, n) for n in slides]
    notes = [n for n in names if re.match(r"ppt/notesSlides/notesSlide\d+\.xml$", n)]
    out["Ghi chú diễn giả"] = {"nội dung": any("nhắc số liệu quý 3" in alltext(xml(z, n)) for n in notes)}
    tr = False
    if s1 is not None:
        tr = len(list(s1.iter(q(PML, "transition")))) > 0 or "<p14:" in etree.tostring(s1).decode() and "transition" in etree.tostring(s1).decode()
    out["Chuyển cảnh"] = {"transition": tr, "fade": bool(s1 is not None and ("fade" in etree.tostring(s1).decode()))}
    # slide mới do bước sửa có thể chèn xen giữa, nên tìm trên mọi slide
    import hashlib
    fix_md5 = hashlib.md5(png_bytes(80, 60)).hexdigest()
    media_ok = [n for n in names if n.startswith("ppt/media/") and hashlib.md5(z.read(n)).hexdigest() == fix_md5]
    out["Ảnh"] = {"ảnh mẫu còn nguyên byte": len(media_ok) >= 1, "pic trong slide": sum(1 for sl in allsl for pc in sl.iter(q(PML, "pic")))>=1}
    strange = sum(1 for n in names if n.startswith("ppt/media/") and hashlib.md5(z.read(n)).hexdigest() != fix_md5)
    out["_extra_ảnh lạ trong tệp lưu"] = {"tệp ảnh không có trong gốc": strange}
    rl = "".join((zread(z, n) or b"").decode("utf-8", "ignore") for n in names if re.match(r"ppt/slides/_rels/.*\.rels$", n))
    out["Hyperlink trong chữ"] = {"đích": "example.com/dung-office" in rl}
    out["Nhóm hình (grpSp)"] = {"nhóm": sum(1 for sl in allsl for g in sl.iter(q(PML, "grpSp")) if any((x.get("name") or "").startswith("Nhom") for x in g.iter(q(PML, "cNvPr"))))}
    out["Slide ẩn"] = {"show=0": any(sl.get("show") == "0" for sl in allsl)}
    out["Hoạt ảnh (timing)"] = {"p:timing": sum(1 for sl in allsl if len(list(sl.iter(q(PML, "timing")))) > 0 and len(list(sl.iter(q(PML, "spTgt")))) > 0)}
    tbl = [t for sl in allsl for t in sl.iter(q(A, "tbl"))]
    out["Bảng"] = {"bảng": len(tbl), "ô 'Ô 3-3'": any("Ô 3-3" in alltext(sl) for sl in allsl)}
    chart = [n for n in names if re.match(r"ppt/charts/chart\d*\.xml$", n)]
    out["Biểu đồ"] = {"chart part": len(chart), "series Doanh thu": any("Doanh thu" in alltext(xml(z, n)) for n in chart)}
    masters = [n for n in names if re.match(r"ppt/slideMasters/slideMaster\d+\.xml$", n)]
    mtxt = "".join((zread(z, n) or b"").decode("utf-8", "ignore") for n in masters)
    out["Slide master tuỳ biến"] = {"khối LogoMaster/DŨNG": "DŨNG" in mtxt, "nền F4F8FF": "F4F8FF" in mtxt.upper()}
    title1 = alltext(s1) if s1 is not None else ""
    out["Tiêu đề và nội dung slide"] = {"tiêu đề": "Tiêu đề trình chiếu" in title1, "nội dung": "Ý thứ hai" in title1}
    out["_edit"] = {"số slide ≥ 3 (đã chèn thêm)": len(slides) >= 3}
    return out


def png_bytes(w=40, h=30, rgb=(200, 60, 60)):
    import struct, zlib
    raw = b"".join(b"\x00" + bytes(rgb) * w for _ in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")


PROBES = {".docx": probe_docx, ".xlsx": probe_xlsx, ".pptx": probe_pptx}


def classify(orig, saved):
    if not any(bool(v) for v in orig.values()):
        return "TẠO LỖI"
    if saved == orig:
        return "GIỮ"
    if not any(bool(v) for v in saved.values()):
        return "MẤT"
    return "BIẾN ĐỔI"


def main():
    src, dst = sys.argv[1], sys.argv[2]
    rep = sys.argv[3] if len(sys.argv) > 3 else None
    rows = []
    for fn in sorted(os.listdir(src)):
        ext = os.path.splitext(fn)[1].lower()
        if ext not in PROBES or not os.path.exists(os.path.join(dst, fn)):
            if ext in PROBES:
                print(f"[LỖI]  không có bản đã lưu của {fn}")
            continue
        o = PROBES[ext](os.path.join(src, fn))
        try:
            s = PROBES[ext](os.path.join(dst, fn))
        except Exception as e:  # noqa
            print(f"[LỖI]  không đọc được bản đã lưu của {fn}: {e}")
            continue
        print(f"== {fn}")
        for feat in o:
            if feat.startswith("_extra"):
                ov, sv = o[feat], s.get(feat, {})
                grew = any(sv.get(k, 0) > ov.get(k, 0) for k in sv)
                print(f"   [{'PHÁT HIỆN THÊM' if grew else 'KHÔNG THÊM'}] {feat[7:]}: gốc={ov} → lưu={sv}")
                rows.append((fn, feat[7:], "PHÁT HIỆN THÊM" if grew else "GIỮ", f"gốc={ov} → lưu={sv}"))
                continue
            if feat.startswith("_"):
                kind = "SỬA" if feat == "_edit" else "KIỂM TRA"
                ok = bool(s.get(feat)) and all(s[feat].values())
                print(f"   [{kind}] {feat[1:]}: {s.get(feat)}")
                rows.append((fn, ("Chỗ vừa sửa" if feat == "_edit" else "Văn bản gốc còn đủ" if feat == "_text" else feat[1:]), "GIỮ" if ok else "MẤT", f"{s.get(feat)}"))
                continue
            c = classify(o[feat], s.get(feat, {}))
            detail = ""
            if c in ("MẤT", "BIẾN ĐỔI"):
                detail = f"gốc={o[feat]} → lưu={s.get(feat)}"
            print(f"   [{c}] {feat}" + (f": {detail}" if detail else ""))
            rows.append((fn, feat, c, detail))
    tally = {}
    for _, _, c, _ in rows:
        tally[c] = tally.get(c, 0) + 1
    print("\nTỔNG:", ", ".join(f"{k}={v}" for k, v in sorted(tally.items())))
    if rep:
        with open(rep, "w", encoding="utf-8") as f:
            f.write("| Tệp | Tính năng | Kết quả | Chi tiết |\n|---|---|---|---|\n")
            for r in rows:
                f.write("| " + " | ".join(x.replace("|", "/") for x in r) + " |\n")
    print("KẾT QUẢ ĐỐI CHIẾU: XONG (xem từng dòng; không chặn CI)")


if __name__ == "__main__":
    main()
