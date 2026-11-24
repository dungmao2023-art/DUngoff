// SPDX-License-Identifier: MPL-2.0
#include "FileOps.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <algorithm>

namespace ft {

#ifdef Q_OS_WIN
static constexpr bool kCaseInsensitiveFs = true;
#else
static constexpr bool kCaseInsensitiveFs = false;
#endif

static QString fkey(const QString& s) { return kCaseInsensitiveFs ? s.toLower() : s; }
static QString join(const QString& dir, const QString& name) { return QDir(dir).filePath(name); }

// ---------------- chung ----------------
static bool writeNewFile(const QString& path, const QByteArray& data, QString& err)
{
    if (QFileInfo::exists(path)) { err = QObject::tr("Tệp đích đã tồn tại, không ghi đè: %1").arg(path); return false; }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { err = QObject::tr("Không tạo được tệp: %1").arg(f.errorString()); return false; }
    f.write(data);
    if (!f.commit()) { err = QObject::tr("Không ghi được tệp: %1").arg(f.errorString()); return false; }
    return true;
}

static QString csvCell(QString s)
{
    // chống chèn công thức khi mở bằng Excel: ô bắt đầu bằng = + - @ được thêm dấu nháy
    if (!s.isEmpty() && QStringLiteral("=+-@\t\r").contains(s[0])) s.prepend(QLatin1Char('\''));
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r')))
        s = QLatin1Char('"') + s.replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
    return s;
}
static QByteArray csvBytes(const QStringList& rows)
{
    return QByteArray("\xEF\xBB\xBF") + rows.join(QStringLiteral("\r\n")).toUtf8() + "\r\n";
}

// ---------------- nhật ký ----------------
static bool writeJournal(const QString& path, const QString& kind, const QString& state, const QVector<JournalEntry>& es, QString& err)
{
    QJsonArray arr;
    for (const auto& e : es) arr.append(QJsonObject{{"from", e.from}, {"to", e.to}});
    QJsonObject o{{"version", 1}, {"kind", kind}, {"time", QDateTime::currentDateTime().toString(Qt::ISODate)}, {"state", state}, {"entries", arr}};
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { err = f.errorString(); return false; }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit()) { err = f.errorString(); return false; }
    return true;
}
static QString newJournalPath(const QString& dir, const QString& kind)
{
    QDir().mkpath(dir);
    return join(dir, QStringLiteral("%1-%2-%3.json").arg(kind, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmsszzz")), QUuid::createUuid().toString(QUuid::Id128).left(6)));
}

// Đổi tên không ghi đè; xử lý trường hợp chỉ đổi hoa/thường trên hệ thống không phân biệt hoa/thường.
static bool safeRename(const QString& from, const QString& to, QString& err)
{
    if (QFileInfo::exists(to)) {
        const QFileInfo a(from), b(to);
        const bool same = !a.canonicalFilePath().isEmpty() && a.canonicalFilePath() == b.canonicalFilePath() && from != to;
        if (!same) { err = QObject::tr("đã có mục trùng tên"); return false; }
        const QString tmp = join(a.absolutePath(), QStringLiteral(".dung-tmp-") + QUuid::createUuid().toString(QUuid::Id128));
        QFile f1(from);
        if (!f1.rename(tmp)) { err = f1.errorString(); return false; }
        QFile f2(tmp);
        if (!f2.rename(to)) { err = f2.errorString(); QFile(tmp).rename(from); return false; }
        return true;
    }
    QFile f(from);
    if (!f.rename(to)) { err = f.errorString().isEmpty() ? QObject::tr("không đổi được tên") : f.errorString(); return false; }
    return true;
}

OpResult undoJournal(const QString& journalPath)
{
    OpResult r;
    QFile jf(journalPath);
    if (!jf.open(QIODevice::ReadOnly)) { r.error = QObject::tr("Không đọc được nhật ký: %1").arg(jf.errorString()); return r; }
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(jf.readAll(), &pe);
    jf.close();
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) { r.error = QObject::tr("Nhật ký hỏng, không hoàn tác được."); return r; }
    QVector<JournalEntry> es;
    for (const auto& v : doc.object().value("entries").toArray()) es.push_back({v.toObject().value("from").toString(), v.toObject().value("to").toString()});
    if (es.isEmpty()) { r.error = QObject::tr("Nhật ký không có mục nào."); return r; }
    // Hoàn tác 2 pha (giống lúc áp dụng) để hoán đổi/chuỗi đổi tên vẫn đúng: đưa tất cả `to` ra tên tạm,
    // rồi đặt về `from`. Mục nào `to` không còn hoặc `from` đã bị tệp khác chiếm thì bỏ qua và BÁO, không ghi đè.
    int skipped = 0;
    struct Step { int idx; QString tmp; };
    QVector<Step> held;
    for (int i = int(es.size()) - 1; i >= 0; --i) {
        const auto& e = es[i];
        if (!QFileInfo::exists(e.to)) { r.warnings << QObject::tr("Không thấy '%1' — bỏ qua.").arg(e.to); ++skipped; continue; }
        const QString tmp = join(QFileInfo(e.to).absolutePath(), QStringLiteral(".dung-tmp-") + QUuid::createUuid().toString(QUuid::Id128));
        QString err;
        if (!safeRename(e.to, tmp, err)) { r.warnings << QObject::tr("Không đưa '%1' về '%2' (%3) — bỏ qua.").arg(e.to, e.from, err); ++skipped; continue; }
        held.push_back({i, tmp});
    }
    for (const Step& st : held) {
        const auto& e = es[st.idx];
        QString err;
        if (QFileInfo::exists(e.from)) {
            r.warnings << QObject::tr("Không đưa '%1' về '%2' (đã có mục khác ở đó) — giữ nguyên tên mới.").arg(e.to, e.from);
            ++skipped;
            if (!safeRename(st.tmp, e.to, err)) r.warnings << QObject::tr("Không đặt lại được '%1': %2 (tệp đang nằm ở '%3')").arg(e.to, err, st.tmp);
            continue;
        }
        QDir().mkpath(QFileInfo(e.from).absolutePath());
        if (!safeRename(st.tmp, e.from, err)) {
            r.warnings << QObject::tr("Không đưa '%1' về '%2' (%3) — bỏ qua.").arg(e.to, e.from, err);
            ++skipped;
            if (!safeRename(st.tmp, e.to, err)) r.warnings << QObject::tr("Không đặt lại được '%1': %2 (tệp đang nằm ở '%3')").arg(e.to, err, st.tmp);
            continue;
        }
        ++r.done;
    }
    r.ok = skipped == 0;
    if (!r.ok) r.error = QObject::tr("Hoàn tác chưa đầy đủ: %1 mục bị bỏ qua.").arg(skipped);
    QString e2;
    writeJournal(journalPath, doc.object().value("kind").toString(), r.ok ? "undone" : "undone-partial", es, e2);
    r.journalPath = journalPath;
    return r;
}

// ---------------- đổi tên ----------------
QString removeVietnameseDiacritics(const QString& s)
{
    QString t = s;
    t.replace(QChar(0x0111), QLatin1Char('d')).replace(QChar(0x0110), QLatin1Char('D'));
    const QString d = t.normalized(QString::NormalizationForm_D);
    QString out;
    for (QChar c : d) if (c.category() != QChar::Mark_NonSpacing) out += c;
    return out.normalized(QString::NormalizationForm_C);
}

bool RenamePlan::hasProblems() const
{
    for (const auto& i : items) if (i.status == RenameStatus::Conflict || i.status == RenameStatus::Invalid) return true;
    return false;
}
int RenamePlan::changeCount() const
{
    int n = 0;
    for (const auto& i : items) if (i.status == RenameStatus::Ok) ++n;
    return n;
}

static QString titleCase(const QString& s)
{
    QString out; bool up = true;
    for (QChar c : s) {
        if (c.isSpace() || c == QLatin1Char('_') || c == QLatin1Char('-')) { up = true; out += c; }
        else { out += up ? c.toUpper() : c.toLower(); up = false; }
    }
    return out;
}

static QString validateName(const QString& name, bool hasExt)
{
    if (name.isEmpty()) return QObject::tr("tên trống");
    if (name.size() > 255) return QObject::tr("tên dài quá 255 ký tự");
    for (QChar c : name) {
        if (c.unicode() < 32 || QStringLiteral("\\/:*?\"<>|").contains(c)) return QObject::tr("chứa ký tự không hợp lệ ( \\ / : * ? \" < > | )");
    }
    if (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) return QObject::tr("không được kết thúc bằng dấu chấm hoặc khoảng trắng");
    if (name.startsWith(QLatin1Char(' '))) return QObject::tr("không được bắt đầu bằng khoảng trắng");
    static const QRegularExpression reserved(QStringLiteral("^(con|prn|aux|nul|com[1-9]|lpt[1-9])$"), QRegularExpression::CaseInsensitiveOption);
    Q_UNUSED(hasExt)
    const QString stem = name.left(name.indexOf(QLatin1Char('.'))); // CON.txt cũng bị Windows coi là tên dành riêng
    if (reserved.match(stem).hasMatch()) return QObject::tr("trùng tên dành riêng của Windows (%1)").arg(stem);
    return QString();
}

RenamePlan planRename(const QStringList& paths, const RenameOptions& o)
{
    RenamePlan plan;
    int idx = 0;
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        RenameItem it;
        it.dir = fi.absolutePath();
        it.oldName = fi.fileName();
        const bool useExt = o.keepExtension && fi.isFile() && !fi.suffix().isEmpty();
        const QString base = useExt ? fi.completeBaseName() : fi.fileName();
        const QString ext = useExt ? fi.suffix() : QString();
        QString nb = o.pattern.isEmpty() ? QStringLiteral("{name}") : o.pattern;
        const qint64 n = o.start + qint64(idx) * o.step;
        nb.replace(QStringLiteral("{name}"), base);
        nb.replace(QStringLiteral("{n}"), QStringLiteral("%1").arg(n, qMax(0, o.pad), 10, QLatin1Char('0')));
        nb.replace(QStringLiteral("{date}"), fi.lastModified().toString(QStringLiteral("yyyy-MM-dd")));
        nb.replace(QStringLiteral("{today}"), QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
        nb.replace(QStringLiteral("{parent}"), QFileInfo(it.dir).fileName());
        nb.replace(QStringLiteral("{ext}"), fi.suffix());
        if (!o.find.isEmpty()) nb.replace(o.find, o.replace, o.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        if (o.removeDiacritics) nb = removeVietnameseDiacritics(nb);
        if (o.normalizeSpaces) nb = nb.simplified();
        if (o.caseMode == CaseMode::Lower) nb = nb.toLower();
        else if (o.caseMode == CaseMode::Upper) nb = nb.toUpper();
        else if (o.caseMode == CaseMode::Title) nb = titleCase(nb);
        it.newName = useExt ? nb + QLatin1Char('.') + ext : nb;
        if (it.newName == it.oldName) it.status = RenameStatus::Unchanged;
        else {
            const QString bad = validateName(it.newName, useExt);
            if (!bad.isEmpty()) { it.status = RenameStatus::Invalid; it.note = bad; }
        }
        plan.items.push_back(it);
        ++idx;
    }
    // xung đột: trùng tên mới giữa các mục, hoặc trùng mục đang có mà không bị đổi đi
    QMap<QString, QSet<QString>> movingAway; // dir → tên cũ (khoá hoá) của mục sẽ đổi
    QMap<QString, int> newCount;             // dir|tên mới → số lần
    for (const auto& it : plan.items) {
        if (it.status != RenameStatus::Ok) continue;
        movingAway[fkey(it.dir)].insert(fkey(it.oldName));
        newCount[fkey(it.dir) + QLatin1Char('|') + it.newName.toLower()]++;
    }
    for (auto& it : plan.items) {
        if (it.status != RenameStatus::Ok) continue;
        if (newCount[fkey(it.dir) + QLatin1Char('|') + it.newName.toLower()] > 1) {
            it.status = RenameStatus::Conflict; it.note = QObject::tr("trùng tên mới với một mục khác trong danh sách"); continue;
        }
        const QString target = join(it.dir, it.newName);
        const bool caseOnly = kCaseInsensitiveFs && it.newName.compare(it.oldName, Qt::CaseInsensitive) == 0;
        if (!caseOnly && QFileInfo::exists(target) && !movingAway[fkey(it.dir)].contains(fkey(it.newName))) {
            it.status = RenameStatus::Conflict; it.note = QObject::tr("đã có mục cùng tên trong thư mục");
        }
    }
    return plan;
}

OpResult applyRename(const RenamePlan& plan, const QString& journalDir)
{
    OpResult r;
    if (plan.hasProblems()) { r.error = QObject::tr("Kế hoạch còn xung đột/tên không hợp lệ — không đổi gì."); return r; }
    QVector<JournalEntry> es;
    for (const auto& it : plan.items) if (it.status == RenameStatus::Ok) es.push_back({join(it.dir, it.oldName), join(it.dir, it.newName)});
    if (es.isEmpty()) { r.ok = true; r.warnings << QObject::tr("Không có tên nào thay đổi."); return r; }
    // kiểm tra lại trạng thái đĩa ngay trước khi đổi
    for (const auto& e : es) if (!QFileInfo::exists(e.from)) { r.error = QObject::tr("Tệp đã thay đổi từ lúc xem trước: không thấy '%1'.").arg(e.from); return r; }

    r.journalPath = newJournalPath(journalDir, QStringLiteral("rename"));
    QString jerr;
    if (!writeJournal(r.journalPath, QStringLiteral("rename"), QStringLiteral("pending"), es, jerr)) {
        r.error = QObject::tr("Không ghi được nhật ký hoàn tác (%1) — không đổi gì.").arg(jerr); r.journalPath.clear(); return r;
    }
    QVector<QString> tmp(es.size());
    int p1 = 0, p2 = 0;
    auto rollback = [&]() {
        bool clean = true;
        for (int i = p2 - 1; i >= 0; --i) { QString e; if (!safeRename(es[i].to, tmp[i], e)) { clean = false; r.warnings << QObject::tr("Không quay lại được '%1': %2").arg(es[i].to, e); } }
        for (int i = p1 - 1; i >= 0; --i) { QString e; if (!safeRename(tmp[i], es[i].from, e)) { clean = false; r.warnings << QObject::tr("Không quay lại được '%1': %2").arg(tmp[i], e); } }
        return clean;
    };
    for (int i = 0; i < es.size(); ++i) {
        tmp[i] = join(QFileInfo(es[i].from).absolutePath(), QStringLiteral(".dung-tmp-") + QUuid::createUuid().toString(QUuid::Id128));
        QString e;
        if (!safeRename(es[i].from, tmp[i], e)) {
            r.error = QObject::tr("Không đổi được '%1': %2 (tệp có thể đang được mở/khoá). Đã khôi phục tên cũ.").arg(es[i].from, e);
            if (rollback()) QFile::remove(r.journalPath); else r.error += QObject::tr(" Một số mục chưa khôi phục được — xem nhật ký.");
            if (QFileInfo::exists(r.journalPath)) {} else r.journalPath.clear();
            return r;
        }
        ++p1;
    }
    for (int i = 0; i < es.size(); ++i) {
        QString e;
        if (!safeRename(tmp[i], es[i].to, e)) {
            r.error = QObject::tr("Không đặt được tên '%1': %2. Đã khôi phục tên cũ.").arg(es[i].to, e);
            if (rollback()) { QFile::remove(r.journalPath); r.journalPath.clear(); } else r.error += QObject::tr(" Một số mục chưa khôi phục được — xem nhật ký.");
            return r;
        }
        ++p2;
    }
    writeJournal(r.journalPath, QStringLiteral("rename"), QStringLiteral("done"), es, jerr);
    r.ok = true; r.done = es.size();
    return r;
}

// ---------------- tìm tệp trùng ----------------
static bool hashFile(const QString& path, QByteArray& out, QString& err, qint64& bytesRead, const std::function<bool()>& keepGoing)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { err = f.errorString(); return false; }
    QCryptographicHash h(QCryptographicHash::Sha256);
    QByteArray buf(1 << 20, 0);
    for (;;) {
        const qint64 n = f.read(buf.data(), buf.size());
        if (n < 0) { err = f.errorString(); return false; }
        if (n == 0) break;
        h.addData(QByteArrayView(buf.constData(), n));
        bytesRead += n;
        if (!keepGoing()) { err = QStringLiteral("cancel"); return false; }
    }
    out = h.result().toHex();
    return true;
}

static QStringList listFiles(const QString& root, bool recursive, QStringList* skipped)
{
    QStringList out;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Dirs,
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        const QString p = it.next();
        const QFileInfo fi = it.fileInfo();
        if (fi.isDir()) continue; // thư mục chỉ để duyệt
        const QString rel = QDir::fromNativeSeparators(QDir(root).relativeFilePath(p));
        if (rel.startsWith(QStringLiteral("_to_delete/")) || rel.contains(QStringLiteral("/_to_delete/"))) continue; // bản đã chuyển trước đó
        out << p;
    }
    Q_UNUSED(skipped)
    return out;
}

