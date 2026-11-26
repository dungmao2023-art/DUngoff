// SPDX-License-Identifier: MPL-2.0
#include "RecoveryStore.h"

#include "../extras/WorkspaceStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMutex>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <memory>

namespace recovery {

namespace {
QString g_testDir;
QMutex g_mutex;

QString lockPathFor(const QString& runId) { return dir() + QStringLiteral("/run-") + runId + QStringLiteral(".lock"); }

std::unique_ptr<QLockFile> makeLock(const QString& runId) {
    auto lf = std::make_unique<QLockFile>(lockPathFor(runId));
    lf->setStaleLockTime(0);  // chỉ coi là "chết" khi tiến trình không còn chạy, không theo tuổi tệp khoá
    return lf;
}

qint64 dirBytes() {
    qint64 n = 0;
    const auto list = QDir(dir()).entryInfoList(QDir::Files);
    for (const QFileInfo& f : list) n += f.size();
    return n;
}

bool readMeta(const QString& metaPath, Entry& e) {
    QFile f(metaPath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject o = doc.object();
    e.id = QFileInfo(metaPath).completeBaseName();
    e.runId = o.value(QStringLiteral("runId")).toString();
    e.displayName = o.value(QStringLiteral("displayName")).toString();
    e.originalPath = o.value(QStringLiteral("originalPath")).toString();
    e.ext = o.value(QStringLiteral("ext")).toString();
    e.isNew = o.value(QStringLiteral("isNew")).toBool();
    e.saved = QDateTime::fromString(o.value(QStringLiteral("saved")).toString(), Qt::ISODate);
    e.metaPath = metaPath;
    e.dataPath = dir() + QLatin1Char('/') + e.id + QLatin1Char('.') + e.ext;
    e.size = QFileInfo(e.dataPath).size();
    return !e.runId.isEmpty() && !e.ext.isEmpty();
}
}  // namespace

QString dir() {
    const QString d = g_testDir.isEmpty() ? workspace::dataDir() + QStringLiteral("/recovery") : g_testDir;
    QDir().mkpath(d);
    return d;
}

void setDirForTest(const QString& path) { g_testDir = path; }

struct Session::Impl {
    QString runId;
    std::unique_ptr<QLockFile> lock;
    int counter = 0;
};

Session& Session::instance() {
    static Session s;
    return s;
}

bool Session::begin() {
    QMutexLocker g(&g_mutex);
    if (d && d->lock) return true;
    if (!d) d = new Impl();
    d->runId = QUuid::createUuid().toString(QUuid::Id128).left(12);
    d->lock = makeLock(d->runId);
    if (!d->lock->tryLock(0)) { d->lock.reset(); return false; }
    return true;
}

bool Session::active() const { return d && d->lock; }
QString Session::runId() const { return d ? d->runId : QString(); }

QString Session::newDocId() {
    QMutexLocker g(&g_mutex);
    if (!d) d = new Impl();
    return QString::number(++d->counter);
}

QString Session::dataPath(const QString& docId, const QString& ext) const {
    return dir() + QLatin1Char('/') + runId() + QLatin1Char('-') + docId + QLatin1Char('.') + ext;
}

bool Session::commit(const QString& docId, const QString& ext, const QString& displayName, const QString& originalPath, bool isNew) {
    if (!active()) return false;
    if (!QFileInfo::exists(dataPath(docId, ext))) return false;
    QJsonObject o;
    o[QStringLiteral("runId")] = runId();
    o[QStringLiteral("displayName")] = displayName;
    o[QStringLiteral("originalPath")] = originalPath;
    o[QStringLiteral("ext")] = ext;
    o[QStringLiteral("isNew")] = isNew;
    o[QStringLiteral("saved")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSaveFile f(dir() + QLatin1Char('/') + runId() + QLatin1Char('-') + docId + QStringLiteral(".json"));
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return f.commit();
}

void Session::discard(const QString& docId) {
    if (!d || docId.isEmpty()) return;
    const QString base = dir() + QLatin1Char('/') + runId() + QLatin1Char('-') + docId;
    QFile::remove(base + QStringLiteral(".json"));  // mô tả trước: không bao giờ có mô tả mà thiếu dữ liệu
    for (const char* e : {"docx", "xlsx", "pptx", "odg"}) QFile::remove(base + QLatin1Char('.') + QLatin1String(e));
}

bool Session::hasRoom(qint64 maxTotalBytes) const { return dirBytes() < maxTotalBytes; }

void Session::end() {
    QMutexLocker g(&g_mutex);
    if (!d) return;
    if (!d->runId.isEmpty()) {
        const auto list = QDir(dir()).entryInfoList(QStringList{d->runId + QStringLiteral("-*")}, QDir::Files);
        for (const QFileInfo& f : list) QFile::remove(f.absoluteFilePath());
    }
    if (d->lock) d->lock->unlock();
    d->lock.reset();
    d->runId.clear();
}

QList<Entry> findOrphans() {
    QList<Entry> out;
    const QString self = Session::instance().runId();
    const auto metas = QDir(dir()).entryInfoList(QStringList{QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo& mi : metas) {
        Entry e;
        if (!readMeta(mi.absoluteFilePath(), e)) {
            QFile::rename(mi.absoluteFilePath(), mi.absoluteFilePath() + QStringLiteral(".hong"));  // giữ lại bản mô tả hỏng
            continue;
        }
        if (e.runId == self) continue;
        if (!QFileInfo::exists(e.dataPath)) { QFile::remove(e.metaPath); continue; }  // mô tả mà không có dữ liệu: dọn
        auto lock = makeLock(e.runId);
        if (!lock->tryLock(0)) continue;  // phiên đó còn sống (ứng dụng khác đang chạy)
        lock->unlock();
        out.append(e);
    }
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.saved > b.saved; });
    return out;
}

bool discardEntry(const Entry& e) {
    bool ok = true;
    if (!e.metaPath.isEmpty() && QFile::exists(e.metaPath)) ok = QFile::remove(e.metaPath) && ok;
    if (!e.dataPath.isEmpty() && QFile::exists(e.dataPath)) ok = QFile::remove(e.dataPath) && ok;
    return ok;
}

int pruneOld(int maxAgeDays, qint64 maxTotalBytes) {
    int removed = 0;
    const QDateTime now = QDateTime::currentDateTime();
    QList<Entry> orphans = findOrphans();
    for (const Entry& e : orphans)
        if (e.saved.isValid() && e.saved.daysTo(now) > maxAgeDays && discardEntry(e)) ++removed;
    orphans = findOrphans();
    std::sort(orphans.begin(), orphans.end(), [](const Entry& a, const Entry& b) { return a.saved < b.saved; });  // cũ nhất trước
    for (const Entry& e : orphans) {
        if (dirBytes() <= maxTotalBytes) break;
        if (discardEntry(e)) ++removed;
    }
    // Tệp dữ liệu ghi dở (không có mô tả) của phiên đã chết và cũ hơn 1 ngày
    const auto files = QDir(dir()).entryInfoList(QDir::Files);
    for (const QFileInfo& f : files) {
        const QString n = f.fileName();
        if (n.endsWith(QStringLiteral(".json")) || n.endsWith(QStringLiteral(".lock")) || n.endsWith(QStringLiteral(".hong"))) continue;
        const int dash = n.indexOf(QLatin1Char('-'));
        if (dash <= 0) continue;
        const QString runId = n.left(dash);
        if (runId == Session::instance().runId()) continue;
        const QString meta = dir() + QLatin1Char('/') + n.left(n.lastIndexOf(QLatin1Char('.'))) + QStringLiteral(".json");
        if (QFile::exists(meta)) continue;
        if (f.lastModified().daysTo(now) < 1) continue;
        auto lock = makeLock(runId);
        if (!lock->tryLock(0)) continue;
        lock->unlock();
        if (QFile::remove(f.absoluteFilePath())) ++removed;
    }
    return removed;
}

}  // namespace recovery
