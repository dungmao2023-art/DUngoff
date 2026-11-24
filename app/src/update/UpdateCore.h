// Dũng Office — lõi kiểm tra cập nhật (chỉ cần QtCore, kiểm thử được không cần mạng).
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace update {

struct Version {
    bool valid = false;
    QList<int> nums;     // 1.2.3 -> {1,2,3}
    QStringList pre;     // "beta.2" -> {"beta","2"}; rỗng = bản chính thức
};

// Chấp nhận "v1.2.3", "1.2", "1.2.3-beta.2", "1.2.3+build5" (bỏ phần build). Sai định dạng -> valid=false.
Version parseVersion(const QString& s);
// -1: a<b, 0: bằng, 1: a>b. Bản có hậu tố thử nghiệm (-beta) nhỏ hơn bản chính thức cùng số.
int compare(const Version& a, const Version& b);
// true khi candidate mới hơn current. Không phân tích được phiên bản nào -> false và err có lý do (KHÔNG coi là "mới hơn").
bool isNewer(const QString& current, const QString& candidate, QString* err = nullptr);

struct Asset {
    QString name;
    QString url;
    qint64 size = 0;
};

struct Release {
    bool ok = false;
    QString error;
    QString tag, version, notes, pageUrl;
    bool prerelease = false, draft = false;
    QList<Asset> assets;     // chỉ giữ tệp có URL https thuộc github.com / githubusercontent.com
    QStringList rejected;    // tên tệp bị loại vì URL không tin cậy
};

// Phân tích JSON của GET /repos/{owner}/{repo}/releases/latest.
Release parseRelease(const QByteArray& json);

// Chỉ https và máy chủ chính xác github.com hoặc *.github.com / *.githubusercontent.com (chống lừa kiểu github.com.evil.com, user@host).
bool isTrustedUrl(const QUrl& u);

// Tên tệp bộ cài: DungOffice-Setup-<...>x64.exe
Asset findInstaller(const Release& r);
// Tệp checksum: "<tên bộ cài>.sha256" hoặc SHA256SUMS(.txt)
Asset findChecksumAsset(const Release& r, const QString& installerName);

// Đọc nội dung dạng sha256sum ("<hex>  <tên>" hoặc "<hex> *<tên>"), tên chữ thường -> hex chữ thường.
// Dòng sai định dạng bị bỏ qua. Một tên có hai mã khác nhau -> loại bỏ tên đó (xung đột = không tin).
QMap<QString, QString> parseChecksums(const QString& text);
// Mã SHA-256 mong đợi cho bộ cài; rỗng + err nếu không có/xung đột. Tệp checksum chỉ có 1 dòng không tên được chấp nhận
// khi là tệp ".sha256" riêng của bộ cài.
QString expectedHash(const QString& checksumText, const QString& installerName, QString* err);

// Tính SHA-256 của tệp và so với expectedHex (64 ký tự hex). Thiếu tệp/mã sai định dạng/không khớp -> false và err.
bool verifyFile(const QString& path, const QString& expectedHex, QString* err);

}  // namespace update
