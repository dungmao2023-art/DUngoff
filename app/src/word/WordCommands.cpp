// SPDX-License-Identifier: MPL-2.0
#include "WordCommands.h"

#include <cctype>
#include <cstdio>

namespace dung::word {

static std::string esc(const std::string& s) {
    std::string v;
    for (unsigned char c : s) {
        switch (c) {
        case '"': v += "\\\""; break;
        case '\\': v += "\\\\"; break;
        case '\n': v += "\\n"; break;
        case '\r': v += "\\r"; break;
        case '\t': v += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); v += b; }
            else v += (char)c;
        }
    }
    return v;
}

static std::string wrap(const std::string& name, const char* type, const std::string& valueJson) {
    return "\"" + esc(name) + "\":{\"type\":\"" + type + "\",\"value\":" + valueJson + "}";
}

std::string argString(const std::string& n, const std::string& v) { return "{" + wrap(n, "string", "\"" + esc(v) + "\"") + "}"; }
std::string argLong(const std::string& n, long long v) { return "{" + wrap(n, "long", std::to_string(v)) + "}"; }
std::string argFloat(const std::string& n, double v) {
    char b[48];
    std::snprintf(b, sizeof b, "%.2f", v);
    return "{" + wrap(n, "float", b) + "}";
}
std::string argBool(const std::string& n, bool v) { return "{" + wrap(n, "boolean", v ? "true" : "false") + "}"; }

std::string join(const std::vector<std::string>& args) {
    std::string out = "{";
    bool first = true;
    for (const auto& a : args) {
        if (a.size() < 2) continue;
        if (!first) out += ",";
        out += a.substr(1, a.size() - 2);   // bỏ { }
        first = false;
    }
    return out + "}";
}

Call fontName(const std::string& family) {
    return {".uno:CharFontName", join({argString("CharFontName.FamilyName", family),
                                       argString("CharFontName.StyleName", ""),
                                       argLong("CharFontName.Pitch", 0),
                                       argLong("CharFontName.CharSet", -1)})};
}
Call fontSize(double pt) { return {".uno:FontHeight", argFloat("FontHeight.Height", pt)}; }
Call fontColor(unsigned rgb) { return {".uno:Color", argLong("Color", rgb & 0xFFFFFF)}; }
Call backColor(unsigned rgb) { return {".uno:BackColor", argLong("BackColor", rgb & 0xFFFFFF)}; }
Call paragraphStyle(const std::string& s) {
    return {".uno:StyleApply", join({argString("Style", s), argString("FamilyName", "ParagraphStyles")})};
}
Call insertTable(int c, int r) { return {".uno:InsertTable", join({argLong("Columns", c), argLong("Rows", r)})}; }
Call splitCell(int amount, bool horizontal) {
    return {".uno:SplitCell", join({argLong("Amount", amount), argBool("Horizontal", horizontal), argBool("Proportional", false)})};
}
Call tableDimension(bool row, unsigned twips) {
    const std::string name=row?"TableRowHeight":"TableColumWidth";
    // CntUInt32Item::PutValue in svl/source/items/cintitem.cxx
    // extracts sal_Int32, despite the property's UInt32 item name.
    return {".uno:"+name,argLong(name,twips)};
}
Call enablePageBand(bool header, const std::string& pageStyle) {
    return {header ? ".uno:InsertPageHeader" : ".uno:InsertPageFooter", join({argString("PageStyle", pageStyle), argBool("On", true)})};
}
Call insertFootnote(bool endnote) { return {endnote ? ".uno:InsertEndnote" : ".uno:InsertFootnote", ""}; }
Call insertImage(const std::string& url) { return {".uno:InsertGraphic", argString("FileName", url)}; }
Call hyperlink(const std::string& text, const std::string& url) {
    return {".uno:SetHyperlink", join({argString("Hyperlink.Text", text), argString("Hyperlink.URL", url)})};
}
// SearchItem.Command theo SvxSearchCmd: 0=Tìm, 1=Tìm tất cả, 2=Thay, 3=Thay tất cả.
static Call search(const std::string& find, const std::string& rep, int cmd, bool backward, bool matchCase) {
    // sfx2/sdi/sfxitems.sdi: TransliterateFlags is INT32.
    // i18n::TransliterationModules::IGNORE_CASE = 0x100.
    // Explicit ABSOLUTE literal search and no selection-only/word-only flags.
    return {".uno:ExecuteSearch", join({argString("SearchItem.SearchString", find),
                                        argString("SearchItem.ReplaceString", rep),
                                        argLong("SearchItem.Command", cmd),
                                        argLong("SearchItem.AlgorithmType", 0),
                                        argLong("SearchItem.SearchFlags", 0),
                                        argLong("SearchItem.TransliterateFlags", matchCase ? 0 : 0x100),
                                        argBool("SearchItem.Backward", backward)})};
}
Call findNext(const std::string& t, bool backward, bool matchCase) { return search(t, "", 0, backward, matchCase); }
Call replaceAll(const std::string& f, const std::string& r, bool matchCase) { return search(f, r, 3, false, matchCase); }
Call replaceOne(const std::string& f, const std::string& r) { return search(f, r, 2, false, true); }

