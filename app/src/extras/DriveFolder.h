// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QStringList>
#include <QUuid>
namespace drivefolder {
inline QStringList categories(){return {QStringLiteral("Word"),QStringLiteral("Excel"),QStringLiteral("PPT"),QStringLiteral("PDF"),QStringLiteral("Kanban")};}
inline QString category(const QString& file){
    const auto ext=QFileInfo(file).suffix().toLower();
    if(QStringList{"docx","doc","odt","rtf","txt","docm","dotx"}.contains(ext))return QStringLiteral("Word");
    if(QStringList{"xlsx","xls","ods","csv","xlsm","xlsb"}.contains(ext))return QStringLiteral("Excel");
    if(QStringList{"pptx","ppt","odp","ppsx","pptm"}.contains(ext))return QStringLiteral("PPT");
    if(ext=="pdf")return QStringLiteral("PDF");
    return {};
}
inline QString child(const QString& parent,const QString& name){
    if(parent.isEmpty()||name.isEmpty()||name.contains('/')||name.contains('\\')||name=="."||name=="..")return {};
    QDir dir(parent);const auto path=dir.filePath(name);
    QFileInfo info(path);if(info.isSymLink())return {};
    if(!info.exists()&&!dir.mkdir(name))return {};
    info.refresh();if(!info.isDir()||info.isSymLink())return {};
    const auto canonical=info.canonicalFilePath();
    if(QFileInfo(canonical).dir().canonicalPath()!=dir.canonicalPath())return {};
    return canonical;
}
inline QString office(const QString& root){
    const auto base=QDir(root).canonicalPath();if(root.isEmpty()||base.isEmpty()||!QFileInfo(base).isDir())return {};
    const auto path=child(base,QStringLiteral("Office"));if(path.isEmpty())return {};
    for(const auto& name:categories())if(child(path,name).isEmpty())return {};
    return path;
}
inline bool inFolder(const QString& file,const QString& folder){
    const QFileInfo info(file);
    return !folder.isEmpty()&&info.isFile()&&info.isReadable()&&!info.isSymLink()&&info.dir().canonicalPath()==QDir(folder).canonicalPath();
}
inline QString availablePath(const QString& folder,const QString& name){
    if(folder.isEmpty()||name.isEmpty()||QFileInfo(name).fileName()!=name)return {};
    const QFileInfo info(name);QDir dir(folder);auto dest=dir.filePath(name);
    if(!QFileInfo::exists(dest)&&!QFileInfo(dest).isSymLink())return dest;
    const auto suffix=info.suffix().isEmpty()?QString():QStringLiteral(".")+info.suffix();
    return dir.filePath(info.completeBaseName()+QStringLiteral("-")+QUuid::createUuid().toString(QUuid::WithoutBraces)+suffix);
}
inline QString copyFile(const QString& source,const QString& folder){
    const QFileInfo info(source);
    if(!info.isFile()||!info.isReadable()||info.isSymLink()||!QFileInfo(folder).isDir())return {};
    const auto dest=availablePath(folder,info.fileName());
    return !dest.isEmpty()&&QFile::copy(source,dest)?dest:QString();
}
}
