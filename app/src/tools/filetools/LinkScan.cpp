// SPDX-License-Identifier: MPL-2.0
#include "LinkScan.h"
#include "ZipLite.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>

namespace ft {

int ScanLinksResult::brokenCount() const
{
    int n = 0;
    for (const auto& f : findings) if (f.state == LinkState::Broken) ++n;
    return n;
}

static bool isUnc(const QString& s) { return s.startsWith(QLatin1String("\\\\")) || s.startsWith(QLatin1String("//")); }
static bool hasDrive(const QString& s) { return s.size() >= 2 && s[0].isLetter() && s[1] == QLatin1Char(':') && (s.size() == 2 || s[2] == QLatin1Char('\\') || s[2] == QLatin1Char('/')); }

TargetType classifyTarget(const QString& target)
{
    const QString t = target.trimmed().toLower();
    if (t.startsWith(QLatin1String("http://")) || t.startsWith(QLatin1String("https://")) || t.startsWith(QLatin1String("ftp://"))) return TargetType::Web;
    if (t.startsWith(QLatin1String("mailto:"))) return TargetType::Mail;
    if (t.startsWith(QLatin1String("file:")) || isUnc(t) || hasDrive(t)) return TargetType::File;
    if (t.contains(QLatin1String("://")) || t.startsWith(QLatin1String("tel:")) || t.startsWith(QLatin1String("javascript:"))) return TargetType::Other;
    if (t.isEmpty() || t.startsWith(QLatin1Char('#'))) return TargetType::Other;
    return TargetType::File;   // đường dẫn tương đối
}

QString linkStateText(LinkState s)
{
    switch (s) {
    case LinkState::Ok: return QObject::tr("Còn");
    case LinkState::Broken: return QObject::tr("HỎNG");
    case LinkState::Unchecked: return QObject::tr("Chưa kiểm tra");
    }
    return {};
}
QString targetTypeText(TargetType t)
{
    switch (t) {
    case TargetType::Web: return QObject::tr("Web");
    case TargetType::Mail: return QObject::tr("Thư điện tử");
    case TargetType::File: return QObject::tr("Tệp/thư mục");
    case TargetType::Other: return QObject::tr("Khác");
    }
    return {};
}

QString resolveLocalTarget(const QString& target, const QString& baseDir, bool* checkable)
{
    if (checkable) *checkable = true;
    QString t = target.trimmed();
    if (t.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
        t = t.mid(5);
        int slashes = 0;
        while (slashes < t.size() && t[slashes] == QLatin1Char('/')) ++slashes;
        QString rest = t.mid(slashes);
        rest = QUrl::fromPercentEncoding(rest.toUtf8());
        if (hasDrive(rest)) t = rest;
        else if (slashes >= 3) t = QLatin1Char('/') + rest;             // file:///home/x
        else if (slashes == 2) t = QStringLiteral("//") + rest;         // file://máy/chia-sẻ
        else t = rest;
    } else {
        t = QUrl::fromPercentEncoding(t.toUtf8());
    }
#ifdef Q_OS_WIN
    if (isUnc(t) || hasDrive(t)) return QDir::toNativeSeparators(t);
    if (QDir::isAbsolutePath(t)) return QDir::toNativeSeparators(t);
    return QDir::toNativeSeparators(QDir(baseDir).filePath(QDir::fromNativeSeparators(t)));
#else
    if (isUnc(t) || hasDrive(t)) { if (checkable) *checkable = false; return t; }   // đường dẫn kiểu Windows trên hệ khác
    t.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (QDir::isAbsolutePath(t)) return t;
    return QDir(baseDir).filePath(t);
#endif
}

static void judgeFile(LinkFinding& f, const QString& baseDir, bool checkNet)
{
    f.type = TargetType::File;
    bool checkable = true;
    f.resolved = resolveLocalTarget(f.target, baseDir, &checkable);
    if (!checkable) { f.state = LinkState::Unchecked; f.note = QObject::tr("Đường dẫn kiểu Windows — không kiểm tra được trên hệ điều hành này"); return; }
    if (isUnc(f.resolved) && !checkNet) { f.state = LinkState::Unchecked; f.note = QObject::tr("Đường dẫn mạng — bỏ qua để tránh treo (bật 'kiểm tra cả đường dẫn mạng')"); return; }
    if (QFileInfo::exists(f.resolved)) f.state = LinkState::Ok;
    else { f.state = LinkState::Broken; f.note = QObject::tr("Không thấy tệp/thư mục đích"); }
}

static void judge(LinkFinding& f, const QString& baseDir, bool checkNet)
{
    f.type = classifyTarget(f.target);
    switch (f.type) {
    case TargetType::File: judgeFile(f, baseDir, checkNet); break;
    case TargetType::Web: f.state = LinkState::Unchecked; f.note = QObject::tr("Liên kết web — cần mạng để kiểm tra"); break;
    case TargetType::Mail: f.state = LinkState::Unchecked; f.note = QObject::tr("Địa chỉ thư điện tử"); break;
    case TargetType::Other: f.state = LinkState::Unchecked; break;
    }
}

// ---------------- .url ----------------
QVector<LinkFinding> readUrlFile(const QString& path, QString* err)
{
    QVector<LinkFinding> r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = f.errorString(); return r; }
    const QByteArray data = f.read(64 * 1024);
    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter)) text = QString::fromLocal8Bit(data);
    for (const QString& raw : text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts)) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("URL="), Qt::CaseInsensitive)) {
            LinkFinding lf;
            lf.source = path; lf.kind = QObject::tr("Lối tắt web (.url)"); lf.target = line.mid(4).trimmed();
            judge(lf, QFileInfo(path).absolutePath(), false);
            r << lf;
            break;
        }
    }
    if (r.isEmpty() && err) *err = QObject::tr("không thấy dòng URL=");
    return r;
}

