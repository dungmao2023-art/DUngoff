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
Call insertImage(const std::string& url) { return {".uno:InsertGraphic", argString("FileName", url)}; }
Call hyperlink(const std::string& text, const std::string& url) {
    return {".uno:SetHyperlink", join({argString("Hyperlink.Text", text), argString("Hyperlink.URL", url)})};
}
// SearchItem.Command theo SvxSearchCmd: 0=Tìm, 1=Tìm tất cả, 2=Thay, 3=Thay tất cả.
static Call search(const std::string& find, const std::string& rep, int cmd, bool backward) {
    return {".uno:ExecuteSearch", join({argString("SearchItem.SearchString", find),
                                        argString("SearchItem.ReplaceString", rep),
                                        argLong("SearchItem.Command", cmd),
                                        argBool("SearchItem.Backward", backward)})};
}
Call findNext(const std::string& t, bool backward) { return search(t, "", 0, backward); }
Call replaceAll(const std::string& f, const std::string& r) { return search(f, r, 3, false); }
Call replaceOne(const std::string& f, const std::string& r) { return search(f, r, 2, false); }

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
