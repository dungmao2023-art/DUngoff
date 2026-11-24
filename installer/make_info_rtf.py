# Sinh installer/thong-tin.rtf (Inno Setup hiển thị RTF; ký tự tiếng Việt viết dạng \uN).
# Chạy: python3 installer/make_info_rtf.py   (sửa EMAIL bên dưới trước khi phát hành)
EMAIL = "[điền email liên hệ của tác giả]"

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
 head("DŨNG OFFICE — bộ Office miễn phí cho người Việt"),
 hot("Tác giả và chủ dự án: Trần Văn Dũng"),
 txt("Dũng Office chạy ngoại tuyến, không cần tài khoản, không gửi tệp của bạn lên mạng."),
 head("Các bên cung cấp module"),
 txt("Dũng Office dùng nguyên bản các thành phần mã nguồn mở sau (chưa sửa đổi mã):"),
 txt("•  LibreOffice (The Document Foundation) — bộ máy xử lý Word, Excel, PowerPoint; giấy phép MPL 2.0."),
 txt("•  Qt 6 (The Qt Company) — giao diện; giấy phép LGPL v3."),
 txt("•  qpdf (Jay Berkenbilt) — công cụ PDF; giấy phép Apache 2.0."),
 txt("Chi tiết đầy đủ trong tệp THIRD_PARTY_NOTICES.md đi kèm. Dũng Office là dự án độc lập, không liên kết với các bên trên."),
 head("Tính năng riêng cho người Việt"),
 hot("Căn lề chuẩn theo thể thức văn bản hành chính Việt Nam"),
 hot("Kho thư viện mẫu theo quy định pháp luật Việt Nam"),
 txt("Hai mục trên đang được phát triển và sẽ có trong các bản cập nhật; mỗi mẫu đều ghi rõ nguồn và thời điểm áp dụng."),
 txt("Đã có: giao diện tiếng Việt, mở/sửa/lưu DOCX, XLSX, PPTX, công cụ PDF (gộp, tách, xoay, nén), luôn lưu ra bản mới để không mất tệp gốc."),
 head("Cam kết"),
 hot("MIỄN PHÍ TRỌN ĐỜI — không quảng cáo, không watermark, không giới hạn lượt dùng"),
 hot("CẬP NHẬT MIỄN PHÍ — mọi bản cập nhật đều không thu phí"),
 txt("Bạn quyết định có cập nhật hay không; ứng dụng không tự gửi dữ liệu đi đâu."),
 head("Góp ý và đề xuất tính năng"),
 hot("Nếu bạn muốn bổ sung tính năng, vui lòng liên hệ qua email: " + EMAIL),
]
rtf = ("{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fswiss Segoe UI;}}"
       "{\\colortbl;\\red0\\green0\\blue0;\\red31\\green80\\blue187;\\red255\\green255\\blue200;\\red160\\green40\\blue40;}\n"
       + "\n".join(body) + "}")
open("installer/thong-tin.rtf", "w", encoding="ascii").write(rtf)
print("đã ghi installer/thong-tin.rtf")
