// SPDX-License-Identifier: MPL-2.0
#include "TemplateLibrary.h"

#include "../extras/WorkspaceStore.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QVersionNumber>
#ifdef Q_OS_WIN
#include <private/qzipreader_p.h> // đọc zip UTF-8 đúng tên tiếng Việt (tar của Windows không đọc được mọi tệp zip)
#endif

namespace tpl {
namespace {

const char* kCsv = "00_HUONG_DAN_VA_DANH_MUC/DANH_MUC_TAP_TIN.csv";
const char* kSrc = "00_HUONG_DAN_VA_DANH_MUC/NGUON_THAM_KHAO_CHINH_THUC.csv";

bool copyTree(const QString& from, const QString& to) {
    if (!QDir().mkpath(to)) return false;
    QDir src(from);
    for (const QFileInfo& fi : src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        const QString dst = to + QLatin1Char('/') + fi.fileName();
        if (fi.isDir()) {
            if (!copyTree(fi.absoluteFilePath(), dst)) return false;
        } else if (!QFile::copy(fi.absoluteFilePath(), dst)) {
            return false;
        }
    }
    return true;
}

// Đọc CSV UTF-8 (có thể có BOM), trường có thể đặt trong dấu nháy kép.
QList<QStringList> readCsv(const QString& path) {
    QList<QStringList> rows;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return rows;
    QString text = QString::fromUtf8(f.readAll());
    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);
    QStringList row;
    QString cur;
    bool q = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (q) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) { cur += QLatin1Char('"'); ++i; }
                else q = false;
            } else cur += c;
        } else if (c == QLatin1Char('"')) q = true;
        else if (c == QLatin1Char(',')) { row << cur; cur.clear(); }
        else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            if (c == QLatin1Char('\r') && i + 1 < text.size() && text[i + 1] == QLatin1Char('\n')) ++i;
            row << cur; cur.clear();
            if (!(row.size() == 1 && row[0].isEmpty())) rows << row;
            row.clear();
        } else cur += c;
    }
    if (!cur.isEmpty() || !row.isEmpty()) { row << cur; rows << row; }
    return rows;
}

QString libFolderName(const QString& id) { return id; }

}  // namespace

QString installedRoot() {
    const QString d = workspace::dataDir() + QStringLiteral("/templates");
    QDir().mkpath(d);
    return d;
}

bool findLibraryRoot(const QString& dir, QString* root) {
    auto isRoot = [](const QString& d) {
        return QFile::exists(d + QLatin1Char('/') + QLatin1String(kCsv)) || QFile::exists(d + QStringLiteral("/manifest.json"));
    };
    if (isRoot(dir)) { *root = dir; return true; }
    QDir d(dir);
    for (const QFileInfo& fi : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (isRoot(fi.absoluteFilePath())) { *root = fi.absoluteFilePath(); return true; }
        QDir d2(fi.absoluteFilePath());
        for (const QFileInfo& f2 : d2.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
            if (isRoot(f2.absoluteFilePath())) { *root = f2.absoluteFilePath(); return true; }
    }
    return false;
}

LibInfo readInfo(const QString& root) {
    LibInfo li;
    li.root = root;
    QFile mf(root + QStringLiteral("/manifest.json"));
    if (mf.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(mf.readAll()).object();
        li.id = o.value(QStringLiteral("id")).toString();
        li.name = o.value(QStringLiteral("name")).toString();
        li.version = o.value(QStringLiteral("version")).toString();
    }
    const QString folder = QFileInfo(root).fileName();
    if (li.id.isEmpty()) li.id = QStringLiteral("thu-vien-bieu-mau");
    if (li.name.isEmpty()) li.name = folder;
    if (li.version.isEmpty()) {
        const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("(\\d{4}(?:[._]\\d+)?)$")).match(folder);
        li.version = m.hasMatch() ? QString(m.captured(1)).replace(QLatin1Char('_'), QLatin1Char('.')) : QStringLiteral("0");
    }
    li.valid = QFile::exists(root + QLatin1Char('/') + QLatin1String(kCsv)) || QFile::exists(root + QStringLiteral("/manifest.json"));
    li.count = qMax(0, readCsv(root + QLatin1Char('/') + QLatin1String(kCsv)).size() - 1);
    return li;
}

