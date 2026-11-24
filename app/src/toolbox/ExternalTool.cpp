// SPDX-License-Identifier: MPL-2.0
#include "ExternalTool.h"

#include "../extras/WorkspaceStore.h"
#include "ToolRegistry.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

namespace toolbox {
namespace {

constexpr qint64 kMaxManifestBytes = 256 * 1024;
constexpr qint64 kMaxToolDirBytes = 8 * 1024 * 1024;
constexpr int kMaxToolDirFiles = 500;
constexpr qint64 kMaxOutputBytes = 2 * 1024 * 1024;
constexpr qint64 kMaxLogBytes = 1024 * 1024;

bool isBlockedProgram(const QString& name) {
    QString n = name.toLower();
    if (n.endsWith(QStringLiteral(".exe"))) n.chop(4);
    static const QStringList blocked = {
        QStringLiteral("cmd"),      QStringLiteral("powershell"), QStringLiteral("pwsh"),   QStringLiteral("sh"),
        QStringLiteral("bash"),     QStringLiteral("zsh"),        QStringLiteral("dash"),   QStringLiteral("wscript"),
        QStringLiteral("cscript"),  QStringLiteral("mshta"),      QStringLiteral("rundll32"), QStringLiteral("regsvr32"),
        QStringLiteral("wmic"),     QStringLiteral("msiexec"),    QStringLiteral("certutil"), QStringLiteral("bitsadmin")};
    return blocked.contains(n);
}

bool hasBadExtension(const QString& name) {
    const QString n = name.toLower();
    for (const char* ext : {".bat", ".cmd", ".com", ".scr", ".lnk", ".msi", ".vbs", ".ps1", ".js", ".jse", ".wsf", ".hta"})
        if (n.endsWith(QLatin1String(ext))) return true;
    return false;
}

bool strField(const QJsonObject& o, const char* key, QString* out, QString* err, bool required, int maxLen) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isUndefined() || v.isNull()) {
        if (required) { *err = QStringLiteral("thiếu trường \"%1\"").arg(QLatin1String(key)); return false; }
        return true;
    }
    if (!v.isString()) { *err = QStringLiteral("trường \"%1\" phải là chữ").arg(QLatin1String(key)); return false; }
    const QString s = v.toString();
    if (s.size() > maxLen) { *err = QStringLiteral("trường \"%1\" dài quá %2 ký tự").arg(QLatin1String(key)).arg(maxLen); return false; }
    if (s.contains(QChar(0))) { *err = QStringLiteral("trường \"%1\" có ký tự không hợp lệ").arg(QLatin1String(key)); return false; }
    *out = s;
    return true;
}

QString sha256Dir(const QString& dir, QString* err) {
    QStringList files;
    qint64 total = 0;
    QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        const QFileInfo fi(p);
        if (fi.isSymLink()) { *err = QStringLiteral("thư mục tool có liên kết tượng trưng (%1) — không cho phép").arg(fi.fileName()); return {}; }
        total += fi.size();
        files << p;
        if (files.size() > kMaxToolDirFiles) { *err = QStringLiteral("thư mục tool có quá %1 tệp").arg(kMaxToolDirFiles); return {}; }
        if (total > kMaxToolDirBytes) { *err = QStringLiteral("thư mục tool lớn hơn 8 MB"); return {}; }
    }
    // thư mục con là liên kết tượng trưng cũng bị từ chối
    QDirIterator dt(dir, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System, QDirIterator::Subdirectories);
    while (dt.hasNext()) {
        const QFileInfo fi(dt.next());
        if (fi.isSymLink()) { *err = QStringLiteral("thư mục tool có liên kết tượng trưng (%1) — không cho phép").arg(fi.fileName()); return {}; }
    }
    files.sort();
    QCryptographicHash h(QCryptographicHash::Sha256);
    const QDir base(dir);
    for (const QString& p : files) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) { *err = QStringLiteral("không đọc được %1").arg(QFileInfo(p).fileName()); return {}; }
        h.addData(base.relativeFilePath(p).toUtf8());
        h.addData(QByteArray(1, '\0'));
        h.addData(QByteArray::number(f.size()));
        h.addData(QByteArray(1, '\0'));
        while (!f.atEnd()) h.addData(f.read(1 << 16));
    }
    return QString::fromLatin1(h.result().toHex());
}

