// SPDX-License-Identifier: MPL-2.0
// "ƒx · Tôi muốn": danh mục hàm tiếng Việt + tìm theo ý định + dựng công thức. Chỉ cần QtCore.
// KHÔNG tự tính công thức: công thức được đưa cho engine LibreOffice (bộ tính thật) qua ô tính.
// Mọi hàm ở đây là hàm chuẩn của bảng tính; hàm nào engine chưa chạy được sẽ được CI ghi nhận
// (tools/smoketest --excel: mục "Thăm dò hàm") — danh mục không tự khẳng định.
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace dataassist {

enum class Category { Math, Condition, Lookup, Text, Date, Finance };
QString categoryName(Category c);              // "Tính toán", "Điều kiện", "Tra cứu", "Văn bản", "Ngày tháng", "Tài chính"
QList<Category> allCategories();

struct ArgInfo { QString name; QString hint; bool optional = false; };
struct FuncInfo {
    QString name;          // SUM
    Category cat;
    QString intent;        // "Tôi muốn cộng tổng một vùng số"
    QString description;   // mô tả ngắn
    QVector<ArgInfo> args;
    QString example;       // ví dụ có dấu phân cách ','
    QStringList keywords;  // từ khoá tiếng Việt (có/không dấu) để tìm
    bool newerEngineOnly = false; // hàm mảng động (FILTER/SORT/UNIQUE): cần engine mới, tệp xlsx cũ có thể không mở được trên Excel cũ
};

const QVector<FuncInfo>& catalog();
const FuncInfo* findFunction(const QString &name);          // không phân biệt hoa/thường
QVector<const FuncInfo*> byCategory(Category c);

// Bỏ dấu tiếng Việt + hạ chữ thường để so khớp ("tổng" == "tong").
QString foldVietnamese(const QString &s);
// Tìm theo ý người dùng ("cộng tổng", "tìm theo mã", "đếm ô có điều kiện"). Xếp theo điểm giảm dần.
QVector<const FuncInfo*> search(const QString &query, int maxResults = 12);

// Gợi ý hàm thay thế khi engine không chạy được hàm mới (XLOOKUP/FILTER/SORT/UNIQUE). Rỗng nếu không có.
QString replacementHint(const QString &name);

// Ba hàm nhiều điều kiện có số cặp (vùng, điều kiện) linh hoạt.
bool hasConditionPairs(const FuncInfo& f);
QVector<ArgInfo> conditionArgs(const FuncInfo& f, int pairs);

// Dựng công thức "=NAME(a,b,c)" từ giá trị nhập cho từng đối số.
// Đối số bắt buộc để trống => ok=false, error nêu tên đối số. Đối số tuỳ chọn trống ở CUỐI thì bỏ; ở GIỮA thì lỗi.
// sep: dấu ngăn đối số (',' hoặc ';'). Không đổi nội dung đối số của người dùng.
struct BuildResult { bool ok = false; QString formula; QString error; };
BuildResult buildFormula(const FuncInfo &f, const QStringList &argValues, QChar sep = QLatin1Char(','));

// Kiểm tra nhanh cú pháp công thức người dùng gõ: ngoặc cân, dấu nháy đóng, có '=' đầu. Không phải bộ phân tích đầy đủ.
struct SyntaxCheck { bool ok = true; QString message; };
SyntaxCheck quickSyntaxCheck(const QString &formula);

}  // namespace dataassist
