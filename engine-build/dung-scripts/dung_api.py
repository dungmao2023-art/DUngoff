# -*- coding: utf-8 -*-
# Thư viện script Python do Dũng Office đóng gói (cài vào hồ sơ engine: user/Scripts/python/dung_api.py,
# hoặc vào share/Scripts/python của engine). CHỈ để ứng dụng gọi qua LOK runMacro:
#   vnd.sun.star.script:dung_api.py$call_api?language=Python&location=user
# Ứng dụng KHÔNG BAO GIỜ chạy macro nằm trong tệp của người dùng; thư viện này nằm ngoài tài liệu.
#
# Giao thức (runMacro không truyền được đối số và không trả về giá trị):
#   - Ứng dụng ghi <hồ sơ>/user/dung-api/args.json:  {"nonce": "...", "fn": "tên_hàm", "args": {...}, "expect_url": "file:///..."}
#   - Ứng dụng gọi runMacro tới call_api().
#   - call_api() đọc args.json (rồi xoá ngay), kiểm tra tên hàm trong WHITELIST và từng đối số theo lược đồ,
#     chạy hàm trên tài liệu hiện hành, ghi <hồ sơ>/user/dung-api/result.json:
#       {"nonce": "...", "ok": true|false, "message": "...", "data": {...}, "mutated": true|false}
# Không dùng eval/exec; không nhận đường dẫn tệp tuỳ ý; đối số lạ bị từ chối.
import json
import os
import re

try:  # có sẵn khi chạy trong LibreOffice / pyuno
    import uno
except Exception:  # pragma: no cover - chỉ để import được ngoài LibreOffice khi kiểm thử cú pháp
    uno = None

API_VERSION = 1

# ----------------------------------------------------------------------------- lỗi và kiểm tra đối số


class ApiError(Exception):
    """Lỗi có thông báo tiếng Việt cho người dùng."""


_A1 = re.compile(r"^[A-Za-z]{1,3}[0-9]{1,7}(:[A-Za-z]{1,3}[0-9]{1,7})?$")
_COLOR = re.compile(r"^#[0-9A-Fa-f]{6}$")
_NAME = re.compile(r"^[^\x00-\x1f]{1,64}$")


def _num(a, name, lo, hi, default=None):
    if name not in a:
        if default is None:
            raise ApiError("Thiếu giá trị '%s'." % name)
        return default
    v = a[name]
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        raise ApiError("'%s' phải là số." % name)
    if v < lo or v > hi:
        raise ApiError("'%s' phải trong khoảng %s đến %s." % (name, lo, hi))
    return float(v)


def _int(a, name, lo, hi, default=None):
    v = _num(a, name, lo, hi, default)
    if v != int(v):
        raise ApiError("'%s' phải là số nguyên." % name)
    return int(v)


def _bool(a, name, default=None):
    if name not in a:
        if default is None:
            raise ApiError("Thiếu giá trị '%s'." % name)
        return default
    if not isinstance(a[name], bool):
        raise ApiError("'%s' phải là đúng/sai." % name)
    return a[name]


def _enum(a, name, allowed, default=None):
    if name not in a:
        if default is None:
            raise ApiError("Thiếu giá trị '%s'." % name)
        return default
    v = a[name]
    if v not in allowed:
        raise ApiError("'%s' chỉ nhận một trong: %s." % (name, ", ".join(allowed)))
    return v


def _str(a, name, maxlen, default=None, allow_empty=True):
    if name not in a:
        if default is None:
            raise ApiError("Thiếu giá trị '%s'." % name)
        return default
    v = a[name]
    if not isinstance(v, str):
        raise ApiError("'%s' phải là chuỗi." % name)
    if len(v) > maxlen:
        raise ApiError("'%s' dài quá %d ký tự." % (name, maxlen))
    if not allow_empty and not v:
        raise ApiError("'%s' không được để trống." % name)
    return v


def _color(a, name, default=None):
    if name not in a:
        return default
    v = a[name]
    if not isinstance(v, str) or not _COLOR.match(v):
        raise ApiError("'%s' phải có dạng #RRGGBB." % name)
    return int(v[1:], 16)


