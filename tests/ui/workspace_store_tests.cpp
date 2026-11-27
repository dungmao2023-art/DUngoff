// SPDX-License-Identifier: MPL-2.0
#include "../../app/src/extras/WorkspaceStore.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonObject>
#include <QUuid>
#include <cstdio>

static bool writeRaw(const QString& path,const QByteArray& bytes) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes)==bytes.size() && f.flush();
}
static QByteArray readRaw(const QString& path) {
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    app.setOrganizationName(QStringLiteral("DungOfficeWorkspaceTests"));
    app.setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QStandardPaths::setTestModeEnabled(true);
    const QString dir=workspace::dataDir();
    const QString path=dir+QStringLiteral("/workspace.json");
    const QJsonObject doc{{QStringLiteral("note"),QStringLiteral("Tiếng Việt")}};
    int result=0;
    if(!workspace::save(QStringLiteral("workspace"),doc)||workspace::load(QStringLiteral("workspace"))!=doc) result=1;
    else if(!writeRaw(path,"first broken JSON")||workspace::save(QStringLiteral("workspace"),doc)||readRaw(path)!="first broken JSON") result=2;
    else if(!workspace::load(QStringLiteral("workspace")).isEmpty()||readRaw(path+QStringLiteral(".hong"))!="first broken JSON") result=3;
    else if(!writeRaw(path,"second broken JSON")||!workspace::load(QStringLiteral("workspace")).isEmpty()||readRaw(path+QStringLiteral(".hong.1"))!="second broken JSON"||readRaw(path+QStringLiteral(".hong"))!="first broken JSON") result=4;
    else if(!workspace::save(QStringLiteral("workspace"),doc)||workspace::load(QStringLiteral("workspace"))!=doc) result=5;
    if(!result) {
        bool readable=false;
        if(workspace::load(QStringLiteral("workspace"),&readable)!=doc||!readable) result=6;
        else if(!workspace::load(QStringLiteral("missing"),&readable).isEmpty()||!readable) result=7;
        else if(!QDir(dir).mkdir(QStringLiteral("blocked.json"))) result=8;
        else if(!workspace::load(QStringLiteral("blocked"),&readable).isEmpty()||readable||
                workspace::save(QStringLiteral("blocked"),doc)) result=9;
    }
    QDir(dir).removeRecursively();
    if(result) std::fprintf(stderr,"WORKSPACE_STORE_FAIL %d\n",result);
    else std::puts("WORKSPACE_STORE_PASS Unicode, corrupt overwrite guard, two preserved backups, recovery save, unreadable target rejected");
    return result;
}
