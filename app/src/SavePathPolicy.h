// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QString>
#include <QFileInfo>

namespace savepath {
inline QString resolve(QString path, const QString& ext, bool forceSuffix=false) {
    if (path.isEmpty() || ext.isEmpty()) return path;
    const QString suffix = QLatin1Char('.') + ext;
    if ((forceSuffix && !path.endsWith(suffix, Qt::CaseInsensitive)) ||
        (!forceSuffix && QFileInfo(path).suffix().isEmpty()))
        path += suffix;
    return path;
}
inline bool needsConfirmation(const QString& resolved) {
    if (resolved.isEmpty()) return false;
    const QFileInfo info(resolved);
    return info.exists() || info.isSymLink();
}
}
