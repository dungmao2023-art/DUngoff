// SPDX-License-Identifier: MPL-2.0
#include "WorkspaceStore.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QLockFile>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace workspace {

QString dataDir() {
    QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (d.isEmpty()) d = QDir::homePath() + QStringLiteral("/.dungoffice");
    QDir().mkpath(d);
    return d;
}

QJsonObject loadFile(const QString& path) {
    QLockFile lock(path+QStringLiteral(".lock"));
    if(!lock.tryLock(100))return {};
    if(QFileInfo(path).isSymLink())return {};
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const QByteArray raw = f.readAll();
    f.close();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QString backup=path+QStringLiteral(".hong");
        if(QFileInfo::exists(backup)||QFileInfo(backup).isSymLink())
            backup+=QStringLiteral("-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
        // If quarantine fails, leave original in place; saveFile rejects it.
        QFile::rename(path,backup);
        return {};
    }
    return doc.object();
}

bool saveFile(const QString& path,const QJsonObject& obj) {
    QLockFile lock(path+QStringLiteral(".lock"));
    if(!lock.tryLock(100))return false;
    const QFileInfo info(path);
    if(info.isSymLink())return false;
    if(info.exists()) {
        if(!info.isFile())return false;
        QFile prior(path);
        if(!prior.open(QIODevice::ReadOnly))return false;
        QJsonParseError error;
        const auto old=QJsonDocument::fromJson(prior.readAll(),&error);
        prior.close();
        if(error.error!=QJsonParseError::NoError||!old.isObject())return false;
    }
    QSaveFile f(path);
    f.setDirectWriteFallback(false);
    if(!f.open(QIODevice::WriteOnly))return false;
    const auto bytes=QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if(f.write(bytes)!=bytes.size()){f.cancelWriting();return false;}
    return f.commit();
}
QJsonObject load(const QString& name){return loadFile(dataDir()+QLatin1Char('/')+name+QStringLiteral(".json"));}
bool save(const QString& name,const QJsonObject& obj){return saveFile(dataDir()+QLatin1Char('/')+name+QStringLiteral(".json"),obj);}

}  // namespace workspace