DupResult findDuplicates(const QStringList& roots, const DupOptions& o, const Progress& prog)
{
    DupResult r;
    QMap<qint64, QStringList> bySize;
    QSet<QString> seen;
    for (const QString& root : roots) {
        for (const QString& p : listFiles(root, o.recursive, &r.skipped)) {
            const QFileInfo fi(p);
            const QString canon = fi.canonicalFilePath();
            const QString key = canon.isEmpty() ? p : canon;
            if (seen.contains(key)) continue;
            seen.insert(key);
            if (fi.size() < o.minSize) continue;
            bySize[fi.size()].append(fi.absoluteFilePath());
        }
    }
    qint64 total = 0;
    for (auto it = bySize.begin(); it != bySize.end();) { if (it.value().size() < 2) it = bySize.erase(it); else { total += it.key() * it.value().size(); ++it; } }
    qint64 done = 0;
    bool stop = false;
    auto keep = [&]() { if (prog && !prog(done, total)) { stop = true; return false; } return true; };
    for (auto it = bySize.begin(); it != bySize.end() && !stop; ++it) {
        QMap<QByteArray, QStringList> byHash;
        for (const QString& p : it.value()) {
            QByteArray h; QString err; qint64 rd = 0;
            auto k2 = [&]() { done += 0; return keep(); };
            const bool ok = hashFile(p, h, err, rd, k2);
            done += rd;
            if (!ok) {
                if (stop) break;
                r.skipped << QStringLiteral("%1: %2").arg(p, err);
                continue;
            }
            byHash[h].append(p);
        }
        if (stop) break;
        for (auto h = byHash.begin(); h != byHash.end(); ++h)
            if (h.value().size() >= 2) {
                DupGroup g; g.size = it.key(); g.sha256 = QString::fromLatin1(h.key()); g.files = h.value(); g.files.sort();
                r.groups.push_back(g);
                r.wastedBytes += g.size * (g.files.size() - 1);
            }
    }
    if (stop) { r.cancelled = true; r.groups.clear(); r.wastedBytes = 0; }
    std::sort(r.groups.begin(), r.groups.end(), [](const DupGroup& a, const DupGroup& b) { return a.size * a.files.size() > b.size * b.files.size(); });
    return r;
}