QString canonicalOrEmpty(const QString& p) { return QFileInfo(p).canonicalFilePath(); }

bool isInside(const QString& canonicalChild, const QString& canonicalDir) {
    const QString d = canonicalDir.endsWith(QLatin1Char('/')) ? canonicalDir : canonicalDir + QLatin1Char('/');
#ifdef Q_OS_WIN
    return canonicalChild.startsWith(d, Qt::CaseInsensitive);
#else
    return canonicalChild.startsWith(d);
#endif
}

} // namespace

bool isSafeRelativePath(const QString& rel, QString* why) {
    auto no = [&](const QString& w) { if (why) *why = w; return false; };
    if (rel.isEmpty()) return no(QStringLiteral("đường dẫn rỗng"));
    if (rel.contains(QChar(0))) return no(QStringLiteral("đường dẫn có ký tự không hợp lệ"));
    if (rel.contains(QLatin1Char('\\'))) return no(QStringLiteral("dùng '/' thay cho '\\' trong đường dẫn"));
    if (rel.contains(QLatin1Char(':'))) return no(QStringLiteral("không dùng ổ đĩa/':' trong đường dẫn"));
    if (rel.startsWith(QLatin1Char('/')) || rel.startsWith(QLatin1Char('~'))) return no(QStringLiteral("không dùng đường dẫn tuyệt đối"));
    if (QFileInfo(rel).isAbsolute()) return no(QStringLiteral("không dùng đường dẫn tuyệt đối"));
    for (const QString& seg : rel.split(QLatin1Char('/')))
        if (seg == QLatin1String("..")) return no(QStringLiteral("không được thoát ra ngoài thư mục tool ('..')"));
    return true;
}