// ---------------- .lnk (MS-SHLLINK) ----------------
static quint32 u32(const QByteArray& b, int o) { return o + 4 <= b.size() ? (quint32(quint8(b[o])) | (quint32(quint8(b[o + 1])) << 8) | (quint32(quint8(b[o + 2])) << 16) | (quint32(quint8(b[o + 3])) << 24)) : 0; }
static quint16 u16(const QByteArray& b, int o) { return o + 2 <= b.size() ? quint16(quint8(b[o]) | (quint8(b[o + 1]) << 8)) : 0; }
static QString cstrAnsi(const QByteArray& b, int o)
{
    if (o < 0 || o >= b.size()) return {};
    const int e = b.indexOf('\0', o);
    return QString::fromLocal8Bit(b.constData() + o, (e < 0 ? b.size() : e) - o);
}
static QString cstrUni(const QByteArray& b, int o)
{
    if (o < 0 || o + 2 > b.size()) return {};
    int e = o;
    while (e + 1 < b.size() && !(b[e] == 0 && b[e + 1] == 0)) e += 2;
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData() + o), (e - o) / 2);
}

QVector<LinkFinding> readLnkFile(const QString& path, QString* err, bool checkNetworkPaths)
{
    QVector<LinkFinding> r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = f.errorString(); return r; }
    const QByteArray b = f.read(1024 * 1024);
    if (b.size() < 76 || u32(b, 0) != 0x4C) { if (err) *err = QObject::tr("không phải tệp lối tắt .lnk hợp lệ"); return r; }
    const quint32 flags = u32(b, 20);
    const bool hasIdList = flags & 1, hasLinkInfo = flags & 2, hasName = flags & 4, hasRel = flags & 8, hasWork = flags & 0x10, hasArgs = flags & 0x20, hasIcon = flags & 0x40, isUni = flags & 0x80;
    int p = 76;
    if (hasIdList) p += 2 + u16(b, p);
    QString target;
    if (hasLinkInfo && p + 28 <= b.size()) {
        const int liStart = p;
        const quint32 liSize = u32(b, p), hdr = u32(b, p + 4), liFlags = u32(b, p + 8);
        const quint32 localOff = u32(b, p + 16), netOff = u32(b, p + 20), suffixOff = u32(b, p + 24);
        QString base, suffix;
        if (liFlags & 1) {
            if (hdr >= 0x24) {
                const quint32 uniBase = u32(b, p + 28), uniSuffix = u32(b, p + 32);
                base = uniBase ? cstrUni(b, liStart + int(uniBase)) : cstrAnsi(b, liStart + int(localOff));
                suffix = uniSuffix ? cstrUni(b, liStart + int(uniSuffix)) : cstrAnsi(b, liStart + int(suffixOff));
            } else { base = cstrAnsi(b, liStart + int(localOff)); suffix = cstrAnsi(b, liStart + int(suffixOff)); }
            target = base + suffix;
        } else if ((liFlags & 2) && netOff) {
            const int ns = liStart + int(netOff);
            const quint32 netNameOff = u32(b, ns + 8);
            const QString net = cstrAnsi(b, ns + int(netNameOff));
            suffix = cstrAnsi(b, liStart + int(suffixOff));
            target = net + (suffix.isEmpty() ? QString() : QStringLiteral("\\") + suffix);
        }
        p += int(liSize);
    }
    auto readStr = [&](bool present) -> QString {
        if (!present) return {};
        if (p + 2 > b.size()) return {};
        const int n = u16(b, p); p += 2;
        QString s;
        if (isUni) { if (p + n * 2 <= b.size()) s = QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData() + p), n); p += n * 2; }
        else { if (p + n <= b.size()) s = QString::fromLocal8Bit(b.constData() + p, n); p += n; }
        return s;
    };
    readStr(hasName);
    const QString rel = readStr(hasRel);
    readStr(hasWork); readStr(hasArgs); readStr(hasIcon);

    LinkFinding lf;
    lf.source = path; lf.kind = QObject::tr("Đích lối tắt (.lnk)");
    const QString baseDir = QFileInfo(path).absolutePath();
    if (!target.isEmpty()) { lf.target = target; judge(lf, baseDir, checkNetworkPaths); }
    else if (!rel.isEmpty()) { lf.target = rel; judge(lf, baseDir, checkNetworkPaths); lf.note = (lf.note.isEmpty() ? QString() : lf.note + QStringLiteral("; ")) + QObject::tr("theo đường dẫn tương đối"); }
    else { lf.target = QString(); lf.state = LinkState::Unchecked; lf.note = QObject::tr("Lối tắt tới mục hệ thống (không có đường dẫn tệp) — không kiểm tra"); }
    r << lf;
    return r;
}