OpResult moveDuplicates(const DupResult& r, const QStringList& selected, const QString& destDir, const QString& journalDir)
{
    OpResult res;
    if (selected.isEmpty()) { res.error = QObject::tr("Chưa chọn tệp nào."); return res; }
    QSet<QString> sel;
    for (const QString& s : selected) sel.insert(QFileInfo(s).absoluteFilePath());
    QSet<QString> known;
    for (const auto& g : r.groups) {
        int c = 0;
        for (const QString& f : g.files) { known.insert(f); if (sel.contains(f)) ++c; }
        if (c == g.files.size()) { res.error = QObject::tr("Không thể chuyển TẤT CẢ các bản của một nhóm (phải giữ lại ít nhất một): %1").arg(g.files.value(0)); return res; }
    }
    for (const QString& s : sel) if (!known.contains(s)) { res.error = QObject::tr("'%1' không nằm trong kết quả quét.").arg(s); return res; }
    // kiểm tra tệp chưa đổi từ lúc quét
    for (const auto& g : r.groups) for (const QString& f : g.files) if (sel.contains(f)) {
        const QFileInfo fi(f);
        if (!fi.exists() || fi.size() != g.size) { res.error = QObject::tr("Tệp đã thay đổi từ lúc quét: %1 — hãy quét lại.").arg(f); return res; }
    }
    if (!QDir().mkpath(destDir)) { res.error = QObject::tr("Không tạo được thư mục đích: %1").arg(destDir); return res; }
    QVector<JournalEntry> es;
    QSet<QString> used;
    for (const QString& f : sel) {
        const QFileInfo fi(f);
        QString name = fi.fileName(), cand = name;
        for (int i = 2; QFileInfo::exists(join(destDir, cand)) || used.contains(fkey(cand)); ++i)
            cand = fi.completeBaseName() + QStringLiteral(" (%1)").arg(i) + (fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix());
        used.insert(fkey(cand));
        es.push_back({f, join(destDir, cand)});
    }
    res.journalPath = newJournalPath(journalDir, QStringLiteral("move"));
    QString jerr;
    if (!writeJournal(res.journalPath, QStringLiteral("move"), QStringLiteral("pending"), es, jerr)) {
        res.error = QObject::tr("Không ghi được nhật ký hoàn tác (%1) — không chuyển gì.").arg(jerr); res.journalPath.clear(); return res;
    }
    int done = 0;
    for (const auto& e : es) {
        QString err;
        if (!safeRename(e.from, e.to, err)) {
            res.error = QObject::tr("Không chuyển được '%1': %2. (Thư mục đích cần cùng ổ đĩa với tệp.) Đã khôi phục những tệp đã chuyển.").arg(e.from, err);
            for (int i = done - 1; i >= 0; --i) { QString e2; if (!safeRename(es[i].to, es[i].from, e2)) res.warnings << QObject::tr("Không khôi phục được '%1': %2").arg(es[i].to, e2); }
            if (res.warnings.isEmpty()) { QFile::remove(res.journalPath); res.journalPath.clear(); }
            return res;
        }
        ++done;
    }
    writeJournal(res.journalPath, QStringLiteral("move"), QStringLiteral("done"), es, jerr);
    res.ok = true; res.done = done;
    return res;
}

