// SPDX-License-Identifier: MPL-2.0
#include "../../app/src/extras/WorkspaceStore.h"
#include <QApplication>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include "../../app/src/extras/DriveFolder.h"
#include <QPushButton>
#include <QPlainTextEdit>
#include <QJsonArray>
#include "../../app/src/extras/SidePanel.h"
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
    QApplication app(argc,argv);
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
    if(!result) {
        const QJsonObject seed{{QStringLiteral("version"),1},
            {QStringLiteral("futureField"),QStringLiteral("preserve")},
            {QStringLiteral("notes"),QJsonArray{QJsonObject{{QStringLiteral("text"),QStringLiteral("Old note")}}}}};
        if(!workspace::save(QStringLiteral("workspace"),seed)) result=10;
        else {
            const auto before=readRaw(path);
            SidePanel panel;
            auto* editor=panel.findChild<QPlainTextEdit*>(QStringLiteral("WorkspaceNoteEditor"));
            auto* retry=panel.findChild<QPushButton*>(QStringLiteral("WorkspaceSaveNow"));
            auto* status=panel.findChild<QLabel*>(QStringLiteral("WorkspaceSaveStatus"));
            if(!editor||!retry||!status||editor->toPlainText()!=QStringLiteral("Old note")) result=11;
            else {
                editor->setPlainText(QStringLiteral("Ghi chú chưa lưu"));
                if(!retry->isEnabled()||!writeRaw(path,"corrupt during edit")||panel.flush()||
                    !retry->isEnabled()||editor->toPlainText()!=QStringLiteral("Ghi chú chưa lưu")||
                    readRaw(path)!="corrupt during edit") result=12;
                else if(!writeRaw(path,before)) result=13;
                else {
                    retry->click();
                    const auto saved=workspace::load(QStringLiteral("workspace"));
                    if(retry->isEnabled()||saved.value(QStringLiteral("futureField"))!=seed.value(QStringLiteral("futureField"))||
                       saved.value(QStringLiteral("notes")).toArray().at(0).toObject().value(QStringLiteral("text")).toString()!=QStringLiteral("Ghi chú chưa lưu"))
                        result=14;
                }
            }
        }
    }
    if(!result){
        const QJsonObject p1{{"id","p1"},{"name","Dự án A"},{"people",QJsonArray{QJsonObject{{"id","u1"},{"name","Nguyễn An"},{"contact",""}}}}};
        const QJsonObject p2{{"id","p2"},{"name","Dự án B"}};
        const QJsonObject task1{{"id","t1"},{"project","p1"},{"title","Việc A"},{"person","u1"},{"status",0},{"priority",1},{"due",QDate::currentDate().toString(Qt::ISODate)}};
        const QJsonObject task2{{"id","t2"},{"project","p2"},{"title","Việc B"},{"status",3}};
        auto data=workspace::load(QStringLiteral("workspace"));
        data.insert("workProjects",QJsonArray{p1,p2});data.insert("workTasks",QJsonArray{task1,task2});
        if(!workspace::save(QStringLiteral("workspace"),data))result=15;
        else {
            SidePanel panel;panel.showPage(1);
            auto* picker=panel.findChild<QComboBox*>(QStringLiteral("WorkProjectPicker"));
            auto* views=panel.findChild<QTabWidget*>(QStringLiteral("WorkViews"));
            auto* c0=panel.findChild<QListWidget*>(QStringLiteral("WorkColumn0"));
            auto* c3=panel.findChild<QListWidget*>(QStringLiteral("WorkColumn3"));
            if(!picker||!views||views->count()!=2||!c0||!c3||picker->count()!=3||c0->count()!=1||c3->count()!=0||!c0->item(0)->text().contains(QStringLiteral("Nguyễn An")))result=16;
            else {picker->setCurrentIndex(2);if(c0->count()!=0||c3->count()!=1||!c3->item(0)->text().contains(QStringLiteral("Việc B")))result=17;
                else {auto* editor=panel.findChild<QPlainTextEdit*>(QStringLiteral("WorkspaceNoteEditor"));editor->setPlainText(QStringLiteral("keep projects"));
                    if(!panel.flush()||workspace::load(QStringLiteral("workspace")).value("workTasks")!=data.value("workTasks"))result=18;}}
        }
    }
    if(!result){
        QTemporaryDir tmp;
        const auto office=drivefolder::office(tmp.path());
        if(!tmp.isValid()||office.isEmpty())result=19;
        else {
            for(const auto& cat:drivefolder::categories())if(!QFileInfo(QDir(office).filePath(cat)).isDir())result=20;
            const auto source=QDir(tmp.path()).filePath(QStringLiteral("Báo cáo.docx"));
            const auto folder=drivefolder::child(office,QStringLiteral("Word"));
            if(!writeRaw(source,"doc")||drivefolder::category(source)!=QStringLiteral("Word"))result=21;
            else {
                const auto a=drivefolder::copyFile(source,folder),b=drivefolder::copyFile(source,folder);
                if(a.isEmpty()||b.isEmpty()||a==b||readRaw(a)!="doc"||readRaw(b)!="doc"||!drivefolder::inFolder(a,folder)||drivefolder::inFolder(source,folder))result=22;
            }
        }
        if(!result){
            SidePanel panel;panel.showPage(1);
            const auto snapshot=panel.exportWorkProject();
            if(snapshot.isEmpty()||!panel.importWorkProject(snapshot))result=23;
            else {
                const auto imported=panel.exportWorkProject();
                const auto old=snapshot.value("project").toObject(),next=imported.value("project").toObject();
                if(old.value("id")==next.value("id")||imported.value("tasks").toArray().size()!=1||
                   imported.value("tasks").toArray().at(0).toObject().value("person")!=next.value("people").toArray().at(0).toObject().value("id"))result=24;
                auto broken=snapshot;broken.insert("version",99);if(panel.importWorkProject(broken))result=25;
            }
        }
    }
    QDir(dir).removeRecursively();
    if(result) std::fprintf(stderr,"WORKSPACE_STORE_FAIL %d\n",result);
    else std::puts("WORKSPACE_STORE_PASS Unicode, corrupt overwrite guard, two preserved backups, recovery save, unreadable target rejected, note retry preserves pending edits");
    return result;
}
