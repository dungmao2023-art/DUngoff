# Sinh installer/thong-tin.rtf (Inno Setup hiển thị RTF; ký tự tiếng Việt viết dạng \uN).
# Chạy: python3 installer/make_info_rtf.py

def esc(t):
    out = []
    for ch in t:
        o = ord(ch)
        if ch in "\\{}": out.append("\\" + ch)
        elif o < 128: out.append(ch)
        else:
            if o > 32767: o -= 65536
            out.append("\\u%d?" % o)
    return "".join(out)

# \cf2 = xanh đậm, \highlight3 = nền vàng nhạt; \b = đậm
def head(t): return "{\\pard\\sa120\\sb200\\f0\\fs30\\b\\cf2 " + esc(t) + "\\b0\\cf0\\par}"
def hot(t):  return "{\\pard\\sa100\\f0\\fs24\\b\\highlight3\\cf4 " + esc(" " + t + " ") + "\\highlight0\\b0\\cf0\\par}"
def txt(t):  return "{\\pard\\sa80\\f0\\fs22 " + esc(t) + "\\par}"

body = [
 head("DŨNG OFFICE"),
 hot("Luôn miễn phí và cập nhật miễn phí trọn đời"),
 txt("Không quảng cáo, không watermark, không giới hạn lượt sử dụng."),
 head("Mã nguồn và thành phần sử dụng"),
 txt("• LibreOffice / LibreOfficeKit — xử lý tài liệu; MPL 2.0 và giấy phép thành phần."),
 txt("• Qt 6 / Qt PDF (PDFium) — giao diện và xem PDF; LGPL v3 và giấy phép thành phần."),
 txt("• qpdf — công cụ PDF; Apache 2.0."),
 txt("• hunspell-vi — từ điển tiếng Việt tùy chọn; GPL."),
 txt("• Inno Setup — công cụ tạo bộ cài."),
 txt("Nguồn, tác giả và giấy phép đầy đủ: THIRD_PARTY_NOTICES.md, LICENSE và các thông báo đi kèm."),
]
rtf = ("{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fswiss Segoe UI;}}"
       "{\\colortbl;\\red0\\green0\\blue0;\\red31\\green80\\blue187;\\red255\\green255\\blue200;\\red160\\green40\\blue40;}\n"
       + "\n".join(body) + "}")
open("installer/thong-tin.rtf", "w", encoding="ascii").write(rtf)
print("đã ghi installer/thong-tin.rtf")