// ---------------- so sánh thư mục ----------------
struct FInfo { QString abs; qint64 size = 0; QDateTime mtime; QString rel; };

static QMap<QString, FInfo> scanTree(const QString& root)
{
    QMap<QString, FInfo> m;
    const QDir rd(root);
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        const QFileInfo fi = it.fileInfo();
        FInfo f; f.abs = p; f.size = fi.size(); f.mtime = fi.lastModified(); f.rel = QDir::fromNativeSeparators(rd.relativeFilePath(p));
        m.insert(fkey(f.rel), f);
    }
    return m;
}

QString diffStateText(DiffState s)
{
    switch (s) {
    case DiffState::Same: return QObject::tr("Giống nhau");
    case DiffState::OnlyA: return QObject::tr("Chỉ có ở A");
    case DiffState::OnlyB: return QObject::tr("Chỉ có ở B");
    case DiffState::Different: return QObject::tr("Khác nhau");
    case DiffState::NewerA: return QObject::tr("A mới hơn");
    case DiffState::NewerB: return QObject::tr("B mới hơn");
    }
    return QString();
}

CompareResult compareFolders(const QString& a, const QString& b, const CompareOptions& o, const Progress& prog)
{
    CompareResult r;
    const auto A = scanTree(a), B = scanTree(b);
    QSet<QString> keys;
    for (auto it = A.begin(); it != A.end(); ++it) keys.insert(it.key());
    for (auto it = B.begin(); it != B.end(); ++it) keys.insert(it.key());
    QStringList sorted = keys.values();
    sorted.sort();
    qint64 n = 0;
    for (const QString& k : sorted) {
        if (prog && !prog(n++, sorted.size())) { r.cancelled = true; r.entries.clear(); return r; }
        DiffEntry e;
        const bool ia = A.contains(k), ib = B.contains(k);
        if (ia) { e.rel = A[k].rel; e.sizeA = A[k].size; e.timeA = A[k].mtime; }
        if (ib) { if (e.rel.isEmpty()) e.rel = B[k].rel; e.sizeB = B[k].size; e.timeB = B[k].mtime; }
        if (ia && !ib) e.state = DiffState::OnlyA;
        else if (!ia && ib) e.state = DiffState::OnlyB;
        else {
            const qint64 dt = e.timeA.msecsTo(e.timeB); // >0: B mới hơn
            bool sameContent;
            if (e.sizeA != e.sizeB) sameContent = false;
            else if (o.compareContent) {
                QByteArray h1, h2; QString err; qint64 rd = 0;
                const bool ok1 = hashFile(A[k].abs, h1, err, rd, []() { return true; });
                const bool ok2 = hashFile(B[k].abs, h2, err, rd, []() { return true; });
                if (!ok1 || !ok2) { r.skipped << QStringLiteral("%1: %2").arg(e.rel, err); sameContent = false; }
                else sameContent = (h1 == h2);
            } else sameContent = qAbs(dt) <= 2000;
            if (sameContent) e.state = DiffState::Same;
            else if (dt > 2000) e.state = DiffState::NewerB;
            else if (dt < -2000) e.state = DiffState::NewerA;
            else e.state = DiffState::Different;
        }
        r.entries.push_back(e);
    }
    return r;
}