ParseResult parseManifest(const QByteArray& json, const QString& toolDir) {
    ParseResult r;
    auto fail = [&](const QString& e) { r.ok = false; r.error = e; return r; };
    if (json.size() > kMaxManifestBytes) return fail(QStringLiteral("tool.json lớn hơn 256 KB"));
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail(QStringLiteral("tool.json không phải JSON hợp lệ (%1)").arg(pe.error != QJsonParseError::NoError ? pe.errorString() : QStringLiteral("không phải đối tượng")));
    const QJsonObject o = doc.object();
    QString err;
    ToolSpec s;
    if (o.contains(QStringLiteral("schema")) && o.value(QStringLiteral("schema")).toInt(-1) != 1)
        return fail(QStringLiteral("\"schema\" phải là 1 (phiên bản tool.json không được hỗ trợ)"));
    if (!strField(o, "id", &s.id, &err, true, 41) || !strField(o, "name", &s.name, &err, true, 80) ||
        !strField(o, "description", &s.description, &err, false, 300) || !strField(o, "category", &s.category, &err, false, 40) ||
        !strField(o, "version", &s.version, &err, false, 30) || !strField(o, "command", &s.command, &err, true, 260))
        return fail(err);
    if (!isValidId(s.id)) return fail(QStringLiteral("\"id\" chỉ gồm chữ thường, số, '_' hoặc '-' (2–41 ký tự, bắt đầu bằng chữ/số)"));
    if (s.name.trimmed().isEmpty()) return fail(QStringLiteral("\"name\" không được rỗng"));
    if (s.category.trimmed().isEmpty()) s.category = QStringLiteral("Tool ngoài");

    // --- lệnh ---
    if (s.command.contains(QLatin1Char('/'))) {
        QString why;
        if (!isSafeRelativePath(s.command, &why)) return fail(QStringLiteral("\"command\": %1").arg(why));
    } else {
        static const QRegularExpression bare(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,63}$"));
        if (!bare.match(s.command).hasMatch())
            return fail(QStringLiteral("\"command\" phải là tên chương trình trần (vd python) hoặc đường dẫn tương đối dạng ./chay.exe"));
    }
    if (isBlockedProgram(QFileInfo(s.command).fileName()))
        return fail(QStringLiteral("\"command\": không cho gọi trực tiếp trình lệnh hệ thống (%1); hãy dùng python/AutoHotkey và script trong thư mục tool").arg(s.command));
    if (hasBadExtension(s.command))
        return fail(QStringLiteral("\"command\": không hỗ trợ tệp .bat/.cmd/.vbs/.ps1/.lnk... (nguy cơ chèn lệnh); dùng chương trình thật như python"));

    // --- thư mục chạy ---
    if (o.contains(QStringLiteral("workingDir"))) {
        if (!strField(o, "workingDir", &s.workingDir, &err, false, 200)) return fail(err);
        QString why;
        if (s.workingDir != QLatin1String(".") && !isSafeRelativePath(s.workingDir, &why)) return fail(QStringLiteral("\"workingDir\": %1").arg(why));
    }
    if (o.contains(QStringLiteral("timeoutSec"))) {
        const int t = o.value(QStringLiteral("timeoutSec")).toInt(-1);
        if (t < 1 || t > 3600) return fail(QStringLiteral("\"timeoutSec\" phải từ 1 đến 3600"));
        s.timeoutSec = t;
    }

    // --- trường nhập ---
    if (o.contains(QStringLiteral("fields"))) {
        if (!o.value(QStringLiteral("fields")).isArray()) return fail(QStringLiteral("\"fields\" phải là danh sách"));
        const QJsonArray fa = o.value(QStringLiteral("fields")).toArray();
        if (fa.size() > 40) return fail(QStringLiteral("tối đa 40 trường nhập"));
        static const QRegularExpression fid(QStringLiteral("^[A-Za-z][A-Za-z0-9_]{0,31}$"));
        static const QStringList types = {QStringLiteral("text"), QStringLiteral("multiline"), QStringLiteral("number"),
                                          QStringLiteral("file"), QStringLiteral("folder"), QStringLiteral("choice")};
        for (int i = 0; i < fa.size(); ++i) {
            if (!fa.at(i).isObject()) return fail(QStringLiteral("trường nhập #%1 không phải đối tượng").arg(i + 1));
            const QJsonObject fo = fa.at(i).toObject();
            FieldSpec f;
            QString e2;
            if (!strField(fo, "id", &f.id, &e2, true, 32) || !strField(fo, "label", &f.label, &e2, true, 80) ||
                !strField(fo, "hint", &f.hint, &e2, false, 200) || !strField(fo, "default", &f.def, &e2, false, 4000))
                return fail(QStringLiteral("trường nhập #%1: %2").arg(i + 1).arg(e2));
            if (!fid.match(f.id).hasMatch() || f.id == QLatin1String("tooldir"))
                return fail(QStringLiteral("trường nhập #%1: mã \"%2\" không hợp lệ (chữ cái đầu, chỉ chữ/số/_; \"tooldir\" dành riêng)").arg(i + 1).arg(f.id));
            for (const auto& other : s.fields)
                if (other.id == f.id) return fail(QStringLiteral("trường nhập trùng mã \"%1\"").arg(f.id));
            QString ty = fo.value(QStringLiteral("type")).toString(QStringLiteral("text"));
            if (!types.contains(ty)) return fail(QStringLiteral("trường \"%1\": kiểu \"%2\" không hỗ trợ (text, multiline, number, file, folder, choice)").arg(f.id, ty));
            f.type = ty;
            f.required = fo.value(QStringLiteral("required")).toBool(false);
            if (fo.contains(QStringLiteral("maxLen"))) {
                f.maxLen = fo.value(QStringLiteral("maxLen")).toInt(-1);
                if (f.maxLen < 1 || f.maxLen > 100000) return fail(QStringLiteral("trường \"%1\": maxLen phải từ 1 đến 100000").arg(f.id));
            }
            if (fo.contains(QStringLiteral("min"))) { f.hasMin = true; f.min = fo.value(QStringLiteral("min")).toDouble(); }
            if (fo.contains(QStringLiteral("max"))) { f.hasMax = true; f.max = fo.value(QStringLiteral("max")).toDouble(); }
            if (f.hasMin && f.hasMax && f.min > f.max) return fail(QStringLiteral("trường \"%1\": min lớn hơn max").arg(f.id));
            if (ty == QLatin1String("choice")) {
                if (!fo.value(QStringLiteral("choices")).isArray()) return fail(QStringLiteral("trường \"%1\": kiểu choice cần \"choices\"").arg(f.id));
                for (const auto& c : fo.value(QStringLiteral("choices")).toArray()) {
                    if (!c.isString() || c.toString().isEmpty()) return fail(QStringLiteral("trường \"%1\": \"choices\" chỉ gồm chữ không rỗng").arg(f.id));
                    f.choices << c.toString();
                }
                if (f.choices.isEmpty() || f.choices.size() > 100) return fail(QStringLiteral("trường \"%1\": \"choices\" cần 1–100 giá trị").arg(f.id));
                if (!f.def.isEmpty() && !f.choices.contains(f.def)) return fail(QStringLiteral("trường \"%1\": giá trị mặc định không nằm trong choices").arg(f.id));
            }
            s.fields << f;
        }
    }

    // --- đối số ---
    if (o.contains(QStringLiteral("args"))) {
        if (!o.value(QStringLiteral("args")).isArray()) return fail(QStringLiteral("\"args\" phải là danh sách chữ"));
        const QJsonArray aa = o.value(QStringLiteral("args")).toArray();
        if (aa.size() > 64) return fail(QStringLiteral("tối đa 64 đối số"));
        static const QRegularExpression ph(QStringLiteral("\\{([A-Za-z0-9_]+)\\}"));
        for (int i = 0; i < aa.size(); ++i) {
            if (!aa.at(i).isString()) return fail(QStringLiteral("đối số #%1 phải là chữ").arg(i + 1));
            const QString a = aa.at(i).toString();
            if (a.size() > 1000 || a.contains(QChar(0))) return fail(QStringLiteral("đối số #%1 không hợp lệ").arg(i + 1));
            auto m = ph.globalMatch(a);
            while (m.hasNext()) {
                const QString name = m.next().captured(1);
                bool known = name == QLatin1String("tooldir");
                for (const auto& f : s.fields) known = known || f.id == name;
                if (!known) return fail(QStringLiteral("đối số #%1 dùng {%2} nhưng không có trường nhập nào tên đó").arg(i + 1).arg(name));
            }
            s.args << a;
        }
    }
    s.dir = toolDir;
    r.ok = true;
    r.spec = s;
    return r;
}

