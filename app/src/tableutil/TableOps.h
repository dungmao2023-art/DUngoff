// Tiện ích bảng tính (chỉ cần QtCore). Mọi ô là CHUỖI: không tự ép kiểu, số 0 đầu và số dài giữ nguyên.
// Mọi thao tác trả về BẢN MỚI; bảng gốc không bị sửa, tệp gốc không bị ghi đè.
#pragma once
#include <QString>
#include <QByteArray>
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
// Limits apply to the entire input, not a truncated prefix.
inline constexpr qint64 kMaxCsvBytes=100LL*1024*1024;
inline constexpr qsizetype kMaxCsvRecords=250001; // includes header
inline constexpr qsizetype kMaxCsvColumns=1024;
inline constexpr qsizetype kMaxCsvCells=5000000;
inline constexpr qsizetype kMaxCsvCellChars=1048576;
bool readCsvFileBytes(const QString& path,QByteArray& out,QString& error);
bool readCsv(const QString &path, Table &out, QString &error, QChar sep = QLatin1Char(','));
bool writeCsvCopy(const QString &path, const Table &t, QString &error, QChar sep = QLatin1Char(','));

// ---- Nhận diện bảng mã CSV (KHÔNG đoán âm thầm) ----
enum class Encoding { Unknown, Utf8, Utf8Bom, Utf16LE, Utf16BE, Windows1258 };
QString encodingName(Encoding e);
struct EncodingGuess {
    Encoding enc = Encoding::Unknown; // bảng mã đề xuất (Unknown nếu không có ứng viên giải mã được)
    double confidence = 0;            // 0..1
    bool needsUserChoice = true;      // true => giao diện phải bắt người dùng xem trước và xác nhận
    QString reason;                   // giải thích bằng tiếng Việt
    QList<Encoding> alternatives;     // các bảng mã khác người dùng có thể chọn
};
// Chỉ tin chắc (>= 0.95, không cần chọn) khi: có BOM hợp lệ, hoặc UTF-8 hợp lệ. Còn lại luôn cần người dùng chọn.
EncodingGuess detectCsvEncoding(const QByteArray &raw);
// Giải mã NGHIÊM NGẶT theo bảng mã đã chọn (bỏ BOM nếu có). Byte không hợp lệ => false, không thay ký tự.
bool decodeText(const QByteArray &raw, Encoding enc, QString &text, QString &error);
// Tối đa maxLines dòng đầu (đã giải mã) để xem trước.
QStringList previewLines(const QString &decodedText, int maxLines = 20);
QChar guessSeparator(const QString &decodedText); // ',' ';' hoặc tab; mặc định ','
// Đọc CSV theo bảng mã người dùng đã chọn. Không sửa tệp gốc.
bool readCsvWith(const QString &path, Encoding enc, Table &out, QString &error, QChar sep = QLatin1Char(','));
// Parse the exact bytes already previewed; leave out unchanged on any error.
bool readCsvBytes(const QByteArray& raw, Encoding enc, Table& out, QString& error, QChar sep = QLatin1Char(','));

// Group exact text keys, in first-seen order (blank is its own group).
// valueCol=-1 counts rows only; otherwise sum canonical decimals, exactly to 4 places.
struct CsvGroupResult { bool ok=false; Table table; QString error; };
CsvGroupResult groupCsv(const Table& source, int keyCol, int valueCol=-1);

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

// Read-only filter then stable text sort. -1 disables a step; no implicit numeric conversion.
enum class FilterMode { Equal, Contains, Blank, NotBlank };
struct FilterSortSpec {
    int filterCol = -1, sortCol = -1;
    FilterMode mode = FilterMode::Equal;
    QString value;
    bool ignoreCase = false, descending = false;
    // Empty lists preserve the original schema. Otherwise project in the supplied order.
    QList<int> outputColumns;
    QStringList outputNames;
    // Optional steps: clean one source column, filter, deduplicate by one key, sort, project.
    int cleanCol=-1, dedupCol=-1;
    ColumnKind cleanKind=ColumnKind::Keep;
    MoneyLocale moneyLocale=MoneyLocale::VN;
    bool dedupFold=false;
    // Group after projection. These are source indices and must survive projection.
    int groupKey=-1, groupValue=-1;
};
struct FilterSortResult {
    bool ok = false; Table table; QString error;
    qsizetype cleanedCells=0, cleanWarnings=0, removedDuplicates=0, filteredRows=0;
    qsizetype groupedInputRows=0;
    bool grouped=false;
};
FilterSortResult filterSort(const Table& source, const FilterSortSpec& spec);

// JSON recipe stores only schema and options, never the source path or rows.
QByteArray encodeFilterSortRecipe(const QStringList& header, const FilterSortSpec& spec);
bool decodeFilterSortRecipe(const QByteArray& json, const QStringList& header, FilterSortSpec& spec, QString& error);

enum class JoinKind { Left, Inner };
struct JoinResult {
    bool ok = false;
    Table table;
    QString error;
    qsizetype matched = 0, unmatched = 0, blankLeft = 0, blankRight = 0;
};
// Exact text-key join. Reject duplicate nonempty lookup keys, preserve left order.
JoinResult joinLookup(const Table& left, const Table& right, int leftKey, int rightKey, JoinKind kind);

struct QueryJoinSpec {
    bool enabled=false;
    int leftKey=-1,rightKey=-1;
    JoinKind kind=JoinKind::Left;
    QStringList rightHeader;
};
JoinResult queryJoinInput(const Table& left,const Table& right,const QueryJoinSpec& join);
QByteArray encodeQueryRecipe(const QStringList& leftHeader,const QueryJoinSpec& join,const FilterSortSpec& steps);
bool decodeQueryRecipe(const QByteArray& json,const QStringList& leftHeader,const QStringList& rightHeader,
                       QueryJoinSpec& join,FilterSortSpec& steps,QString& error);
} // namespace tbl