static std::string htmlEscape(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\n': out += "<br>"; break;
        case '\r': break;
        default: out += ch;
        }
    }
    return out;
}
std::string adminBlockHtml(const AdminBlock& b, bool head, bool tail) {
    auto v = [](const std::string& s, const char* fallback) { return htmlEscape(s.empty() ? std::string(fallback) : s); };
    std::string html = "<html><body style=\"font-family:Times New Roman,serif;font-size:13pt;color:#000\">";
    const std::string table = "<table width=\"100%\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse:collapse;font-family:Times New Roman,serif;font-size:13pt\"><tr>";
    const std::string left = "<td width=\"42%\" valign=\"top\" style=\"border:none\">";
    const std::string right = "<td width=\"58%\" valign=\"top\" style=\"border:none\">";
    if (head) {
        html += table + left + "<p align=\"center\">";
        if (!b.parentAgency.empty()) html += htmlEscape(b.parentAgency) + "<br>";
        html += "<b>" + v(b.agency, "TÊN CƠ QUAN, TỔ CHỨC") + "</b><br>────────</p><p align=\"center\">Số: " + v(b.number, "…/…") + "</p></td>";
        html += right + "<p align=\"center\" style=\"font-size:12pt\"><b>CỘNG HÒA XÃ HỘI CHỦ NGHĨA VIỆT NAM</b></p>"
                "<p align=\"center\" style=\"font-size:13pt\"><b>Độc lập - Tự do - Hạnh phúc</b><br>────────────────</p>"
                "<p align=\"center\"><i>" + v(b.placeDate, "…, ngày … tháng … năm …") + "</i></p></td></tr></table>";
        if (!b.title.empty()) html += "<p align=\"center\" style=\"font-size:14pt\"><b>" + htmlEscape(b.title) + "</b></p>";
        if (!b.subject.empty()) html += "<p align=\"center\"><b>" + htmlEscape(b.subject) + "</b></p>";
        html += "<p>&nbsp;</p>";
    }
    if (tail) {
        html += table + left + "<p style=\"font-size:12pt\"><b><i>Nơi nhận:</i></b><br><span style=\"font-size:11pt\">" + v(b.recipient, "- …;\n- Lưu: VT.") + "</span></p></td>";
        html += right + "<p align=\"center\"><b>" + v(b.signerRole, "THỦ TRƯỞNG ĐƠN VỊ") + "</b></p><p>&nbsp;</p><p>&nbsp;</p><p align=\"center\"><b>" + v(b.signerName, "Họ và tên") + "</b></p></td></tr></table><p>&nbsp;</p>";
    }
    return html + "</body></html>";
}

const std::vector<StyleChoice>& paragraphStyles() {
    static const std::vector<StyleChoice> v = {
        {"Văn bản thường", "Default Paragraph Style"},
        {"Tiêu đề 1", "Heading 1"},
        {"Tiêu đề 2", "Heading 2"},
        {"Tiêu đề 3", "Heading 3"},
    };
    return v;
}
const std::vector<double>& commonFontSizes() {
    static const std::vector<double> v = {8, 9, 10, 10.5, 11, 12, 14, 16, 18, 20, 24, 28, 36, 48, 72};
    return v;
}

std::vector<std::string> parseFontList(const std::string& json) {
    // Dạng: {"commandName":".uno:CharFontName","commandValues":{"Tên phông":[cỡ,...],...}}
    std::vector<std::string> fonts;
    const size_t cv = json.find("\"commandValues\"");
    if (cv == std::string::npos) return fonts;
    size_t i = json.find('{', cv);
    if (i == std::string::npos) return fonts;
    int depth = 0;
    for (; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '{' || c == '[') { ++depth; continue; }
        if (c == '}' || c == ']') { if (--depth <= 0) break; continue; }
        if (c == '"') {
            std::string s;
            for (++i; i < json.size() && json[i] != '"'; ++i) {
                if (json[i] == '\\' && i + 1 < json.size()) ++i;
                s += json[i];
            }
            // Khoá của đối tượng ở độ sâu 1 = tên phông (theo sau bởi ':')
            size_t j = i + 1;
            while (j < json.size() && std::isspace((unsigned char)json[j])) ++j;
            if (depth == 1 && j < json.size() && json[j] == ':' && !s.empty()) fonts.push_back(s);
        }
    }
    return fonts;
}

int countWords(const std::string& s) {
    int n = 0;
    bool in = false;
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = (unsigned char)s[i];
        bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
        // U+00A0 (C2 A0) coi là khoảng trắng
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xA0) { space = true; ++i; }
        if (space) in = false;
        else if (!in) { in = true; ++n; }
    }
    return n;
}

}  // namespace dung::word