def _only(a, allowed):
    extra = [k for k in a if k not in allowed]
    if extra:
        raise ApiError("Đối số không được hỗ trợ: %s." % ", ".join(sorted(extra)))


def _mm100(mm):
    return int(round(float(mm) * 100.0))


# ----------------------------------------------------------------------------- tiện ích UNO


def _pv(name, value):
    p = uno.createUnoStruct("com.sun.star.beans.PropertyValue")
    p.Name = name
    p.Value = value
    return p


def _is_writer(doc):
    return doc.supportsService("com.sun.star.text.TextDocument")


def _is_calc(doc):
    return doc.supportsService("com.sun.star.sheet.SpreadsheetDocument")


def _need_writer(doc):
    if not _is_writer(doc):
        raise ApiError("Chức năng này chỉ dùng cho tài liệu Word.")


def _need_calc(doc):
    if not _is_calc(doc):
        raise ApiError("Chức năng này chỉ dùng cho bảng tính Excel.")


def _used_page_styles(doc):
    """Kiểu trang đang được dùng: lấy từ PageStyleName của từng đoạn/bảng (không phụ thuộc isInUse, vốn
    sai khi chưa có bố cục), luôn có ít nhất kiểu của đoạn đầu."""
    styles = doc.StyleFamilies.getByName("PageStyles")
    names = []
    enum = doc.Text.createEnumeration()
    while enum.hasMoreElements():
        el = enum.nextElement()
        try:
            nm = el.PageStyleName
        except Exception:
            continue
        if nm and nm not in names:
            names.append(nm)
    if not names:
        try:
            names.append(doc.getCurrentController().getViewCursor().PageStyleName)
        except Exception:
            names.append("Standard")
    return [styles.getByName(n) for n in names if styles.hasByName(n)]


def _sheet(doc, a):
    name = a.get("sheet")
    sheets = doc.Sheets
    if name is None or name == "":
        ctrl = doc.getCurrentController()
        return ctrl.getActiveSheet()
    if not isinstance(name, str) or not sheets.hasByName(name):
        raise ApiError("Không có sheet tên '%s'." % (name if isinstance(name, str) else "?"))
    return sheets.getByName(name)


def _range(sheet, a, key="range"):
    r = a.get(key)
    if not isinstance(r, str) or not _A1.match(r):
        raise ApiError("'%s' phải là vùng dạng A1 hoặc A1:C10." % key)
    return sheet.getCellRangeByName(r.upper())


# ----------------------------------------------------------------------------- hàm Writer


def read_page_margins(doc, a):
    _only(a, ())
    _need_writer(doc)
    res = []
    for st in _used_page_styles(doc):
        res.append({
            "style": st.Name,
            "left_mm": st.LeftMargin / 100.0, "right_mm": st.RightMargin / 100.0,
            "top_mm": st.TopMargin / 100.0, "bottom_mm": st.BottomMargin / 100.0,
            "width_mm": st.Width / 100.0, "height_mm": st.Height / 100.0,
            "landscape": bool(st.IsLandscape),
        })
    return {"data": {"styles": res}, "message": "Đã đọc lề của %d kiểu trang." % len(res), "mutated": False}


def page_margins(doc, a):
    _only(a, ("left_mm", "right_mm", "top_mm", "bottom_mm"))
    _need_writer(doc)
    left = _num(a, "left_mm", 0, 200)
    right = _num(a, "right_mm", 0, 200)
    top = _num(a, "top_mm", 0, 200)
    bottom = _num(a, "bottom_mm", 0, 200)
    n = 0
    for st in _used_page_styles(doc):
        if left + right >= st.Width / 100.0 or top + bottom >= st.Height / 100.0:
            raise ApiError("Tổng lề lớn hơn kích thước trang.")
        st.LeftMargin = _mm100(left)
        st.RightMargin = _mm100(right)
        st.TopMargin = _mm100(top)
        st.BottomMargin = _mm100(bottom)
        n += 1
    return {"message": "Đã đặt lề cho %d kiểu trang." % n, "data": {"styles": n}, "mutated": n > 0}


_PAPER = {"A3": (297, 420), "A4": (210, 297), "A5": (148, 210), "Letter": (215.9, 279.4), "Legal": (215.9, 355.6)}