OpResult exportCompareCsv(const QString& path, const CompareResult& r, const QString& a, const QString& b)
{
    OpResult res;
    QStringList rows;
    rows << QStringLiteral("%1,%2").arg(csvCell(QObject::tr("Thư mục A")), csvCell(a));
    rows << QStringLiteral("%1,%2").arg(csvCell(QObject::tr("Thư mục B")), csvCell(b));
    rows << QObject::tr("Đường dẫn tương đối,Trạng thái,Kích thước A,Kích thước B,Sửa lần cuối A,Sửa lần cuối B");
    auto ts = [](const QDateTime& t) { return t.isValid() ? t.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString(); };
    for (const auto& e : r.entries)
        rows << QStringList{csvCell(e.rel), csvCell(diffStateText(e.state)), e.sizeA >= 0 ? QString::number(e.sizeA) : QString(),
                            e.sizeB >= 0 ? QString::number(e.sizeB) : QString(), ts(e.timeA), ts(e.timeB)}.join(QLatin1Char(','));
    res.ok = writeNewFile(path, csvBytes(rows), res.error);
    if (res.ok) res.done = r.entries.size();
    return res;
}

// ---------------- dung lượng ----------------
QString humanSize(qint64 bytes)
{
    static const char* u[] = {"B", "KB", "MB", "GB", "TB"};
    double v = double(bytes);
    int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return i == 0 ? QStringLiteral("%1 B").arg(bytes) : QStringLiteral("%1 %2").arg(v, 0, 'f', 1).arg(QLatin1String(u[i]));
}

