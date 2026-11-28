// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QStringList>
#include <QUuid>
#include <QStorageInfo>
#include <QObject>
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
inline QString fileError(QFileDevice::FileError error,const QString& detail){
    QString explanation;
    switch(error){
    case QFileDevice::ResourceError: explanation=QObject::tr("Thiếu tài nguyên hoặc hết dung lượng lưu trữ.");break;
    case QFileDevice::PermissionsError: explanation=QObject::tr("Không có quyền đọc/ghi. Kiểm tra quyền thư mục và tài khoản Drive.");break;
    case QFileDevice::ReadError: explanation=QObject::tr("Không đọc được dữ liệu. Nếu tệp chỉ có trên đám mây, kiểm tra mạng và Google Drive.");break;
    case QFileDevice::WriteError: explanation=QObject::tr("Không ghi được dữ liệu. Kiểm tra dung lượng, quyền ghi và Drive.");break;
    default: explanation=QObject::tr("Thao tác tệp thất bại. Kiểm tra ổ đĩa, quyền truy cập hoặc kết nối Drive.");break;
    }
    return detail.isEmpty()?explanation:explanation+QStringLiteral(" (")+detail+QStringLiteral(")");
}
inline QString storageError(const QString& folder,qint64 required){
    if(folder.isEmpty()||!QFileInfo(folder).isDir())return QObject::tr("Thư mục không truy cập được. Kiểm tra Google Drive đang chạy, đăng nhập và kết nối mạng.");
    const QStorageInfo volume(folder);
    if(volume.isValid()&&volume.isReady()){
        if(volume.isReadOnly())return QObject::tr("Ổ đĩa chỉ đọc, không thể ghi.");
        if(volume.bytesAvailable()>=0&&required>volume.bytesAvailable())return QObject::tr("Không đủ dung lượng ổ đĩa đích. Giải phóng bộ nhớ hoặc kiểm tra dung lượng Drive.");
    }
    return {};
}
inline QString copyFile(const QString& source,const QString& folder,QString* error=nullptr){
    if(error)error->clear();
    auto fail=[&](const QString& message){if(error)*error=message;return QString();};
    const QFileInfo info(source);
    if(!info.isFile()||!info.isReadable()||info.isSymLink())return fail(QObject::tr("Không đọc được tệp nguồn. Kiểm tra quyền truy cập, Drive và kết nối mạng."));
    const auto problem=storageError(folder,info.size());if(!problem.isEmpty())return fail(problem);
    const auto dest=availablePath(folder,info.fileName());
    if(dest.isEmpty())return fail(QObject::tr("Không tạo được đường dẫn đích."));
    QFile file(source);
    if(!file.copy(dest))return fail(fileError(file.error(),file.errorString()));
    return dest;
}
}