// ---------------- Office (*.rels) ----------------
static QString kindLabel(const QString& typeUri)
{
    const QString k = typeUri.section(QLatin1Char('/'), -1);
    if (k == QLatin1String("hyperlink")) return QObject::tr("Siêu liên kết");
    if (k == QLatin1String("externalLinkPath")) return QObject::tr("Liên kết sổ tính ngoài");
    if (k == QLatin1String("oleObject")) return QObject::tr("Đối tượng OLE liên kết");
    if (k == QLatin1String("image")) return QObject::tr("Ảnh liên kết ngoài");
    if (k == QLatin1String("attachedTemplate")) return QObject::tr("Mẫu đính kèm");
    if (k == QLatin1String("subDocument")) return QObject::tr("Tài liệu con");
    if (k == QLatin1String("video") || k == QLatin1String("audio") || k == QLatin1String("media")) return QObject::tr("Âm thanh/video liên kết");
    return k.isEmpty() ? QObject::tr("Liên kết ngoài") : k;
}

QVector<LinkFinding> readOfficeLinks(const QString& path, QString* err, bool checkNetworkPaths)
{
    QVector<LinkFinding> r;
    ZipReader z;
    if (!z.open(path, err)) return r;
    const QString baseDir = QFileInfo(path).absolutePath();
    int relsSeen = 0;
    for (const ZipEntryInfo& e : z.entries()) {
        if (!e.name.endsWith(QLatin1String(".rels"), Qt::CaseInsensitive)) continue;
        if (++relsSeen > 2000) { if (err) *err = QObject::tr("quá nhiều tệp quan hệ — dừng đọc"); break; }
        QByteArray xml; QString re;
        if (!z.read(e, &xml, &re, 8 * 1024 * 1024)) { if (err && err->isEmpty()) *err = re; continue; }
        // part sở hữu: "word/_rels/document.xml.rels" → "word/document.xml"
        QString owner = e.name;
        owner.remove(QLatin1String("_rels/"));
        owner.chop(5);
        QXmlStreamReader xr(xml);
        while (!xr.atEnd()) {
            xr.readNext();
            if (!xr.isStartElement() || xr.name() != QLatin1String("Relationship")) continue;
            const auto a = xr.attributes();
            if (a.value(QLatin1String("TargetMode")).compare(QLatin1String("External"), Qt::CaseInsensitive) != 0) continue;
            LinkFinding lf;
            lf.source = path; lf.where = owner;
            lf.kind = kindLabel(a.value(QLatin1String("Type")).toString());
            lf.target = a.value(QLatin1String("Target")).toString();
            judge(lf, baseDir, checkNetworkPaths);
            r << lf;
        }
        if (xr.hasError() && err && err->isEmpty()) *err = QObject::tr("'%1': XML hỏng").arg(e.name);
    }
    return r;
}