UsageResult diskUsage(const QString& root, int topFiles, const Progress& prog)
{
    UsageResult r;
    const QDir rd(root);
    qint64 counter = 0;
    bool stop = false;
    auto pushLargest = [&](const QFileInfo& fi) {
        UsageNode n; n.name = fi.fileName(); n.path = fi.absoluteFilePath(); n.size = fi.size(); n.files = 1;
        r.largestFiles.push_back(n);
        if (r.largestFiles.size() > topFiles * 2 + 16) {
            std::sort(r.largestFiles.begin(), r.largestFiles.end(), [](const UsageNode& x, const UsageNode& y) { return x.size > y.size; });
            r.largestFiles.resize(topFiles);
        }
    };
    const auto entries = rd.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::NoSymLinks);
    for (const QFileInfo& e : entries) {
        if (stop) break;
        UsageNode n; n.name = e.fileName(); n.path = e.absoluteFilePath(); n.isDir = e.isDir();
        if (e.isFile()) { n.size = e.size(); n.files = 1; pushLargest(e); }
        else if (e.isDir()) {
            QDirIterator it(e.absoluteFilePath(), QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                const QFileInfo fi = it.fileInfo();
                n.size += fi.size(); ++n.files; pushLargest(fi);
                if (prog && (++counter % 200 == 0) && !prog(counter, 0)) { stop = true; break; }
            }
        }
        r.total += n.size; r.totalFiles += n.files;
        r.children.push_back(n);
    }
    std::sort(r.children.begin(), r.children.end(), [](const UsageNode& x, const UsageNode& y) { return x.size > y.size; });
    std::sort(r.largestFiles.begin(), r.largestFiles.end(), [](const UsageNode& x, const UsageNode& y) { return x.size > y.size; });
    if (r.largestFiles.size() > topFiles) r.largestFiles.resize(topFiles);
    r.cancelled = stop;
    return r;
}

// ---------------- liên kết ----------------
QStringList extractUrls(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("(?:https?://|www\\.)[^\\s<>\"'`]+"), QRegularExpression::CaseInsensitiveOption);
    QStringList out;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        QString u = it.next().captured(0);
        for (;;) {
            if (u.isEmpty()) break;
            const QChar c = u.back();
            if (QStringLiteral(".,;:!?]}>»”’").contains(c)) u.chop(1);
            else if (c == QLatin1Char(')') && u.count(QLatin1Char(')')) > u.count(QLatin1Char('('))) u.chop(1);
            else break;
        }
        if (u.startsWith(QLatin1String("www."), Qt::CaseInsensitive)) u.prepend(QStringLiteral("https://"));
        if (u.size() > 8) out << u;
    }
    return out;
}

static bool isTrackingParam(const QString& keyDecoded)
{
    const QString k = keyDecoded.toLower();
    if (k.startsWith(QLatin1String("utm_"))) return true;
    static const QSet<QString> s{"fbclid", "gclid", "dclid", "gbraid", "wbraid", "msclkid", "yclid", "mc_cid", "mc_eid", "igshid", "igsh",
                                 "_ga", "_gl", "ref_src", "ref_url", "vero_id", "mkt_tok", "oly_enc_id", "oly_anon_id", "twclid", "ttclid", "li_fat_id"};
    return s.contains(k);
}

