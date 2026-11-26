// SPDX-License-Identifier: MPL-2.0
// Tiện ích dữ liệu cho "Tiện ích Excel": thống kê vùng, kiểm tra lỗi công thức, gom nhóm cho biểu đồ nhanh.
// Chỉ cần QtCore. Nhận vùng ô dạng văn bản (TSV, như engine trả về bằng selectionAs "text/plain"), KHÔNG sửa dữ liệu gốc;
// việc làm sạch/tìm trùng dùng lại tbl:: trong app/src/tableutil (bản mới, có xem trước và hoàn tác).
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace dataassist {

// Tách văn bản TSV (dòng \n hoặc \r\n, ô cách nhau tab) thành lưới. Không ép kiểu.
QVector<QStringList> parseTsv(const QString &text);

// Đọc số theo kiểu Việt hoặc Anh một cách NGHIÊM NGẶT. Không đoán khi mơ hồ ("1.234" => mơ hồ).
struct NumParse { bool ok = false; double value = 0; bool ambiguous = false; };
NumParse parseNumber(const QString &s, bool vietnamese);

struct RangeStats {
    int cells = 0, empty = 0, numeric = 0, text = 0, errors = 0, ambiguous = 0;
    double sum = 0, min = 0, max = 0, average = 0;
    bool hasNumbers = false;
    QStringList warnings;
};
// Thống kê vùng. Ô văn bản trông như số bắt đầu bằng 0 (mã, điện thoại) được tính là văn bản, không phải số.
RangeStats rangeStats(const QVector<QStringList> &grid, bool vietnamese = true);

struct FormulaErrorHit { int row = 0, col = 0; QString text; QString meaning; }; // row/col tính từ 1
// Tìm ô hiển thị lỗi (#DIV/0!, #N/A, #NAME?, #REF!, #VALUE!, #NUM!, #NULL!, Err:5xx, #DIV/0...) kèm giải thích tiếng Việt.
QVector<FormulaErrorHit> findFormulaErrors(const QVector<QStringList> &grid);
bool isErrorText(const QString &s);
QString explainError(const QString &s);

// Bảng đếm theo nhãn (cột nhãn) hoặc cộng theo nhãn (cột số) để vẽ biểu đồ nhanh.
struct ChartPoint { QString label; double value = 0; };
struct ChartData { QVector<ChartPoint> points; QStringList warnings; };
// valueCol < 0 => đếm số dòng theo nhãn. Dòng đầu là tiêu đề nếu hasHeader. Giữ thứ tự xuất hiện đầu tiên; tối đa maxPoints nhóm (phần còn lại gộp "Khác").
ChartData groupForChart(const QVector<QStringList> &grid, int labelCol, int valueCol, bool hasHeader, bool vietnamese = true, int maxPoints = 12);

// Read-only grouped summary of displayed cell values; not an Excel PivotTable object.
struct GroupSummaryRow {
    QString label;
    int rows=0, numbers=0;
    double sum=0, min=0, max=0;
};
struct GroupSummary { bool ok=false; QString error; QStringList warnings; QVector<GroupSummaryRow> groups; };
GroupSummary summarizeGroups(const QVector<QStringList>& grid, int labelCol, int valueCol, bool hasHeader, bool vietnamese=true);

}  // namespace dataassist
