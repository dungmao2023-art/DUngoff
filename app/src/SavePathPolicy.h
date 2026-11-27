// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QString>
#include <QFileInfo>
#include <QDir>
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
inline bool samePathText(const QString& a,const QString& b) {
#ifdef Q_OS_WIN
    return a.compare(b,Qt::CaseInsensitive)==0;
#else
    return a==b;
#endif
}
inline bool samePathForProtection(const QString& left,const QString& right) {
    if (left.isEmpty() || right.isEmpty()) return false;
    const QFileInfo a(left),b(right);
    const QString ca=a.canonicalFilePath(),cb=b.canonicalFilePath();
    if (!ca.isEmpty() && !cb.isEmpty()) return samePathText(ca,cb);
    return samePathText(QDir::cleanPath(a.absoluteFilePath()),QDir::cleanPath(b.absoluteFilePath()));
}
inline bool sameExistingPath(const QString& left, const QString& right) {
    if (left.isEmpty() || right.isEmpty()) return false;
    const QString a=QFileInfo(left).canonicalFilePath();
    const QString b=QFileInfo(right).canonicalFilePath();
    if (a.isEmpty() || b.isEmpty()) return false;
    return samePathText(a,b);
}
inline bool conflictsWithOpenPaths(const QString& target,const QStringList& otherPaths) {
    for(const QString& path:otherPaths)
        if(samePathForProtection(target,path)) return true;
    return false;
}
inline bool needsConfirmation(const QString& resolved) {
    if (resolved.isEmpty()) return false;
    const QFileInfo info(resolved);
    return info.exists() || info.isSymLink();
}
}
