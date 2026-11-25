// SPDX-License-Identifier: MPL-2.0
// Đặt lề trang của tệp DOCX bằng cách vá XML (w:pgMar) — KHÔNG phụ thuộc lệnh UNO hay runMacro.
// Dùng khi engine không nhận giá trị lề qua lệnh (engine Windows). Chỉ cần QtCore (+ ZipLite của công cụ tệp).
//
// Nguyên tắc:
//  * Không bao giờ sửa tệp nguồn: luôn ghi ra tệp đích mới.
//  * Mọi mục khác trong gói ZIP được sao chép NGUYÊN BYTE (không giải nén/nén lại); chỉ word/document.xml được ghi lại,
//    và trong đó chỉ các thuộc tính lề bị đổi (phần còn lại của XML giữ nguyên từng byte).
//  * Vá TẤT CẢ các phần (w:sectPr) của tài liệu; không động vào w:sectPrChange (bản ghi thay đổi có theo dõi).
//  * Không đoán: gặp ZIP64, không nhận ra không gian tên Word, lề vô lý... thì báo lỗi và không ghi gì.
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace dung::word {

// Đổi đơn vị sang twip (1/1440 inch), làm tròn gần nhất.
qint32 mmToTwips(double mm);
qint32 cmToTwips(double cm);
qint32 inchToTwips(double inch);
double twipsToMm(qint32 twips);

// Các cạnh cần đặt (đơn vị twip). Cạnh không đặt thì giữ nguyên giá trị hiện có.
struct MarginSpec {
    std::optional<qint32> left, right, top, bottom;
    bool any() const { return left || right || top || bottom; }
};

// Lề của một phần (section). Giá trị -1 = không có hoặc không đọc được.
struct SectionMargins {
    int index = 0;            // thứ tự xuất hiện trong tài liệu (0, 1, ...)
    bool hasPgMar = false;
    qint32 left = -1, right = -1, top = -1, bottom = -1, header = -1, footer = -1, gutter = -1;
    qint32 pageW = -1, pageH = -1;   // khổ giấy (w:pgSz), twip
    bool inChange = false;    // nằm trong w:sectPrChange (chỉ để thông tin, không bao giờ bị vá)
};

struct ReadResult {
    bool ok = false;
    QString error;
    QVector<SectionMargins> sections;   // không gồm phần nằm trong sectPrChange
    bool hasBodySectPr = false;         // có w:sectPr cuối thân tài liệu
};

struct PatchResult {
    bool ok = false;
    QString error;
    int sectionsPatched = 0;    // số phần có sẵn w:pgMar đã đổi
    int sectionsCreated = 0;    // số phần phải tạo mới w:pgMar (hoặc w:sectPr)
    QStringList warnings;
};

// Đọc lề của mọi phần trong word/document.xml của tệp DOCX.
ReadResult readDocxMargins(const QString& docxPath);
// Cùng việc trên nhưng trên nội dung XML đã có (dùng cho kiểm thử).
ReadResult readDocumentXmlMargins(const QByteArray& xml);

// Vá XML trong bộ nhớ. `out` là XML mới (nguyên byte ngoài các thuộc tính lề). Không ghi tệp.
PatchResult patchDocumentXml(const QByteArray& xml, const MarginSpec& spec, QByteArray* out);

// Tạo bản sao của `src` tại `dst` với lề mới. Sau khi ghi, mở lại bản ghi để kiểm: mọi mục đọc được, CRC đúng,
// các mục khác word/document.xml trùng khít bản gốc, đọc lại lề thấy đúng giá trị. Lỗi → không để lại tệp đích.
PatchResult patchDocxMargins(const QString& src, const QString& dst, const MarginSpec& spec);

// Thay một mục trong gói ZIP, sao chép nguyên byte các mục còn lại. Không hỗ trợ ZIP64/mã hoá.
bool replaceZipEntry(const QString& src, const QString& dst, const QString& entryName, const QByteArray& newData,
                     QString* error);

} // namespace dung::word