ParseResult loadToolDir(const QString& root, const QString& folder) {
    ParseResult r;
    auto fail = [&](const QString& e) { r.ok = false; r.error = e; return r; };
    if (folder.isEmpty() || folder.contains(QLatin1Char('/')) || folder.contains(QLatin1Char('\\')) || folder == QLatin1String("..") || folder == QLatin1String("."))
        return fail(QStringLiteral("tên thư mục không hợp lệ"));
    const QString croot = canonicalOrEmpty(root);
    if (croot.isEmpty()) return fail(QStringLiteral("thư mục tool gốc không tồn tại"));
    const QFileInfo di(QDir(croot).filePath(folder));
    if (di.isSymLink()) return fail(QStringLiteral("thư mục tool là liên kết tượng trưng — không cho phép"));
    if (!di.isDir()) return fail(QStringLiteral("không phải thư mục"));
    const QString cdir = di.canonicalFilePath();
    if (!isInside(cdir + QLatin1Char('/'), croot)) return fail(QStringLiteral("thư mục tool nằm ngoài thư mục tool gốc"));

    QFile mf(QDir(cdir).filePath(QStringLiteral("tool.json")));
    if (!mf.exists()) return fail(QStringLiteral("thiếu tool.json"));
    if (QFileInfo(mf).isSymLink()) return fail(QStringLiteral("tool.json là liên kết tượng trưng — không cho phép"));
    if (mf.size() > kMaxManifestBytes) return fail(QStringLiteral("tool.json lớn hơn 256 KB"));
    if (!mf.open(QIODevice::ReadOnly)) return fail(QStringLiteral("không đọc được tool.json"));
    r = parseManifest(mf.readAll(), cdir);
    mf.close();
    if (!r.ok) return r;

    QString err;
    if (r.spec.command.contains(QLatin1Char('/'))) {
        const QFileInfo ci(QDir(cdir).filePath(r.spec.command));
        if (!ci.exists() || !ci.isFile()) return fail(QStringLiteral("không thấy tệp lệnh \"%1\" trong thư mục tool").arg(r.spec.command));
        if (!isInside(ci.canonicalFilePath(), cdir)) return fail(QStringLiteral("tệp lệnh thoát ra ngoài thư mục tool"));
    }
    if (r.spec.workingDir != QLatin1String(".")) {
        const QFileInfo wi(QDir(cdir).filePath(r.spec.workingDir));
        if (!wi.isDir() || !isInside(wi.canonicalFilePath() + QLatin1Char('/'), cdir)) return fail(QStringLiteral("\"workingDir\" không nằm trong thư mục tool"));
    }
    r.spec.fingerprint = sha256Dir(cdir, &err);
    if (r.spec.fingerprint.isEmpty()) return fail(err);
    return r;
}