def page_setup(doc, a):
    _only(a, ("paper", "orientation"))
    _need_writer(doc)
    paper = _enum(a, "paper", tuple(_PAPER.keys()))
    orient = _enum(a, "orientation", ("portrait", "landscape"), "portrait")
    w, h = _PAPER[paper]
    if orient == "landscape":
        w, h = h, w
    n = 0
    for st in _used_page_styles(doc):
        st.IsLandscape = (orient == "landscape")
        st.Width = _mm100(w)
        st.Height = _mm100(h)
        n += 1
    return {"message": "Đã đặt khổ %s %s." % (paper, "ngang" if orient == "landscape" else "dọc"),
            "data": {"styles": n}, "mutated": n > 0}


def header_footer(doc, a):
    _only(a, ("header", "footer", "header_text", "footer_text", "footer_page_number"))
    _need_writer(doc)
    header = _bool(a, "header", False)
    footer = _bool(a, "footer", False)
    htext = _str(a, "header_text", 500, "")
    ftext = _str(a, "footer_text", 500, "")
    pnum = _bool(a, "footer_page_number", False)
    n = 0
    for st in _used_page_styles(doc):
        st.HeaderIsOn = header
        st.FooterIsOn = footer
        if header and htext:
            st.HeaderText.setString(htext)
        if footer and (ftext or pnum):
            text = st.FooterText
            text.setString(ftext + (" " if ftext and pnum else ""))
            if pnum:
                cur = text.createTextCursor()
                cur.gotoEnd(False)
                text.insertString(cur, "Trang ", False)
                field = doc.createInstance("com.sun.star.text.TextField.PageNumber")
                field.NumberingType = 4  # ARABIC
                text.insertTextContent(text.getEnd(), field, False)
        n += 1
    return {"message": "Đã cập nhật đầu/chân trang cho %d kiểu trang." % n, "data": {"styles": n}, "mutated": n > 0}


def toc_insert(doc, a):
    _only(a, ("title", "levels"))
    _need_writer(doc)
    title = _str(a, "title", 200, "Mục lục")
    levels = _int(a, "levels", 1, 10, 3)
    idx = doc.createInstance("com.sun.star.text.ContentIndex")
    idx.Title = title
    idx.Level = levels
    idx.CreateFromOutline = True
    cur = doc.getCurrentController().getViewCursor()
    doc.Text.insertTextContent(cur, idx, False)
    idx.update()
    return {"message": "Đã chèn mục lục (%d cấp)." % levels, "data": {}, "mutated": True}


def toc_update(doc, a):
    _only(a, ())
    _need_writer(doc)
    n = doc.DocumentIndexes.getCount()
    for i in range(n):
        doc.DocumentIndexes.getByIndex(i).update()
    return {"message": "Đã cập nhật %d mục lục/chỉ mục." % n, "data": {"count": n}, "mutated": n > 0}


def _selection_ranges(doc):
    sel = doc.getCurrentController().getSelection()
    try:
        cnt = sel.getCount()
    except Exception:
        cnt = 0
    out = []
    for i in range(cnt):
        out.append(sel.getByIndex(i))
    return out


def paragraph_format(doc, a):
    _only(a, ("align", "left_indent_mm", "right_indent_mm", "first_line_mm",
              "space_before_mm", "space_after_mm", "line_spacing_percent"))
    _need_writer(doc)
    ranges = _selection_ranges(doc)
    if not ranges:
        raise ApiError("Không có vị trí con trỏ/vùng chọn để áp dụng.")
    align = _enum(a, "align", ("left", "center", "right", "justify"), None) if "align" in a else None
    props = {}
    if align:
        props["ParaAdjust"] = uno.Enum("com.sun.star.style.ParagraphAdjust",
                                       {"left": "LEFT", "center": "CENTER", "right": "RIGHT", "justify": "BLOCK"}[align])
    for key, prop, lo, hi in (("left_indent_mm", "ParaLeftMargin", 0, 200), ("right_indent_mm", "ParaRightMargin", 0, 200),
                              ("first_line_mm", "ParaFirstLineIndent", -100, 200),
                              ("space_before_mm", "ParaTopMargin", 0, 100), ("space_after_mm", "ParaBottomMargin", 0, 100)):
        if key in a:
            props[prop] = _mm100(_num(a, key, lo, hi))
    if "line_spacing_percent" in a:
        pct = _int(a, "line_spacing_percent", 50, 500)
        ls = uno.createUnoStruct("com.sun.star.style.LineSpacing")
        ls.Mode = 0  # PROP
        ls.Height = pct
        props["ParaLineSpacing"] = ls
    if not props:
        raise ApiError("Chưa có thuộc tính nào để áp dụng.")
    for r in ranges:
        for k, v in props.items():
            r.setPropertyValue(k, v)
    return {"message": "Đã định dạng đoạn văn.", "data": {"properties": sorted(props.keys())}, "mutated": True}