QString cleanUrl(const QString& url, const LinkOptions& o, QStringList* removed)
{
    QString u = url.trimmed();
    static const QRegularExpression head(QStringLiteral("^(https?)://([^/?#]*)(.*)$"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const auto m = head.match(u);
    if (!m.hasMatch() || m.captured(2).isEmpty()) return url;
    QString scheme = m.captured(1).toLower(), auth = m.captured(2), rest = m.captured(3);
    if (o.lowerHost) {
        const int at = auth.lastIndexOf(QLatin1Char('@'));
        auth = auth.left(at + 1) + auth.mid(at + 1).toLower();
    }
    QString frag;
    const int h = rest.indexOf(QLatin1Char('#'));
    if (h >= 0) { frag = rest.mid(h); rest = rest.left(h); }
    QString path = rest, query;
    const int q = rest.indexOf(QLatin1Char('?'));
    if (q >= 0) { path = rest.left(q); query = rest.mid(q + 1); }
    if (o.stripTracking && !query.isEmpty()) {
        QStringList keep;
        for (const QString& kv : query.split(QLatin1Char('&'), Qt::SkipEmptyParts)) {
            const QString key = QUrl::fromPercentEncoding(kv.left(kv.indexOf(QLatin1Char('=')) < 0 ? kv.size() : kv.indexOf(QLatin1Char('='))).toUtf8());
            if (isTrackingParam(key)) { if (removed) removed->append(key); } else keep << kv;
        }
        query = keep.join(QLatin1Char('&'));
    }
    if (o.dropFragment) frag.clear();
    return scheme + QStringLiteral("://") + auth + path + (query.isEmpty() ? QString() : QLatin1Char('?') + query) + frag;
}

LinkClean cleanLinks(const QStringList& in, const LinkOptions& o)
{
    LinkClean r;
    QSet<QString> seen;
    for (const QString& line : in) {
        const QString t = line.trimmed();
        if (t.isEmpty()) continue;
        if (!t.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) && !t.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
            r.warnings << QObject::tr("Bỏ qua (không phải liên kết http/https): %1").arg(t.left(80)); continue;
        }
        const QString c = cleanUrl(t, o);
        if (c != t) ++r.cleaned;
        if (seen.contains(c)) { ++r.duplicates; continue; }
        seen.insert(c);
        r.urls << c;
    }
    return r;
}

OpResult exportLinks(const QString& path, const QStringList& urls)
{
    OpResult r;
    QByteArray data;
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        QStringList rows{QStringLiteral("URL")};
        for (const QString& u : urls) rows << csvCell(u);
        data = csvBytes(rows);
    } else data = urls.join(QLatin1Char('\n')).toUtf8() + "\n";
    r.ok = writeNewFile(path, data, r.error);
    if (r.ok) r.done = urls.size();
    return r;
}


// ---------------- xuất CSV dùng chung ----------------
OpResult exportCsvRows(const QString& path, const QStringList& header, const QVector<QStringList>& rows)
{
    OpResult r;
    QStringList lines;
    QStringList h;
    for (const QString& c : header) h << csvCell(c);
    lines << h.join(QLatin1Char(','));
    for (const auto& row : rows) {
        QStringList cs;
        for (const QString& c : row) cs << csvCell(c);
        lines << cs.join(QLatin1Char(','));
    }
    r.ok = writeNewFile(path, csvBytes(lines), r.error);
    if (r.ok) r.done = rows.size();
    return r;
}

// ---------------- sắp xếp vào thư mục ----------------
QString fileCategory(const QString& suffix)
{
    static const QMap<QString, QStringList> cats = {
        {QStringLiteral("Tài liệu"), {"doc", "docx", "docm", "dot", "dotx", "odt", "rtf", "txt", "md", "pdf"}},
        {QStringLiteral("Bảng tính"), {"xls", "xlsx", "xlsm", "xlsb", "xlt", "xltx", "ods", "csv", "tsv"}},
        {QStringLiteral("Trình chiếu"), {"ppt", "pptx", "pptm", "pps", "ppsx", "odp"}},
        {QStringLiteral("Ảnh"), {"jpg", "jpeg", "png", "gif", "bmp", "webp", "tif", "tiff", "heic", "svg"}},
        {QStringLiteral("Âm thanh"), {"mp3", "wav", "m4a", "flac", "ogg", "wma"}},
        {QStringLiteral("Video"), {"mp4", "mkv", "avi", "mov", "wmv", "flv"}},
        {QStringLiteral("Nén"), {"zip", "rar", "7z", "tar", "gz", "bz2"}},
        {QStringLiteral("Chương trình"), {"exe", "msi", "bat", "cmd", "ps1"}},
    };
    const QString s = suffix.toLower();
    for (auto it = cats.begin(); it != cats.end(); ++it) if (it.value().contains(s)) return it.key();
    return QStringLiteral("Khác");
}

static QString sanitizeFolderName(QString s)
{
    for (const QChar c : QStringLiteral("<>:\"/\\|?*")) s.replace(c, QLatin1Char('_'));
    s = s.trimmed();
    while (s.endsWith(QLatin1Char('.'))) s.chop(1);
    return s.isEmpty() ? QStringLiteral("Khác") : s;
}