QString toolsRoot() {
    const QString d = workspace::dataDir() + QStringLiteral("/tools");
    QDir().mkpath(d);
    return d;
}

ScanResult scan(const QString& root) {
    ScanResult out;
    const QDir d(root);
    const QStringList names = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList seen;
    for (const QString& n : names) {
        const ParseResult r = loadToolDir(root, n);
        if (!r.ok) { out.errors << QStringLiteral("%1: %2").arg(n, r.error); continue; }
        if (seen.contains(r.spec.id)) { out.errors << QStringLiteral("%1: trùng mã tool \"%2\" với thư mục khác — bỏ qua").arg(n, r.spec.id); continue; }
        seen << r.spec.id;
        out.tools << r.spec;
    }
    return out;
}

// ---------------- xác nhận ----------------
bool isConfirmed(const ToolSpec& s) {
    if (s.id.isEmpty() || s.fingerprint.isEmpty()) return false;
    const QJsonObject c = workspace::load(QStringLiteral("tools")).value(QStringLiteral("confirmed")).toObject();
    return c.value(s.id).toString() == s.fingerprint;
}

void setConfirmed(const ToolSpec& s) {
    if (s.id.isEmpty() || s.fingerprint.isEmpty()) return;
    QJsonObject root = workspace::load(QStringLiteral("tools"));
    QJsonObject c = root.value(QStringLiteral("confirmed")).toObject();
    c.insert(s.id, s.fingerprint);
    root.insert(QStringLiteral("confirmed"), c);
    workspace::save(QStringLiteral("tools"), root);
}

void revokeConfirmation(const QString& id) {
    QJsonObject root = workspace::load(QStringLiteral("tools"));
    QJsonObject c = root.value(QStringLiteral("confirmed")).toObject();
    c.remove(id);
    root.insert(QStringLiteral("confirmed"), c);
    workspace::save(QStringLiteral("tools"), root);
}

// ---------------- chuẩn bị lệnh ----------------
QString describeCommand(const ToolSpec& s) {
    QStringList parts{s.command};
    for (const QString& a : s.args) parts << (a.contains(QLatin1Char(' ')) ? QLatin1Char('"') + a + QLatin1Char('"') : a);
    return parts.join(QLatin1Char(' '));
}