def outline_numbering(doc, a):
    """Đánh số nhiều cấp cho các đoạn Heading (1., 1.1., 1.1.1.)."""
    _only(a, ("levels",))
    _need_writer(doc)
    levels = _int(a, "levels", 1, 10, 3)
    rules = doc.getChapterNumberingRules()
    for i in range(levels):
        props = (_pv("NumberingType", 4),  # ARABIC
                 _pv("ParentNumbering", i + 1),
                 _pv("Suffix", "." if i == 0 else ""))
        uno.invoke(rules, "replaceByIndex", (i, uno.Any("[]com.sun.star.beans.PropertyValue", props)))
    return {"message": "Đã bật đánh số nhiều cấp cho tiêu đề (%d cấp)." % levels, "data": {"levels": levels}, "mutated": True}


def list_apply(doc, a):
    _only(a, ("kind",))
    _need_writer(doc)
    kind = _enum(a, "kind", ("bullet", "numbered", "none"))
    ctx = uno.getComponentContext() if uno else None
    smgr = ctx.ServiceManager
    helper = smgr.createInstanceWithContext("com.sun.star.frame.DispatchHelper", ctx)
    frame = doc.getCurrentController().getFrame()
    cmd = {"bullet": ".uno:DefaultBullet", "numbered": ".uno:DefaultNumbering", "none": ".uno:RemoveBullets"}[kind]
    helper.executeDispatch(frame, cmd, "", 0, ())
    return {"message": "Đã áp dụng danh sách (%s)." % kind, "data": {}, "mutated": True}


# ----------------------------------------------------------------------------- hàm Calc

_LOCALE_VI = ("vi", "VN", "")
_NUMFMT = {
    "general": "General",
    "number_0": "#,##0",
    "number_2": "#,##0.00",
    "percent": "0.00%",
    "date_dmy": "DD/MM/YYYY",
    "datetime": "DD/MM/YYYY HH:MM",
    "vnd": '#,##0" ₫"',
    "text": "@",
}


def _numfmt_key(doc, code):
    loc = uno.createUnoStruct("com.sun.star.lang.Locale")
    loc.Language, loc.Country, loc.Variant = _LOCALE_VI
    fmts = doc.NumberFormats
    key = fmts.queryKey(code, loc, False)
    if key == -1:
        key = fmts.addNew(code, loc)
    return key


def _border_line(width=26):
    b = uno.createUnoStruct("com.sun.star.table.BorderLine2")
    b.OuterLineWidth = width
    b.LineWidth = width
    b.Color = 0
    b.LineStyle = 0  # SOLID
    return b


