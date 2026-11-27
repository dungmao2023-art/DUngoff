#pragma once
// Logic in thuần (chỉ QtCore): khoảng trang, số bản, cảnh báo lệch số trang xem trước / hiển thị.
#include <QString>
#include <QVector>

namespace printlogic {

struct RangeResult {
    bool ok = false;
    QString error;           // tiếng Việt, rõ ràng
    QVector<int> pages;      // số trang 1-based, tăng dần, không trùng
};

// "" hoặc "tất cả" → mọi trang. Cú pháp: "1-3, 5, 8-" ("8-" = đến hết, "-3" = từ đầu đến 3).
// Dấu phân cách: , hoặc ; — Từ chối: trang < 1, vượt tổng số trang, khoảng ngược (5-2), ký tự lạ.
RangeResult parseRanges(const QString& text, int total);

// Danh sách trang theo thứ tự gửi máy in. collate=true: 1,2,3,1,2,3; false: 1,1,2,2,3,3.
QVector<int> expandCopies(const QVector<int>& pages, int copies, bool collate);

// Cảnh báo khi số trang của bản in (PDF xem trước) khác số trang tài liệu đang hiển thị.
// displayPages <= 0: không so sánh được (vd Excel: phần = sheet) → chuỗi rỗng.
QString mismatchWarning(int previewPages, int displayPages);

// Persist the print PDF at a user-selected new path. Never overwrite existing data.
bool copyPrintPdf(const QString& source,const QString& destination,QString* error=nullptr);

} // namespace printlogic
