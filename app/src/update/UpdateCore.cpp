// SPDX-License-Identifier: MPL-2.0
#include "UpdateCore.h"

#include <QCryptographicHash>
#include <QObject>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace update {

Version parseVersion(const QString& in) {
    Version v;
    QString s = in.trimmed();
    if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V'))) s = s.mid(1);
    const int plus = s.indexOf(QLatin1Char('+'));
    if (plus >= 0) s = s.left(plus);
    QString pre;
    const int dash = s.indexOf(QLatin1Char('-'));
    if (dash >= 0) { pre = s.mid(dash + 1); s = s.left(dash); if (pre.isEmpty()) return v; }
    if (s.isEmpty()) return v;
    static const QRegularExpression numRe(QStringLiteral("^\\d{1,9}$"));
    for (const QString& part : s.split(QLatin1Char('.'))) {
        if (!numRe.match(part).hasMatch()) return v;
        v.nums.append(part.toInt());
    }
    if (v.nums.size() > 4) return v;
    if (!pre.isEmpty()) {
        static const QRegularExpression idRe(QStringLiteral("^[0-9A-Za-z]+$"));
        for (const QString& id : pre.split(QLatin1Char('.'))) {
            if (!idRe.match(id).hasMatch()) return v;
            v.pre.append(id);
        }
    }
    v.valid = true;
    return v;
}

static bool isNum(const QString& s) {
    for (QChar c : s) if (!c.isDigit()) return false;
    return !s.isEmpty();
}