def cell_format(doc, a):
    _only(a, ("sheet", "range", "number_format", "font_name", "font_size", "bold", "italic",
              "bg_color", "font_color", "border", "halign", "wrap"))
    _need_calc(doc)
    sheet = _sheet(doc, a)
    rng = _range(sheet, a)
    changed = []
    if "number_format" in a:
        nf = _enum(a, "number_format", tuple(_NUMFMT.keys()))
        rng.NumberFormat = _numfmt_key(doc, _NUMFMT[nf])
        changed.append("number_format")
    if "font_name" in a:
        rng.CharFontName = _str(a, "font_name", 64, allow_empty=False)
        changed.append("font_name")
    if "font_size" in a:
        rng.CharHeight = _num(a, "font_size", 4, 96)
        changed.append("font_size")
    if "bold" in a:
        rng.CharWeight = 150.0 if _bool(a, "bold") else 100.0
        changed.append("bold")
    if "italic" in a:
        rng.CharPosture = uno.Enum("com.sun.star.awt.FontSlant", "ITALIC" if _bool(a, "italic") else "NONE")
        changed.append("italic")
    bg = _color(a, "bg_color")
    if bg is not None:
        rng.CellBackColor = bg
        changed.append("bg_color")
    fc = _color(a, "font_color")
    if fc is not None:
        rng.CharColor = fc
        changed.append("font_color")
    if "halign" in a:
        h = _enum(a, "halign", ("left", "center", "right", "justify"))
        rng.HoriJustify = uno.Enum("com.sun.star.table.CellHoriJustify",
                                   {"left": "LEFT", "center": "CENTER", "right": "RIGHT", "justify": "BLOCK"}[h])
        changed.append("halign")
    if "wrap" in a:
        rng.IsTextWrapped = _bool(a, "wrap")
        changed.append("wrap")
    if "border" in a:
        kind = _enum(a, "border", ("none", "outline", "all"))
        tb = rng.TableBorder2
        line = _border_line() if kind != "none" else uno.createUnoStruct("com.sun.star.table.BorderLine2")
        tb.TopLine = tb.BottomLine = tb.LeftLine = tb.RightLine = line
        inner = _border_line() if kind == "all" else uno.createUnoStruct("com.sun.star.table.BorderLine2")
        tb.HorizontalLine = tb.VerticalLine = inner
        tb.IsTopLineValid = tb.IsBottomLineValid = tb.IsLeftLineValid = tb.IsRightLineValid = True
        tb.IsHorizontalLineValid = tb.IsVerticalLineValid = True
        rng.TableBorder2 = tb
        changed.append("border")
    if not changed:
        raise ApiError("Chưa có thuộc tính định dạng nào.")
    return {"message": "Đã định dạng ô (%s)." % ", ".join(changed), "data": {"changed": changed}, "mutated": True}


def data_validation(doc, a):
    _only(a, ("sheet", "range", "kind", "items", "min", "max", "error_message", "clear"))
    _need_calc(doc)
    sheet = _sheet(doc, a)
    rng = _range(sheet, a)
    v = rng.Validation
    if _bool(a, "clear", False):
        v.Type = uno.Enum("com.sun.star.sheet.ValidationType", "ANY")
        rng.Validation = v
        return {"message": "Đã bỏ kiểm tra dữ liệu.", "data": {}, "mutated": True}
    kind = _enum(a, "kind", ("list", "whole", "decimal", "textlength"))
    vt = uno.Enum
    if kind == "list":
        items = a.get("items")
        if not isinstance(items, list) or not items or len(items) > 100:
            raise ApiError("'items' phải là danh sách từ 1 đến 100 mục.")
        for it in items:
            if not isinstance(it, str) or not it or len(it) > 255 or '"' in it or ";" in it:
                raise ApiError("Mỗi mục danh sách phải là chuỗi 1–255 ký tự, không chứa dấu \" hoặc ;.")
        v.Type = vt("com.sun.star.sheet.ValidationType", "LIST")
        v.Formula1 = ";".join('"%s"' % it for it in items)
        v.ShowList = 1
    else:
        lo = _num(a, "min", -1e15, 1e15)
        hi = _num(a, "max", -1e15, 1e15)
        if lo > hi:
            raise ApiError("'min' không được lớn hơn 'max'.")
        v.Type = vt("com.sun.star.sheet.ValidationType",
                    {"whole": "WHOLE", "decimal": "DECIMAL", "textlength": "TEXT_LEN"}[kind])
        v.Operator = vt("com.sun.star.sheet.ConditionOperator", "BETWEEN")
        v.Formula1 = repr(lo if kind == "decimal" else int(lo))
        v.Formula2 = repr(hi if kind == "decimal" else int(hi))
    v.ShowErrorMessage = True
    v.ErrorAlertStyle = vt("com.sun.star.sheet.ValidationAlertStyle", "STOP")
    v.ErrorTitle = "Dữ liệu không hợp lệ"
    v.ErrorMessage = _str(a, "error_message", 255, "Giá trị không nằm trong phạm vi cho phép.")
    rng.Validation = v
    return {"message": "Đã đặt kiểm tra dữ liệu (%s)." % kind, "data": {}, "mutated": True}


