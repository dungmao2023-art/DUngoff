// SPDX-License-Identifier: MPL-2.0
// Dựng đối số JSON cho các lệnh UNO của Excel. Chỉ gồm lệnh ĐÃ KIỂM CHỨNG bằng engine
// (xem tools/smoketest/excel_tests.cpp). Không phụ thuộc Qt để dùng chung cho app và kiểm thử.
#pragma once
#include <cstdio>
#include <string>

namespace dung::excel {

inline std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += (char)c;
        }
    }
    return o;
}
inline std::string strArg(const std::string& k, const std::string& v) {
    return "\"" + k + "\":{\"type\":\"string\",\"value\":\"" + jsonEscape(v) + "\"}";
}
inline std::string numArg(const std::string& k, const char* type, long long v) {
    return "\"" + k + "\":{\"type\":\"" + type + "\",\"value\":" + std::to_string(v) + "}";
}
inline std::string boolArg(const std::string& k, bool v) {
    return "\"" + k + "\":{\"type\":\"boolean\",\"value\":" + (v ? "true" : "false") + "}";
}

// UI policy: validate count and inputs before multiplication to avoid overflow.
inline bool linearSeriesEnd(long long start,long long step,int count,long long& end) {
    constexpr long long limit=1000000000LL;
    if(count<2||count>10000||start< -limit||start>limit||
       step< -1000000LL||step>1000000LL||step==0) return false;
    const auto result=start+step*(count-1);
    if(result< -limit||result>limit) return false;
    end=result;
    return true;
}

// Linear integer series, UNO arguments are strings parsed by Calc's number formatter.
// Upstream sc/sdi/scalc.sdi and cellsh1.cxx; real-engine CI must verify these.
inline std::string fillLinear(bool vertical, long long start, long long step, long long end) {
    return "{" + strArg("FillDir",vertical?"B":"R") + "," + strArg("FillCmd","L") + "," +
        strArg("FillDateCmd","D") + "," + strArg("FillStart",std::to_string(start)) + "," +
        strArg("FillStep",std::to_string(step)) + "," + strArg("FillMax",std::to_string(end)) + "}";
}

// ---- Sheet ----
// Tên sheet hợp lệ của Excel: 1–31 ký tự, không có []:*?/\\, không bắt đầu/kết thúc bằng dấu '.
inline bool validSheetName(const std::string& n) {
    if (n.empty() || n.front() == '\'' || n.back() == '\'') return false;
    for (char c : n) if (std::string("[]:*?/\\").find(c) != std::string::npos) return false;
    // đếm ký tự UTF-8 (không đếm byte tiếp nối)
    size_t chars = 0; for (unsigned char c : n) if ((c & 0xC0) != 0x80) ++chars;
    return chars <= 31;
}
inline std::string sheetInsert(const std::string& name, long index1) {
    return "{" + strArg("Name", name) + "," + numArg("Index", "long", index1) + "}";
}
inline std::string sheetRename(const std::string& name) { return "{" + strArg("Name", name) + "}"; }
inline std::string sheetRemove(long index1) { return "{" + numArg("Index", "long", index1) + "}"; }
// Chuyển/nhân đôi sheet HIỆN TẠI. docName phải là tên tài liệu thật (không để trống).
inline std::string sheetMove(const std::string& docName, long index1, bool copy) {
    return "{" + strArg("DocName", docName) + "," + numArg("Index", "long", index1) + "," + boolArg("Copy", copy) + "}";
}
// ---- Định dạng ----
inline std::string fontHeight(int pt) {
    return "{\"FontHeight.Height\":{\"type\":\"float\",\"value\":\"" + std::to_string(pt) + "\"}}";
}
inline std::string fontName(const std::string& family) {
    return "{" + strArg("CharFontName.FamilyName", family) + "," + strArg("CharFontName.StyleName", "") + "," +
           numArg("CharFontName.Family", "short", 0) + "," + numArg("CharFontName.CharSet", "short", 0) + "," +
           numArg("CharFontName.Pitch", "short", 0) + "}";
}
inline std::string textColor(unsigned rgb) { return "{" + numArg("Color", "long", rgb & 0xFFFFFF) + "}"; }
inline std::string fillColor(unsigned rgb) { return "{" + numArg("BackgroundColor", "long", rgb & 0xFFFFFF) + "}"; }
// ---- Kích thước (đơn vị 1/100 mm) ----
inline std::string columnWidth(long hundredthMm) { return "{" + numArg("ColumnWidth", "long", hundredthMm) + "}"; }
inline std::string rowHeight(long hundredthMm) { return "{" + numArg("RowHeight", "long", hundredthMm) + "}"; }
// ---- Tìm & thay thế. Mặc định CHỈ thay trong giá trị/văn bản, không động vào công thức. ----
// Explicit literal, current-sheet-only search. Do not inherit regex or all-sheet
// preferences from an earlier native search. CellType 1 = values.
inline std::string searchArgs(const std::string& find, const std::string& repl, int command, bool matchCase) {
    return "{" + strArg("SearchItem.SearchString", find) + "," + strArg("SearchItem.ReplaceString", repl) + "," +
           numArg("SearchItem.Command", "long", command) + "," + numArg("SearchItem.CellType", "long", 1) + "," +
           numArg("SearchItem.AlgorithmType", "long", 0) + "," + numArg("SearchItem.SearchFlags", "long", 0) + "," +
           numArg("SearchItem.TransliterateFlags", "long", matchCase ? 0 : 0x100) + "," +
           boolArg("SearchItem.AllTables", false) + "," + boolArg("SearchItem.Backward", false) + "}";
}
inline std::string replaceAll(const std::string& find, const std::string& repl, bool matchCase = false) {
    return searchArgs(find, repl, 3, matchCase);
}
inline std::string findNext(const std::string& find, bool matchCase = false) {
    return searchArgs(find, "", 0, matchCase);
}
// Chuyển cm -> 1/100 mm
inline long cmToHundredthMm(double cm) { return (long)(cm * 1000.0 + 0.5); }

} // namespace dung::excel
