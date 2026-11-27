// SPDX-License-Identifier: MPL-2.0
#include "TemplateLibrary.h"

#include "../extras/WorkspaceStore.h"

#include <QDir>
#include <QSet>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QVersionNumber>
#include <QLockFile>
#include "../tools/filetools/ZipLite.h"

namespace tpl {
namespace {

const char* kCsv = "00_HUONG_DAN_VA_DANH_MUC/DANH_MUC_TAP_TIN.csv";
const char* kSrc = "00_HUONG_DAN_VA_DANH_MUC/NGUON_THAM_KHAO_CHINH_THUC.csv";

constexpr qint64 kPackBytes=100LL*1024*1024;
constexpr qint64 kEntryBytes=32LL*1024*1024;
constexpr int kPackEntries=3000;

bool safeId(const QString& id){
    static const QRegularExpression re(QStringLiteral("\\A[a-z0-9][a-z0-9-]{0,63}\\z"));
    static const QRegularExpression device(QStringLiteral("\\A(?:con|prn|aux|nul|com[1-9]|lpt[1-9])\\z"),QRegularExpression::CaseInsensitiveOption);
    return re.match(id).hasMatch()&&!device.match(id).hasMatch();
}
bool safeRel(const QString& name){
    if(name.isEmpty()||name.size()>1024||name.contains(QChar::ReplacementCharacter))return false;
    static const QRegularExpression invalid(QStringLiteral("[\\x00-\\x1f\\x7f<>:\"\\\\|?*]"));
    static const QRegularExpression device(QStringLiteral("\\A(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\\.|\\z)"),QRegularExpression::CaseInsensitiveOption);
    if(name.contains(invalid))return false;
    for(const auto& part:name.split(QLatin1Char('/'))){
        if(part.isEmpty()||part==QLatin1String(".")||part==QLatin1String("..")||
           part.endsWith(QLatin1Char('.'))||part.endsWith(QLatin1Char(' '))||device.match(part).hasMatch())return false;
    }
    return true;
}
bool inside(const QString& child,const QString& root){
#ifdef Q_OS_WIN
    return child.startsWith(root+QLatin1Char('/'),Qt::CaseInsensitive);
#else
    return child.startsWith(root+QLatin1Char('/'));
#endif
}
bool safeFile(const QString& root,const QString& rel){
    if(!safeRel(rel))return false;
    const auto base=QFileInfo(root).canonicalFilePath();
    const QFileInfo fi(QDir(root).filePath(rel));
    return !base.isEmpty()&&fi.isFile()&&!fi.isSymLink()&&inside(fi.canonicalFilePath(),base);
}

bool copyTree(const QString& from,const QString& to){
    if(!QDir().mkpath(to))return false;
    QDir base(from);const auto canonical=QFileInfo(from).canonicalFilePath();
    QDirIterator it(from,QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    QSet<QString> names;qint64 total=0;
    while(it.hasNext()){
        const QString path=it.next();const QFileInfo fi(path);
        const QString rel=base.relativeFilePath(path),key=rel.toCaseFolded();
        if(!safeRel(rel)||names.contains(key)||names.size()>=kPackEntries||fi.isSymLink()||
           !inside(fi.canonicalFilePath(),canonical)||(!fi.isDir()&&!fi.isFile()))return false;
        names.insert(key);
        const QString dest=QDir(to).filePath(rel);
        if(fi.isDir()){if(!QDir().mkpath(dest))return false;continue;}
        if(fi.size()<0||fi.size()>kEntryBytes||fi.size()>kPackBytes-total)return false;
        if(!QDir().mkpath(QFileInfo(dest).absolutePath())||!QFile::copy(path,dest))return false;
        const auto size=QFileInfo(dest).size();
        if(size!=fi.size()||size>kPackBytes-total)return false;
        total+=size;
    }
    return true;
}

// Đọc CSV UTF-8 (có thể có BOM), trường có thể đặt trong dấu nháy kép.
QList<QStringList> readCsv(const QString& path) {
    QList<QStringList> rows;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return rows;
    if(f.size()>4LL*1024*1024)return rows;
    const auto raw=f.read(4LL*1024*1024+1);
    if(f.error()!=QFileDevice::NoError||!f.atEnd()||raw.size()>4LL*1024*1024)return rows;
    // Qt may remove the initial UTF-8 BOM during decoding. Compare the payload only.
    const auto payload=raw.startsWith("\xEF\xBB\xBF")?raw.mid(3):raw;
    QString text=QString::fromUtf8(payload);
    if(text.toUtf8()!=payload)return rows;
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
    if(q)return {}; // unterminated quoted field is never a valid catalogue
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
    if(QFileInfo(mf).isSymLink())return li;
    if(mf.exists()){
        if(!mf.open(QIODevice::ReadOnly)||mf.size()>65536)return li;
        const auto bytes=mf.read(65537);
        QJsonParseError parse;
        const auto doc=QJsonDocument::fromJson(bytes,&parse);
        if(mf.error()!=QFileDevice::NoError||!mf.atEnd()||bytes.size()>65536||parse.error!=QJsonParseError::NoError||!doc.isObject())return li;
        const QJsonObject o=doc.object();
        if(!o.value("id").isString()||!safeId(o.value("id").toString()))return li;
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
    if(!safeId(li.id)||!safeFile(root,QLatin1String(kCsv)))return li;
    const auto rows=readCsv(root+QLatin1Char('/')+QLatin1String(kCsv));
    if(rows.size()<2||rows.size()>kPackEntries)return li;
    QSet<QString> seen;
    for(int i=1;i<rows.size();++i){
        const auto rel=rows[i].value(0);const auto key=rel.toCaseFolded();
        const auto ext=QFileInfo(rel).suffix().toUpper();
        if(!safeFile(root,rel)||seen.contains(key)||
           (ext!="DOCX"&&ext!="XLSX"&&ext!="PPTX")||rows[i].value(4).trimmed().toUpper()!=ext)return li;
        seen.insert(key);
    }
    li.valid=true;li.count=seen.size();
    return li;
}

LibInfo installed(const QString& id) {
    if(!safeId(id))return {};
    const QString dir = installedRoot() + QLatin1Char('/') + libFolderName(id);
    if (!QDir(dir).exists()||QFileInfo(dir).isSymLink()) return {};
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
    const LibInfo incoming=readInfo(root);
    if(!incoming.valid){r.message=QObject::tr("Gói mẫu không hợp lệ: mã gói, danh mục, đường dẫn hoặc tệp mẫu sai.");return r;}
    const QString top=installedRoot();
    const auto sourceRoot=QFileInfo(root).canonicalFilePath(),installTop=QFileInfo(top).canonicalFilePath();
    if(sourceRoot.isEmpty()||installTop.isEmpty()||sourceRoot==installTop||inside(sourceRoot,installTop)||inside(installTop,sourceRoot)){
        r.message=QObject::tr("Không cài gói trực tiếp từ hoặc chứa thư mục mẫu đang cài.");return r;
    }
    QLockFile lock(QDir(top).filePath(QStringLiteral(".install.lock")));
    lock.setStaleLockTime(0);
    if(!lock.tryLock()){r.message=QObject::tr("Đang có thao tác cài mẫu khác hoặc khóa cài đặt chưa được giải phóng.");return r;}
    const QString target=QDir(top).filePath(incoming.id);
    const QFileInfo targetInfo(target);
    if(targetInfo.isSymLink()||(targetInfo.exists()&&!targetInfo.isDir())){
        r.message=QObject::tr("Đích cài mẫu không phải thư mục an toàn.");return r;
    }
    const LibInfo cur=installed(incoming.id);
    if(cur.valid&&!allowOlder&&compareVersion(incoming.version,cur.version)<0){
        r.needConfirm=true;r.info=incoming;
        r.message=QObject::tr("Gói bạn chọn (phiên bản %1) CŨ HƠN gói đang cài (%2). Giữ gói mới hơn?").arg(incoming.version,cur.version);
        return r;
    }
    QTemporaryDir stage(QDir(top).filePath(QStringLiteral(".install-XXXXXX")));
    if(!stage.isValid()){r.message=QObject::tr("Không tạo được thư mục cài tạm.");return r;}
    const auto tmpNew=stage.filePath("new"),tmpOld=stage.filePath("old");
    if(!copyTree(sourceRoot,tmpNew)){
        r.message=QObject::tr("Không chép được gói mẫu an toàn; gói cũ giữ nguyên.");return r;
    }
    const auto verified=readInfo(tmpNew);
    if(!verified.valid||verified.id!=incoming.id||verified.version!=incoming.version||verified.count!=incoming.count){
        r.message=QObject::tr("Bản chép không khớp danh mục; gói cũ giữ nguyên.");return r;
    }
    const bool hadOld=targetInfo.exists();
    if(hadOld&&!QDir().rename(target,tmpOld)){r.message=QObject::tr("Không chuyển được gói cũ; gói cũ giữ nguyên.");return r;}
    if(!QDir().rename(tmpNew,target)){
        if(hadOld&&!QDir().rename(tmpOld,target)){
            stage.setAutoRemove(false);
            r.message=QObject::tr("Không đặt được gói mới và chưa phục hồi tự động; gói cũ còn tại: %1").arg(tmpOld);
        }else r.message=QObject::tr("Không đặt được gói mới; gói cũ giữ nguyên.");
        return r;
    }
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
    QString error;ft::ZipReader zip;
    if(!QFileInfo(zipPath).isFile()||QFileInfo(zipPath).size()>kPackBytes||!zip.open(zipPath,&error)||
       zip.entries().isEmpty()||zip.entries().size()>kPackEntries){
        r.message=QObject::tr("Gói ZIP không hợp lệ hoặc quá lớn: ")+error;return r;
    }
    QSet<QString> names,parents;qint64 total=0;
    for(const auto& e:zip.entries()){
        QString name=e.name;if(name.endsWith(QLatin1Char('/')))name.chop(1);
        if(!safeRel(name)||names.contains(name.toCaseFolded())||e.encrypted||e.zip64||
           e.usize>quint64(kEntryBytes)||e.csize>quint64(kEntryBytes)||e.usize>quint64(kPackBytes-total)){
            r.message=QObject::tr("Mục ZIP không an toàn hoặc quá lớn: ")+name.left(120);return r;
        }
        names.insert(name.toCaseFolded());total+=qint64(e.usize);
        for(int i=name.indexOf(QLatin1Char('/'));i>=0;i=name.indexOf(QLatin1Char('/'),i+1))parents.insert(name.left(i));
    }
    for(const auto& e:zip.entries()){
        QString name=e.name;const bool slash=name.endsWith(QLatin1Char('/'));if(slash)name.chop(1);
        const QString dest=QDir(tmp.path()).filePath(name);
        QByteArray bytes;
        if(!zip.read(e,&bytes,&error,kEntryBytes)){r.message=QObject::tr("Mục ZIP hỏng: ")+error;return r;}
        if(slash||parents.contains(name)){
            if(!bytes.isEmpty()||!QDir().mkpath(dest)){r.message=QObject::tr("Thư mục ZIP không hợp lệ: ")+name;return r;}
            continue;
        }
        if(!QDir().mkpath(QFileInfo(dest).absolutePath())){r.message=QObject::tr("Không tạo được thư mục tạm.");return r;}
        QFile file(dest);
        if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||file.write(bytes)!=bytes.size()||!file.flush()){
            r.message=QObject::tr("Không giải nén đầy đủ mục: ")+name;return r;
        }
    }
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
        if(!in.valid)continue;
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
        if (!safeFile(lib.root,r[0])) continue;
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
    if(!lib.valid||!safeFile(lib.root,QLatin1String(kSrc)))return out;
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
    if (QFile::exists(destFile)||QFileInfo(destFile).isSymLink()) {
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