int compare(const Version& a, const Version& b) {
    const int n = qMax(a.nums.size(), b.nums.size());
    for (int i = 0; i < n; ++i) {
        const int x = i < a.nums.size() ? a.nums[i] : 0;
        const int y = i < b.nums.size() ? b.nums[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    if (a.pre.isEmpty() && b.pre.isEmpty()) return 0;
    if (a.pre.isEmpty()) return 1;    // bản chính thức > bản thử nghiệm
    if (b.pre.isEmpty()) return -1;
    const int m = qMax(a.pre.size(), b.pre.size());
    for (int i = 0; i < m; ++i) {
        if (i >= a.pre.size()) return -1;  // ít trường hơn nhỏ hơn
        if (i >= b.pre.size()) return 1;
        const QString &x = a.pre[i], &y = b.pre[i];
        const bool xn = isNum(x), yn = isNum(y);
        if (xn && yn) {
            const qlonglong xi = x.toLongLong(), yi = y.toLongLong();
            if (xi != yi) return xi < yi ? -1 : 1;
        } else if (xn != yn) {
            return xn ? -1 : 1;  // số < chữ
        } else if (x != y) {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}

bool isNewer(const QString& current, const QString& candidate, QString* err) {
    const Version c = parseVersion(current), d = parseVersion(candidate);
    if (!c.valid || !d.valid) {
        if (err) *err = QObject::tr("Không đọc được số phiên bản (\"%1\" / \"%2\"), không kết luận có bản mới.").arg(current, candidate);
        return false;
    }
    return compare(d, c) > 0;
}

bool isTrustedUrl(const QUrl& u) {
    if (!u.isValid() || u.scheme() != QLatin1String("https")) return false;
    if (!u.userInfo().isEmpty()) return false;
    const QString h = u.host().toLower();
    auto ends = [&](const QString& dom) { return h == dom || h.endsWith(QLatin1Char('.') + dom); };
    return ends(QStringLiteral("github.com")) || ends(QStringLiteral("githubusercontent.com"));
}

Release parseRelease(const QByteArray& json) {
    Release r;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QObject::tr("Phản hồi từ máy chủ không phải JSON hợp lệ.");
        return r;
    }
    const QJsonObject o = doc.object();
    r.tag = o.value(QStringLiteral("tag_name")).toString();
    if (r.tag.isEmpty()) { r.error = QObject::tr("Phản hồi thiếu số phiên bản (tag_name)."); return r; }
    const Version v = parseVersion(r.tag);
    if (!v.valid) { r.error = QObject::tr("Số phiên bản \"%1\" không đúng định dạng.").arg(r.tag); return r; }
    r.version = r.tag.startsWith(QLatin1Char('v')) || r.tag.startsWith(QLatin1Char('V')) ? r.tag.mid(1) : r.tag;
    r.notes = o.value(QStringLiteral("body")).toString();
    r.pageUrl = o.value(QStringLiteral("html_url")).toString();
    r.prerelease = o.value(QStringLiteral("prerelease")).toBool();
    r.draft = o.value(QStringLiteral("draft")).toBool();
    for (const QJsonValue& av : o.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject a = av.toObject();
        Asset as;
        as.name = a.value(QStringLiteral("name")).toString();
        as.url = a.value(QStringLiteral("browser_download_url")).toString();
        as.size = a.value(QStringLiteral("size")).toVariant().toLongLong();
        if (as.name.isEmpty() || as.name.contains(QLatin1Char('/')) || as.name.contains(QLatin1Char('\\')) || as.name.contains(QStringLiteral(".."))) {
            r.rejected << as.name;
            continue;
        }
        if (!isTrustedUrl(QUrl(as.url))) { r.rejected << as.name; continue; }
        r.assets.append(as);
    }
    r.ok = true;
    return r;
}

Asset findInstaller(const Release& r) {
    static const QRegularExpression re(QStringLiteral("^DungOffice-Setup-.*x64\\.exe$"), QRegularExpression::CaseInsensitiveOption);
    for (const Asset& a : r.assets) if (re.match(a.name).hasMatch()) return a;
    return {};
}

Asset findChecksumAsset(const Release& r, const QString& installerName) {
    for (const Asset& a : r.assets)
        if (a.name.compare(installerName + QStringLiteral(".sha256"), Qt::CaseInsensitive) == 0) return a;
    for (const Asset& a : r.assets) {
        const QString n = a.name.toLower();
        if (n == QLatin1String("sha256sums") || n == QLatin1String("sha256sums.txt")) return a;
    }
    return {};
}

QMap<QString, QString> parseChecksums(const QString& text) {
    QMap<QString, QString> m;
    QStringList conflict;
    static const QRegularExpression re(QStringLiteral("^([0-9A-Fa-f]{64})\\s+\\*?(.+?)\\s*$"));
    for (const QString& raw : text.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const auto mm = re.match(line);
        if (!mm.hasMatch()) continue;
        const QString name = mm.captured(2).toLower();
        const QString hex = mm.captured(1).toLower();
        if (m.contains(name) && m.value(name) != hex) conflict << name;
        m.insert(name, hex);
    }
    for (const QString& c : conflict) m.remove(c);
    return m;
}

QString expectedHash(const QString& checksumText, const QString& installerName, QString* err) {
    const auto m = parseChecksums(checksumText);
    const QString key = installerName.toLower();
    if (m.contains(key)) return m.value(key);
    // Tệp .sha256 riêng của bộ cài có thể chỉ chứa đúng một mã không kèm tên.
    static const QRegularExpression bare(QStringLiteral("^\\s*([0-9A-Fa-f]{64})\\s*$"));
    const auto mm = bare.match(checksumText);
    if (mm.hasMatch()) return mm.captured(1).toLower();
    if (err) *err = QObject::tr("Không có mã SHA-256 rõ ràng cho \"%1\" trong tệp checksum (thiếu hoặc mâu thuẫn).").arg(installerName);
    return {};
}

bool verifyFile(const QString& path, const QString& expectedHex, QString* err) {
    static const QRegularExpression hexRe(QStringLiteral("^[0-9a-f]{64}$"));
    const QString exp = expectedHex.trimmed().toLower();
    if (!hexRe.match(exp).hasMatch()) {
        if (err) *err = QObject::tr("Mã SHA-256 mong đợi không hợp lệ.");
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QObject::tr("Không mở được tệp đã tải để kiểm tra.");
        return false;
    }
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) {
        if (err) *err = QObject::tr("Không đọc hết tệp để tính SHA-256.");
        return false;
    }
    const QString got = QString::fromLatin1(h.result().toHex());
    if (got != exp) {
        if (err) *err = QObject::tr("SHA-256 KHÔNG khớp (nhận %1…, mong đợi %2…). Tệp bị loại bỏ.").arg(got.left(12), exp.left(12));
        return false;
    }
    return true;
}

}  // namespace update