_CF_OPS = {"greater": "GREATER", "less": "LESS", "equal": "EQUAL", "greater_equal": "GREATER_EQUAL",
           "less_equal": "LESS_EQUAL", "not_equal": "NOT_EQUAL", "between": "BETWEEN"}


def conditional_format(doc, a):
    _only(a, ("sheet", "range", "rule", "value", "value2", "bg_color", "font_color", "bold", "clear"))
    _need_calc(doc)
    sheet = _sheet(doc, a)
    rng = _range(sheet, a)
    if _bool(a, "clear", False):
        cf = rng.ConditionalFormat
        cf.clear()
        rng.ConditionalFormat = cf
        return {"message": "Đã bỏ định dạng có điều kiện.", "data": {}, "mutated": True}
    rule = _enum(a, "rule", tuple(_CF_OPS.keys()))
    v1 = _num(a, "value", -1e15, 1e15)
    v2 = _num(a, "value2", -1e15, 1e15, 0.0) if rule == "between" else None
    if rule == "between" and "value2" not in a:
        raise ApiError("Thiếu giá trị 'value2'.")
    bg = _color(a, "bg_color")
    fc = _color(a, "font_color")
    bold = _bool(a, "bold", False)
    if bg is None and fc is None and not bold:
        raise ApiError("Cần ít nhất một kiểu: bg_color, font_color hoặc bold.")
    styles = doc.StyleFamilies.getByName("CellStyles")
    n = 1
    while styles.hasByName("Dung_CF_%d" % n):
        n += 1
    name = "Dung_CF_%d" % n
    st = doc.createInstance("com.sun.star.style.CellStyle")
    styles.insertByName(name, st)
    st = styles.getByName(name)
    if bg is not None:
        st.CellBackColor = bg
    if fc is not None:
        st.CharColor = fc
    if bold:
        st.CharWeight = 150.0
    cf = rng.ConditionalFormat
    props = [_pv("Operator", uno.Enum("com.sun.star.sheet.ConditionOperator", _CF_OPS[rule])),
             _pv("Formula1", repr(v1)), _pv("StyleName", name)]
    if v2 is not None:
        props.append(_pv("Formula2", repr(v2)))
    cf.addNew(tuple(props))
    rng.ConditionalFormat = cf
    return {"message": "Đã thêm định dạng có điều kiện (%s)." % rule, "data": {"style": name}, "mutated": True}


def protect_sheet(doc, a):
    _only(a, ("sheet", "protect", "password"))
    _need_calc(doc)
    sheet = _sheet(doc, a)
    protect = _bool(a, "protect")
    pw = _str(a, "password", 64, "")
    if protect:
        sheet.protect(pw)
        return {"message": "Đã bảo vệ sheet%s." % (" (có mật khẩu)" if pw else ""), "data": {}, "mutated": True}
    if sheet.isProtected():
        try:
            sheet.unprotect(pw)
        except Exception:
            raise ApiError("Không bỏ được bảo vệ: mật khẩu không đúng.")
        if sheet.isProtected():
            raise ApiError("Không bỏ được bảo vệ: mật khẩu không đúng.")
        return {"message": "Đã bỏ bảo vệ sheet.", "data": {}, "mutated": True}
    return {"message": "Sheet không được bảo vệ.", "data": {}, "mutated": False}


def sort_range(doc, a):
    _only(a, ("sheet", "range", "keys", "has_header"))
    _need_calc(doc)
    sheet = _sheet(doc, a)
    rng = _range(sheet, a)
    keys = a.get("keys")
    if not isinstance(keys, list) or not 1 <= len(keys) <= 3:
        raise ApiError("'keys' phải có từ 1 đến 3 khoá sắp xếp.")
    ncols = rng.getColumns().getCount()
    fields = []
    for k in keys:
        if not isinstance(k, dict):
            raise ApiError("Mỗi khoá phải là {column, ascending}.")
        _only(k, ("column", "ascending"))
        col = _int(k, "column", 0, 16383)
        if col >= ncols:
            raise ApiError("Cột %d nằm ngoài vùng chọn (%d cột)." % (col, ncols))
        f = uno.createUnoStruct("com.sun.star.table.TableSortField")
        f.Field = col
        f.IsAscending = _bool(k, "ascending", True)
        f.IsCaseSensitive = False
        fields.append(f)
    desc = (_pv("SortFields", uno.Any("[]com.sun.star.table.TableSortField", tuple(fields))),
            _pv("ContainsHeader", _bool(a, "has_header", True)))
    rng.sort(desc)
    return {"message": "Đã sắp xếp %d khoá." % len(fields), "data": {}, "mutated": True}


