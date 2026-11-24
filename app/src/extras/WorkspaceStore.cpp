// SPDX-License-Identifier: MPL-2.0
#include "WorkspaceStore.h"

#include <QDir>
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

QJsonObject load(const QString& name) {
    const QString path = dataDir() + QLatin1Char('/') + name + QStringLiteral(".json");
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const QByteArray raw = f.readAll();
    f.close();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QFile::rename(path, path + QStringLiteral(".hong")); // giữ lại bản hỏng, không xoá
        return {};
    }
    return doc.object();
}

bool save(const QString& name, const QJsonObject& obj) {
    QSaveFile f(dataDir() + QLatin1Char('/') + name + QStringLiteral(".json"));
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return f.commit();
}

}  // namespace workspace
