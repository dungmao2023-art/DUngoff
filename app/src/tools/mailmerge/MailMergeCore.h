// SPDX-License-Identifier: MPL-2.0
// Điền tên hàng loạt (trộn thư) vào tệp .docx — phần lõi, không phụ thuộc giao diện, kiểm thử được.
//
// Nguyên tắc:
//  * Mẫu gốc KHÔNG bao giờ bị sửa; mọi kết quả ghi ra tệp MỚI, không ghi đè tệp đã có.
//  * Trường dạng {{Tên}} hoặc «Tên» có thể bị Word tách qua nhiều "run" (đoạn định dạng) — lõi gộp lại để tìm,
//    rồi đặt giá trị vào đúng run chứa ký tự đầu của trường nên giữ nguyên định dạng (font, đậm, nghiêng...).
//  * Thay trong thân văn bản, bảng, hộp văn bản, đầu trang, chân trang, chú thích cuối/chân trang.
//  * Trường không có dữ liệu thì GIỮ NGUYÊN chỗ giữ chữ và có cảnh báo, không âm thầm xoá.
#pragma once
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace mailmerge {

struct FieldInfo {
    QString name;       // tên trường (đã bỏ khoảng trắng hai đầu)
    int count = 0;      // số lần xuất hiện trong cả tệp
    QStringList where;  // các phần có trường: "thân", "đầu trang 1", "chân trang 1"...
};

struct ScanResult {
    bool ok = false;
    QString error;
    QVector<FieldInfo> fields;
    QStringList warnings;
};

// Tìm các trường {{...}} và «...» trong mẫu (kể cả khi bị tách qua nhiều run).
ScanResult scanTemplate(const QString &docxPath);

struct OpResult {
    bool ok = false;
    QString error;
    QStringList warnings;
    int replaced = 0;   // số chỗ đã thay
    QString outPath;
};

using Values = QMap<QString, QString>;

// Biến mọi chỗ khớp đoạn chữ đã chọn thành {{tên trường}} -> ghi BẢN MẪU MỚI (srcDocx giữ nguyên).
OpResult makeFieldTemplate(const QString &srcDocx, const QString &dstDocx, const QString &selectedText, const QString &fieldName);

// Thay trường bằng giá trị -> một tệp .docx cho một bản ghi.
OpResult renderOne(const QString &srcDocx, const QString &dstDocx, const Values &values);

// Gộp nhiều bản ghi vào MỘT tệp .docx, mỗi bản ghi một trang mới. Trường trong đầu/chân trang KHÔNG được thay (có cảnh báo).
OpResult renderMerged(const QString &srcDocx, const QString &dstDocx, const QVector<Values> &records);

// Văn bản thuần của bản xem trước (đã thay giá trị), cắt tối đa maxChars ký tự. Không ghi tệp.
QString previewText(const QString &srcDocx, const Values &values, int maxChars, QString *error = nullptr);

// ---------------------------------------------------------------- Dữ liệu nguồn
struct Table {
    QStringList header;
    QVector<QStringList> rows;
    QStringList warnings;
    QString error;
    int firstDataRow = 1;  // số dòng (trong nguồn) của dòng dữ liệu đầu tiên, để báo "dòng N"
    bool ok() const { return error.isEmpty(); }
};

enum class Delim { Auto, Tab, Semicolon, Comma, None };

Table parsePasted(const QString &text, Delim delim, bool firstRowIsHeader);
Table readCsvFile(const QString &path, bool firstRowIsHeader);
QStringList xlsxSheetNames(const QString &path, QString *error = nullptr);
Table readXlsxFile(const QString &path, int sheetIndex, bool firstRowIsHeader);

// Ánh xạ: tên trường -> chỉ số cột (-1 = chưa chọn).
using Mapping = QMap<QString, int>;
Mapping autoMap(const QVector<FieldInfo> &fields, const Table &t);

struct Check {
    QStringList errors;    // chặn xuất
    QStringList warnings;  // cho phép xuất nhưng phải xem
    int usableRecords = 0;
};
Check validate(const QVector<FieldInfo> &fields, const Table &t, const Mapping &m, int maxLen = 40);

// Tạo danh sách bản ghi (bỏ dòng mà mọi giá trị đã ánh xạ đều trống).
QVector<Values> buildRecords(const QVector<FieldInfo> &fields, const Table &t, const Mapping &m, QVector<int> *sourceRows = nullptr);

// ---------------------------------------------------------------- Tên tệp xuất
QString sanitizeFileName(const QString &name);
// Mẫu tên: {Trường} và {#} (số thứ tự có đệm 0). Kết quả: đường dẫn đầy đủ, không trùng nhau, không trùng tệp đã có trong outDir.
QStringList planFileNames(const QVector<Values> &records, const QString &pattern, const QString &ext, const QString &outDir, QStringList *warnings);

}  // namespace mailmerge