_DELIMS = {"comma": ",", "semicolon": ";", "tab": "\t", "space": " ", "pipe": "|"}


def text_to_columns(doc, a):
    _only(a, ("sheet", "range", "delimiter", "overwrite"))
    _need_calc(doc)
    sheet = _sheet(doc, a)
    rng = _range(sheet, a)
    if rng.getColumns().getCount() != 1:
        raise ApiError("Chỉ tách được một cột mỗi lần.")
    delim = _DELIMS[_enum(a, "delimiter", tuple(_DELIMS.keys()))]
    overwrite = _bool(a, "overwrite", False)
    addr = rng.getRangeAddress()
    nrows = addr.EndRow - addr.StartRow + 1
    parts_all = []
    maxc = 1
    for r in range(nrows):
        cell = sheet.getCellByPosition(addr.StartColumn, addr.StartRow + r)
        if cell.getFormula().startswith("="):
            raise ApiError("Ô có công thức, không tách (tránh làm mất công thức).")
        parts = cell.getString().split(delim)
        parts_all.append(parts)
        maxc = max(maxc, len(parts))
    if maxc > 200:
        raise ApiError("Tách ra quá nhiều cột (tối đa 200).")
    # kiểm tra vùng đích trước khi ghi: không ghi đè dữ liệu có sẵn
    if maxc > 1 and not overwrite:
        for r in range(nrows):
            for c in range(1, maxc):
                cell = sheet.getCellByPosition(addr.StartColumn + c, addr.StartRow + r)
                if cell.getType().value != "EMPTY":
                    raise ApiError("Vùng đích có dữ liệu sẵn; không ghi đè (cần cho phép ghi đè).")
    for r, parts in enumerate(parts_all):
        for c, p in enumerate(parts):
            sheet.getCellByPosition(addr.StartColumn + c, addr.StartRow + r).setString(p)  # luôn là chữ, giữ số 0 đầu
    return {"message": "Đã tách %d dòng thành tối đa %d cột (giá trị giữ dạng chữ)." % (nrows, maxc),
            "data": {"rows": nrows, "columns": maxc}, "mutated": maxc > 1}


# ----------------------------------------------------------------------------- chung


def doc_properties(doc, a):
    _only(a, ("title", "subject", "author", "description", "keywords"))
    props = doc.DocumentProperties
    changed = []
    if "title" in a:
        props.Title = _str(a, "title", 255)
        changed.append("title")
    if "subject" in a:
        props.Subject = _str(a, "subject", 255)
        changed.append("subject")
    if "author" in a:
        props.Author = _str(a, "author", 255)
        changed.append("author")
    if "description" in a:
        props.Description = _str(a, "description", 2000)
        changed.append("description")
    if "keywords" in a:
        kw = a["keywords"]
        if not isinstance(kw, list) or len(kw) > 50 or any(not isinstance(k, str) or len(k) > 64 for k in kw):
            raise ApiError("'keywords' phải là danh sách tối đa 50 chuỗi ngắn.")
        props.Keywords = tuple(kw)
        changed.append("keywords")
    if not changed:
        raise ApiError("Chưa có thuộc tính nào để đặt.")
    return {"message": "Đã đặt thuộc tính tài liệu (%s)." % ", ".join(changed), "data": {"changed": changed}, "mutated": True}


def read_doc_properties(doc, a):
    _only(a, ())
    p = doc.DocumentProperties
    return {"message": "Đã đọc thuộc tính tài liệu.", "mutated": False,
            "data": {"title": p.Title, "subject": p.Subject, "author": p.Author,
                     "description": p.Description, "keywords": list(p.Keywords)}}


