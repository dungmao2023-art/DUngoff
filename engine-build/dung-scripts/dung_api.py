# -*- coding: utf-8 -*-
# Thư viện script Python do Dũng Office đóng gói trong engine (share/Scripts/python/dung_api.py).
# CHỈ để ứng dụng gọi qua LOK runMacro (vnd.sun.star.script:dung_api.py$tên_hàm?language=Python&location=share).
# Ứng dụng KHÔNG bao giờ chạy macro nằm trong tệp của người dùng; thư viện này nằm ngoài tài liệu.
# Tham số tuỳ biến (margins_custom...) đọc từ tệp JSON do ứng dụng ghi, đường dẫn trong biến môi trường
# DUNG_API_ARGS_FILE (đặt một lần lúc khởi động). Mọi hàm trả về chuỗi "OK ..." hoặc "LOI ...", ghi log nếu có.
import json
import os


def _mm100(v):
    return int(round(float(v) * 100.0))


def _doc():
    return XSCRIPTCONTEXT.getDocument()  # noqa: F821 - do LibreOffice cung cấp lúc chạy script


def _page_styles(doc):
    return doc.StyleFamilies.getByName("PageStyles")


def apply_margins(doc, left_mm, right_mm, top_mm, bottom_mm):
    """Đặt lề cho mọi kiểu trang đang được dùng (Writer/Calc). Đơn vị mm. Trả về số kiểu đã đổi."""
    styles = _page_styles(doc)
    n = 0
    for name in styles.ElementNames:
        st = styles.getByName(name)
        used = True
        try:
            used = bool(st.isInUse())
        except Exception:
            pass
        if not used:
            continue
        st.LeftMargin = _mm100(left_mm)
        st.RightMargin = _mm100(right_mm)
        st.TopMargin = _mm100(top_mm)
        st.BottomMargin = _mm100(bottom_mm)
        n += 1
    return n


def apply_header_footer(doc, header, footer):
    styles = _page_styles(doc)
    n = 0
    for name in styles.ElementNames:
        st = styles.getByName(name)
        try:
            if not st.isInUse():
                continue
        except Exception:
            pass
        st.HeaderIsOn = bool(header)
        st.FooterIsOn = bool(footer)
        n += 1
    return n


def _safe(fn):
    try:
        return "OK " + str(fn())
    except Exception as e:  # trả lỗi cho ứng dụng thay vì làm sập engine
        return "LOI " + repr(e)


def margins_vn():
    """Thể thức văn bản hành chính (NĐ 30/2020/NĐ-CP): trái 30, phải 15, trên 20, dưới 20 mm."""
    return _safe(lambda: apply_margins(_doc(), 30, 15, 20, 20))


def margins_normal():
    return _safe(lambda: apply_margins(_doc(), 25.4, 25.4, 25.4, 25.4))


def margins_narrow():
    return _safe(lambda: apply_margins(_doc(), 12.7, 12.7, 12.7, 12.7))


def _args():
    path = os.environ.get("DUNG_API_ARGS_FILE", "")
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def margins_custom():
    def run():
        a = _args()
        return apply_margins(_doc(), a["left"], a["right"], a["top"], a["bottom"])
    return _safe(run)


def header_footer_on():
    return _safe(lambda: apply_header_footer(_doc(), True, True))


def header_footer_off():
    return _safe(lambda: apply_header_footer(_doc(), False, False))


def set_title():
    def run():
        a = _args()
        _doc().DocumentProperties.Title = str(a["title"])
        return "title"
    return _safe(run)


g_exportedScripts = (margins_vn, margins_normal, margins_narrow, margins_custom,
                     header_footer_on, header_footer_off, set_title)
