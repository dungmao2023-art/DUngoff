// SPDX-License-Identifier: MPL-2.0
#include "WorkspaceStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace workspace {
namespace {
QString pathFor(const QString& name) {
    return dataDir()+QLatin1Char('/')+name+QStringLiteral(".json");
}
// Never overwrite a corrupt file when its original bytes could not be preserved.
bool preserveCorrupt(const QString& path) {
    const QString base=path+QStringLiteral(".hong");
    for(int i=0;i<1000;++i) {
        const QString dst=i==0?base:base+QLatin1Char('.')+QString::number(i);
        if(QFileInfo::exists(dst)) continue;
        return QFile::rename(path,dst);
    }
    return false;
}
}

QString dataDir() {
    QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (d.isEmpty()) d = QDir::homePath() + QStringLiteral("/.dungoffice");
    QDir().mkpath(d);
    return d;
}

QJsonObject load(const QString& name, bool* ok) {
    if(ok) *ok=false;
    const QString path=pathFor(name);
    QFile f(path);
    if (!f.exists()) { if(ok) *ok=true; return {}; }
    if (!QFileInfo(path).isFile() || !f.open(QIODevice::ReadOnly)) return {};
    const QByteArray raw = f.readAll();
    if(f.error()!=QFileDevice::NoError) return {};
    f.close();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if(ok) *ok=preserveCorrupt(path);
        else preserveCorrupt(path);
        return {};
    }
    if(ok) *ok=true;
    return doc.object();
}

bool save(const QString& name, const QJsonObject& obj) {
    const QString path=pathFor(name);
    QFile existing(path);
    if(existing.exists()) {
        if(!QFileInfo(path).isFile()) return false;
        if(!existing.open(QIODevice::ReadOnly)) return false;
        const auto bytes=existing.readAll();
        if(existing.error()!=QFileDevice::NoError) return false;
        existing.close();
        QJsonParseError err;
        const auto old=QJsonDocument::fromJson(bytes,&err);
        // An invalid existing file might be the user's only recovery copy.
        if(err.error!=QJsonParseError::NoError || !old.isObject()) return false;
    }
    QSaveFile f(path);
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const QByteArray bytes=QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if(f.write(bytes)!=bytes.size()) { f.cancelWriting(); return false; }
    return f.commit();
}

}  // namespace workspace
