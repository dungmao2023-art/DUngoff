// SPDX-License-Identifier: MPL-2.0
// Quét liên kết: tệp .url / .lnk và liên kết ngoài nằm trong docx/xlsx/pptx (tệp quan hệ *.rels).
// Chỉ ĐỌC. Không mở mạng ở lõi: liên kết web được liệt kê "chưa kiểm tra"; việc gửi yêu cầu HEAD do giao diện làm sau khi hỏi người dùng.
// Giới hạn: không đọc trường HYPERLINK() trong công thức Excel hay mã trường trường Word; không đọc tệp .doc/.xls/.ppt cũ.
#pragma once
#include "FileOps.h"

namespace ft {

enum class TargetType { Web, Mail, File, Other };
enum class LinkState { Ok, Broken, Unchecked };

struct LinkFinding {
    QString source;      // tệp chứa liên kết
    QString where;       // phần bên trong (vd word/document.xml) hoặc rỗng
    QString kind;        // "Siêu liên kết", "Đích lối tắt", "Liên kết sổ tính ngoài", ...
    QString target;      // đích như ghi trong tệp
    QString resolved;    // đường dẫn cục bộ đã quy đổi (nếu là tệp)
    QString note;
    TargetType type = TargetType::Other;
    LinkState state = LinkState::Unchecked;
};

struct ScanLinksOptions {
    bool recursive = true;
    bool checkNetworkPaths = false;   // true: kiểm tra cả \\máy\thư-mục (có thể treo vài giây nếu máy tắt)
};
struct ScanLinksResult {
    QVector<LinkFinding> findings;
    QStringList skipped;              // "đường dẫn: lý do"
    int filesScanned = 0;
    bool cancelled = false;
    int brokenCount() const;
};

TargetType classifyTarget(const QString& target);
// Quy đổi đích dạng tệp (file:///, ổ đĩa, UNC, tương đối) sang đường dẫn cục bộ. checkable=false nếu không thể kiểm tra trên hệ này.
QString resolveLocalTarget(const QString& target, const QString& baseDir, bool* checkable);
QString linkStateText(LinkState s);
QString targetTypeText(TargetType t);

QVector<LinkFinding> readUrlFile(const QString& path, QString* err);
QVector<LinkFinding> readLnkFile(const QString& path, QString* err, bool checkNetworkPaths);
QVector<LinkFinding> readOfficeLinks(const QString& path, QString* err, bool checkNetworkPaths);

// Quét danh sách tệp/thư mục. Phần mở rộng hỗ trợ: url, lnk, docx, docm, dotx, xlsx, xlsm, xltx, pptx, pptm, potx.
ScanLinksResult scanLinks(const QStringList& paths, const ScanLinksOptions& o, const Progress& p = {});
OpResult exportFindingsCsv(const QString& path, const QVector<LinkFinding>& f);

} // namespace ft
