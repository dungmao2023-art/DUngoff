// SPDX-License-Identifier: MPL-2.0
// Lệnh UNO cho Word (Writer), không phụ thuộc Qt: dùng chung cho ribbon và kiểm thử engine.
// Tên lệnh/đối số được kiểm chứng bằng `dung-smoketest --word` (xem tools/smoketest/word_tests.cpp),
// không suy đoán.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace dung::word {

// ---- dựng chuỗi đối số JSON cho postUnoCommand ----
std::string argString(const std::string& name, const std::string& value);
std::string argLong(const std::string& name, long long value);
std::string argFloat(const std::string& name, double value);
std::string argBool(const std::string& name, bool value);
// Gộp nhiều đối số thành một đối tượng JSON: join({argString(...), argLong(...)}).
std::string join(const std::vector<std::string>& args);

// ---- lệnh có đối số: trả về {tên lệnh, JSON đối số} ----
using Call = std::pair<std::string, std::string>;
Call fontName(const std::string& family);
Call fontSize(double points);
Call fontColor(unsigned rgb);       // màu chữ
Call backColor(unsigned rgb);       // tô nền ký tự
Call paragraphStyle(const std::string& styleName);
Call insertTable(int columns, int rows);
Call insertImage(const std::string& fileUrl);          // file:///...
Call hyperlink(const std::string& text, const std::string& url);
Call findNext(const std::string& text, bool backward);
Call replaceAll(const std::string& find, const std::string& replacement);
Call replaceOne(const std::string& find, const std::string& replacement);

// Khối hành chính chèn vào nội dung tài liệu (không phải header/footer lặp).
struct AdminBlock {
    std::string agency, parentAgency, number, placeDate, title, subject, recipient, signerRole, signerName;
};
std::string adminBlockHtml(const AdminBlock& b, bool head, bool tail);

// ---- bảng tra cho giao diện ----
struct StyleChoice { const char* label; const char* styleName; };
const std::vector<StyleChoice>& paragraphStyles();   // Thường, Tiêu đề 1-3
const std::vector<double>& commonFontSizes();

// Danh sách phông từ kết quả getCommandValues(".uno:CharFontName") (JSON của engine).
std::vector<std::string> parseFontList(const std::string& json);

// Đếm từ trong văn bản thường (tách theo khoảng trắng Unicode cơ bản).
int countWords(const std::string& utf8);

}  // namespace dung::word