static bool isOfficeExt(const QString& s) { static const QSet<QString> x = {"docx", "docm", "dotx", "dotm", "xlsx", "xlsm", "xltx", "xltm", "pptx", "pptm", "potx", "potm", "ppsx"}; return x.contains(s); }

ScanLinksResult scanLinks(const QStringList& paths, const ScanLinksOptions& o, const Progress& prog)
{
    ScanLinksResult res;
    QStringList files;
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        const bool isLnk = fi.suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0;   // Windows coi .lnk là liên kết tượng trưng
        if (fi.isDir() && !isLnk) {
            // QDir::System: để thấy cả .lnk đang trỏ vào chỗ không còn (Windows xếp chúng vào loại "hệ thống")
            QDirIterator it(p, QDir::Files | QDir::System | QDir::NoDotAndDotDot, o.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
            while (it.hasNext()) files << it.next();
        } else if (fi.isFile() || fi.isSymLink() || isLnk) { if (fi.isSymLink() || fi.exists() || isLnk) files << fi.absoluteFilePath(); else res.skipped << QObject::tr("%1: không tồn tại").arg(p); }
        else res.skipped << QObject::tr("%1: không tồn tại").arg(p);
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    for (const QString& f : files) {
        const QFileInfo fi(f);
        const QString ext = fi.suffix().toLower();
        const bool office = isOfficeExt(ext);
        if (fi.fileName().startsWith(QLatin1String("~$"))) continue;
        if (ext != QLatin1String("url") && ext != QLatin1String("lnk") && !office) continue;
        if (prog && !prog(res.filesScanned, files.size())) { res.cancelled = true; break; }
        ++res.filesScanned;
        QString err;
        QVector<LinkFinding> fs = ext == QLatin1String("url") ? readUrlFile(f, &err) : ext == QLatin1String("lnk") ? readLnkFile(f, &err, o.checkNetworkPaths) : readOfficeLinks(f, &err, o.checkNetworkPaths);
        res.findings += fs;
        if (!err.isEmpty()) res.skipped << QObject::tr("%1: %2").arg(f, err);
    }
    return res;
}

OpResult exportFindingsCsv(const QString& path, const QVector<LinkFinding>& f)
{
    QVector<QStringList> rows;
    for (const auto& x : f) rows.push_back({x.source, x.where, x.kind, targetTypeText(x.type), x.target, linkStateText(x.state), x.note});
    return exportCsvRows(path, {QObject::tr("Tệp"), QObject::tr("Phần"), QObject::tr("Loại liên kết"), QObject::tr("Kiểu đích"), QObject::tr("Đích"), QObject::tr("Trạng thái"), QObject::tr("Ghi chú")}, rows);
}

} // namespace ft
