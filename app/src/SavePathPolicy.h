// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QString>
#include <QFileInfo>
#include <QStringList>

namespace savepath {
inline QString resolve(QString path, const QString& ext, bool forceSuffix=false) {
    if (path.isEmpty() || ext.isEmpty()) return path;
    const QString suffix = QLatin1Char('.') + ext;
    if ((forceSuffix && !path.endsWith(suffix, Qt::CaseInsensitive)) ||
        (!forceSuffix && QFileInfo(path).suffix().isEmpty()))
        path += suffix;
    return path;
}
inline bool nativeSuffixAllowed(const QString& path, const QStringList& nativeExtensions) {
    if (path.isEmpty()) return false;
    const QString ext=QFileInfo(path).suffix();
    return !ext.isEmpty() && nativeExtensions.contains(ext, Qt::CaseInsensitive);
}
inline bool sameExistingPath(const QString& left, const QString& right) {
    if (left.isEmpty() || right.isEmpty()) return false;
    const QString a=QFileInfo(left).canonicalFilePath();
    const QString b=QFileInfo(right).canonicalFilePath();
    if (a.isEmpty() || b.isEmpty()) return false;
    return a==b;
}
inline bool needsConfirmation(const QString& resolved) {
    if (resolved.isEmpty()) return false;
    const QFileInfo info(resolved);
    return info.exists() || info.isSymLink();
}
}