OrganizePlan planOrganize(const QString& srcDir, bool recursive, OrganizeMode m, const QString& destRoot)
{
    OrganizePlan plan;
    const QString srcAbs = QDir(srcDir).absolutePath();
    const QString destAbs = QDir(destRoot.isEmpty() ? srcDir : destRoot).absolutePath();
    QStringList files;
    if (recursive) {
        QDirIterator it(srcAbs, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) files << it.next();
    } else {
        for (const QFileInfo& fi : QDir(srcAbs).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name)) files << fi.absoluteFilePath();
    }
    std::sort(files.begin(), files.end());
    QSet<QString> used;
    const QString destPrefix = fkey(destAbs) + QLatin1Char('/');
    for (const QString& f : files) {
        const QFileInfo fi(f);
        if (recursive && fkey(destAbs) != fkey(srcAbs) && fkey(fi.absoluteFilePath()).startsWith(destPrefix)) { continue; }   // đã nằm trong thư mục đích
        if (fi.fileName().startsWith(QLatin1Char('.')) || fi.fileName().startsWith(QLatin1String("~$")) || fi.fileName().contains(QLatin1String(".dung-tmp-"))) {
            plan.skipped << QObject::tr("%1: tệp ẩn/tạm — bỏ qua").arg(f); continue;
        }
        QString sub;
        const QDateTime t = fi.lastModified();
        switch (m) {
        case OrganizeMode::ByCategory: sub = fileCategory(fi.suffix()); break;
        case OrganizeMode::ByExtension: sub = fi.suffix().isEmpty() ? QStringLiteral("Không đuôi") : fi.suffix().toUpper(); break;
        case OrganizeMode::ByYear: sub = t.toString(QStringLiteral("yyyy")); break;
        case OrganizeMode::ByYearMonth: sub = t.toString(QStringLiteral("yyyy/MM")); break;
        }
        QStringList parts = sub.split(QLatin1Char('/'));
        for (QString& p : parts) p = sanitizeFolderName(p);
        const QString dir = destAbs + QLatin1Char('/') + parts.join(QLatin1Char('/'));
        if (fkey(fi.absolutePath()) == fkey(dir)) { plan.skipped << QObject::tr("%1: đã đúng thư mục").arg(f); continue; }
        QString cand = fi.fileName();
        QString note;
        for (int i = 2; QFileInfo::exists(join(dir, cand)) || used.contains(fkey(join(dir, cand))); ++i) {
            cand = fi.completeBaseName() + QStringLiteral(" (%1)").arg(i) + (fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix());
            note = QObject::tr("trùng tên ở đích → thêm số");
        }
        const QString to = join(dir, cand);
#ifdef Q_OS_WIN
        if (QDir::toNativeSeparators(to).size() >= kWinMaxPath) { plan.skipped << QObject::tr("%1: đường dẫn mới quá dài (%2 ký tự)").arg(f).arg(to.size()); continue; }
#endif
        used.insert(fkey(to));
        if (!note.isEmpty()) ++plan.renamedForConflict;
        plan.items.push_back({f, to, note});
    }
    return plan;
}

OpResult applyOrganize(const OrganizePlan& plan, const QString& journalDir)
{
    OpResult r;
    if (plan.items.isEmpty()) { r.ok = true; r.warnings << QObject::tr("Không có tệp nào cần chuyển."); return r; }
    for (const auto& it : plan.items) {
        if (!QFileInfo::exists(it.from)) { r.error = QObject::tr("Tệp đã thay đổi từ lúc xem trước: không thấy '%1'.").arg(it.from); return r; }
        if (QFileInfo::exists(it.to)) { r.error = QObject::tr("Đích đã có tệp '%1' — hãy xem trước lại.").arg(it.to); return r; }
    }
    QVector<JournalEntry> es;
    for (const auto& it : plan.items) es.push_back({it.from, it.to});
    r.journalPath = newJournalPath(journalDir, QStringLiteral("organize"));
    QString jerr;
    if (!writeJournal(r.journalPath, QStringLiteral("organize"), QStringLiteral("pending"), es, jerr)) {
        r.error = QObject::tr("Không ghi được nhật ký hoàn tác (%1) — không chuyển gì.").arg(jerr); r.journalPath.clear(); return r;
    }
    int done = 0;
    for (const auto& e : es) {
        QString err;
        if (!QDir().mkpath(QFileInfo(e.to).absolutePath())) err = QObject::tr("không tạo được thư mục đích");
        if (err.isEmpty() && !safeRename(e.from, e.to, err)) {}
        if (!err.isEmpty()) {
            r.error = QObject::tr("Không chuyển được '%1': %2. Đã khôi phục những tệp đã chuyển.").arg(e.from, err);
            for (int i = done - 1; i >= 0; --i) { QString e2; if (!safeRename(es[i].to, es[i].from, e2)) r.warnings << QObject::tr("Không khôi phục được '%1': %2").arg(es[i].to, e2); }
            if (r.warnings.isEmpty()) { QFile::remove(r.journalPath); r.journalPath.clear(); }
            return r;
        }
        ++done;
    }
    writeJournal(r.journalPath, QStringLiteral("organize"), QStringLiteral("done"), es, jerr);
    r.ok = true; r.done = done;
    return r;
}

// ---------------- đường dẫn ----------------
PathForms pathForms(const QString& path)
{
    PathForms f;
    const QFileInfo fi(path);
    const QString abs = fi.absoluteFilePath();
    f.full = QDir::toNativeSeparators(QDir::cleanPath(abs));
    f.quoted = QLatin1Char('"') + f.full + QLatin1Char('"');
    f.slashes = QDir::fromNativeSeparators(f.full);
    f.fileUrl = QUrl::fromLocalFile(abs).toString(QUrl::FullyEncoded);
    f.folder = QDir::toNativeSeparators(fi.absolutePath());
    f.name = fi.fileName();
    if (f.full.startsWith(QLatin1String("\\\\?\\"))) f.extended = f.full;
    else if (f.full.startsWith(QLatin1String("\\\\"))) f.extended = QStringLiteral("\\\\?\\UNC\\") + f.full.mid(2);
    else f.extended = QStringLiteral("\\\\?\\") + f.full;
    f.length = int(f.full.size());
    f.tooLong = f.length >= kWinMaxPath;
    f.nearLimit = f.length >= kPathNearLimit;
    return f;
}

LongPathResult findLongPaths(const QString& root, int limit, const Progress& prog)
{
    LongPathResult r;
    QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        ++r.scanned;
        if (prog && (r.scanned % 200 == 0) && !prog(r.scanned, 0)) { r.cancelled = true; break; }
        if (QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath()).size() >= limit) r.paths << p;
    }
    std::sort(r.paths.begin(), r.paths.end());
    return r;
}

} // namespace ft