LibInfo installed(const QString& id) {
    const QString dir = installedRoot() + QLatin1Char('/') + libFolderName(id);
    if (!QDir(dir).exists()) return {};
    return readInfo(dir);
}

int compareVersion(const QString& a, const QString& b) {
    return QVersionNumber::compare(QVersionNumber::fromString(a), QVersionNumber::fromString(b));
}

InstallResult installFromDir(const QString& dir, bool allowOlder) {
    InstallResult r;
    QString root;
    if (!findLibraryRoot(dir, &root)) {
        r.message = QObject::tr("Thư mục này không phải gói mẫu của Dũng Office (thiếu 00_HUONG_DAN_VA_DANH_MUC/DANH_MUC_TAP_TIN.csv).");
        return r;
    }
    const LibInfo incoming = readInfo(root);
    const QString top = installedRoot();
    const QString target = top + QLatin1Char('/') + libFolderName(incoming.id);
    const QString tmpNew = top + QStringLiteral("/.moi-") + incoming.id;
    const QString tmpOld = top + QStringLiteral("/.cu-") + incoming.id;
    const LibInfo cur = installed(incoming.id);
    if (cur.valid && !allowOlder && compareVersion(incoming.version, cur.version) < 0) {
        r.needConfirm = true;
        r.info = incoming;
        r.message = QObject::tr("Gói bạn chọn (phiên bản %1) CŨ HƠN gói đang cài (%2). Giữ gói mới hơn?").arg(incoming.version, cur.version);
        return r;
    }
    QDir(tmpNew).removeRecursively();
    QDir(tmpOld).removeRecursively();
    if (!copyTree(root, tmpNew)) {
        QDir(tmpNew).removeRecursively();
        r.message = QObject::tr("Không chép được gói mẫu (hết chỗ trống hoặc không có quyền ghi?). Gói cũ vẫn giữ nguyên.");
        return r;
    }
    bool hadOld = QDir(target).exists();
    if (hadOld && !QDir().rename(target, tmpOld)) {
        QDir(tmpNew).removeRecursively();
        r.message = QObject::tr("Không thay được gói cũ (có thể một mẫu đang mở). Hãy đóng các tệp mẫu rồi thử lại. Gói cũ vẫn giữ nguyên.");
        return r;
    }
    if (!QDir().rename(tmpNew, target)) {
        if (hadOld) QDir().rename(tmpOld, target); // hoàn lại
        QDir(tmpNew).removeRecursively();
        r.message = QObject::tr("Không đặt được gói mới vào chỗ. Đã hoàn lại gói cũ.");
        return r;
    }
    if (hadOld) QDir(tmpOld).removeRecursively();
    r.ok = true;
    r.replaced = hadOld;
    r.info = readInfo(target);
    r.message = hadOld ? QObject::tr("Đã thay gói mẫu cũ (%1) bằng gói mới (%2). Bản cũ đã bị xoá để tránh dùng nhầm.").arg(cur.version, r.info.version)
                       : QObject::tr("Đã cài gói mẫu phiên bản %1.").arg(r.info.version);
    return r;
}

