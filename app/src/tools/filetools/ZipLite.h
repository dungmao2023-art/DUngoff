// SPDX-License-Identifier: MPL-2.0
// Đọc tệp ZIP (docx/xlsx/pptx) chỉ bằng QtCore: không giải nén ra đĩa, có giới hạn kích thước chống "bom nén",
// kiểm tra CRC32. Không hỗ trợ ZIP64 và mã hoá — gặp thì báo lỗi rõ ràng, không đoán.
#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>

namespace ft {

quint32 zipCrc32(const QByteArray& data);
// Giải nén luồng DEFLATE thô. expectedSize = kích thước khai báo; ra khỏi giới hạn là lỗi.
bool inflateRaw(const QByteArray& in, qint64 expectedSize, qint64 maxSize, QByteArray* out, QString* err);

struct ZipEntryInfo {
    QString name;
    quint16 method = 0;          // 0 = lưu nguyên, 8 = deflate
    quint32 crc = 0;
    quint64 csize = 0, usize = 0, localOffset = 0;
    bool encrypted = false, zip64 = false;
};

class ZipReader {
public:
    bool open(const QString& path, QString* err);
    const QVector<ZipEntryInfo>& entries() const { return m_entries; }
    const ZipEntryInfo* find(const QString& name) const;
    bool read(const ZipEntryInfo& e, QByteArray* out, QString* err, qint64 maxSize = 64LL * 1024 * 1024) const;
private:
    QString m_path;
    QVector<ZipEntryInfo> m_entries;
};

} // namespace ft
