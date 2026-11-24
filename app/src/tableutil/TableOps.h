// Tiện ích bảng tính (chỉ cần QtCore). Mọi ô là CHUỖI: không tự ép kiểu, số 0 đầu và số dài giữ nguyên.
// Mọi thao tác trả về BẢN MỚI; bảng gốc không bị sửa, tệp gốc không bị ghi đè.
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>

namespace tbl {

struct Table {
    QStringList header;
    QVector<QStringList> rows;
    bool operator==(const Table &o) const { return header == o.header && rows == o.rows; }
};

// ---- CSV (UTF-8, có/không BOM). Ghi: KHÔNG ghi đè tệp đã có. ----
bool readCsv(const QString &path, Table &out, QString &error, QChar sep = QLatin1Char(','));
bool writeCsvCopy(const QString &path, const Table &t, QString &error, QChar sep = QLatin1Char(','));

// ---- 1. Dòng trùng ----
struct RemovedRow { int originalIndex; QStringList cells; int keptIndex; };
struct DedupResult {
    Table cleaned;                // bảng sau khi bỏ dòng trùng (giữ dòng đầu tiên)
    QVector<RemovedRow> removed;  // diff: dòng bị bỏ, theo thứ tự gốc
    QStringList warnings;
    bool ok = true;
};
// cols: chỉ số cột làm khoá. trimAndFold: so sánh không phân biệt hoa/thường và khoảng trắng 2 đầu
// (không đổi dữ liệu, chỉ đổi cách so sánh). Khoá rỗng hoàn toàn KHÔNG bị coi là trùng nhau.
DedupResult removeDuplicates(const Table &t, const QList<int> &cols, bool trimAndFold);
// Hoàn tác: ghép lại dòng đã bỏ vào đúng vị trí; kết quả phải == bảng gốc.
Table undoDedup(const DedupResult &r);

// ---- 2. So sánh hai bảng theo cột mã ----
struct CellDiff { QString key; QString column; QString valueA; QString valueB; };
struct CompareResult {
    QVector<QStringList> onlyInA, onlyInB;
    QVector<CellDiff> changed;
    int same = 0;
    QStringList warnings;
    bool ok = true;
};
CompareResult compareTables(const Table &a, const Table &b, const QString &keyColumn);
// Xuất khác biệt thành bảng mới: Loại | Mã | Cột | Giá trị A | Giá trị B
Table diffAsTable(const CompareResult &r);

// ---- 3. Làm sạch ----
enum class MoneyLocale { VN, EN }; // VN: '.' nghìn, ',' thập phân. EN: ngược lại.
struct CleanCell { QString value; bool converted = false; QString warning; };
// Mã/số điện thoại/số dài: chuỗi chỉ gồm số mà bắt đầu bằng 0 (dài >1) hoặc dài >15 chữ số => BẢO VỆ.
bool isProtectedText(const QString &s);
CleanCell cleanDate(const QString &s);                              // -> dd/mm/yyyy nếu chắc chắn
CleanCell cleanMoney(const QString &s, MoneyLocale loc = MoneyLocale::VN); // -> số thuần "1500000" / "1500000.5"

enum class ColumnKind { Keep, Date, Money };
struct CleanChange { int row; int col; QString before; QString after; };
struct CleanResult {
    Table cleaned;
    QVector<CleanChange> changes;
    QStringList warnings; // ô không chuyển được: giữ nguyên + ghi lý do
};
// kinds: theo tên cột; cột không có trong map = Keep.
CleanResult cleanTable(const Table &t, const QList<QPair<QString, ColumnKind>> &kinds, MoneyLocale loc = MoneyLocale::VN);

} // namespace tbl