InstallResult installFromZip(const QString& zipPath, bool allowOlder) {
    InstallResult r;
    QTemporaryDir tmp;
    if (!tmp.isValid()) { r.message = QObject::tr("Không tạo được thư mục tạm."); return r; }
#ifdef Q_OS_WIN
    {
        QZipReader zr(zipPath);
        if (!zr.isReadable() || zr.count() == 0) {
            r.message = QObject::tr("Không giải nén được tệp zip (tệp hỏng hoặc không phải zip).");
            return r;
        }
        // Giải nén từng mục, báo rõ mục lỗi (đường dẫn dài/tên lạ), không báo chung chung.
        const QString root = QDir(tmp.path()).absolutePath();
        for (const auto &fi : zr.fileInfoList()) {
            if (fi.filePath.contains(QStringLiteral(".."))) continue; // chặn zip-slip
            const QString dest = root + QLatin1Char('/') + fi.filePath;
            const QString lp = QStringLiteral("\\\\?\\") + QDir::toNativeSeparators(dest); // tiền tố đường dẫn dài
            if (fi.isDir || !fi.isFile) { QDir().mkpath(lp); continue; }
            QDir().mkpath(QStringLiteral("\\\\?\\") + QDir::toNativeSeparators(QFileInfo(dest).absolutePath()));
            QFile f(lp);
            if (!f.open(QIODevice::WriteOnly) || f.write(zr.fileData(fi.filePath)) < 0) {
                r.message = QObject::tr("Không giải nén được: ") + fi.filePath.left(120) + QStringLiteral(" (") + f.errorString() + QLatin1Char(')');
                return r;
            }
        }
    }
#else
    QProcess p;
    p.start(QStringLiteral("unzip"), {QStringLiteral("-q"), QStringLiteral("-o"), zipPath, QStringLiteral("-d"), tmp.path()});
    if (!p.waitForFinished(10 * 60 * 1000) || p.exitCode() != 0) {
        r.message = QObject::tr("Không giải nén được tệp zip: ") + QString::fromUtf8(p.readAllStandardError()).left(200);
        return r;
    }
#endif
    return installFromDir(tmp.path(), allowOlder);
}

bool seedFromBundled(const QString& appDir) {
    const QString b = appDir + QStringLiteral("/templates");
    QDir d(b);
    bool changed = false;
    for (const QFileInfo& fi : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString root;
        if (!findLibraryRoot(fi.absoluteFilePath(), &root)) continue;
        const LibInfo in = readInfo(root);
        const LibInfo cur = installed(in.id);
        if (!cur.valid || compareVersion(in.version, cur.version) > 0) {
            if (installFromDir(root).ok) changed = true;
        }
    }
    return changed;
}

QList<Entry> entries(const LibInfo& lib) {
    QList<Entry> out;
    const QList<QStringList> rows = readCsv(lib.root + QLatin1Char('/') + QLatin1String(kCsv));
    for (int i = 1; i < rows.size(); ++i) {
        const QStringList& r = rows[i];
        if (r.size() < 1 || r[0].isEmpty()) continue;
        if (!QFile::exists(lib.root + QLatin1Char('/') + r[0])) continue; // chỉ liệt kê mẫu còn tồn tại
        Entry e;
        e.relPath = r[0];
        e.status = r.value(1);
        e.origin = r.value(2);
        e.url = r.value(3);
        e.format = r.value(4);
        out << e;
    }
    return out;
}

QList<Source> sources(const LibInfo& lib) {
    QList<Source> out;
    const QList<QStringList> rows = readCsv(lib.root + QLatin1Char('/') + QLatin1String(kSrc));
    for (int i = 1; i < rows.size(); ++i) {
        Source s;
        s.group = rows[i].value(0);
        s.document = rows[i].value(1);
        s.url = rows[i].value(2);
        s.guide = rows[i].value(3);
        if (!s.document.isEmpty()) out << s;
    }
    return out;
}

bool makeWorkingCopy(const QString& templatePath, const QString& destFile, QString* error) {
    if (QFile::exists(destFile)) {
        if (error) *error = QObject::tr("Tệp đã tồn tại, Dũng Office không ghi đè: ") + QDir::toNativeSeparators(destFile);
        return false;
    }
    if (!QFile::copy(templatePath, destFile)) {
        if (error) *error = QObject::tr("Không sao chép được mẫu sang: ") + QDir::toNativeSeparators(destFile);
        return false;
    }
    QFile::setPermissions(destFile, QFile::permissions(destFile) | QFile::WriteOwner | QFile::WriteUser);
    return true;
}

}  // namespace tpl
