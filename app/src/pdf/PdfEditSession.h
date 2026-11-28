// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "PdfOps.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QList>
#include <memory>
namespace dungpdf {
// Reader edits can be staged in a private temporary directory. Never
// overwrite the original or expose incomplete output to the user.
struct PdfEditSession {
    std::unique_ptr<QTemporaryDir> directory;
    QString original,current;
    int serial=0,changes=0;
    QByteArray identity;
    struct Revision {QString path;QByteArray sha;int changes;int page;};
    QList<Revision> history;
    int cursor=-1;
    bool active() const{return bool(directory);}
    bool canUndo() const{return cursor>0;}
    bool canRedo() const{return cursor>=0&&cursor+1<history.size();}
    bool selectRevision(int index){
        if(!active()||index<0||index>=history.size())return false;
        const auto rev=history.at(index);
        if(rev.sha.size()!=32||fingerprint(rev.path)!=rev.sha)return false;
        cursor=index;current=rev.path;identity=rev.sha;changes=rev.changes;return true;
    }
    void appendRevision(const QString& path,const QByteArray& hash,int page){
        // A new edit after Undo abandons the redo branch.
        while(history.size()>cursor+1){
            const auto removed=history.takeLast();
            if(removed.path!=original&&removed.path!=path)QFile::remove(removed.path);
        }
        history.append({path,hash,changes+1,page});cursor=int(history.size())-1;
        current=path;identity=hash;++changes;
        // Keep at most six versions and 512 MiB of working copies. Never
        // delete the selected version or the actual user-owned source.
        qint64 bytes=0;
        for(const auto& rev:history)if(rev.path!=original)bytes+=qMax<qint64>(0,QFileInfo(rev.path).size());
        while(history.size()>1&&(history.size()>6||bytes>512LL*1024*1024)){
            const auto removed=history.takeFirst();--cursor;
            if(removed.path!=original){bytes-=qMax<qint64>(0,QFileInfo(removed.path).size());QFile::remove(removed.path);}
        }
    }
};
inline bool publishSession(const PdfEditSession& session,const QString& dest){
    if(!session.active()||session.changes<=0||session.identity.size()!=32||
       dungpdf::fingerprint(session.current)!=session.identity)return false;
    const QFileInfo target(dest);
    if(target.exists()||target.isSymLink())return false;
    QTemporaryDir temp(QDir(target.absolutePath()).filePath(QStringLiteral("~dung-pdf-final-XXXXXX")));
    if(!temp.isValid())return false;
    const auto staged=temp.filePath(QStringLiteral("final.pdf"));
    if(!QFile::copy(session.current,staged)||dungpdf::fingerprint(staged)!=session.identity||
       dungpdf::fingerprint(session.current)!=session.identity||!dungpdf::info(staged).ok)return false;
    return QFile::rename(staged,target.absoluteFilePath());
}
} // namespace dungpdf