def ping(doc, a):
    _only(a, ())
    kind = "writer" if _is_writer(doc) else "calc" if _is_calc(doc) else "other"
    return {"message": "OK", "data": {"api_version": API_VERSION, "kind": kind}, "mutated": False}


# Danh sách trắng: chỉ các hàm này được gọi. Thêm hàm mới = thêm vào đây và vào bảng trong ScriptBridge.cpp
# (có kiểm thử đối chiếu hai danh sách).
WHITELIST = {
    "ping": ping,
    "read_page_margins": read_page_margins,
    "page_margins": page_margins,
    "page_setup": page_setup,
    "header_footer": header_footer,
    "toc_insert": toc_insert,
    "toc_update": toc_update,
    "paragraph_format": paragraph_format,
    "outline_numbering": outline_numbering,
    "list_apply": list_apply,
    "cell_format": cell_format,
    "data_validation": data_validation,
    "conditional_format": conditional_format,
    "protect_sheet": protect_sheet,
    "sort_range": sort_range,
    "text_to_columns": text_to_columns,
    "doc_properties": doc_properties,
    "read_doc_properties": read_doc_properties,
}

# ----------------------------------------------------------------------------- điểm vào


def _norm_url(u):
    return (u or "").replace("\\", "/").rstrip("/").lower()


def run_call(doc, request):
    """Chạy một yêu cầu đã giải mã JSON trên tài liệu `doc`. Trả về dict kết quả (luôn có ok/message)."""
    nonce = request.get("nonce", "") if isinstance(request, dict) else ""
    res = {"nonce": nonce, "ok": False, "message": "", "data": {}, "mutated": False}
    try:
        if not isinstance(request, dict):
            raise ApiError("Yêu cầu không hợp lệ.")
        fn = request.get("fn")
        if not isinstance(fn, str) or fn not in WHITELIST:
            raise ApiError("Hàm '%s' không có trong danh sách cho phép." % (fn if isinstance(fn, str) else "?"))
        args = request.get("args", {})
        if not isinstance(args, dict):
            raise ApiError("Đối số phải là một đối tượng JSON.")
        expect = request.get("expect_url", "")
        if doc is None:
            raise ApiError("Không có tài liệu đang mở để áp dụng.")
        if expect:
            cur = ""
            try:
                cur = doc.getURL()
            except Exception:
                pass
            if _norm_url(cur) != _norm_url(expect):
                raise ApiError("Tài liệu đang hoạt động trong engine không phải tài liệu cần sửa; không áp dụng để tránh sửa nhầm.")
        out = WHITELIST[fn](doc, args)
        res.update(ok=True, message=out.get("message", "OK"), data=out.get("data", {}), mutated=bool(out.get("mutated", False)))
    except ApiError as e:
        res["message"] = str(e)
    except Exception as e:  # lỗi không lường trước: trả về cho ứng dụng thay vì làm sập engine
        res["message"] = "Lỗi nội bộ khi chạy %s: %s" % (request.get("fn", "?") if isinstance(request, dict) else "?", e)
    return res


def _dir():
    ctx = XSCRIPTCONTEXT.getComponentContext()  # noqa: F821 - do LibreOffice cung cấp
    sub = ctx.ServiceManager.createInstanceWithContext("com.sun.star.util.PathSubstitution", ctx)
    user = uno.fileUrlToSystemPath(sub.substituteVariables("$(user)", True))
    return os.path.join(user, "dung-api")


def call_api():
    d = _dir()
    args_path = os.path.join(d, "args.json")
    result_path = os.path.join(d, "result.json")
    request = None
    try:
        with open(args_path, "r", encoding="utf-8") as f:
            request = json.load(f)
        try:
            os.remove(args_path)  # có thể chứa mật khẩu sheet: xoá ngay sau khi đọc
        except OSError:
            pass
    except Exception as e:
        res = {"nonce": "", "ok": False, "message": "Không đọc được yêu cầu của ứng dụng: %s" % e, "data": {}, "mutated": False}
    else:
        doc = XSCRIPTCONTEXT.getDocument()  # noqa: F821
        res = run_call(doc, request)
    tmp = result_path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False)
    os.replace(tmp, result_path)


g_exportedScripts = (call_api,)