Prepared prepare(const ToolSpec& s, const QMap<QString, QString>& values) {
    Prepared p;
    auto fail = [&](const QString& e) { p.ok = false; p.error = e; return p; };
    if (s.dir.isEmpty() || s.fingerprint.isEmpty()) return fail(QStringLiteral("tool chưa được nạp từ thư mục tool"));
    if (!isConfirmed(s)) return fail(QStringLiteral("Tool này chưa được bạn xác nhận cho phép chạy."));
    QString err;
    const QString now = sha256Dir(s.dir, &err);
    if (now.isEmpty()) return fail(QStringLiteral("Không kiểm tra lại được thư mục tool: %1").arg(err));
    if (now != s.fingerprint) return fail(QStringLiteral("Thư mục tool đã thay đổi kể từ lúc quét. Hãy quét lại và xác nhận lại."));

    QMap<QString, QString> val;
    for (const FieldSpec& f : s.fields) {
        QString v = values.contains(f.id) ? values.value(f.id) : f.def;
        if (v.contains(QChar(0))) return fail(QStringLiteral("\"%1\": giá trị có ký tự không hợp lệ").arg(f.label));
        if (v.size() > f.maxLen) return fail(QStringLiteral("\"%1\": dài quá %2 ký tự").arg(f.label).arg(f.maxLen));
        if (f.type == QLatin1String("text") && (v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r'))))
            return fail(QStringLiteral("\"%1\": chỉ nhập một dòng").arg(f.label));
        if (v.trimmed().isEmpty()) {
            if (f.required) return fail(QStringLiteral("\"%1\" là bắt buộc").arg(f.label));
            val[f.id] = QString();
            continue;
        }
        if (f.type == QLatin1String("number")) {
            bool ok = false;
            if (v.contains(QLatin1Char(','))) return fail(QStringLiteral("\"%1\": không phải số (dùng dấu chấm cho phần thập phân, không dùng dấu phẩy)").arg(f.label));
            const double d = QLocale::c().toDouble(v.trimmed(), &ok);
            if (!ok) return fail(QStringLiteral("\"%1\": không phải số (dùng dấu chấm cho phần thập phân)").arg(f.label));
            if ((f.hasMin && d < f.min) || (f.hasMax && d > f.max)) return fail(QStringLiteral("\"%1\": ngoài khoảng cho phép").arg(f.label));
            v = v.trimmed();
        } else if (f.type == QLatin1String("choice")) {
            if (!f.choices.contains(v)) return fail(QStringLiteral("\"%1\": giá trị không nằm trong danh sách").arg(f.label));
        } else if (f.type == QLatin1String("file")) {
            if (!QFileInfo(v).isFile()) return fail(QStringLiteral("\"%1\": không thấy tệp").arg(f.label));
            v = QDir::toNativeSeparators(QFileInfo(v).absoluteFilePath());
        } else if (f.type == QLatin1String("folder")) {
            if (!QFileInfo(v).isDir()) return fail(QStringLiteral("\"%1\": không thấy thư mục").arg(f.label));
            v = QDir::toNativeSeparators(QFileInfo(v).absoluteFilePath());
        }
        val[f.id] = v;
    }

    static const QRegularExpression ph(QStringLiteral("\\{([A-Za-z0-9_]+)\\}"));
    QStringList args;
    for (const QString& tpl : s.args) {
        const auto whole = QRegularExpression(QStringLiteral("^\\{([A-Za-z0-9_]+)\\}$")).match(tpl);
        if (whole.hasMatch() && whole.captured(1) != QLatin1String("tooldir") && val.value(whole.captured(1)).isEmpty())
            continue; // trường tuỳ chọn để trống: bỏ đối số
        QString a;
        int last = 0;
        auto it = ph.globalMatch(tpl);
        while (it.hasNext()) {
            const auto m = it.next();
            a += tpl.mid(last, m.capturedStart() - last);
            const QString name = m.captured(1);
            a += (name == QLatin1String("tooldir")) ? QDir::toNativeSeparators(s.dir) : val.value(name);
            last = m.capturedEnd();
        }
        a += tpl.mid(last);
        args << a;
    }

    QString program;
    if (s.command.contains(QLatin1Char('/'))) {
        const QFileInfo ci(QDir(s.dir).filePath(s.command));
        if (!ci.isFile() || !isInside(ci.canonicalFilePath(), s.dir)) return fail(QStringLiteral("Không thấy tệp lệnh trong thư mục tool."));
        program = ci.canonicalFilePath();
    } else {
        program = QStandardPaths::findExecutable(s.command);
        if (program.isEmpty())
            return fail(QStringLiteral("Máy chưa cài \"%1\" (hoặc chưa nằm trong PATH). Dũng Office không kèm theo chương trình này — hãy tự cài rồi thử lại.").arg(s.command));
    }
    p.cmd.program = program;
    p.cmd.args = args;
    p.cmd.workDir = (s.workingDir == QLatin1String(".")) ? s.dir : QFileInfo(QDir(s.dir).filePath(s.workingDir)).canonicalFilePath();
    p.cmd.timeoutSec = s.timeoutSec;
    p.ok = true;
    return p;
}

// ---------------- chạy ----------------
QString ToolRunner::logPath() {
    const QString d = workspace::dataDir() + QStringLiteral("/logs");
    QDir().mkpath(d);
    return d + QStringLiteral("/tools.log");
}

void ToolRunner::logLine(const QString& s) const {
    const QString p = logPath();
    if (QFileInfo(p).size() > kMaxLogBytes) {
        QFile::remove(p + QStringLiteral(".1"));
        QFile::rename(p, p + QStringLiteral(".1"));
    }
    QFile f(p);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    f.write((QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + s + QLatin1Char('\n')).toUtf8());
}

ToolRunner::ToolRunner(QObject* parent) : QObject(parent) {
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_kill = new QTimer(this);
    m_kill->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_timedOut = true;
        stop();
    });
    connect(m_kill, &QTimer::timeout, this, [this] {
        if (m_proc && m_proc->state() != QProcess::NotRunning) m_proc->kill();
    });
}

ToolRunner::~ToolRunner() {
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
            m_proc->waitForFinished(2000);
        }
    }
}

bool ToolRunner::running() const { return m_proc && m_proc->state() != QProcess::NotRunning; }

bool ToolRunner::start(const Command& cmd, QString* error) {
    if (running()) { if (error) *error = QStringLiteral("Tool đang chạy."); return false; }
    delete m_proc;
    m_proc = new QProcess(this);
    m_timedOut = m_stopped = m_truncated = false;
    m_bytes = 0;
    m_decOut = QStringDecoder(QStringConverter::Utf8);
    m_decErr = QStringDecoder(QStringConverter::Utf8);
    m_proc->setProgram(cmd.program);
    m_proc->setArguments(cmd.args);
    m_proc->setWorkingDirectory(cmd.workDir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8")); // để tool Python in tiếng Việt đúng
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    m_proc->setProcessEnvironment(env);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);

    auto pump = [this](bool isErr) {
        const QByteArray b = isErr ? m_proc->readAllStandardError() : m_proc->readAllStandardOutput();
        if (b.isEmpty()) return;
        m_bytes += b.size();
        if (m_bytes > kMaxOutputBytes) {
            if (!m_truncated) {
                m_truncated = true;
                emit output(QStringLiteral("\n[… đầu ra quá dài, phần còn lại không hiển thị …]\n"), true);
            }
            return;
        }
        const QString t = isErr ? QString(m_decErr(b)) : QString(m_decOut(b));
        if (!t.isEmpty()) emit output(t, isErr);
    };
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [pump] { pump(false); });
    connect(m_proc, &QProcess::readyReadStandardError, this, [pump] { pump(true); });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) { onFinished(code, int(st)); });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            logLine(QStringLiteral("[%1] không khởi động được").arg(m_id));
            m_timeout->stop();
            emit output(QStringLiteral("Không khởi động được chương trình.\n"), true);
            emit finished(-1, false, false, true);
        }
    });

    QStringList shown;
    for (const QString& a : cmd.args) shown << (a.size() > 200 ? a.left(200) + QStringLiteral("…") : a);
    logLine(QStringLiteral("[%1] BẮT ĐẦU %2 %3").arg(m_id, QFileInfo(cmd.program).fileName(), shown.join(QLatin1Char(' '))));
    m_startMs = QDateTime::currentMSecsSinceEpoch();
    m_proc->start();
    if (m_proc->state() != QProcess::NotRunning) m_timeout->start(cmd.timeoutSec * 1000); // nếu khởi động hỏng ngay thì không đặt giờ
    return true;
}

void ToolRunner::stop() {
    if (!running()) return;
    if (!m_timedOut) m_stopped = true;
    m_proc->terminate();
    m_kill->start(2000); // không tắt êm thì buộc dừng
}

void ToolRunner::onFinished(int code, int status) {
    m_timeout->stop();
    m_kill->stop();
    // đọc nốt phần còn lại
    if (m_proc) {
        const QByteArray o = m_proc->readAllStandardOutput(), e = m_proc->readAllStandardError();
        if (!o.isEmpty() && m_bytes <= kMaxOutputBytes) emit output(QString(m_decOut(o)), false);
        if (!e.isEmpty() && m_bytes <= kMaxOutputBytes) emit output(QString(m_decErr(e)), true);
    }
    const bool crashed = status == int(QProcess::CrashExit) && !m_stopped && !m_timedOut;
    logLine(QStringLiteral("[%1] KẾT THÚC mã=%2 %3%4%5 %6ms").arg(m_id).arg(code)
                .arg(m_timedOut ? QStringLiteral("hết-giờ ") : QString(), m_stopped ? QStringLiteral("đã-dừng ") : QString(),
                     crashed ? QStringLiteral("bị-lỗi ") : QString())
                .arg(QDateTime::currentMSecsSinceEpoch() - m_startMs));
    emit finished(code, m_timedOut, m_stopped, crashed);
}

} // namespace toolbox
