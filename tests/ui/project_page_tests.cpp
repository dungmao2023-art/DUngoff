// SPDX-License-Identifier: MPL-2.0
#include "../../app/src/extras/ProjectsPage.h"
#include "../../app/src/extras/WorkspaceStore.h"
#include <QApplication>
#include <QFrame>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <cstdio>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DungOfficeProjectTest"));
    QCoreApplication::setApplicationName(QStringLiteral("ProjectDirectory"));
    QStandardPaths::setTestModeEnabled(true);
    const QJsonObject initial{{QStringLiteral("projects"),QJsonArray{
        QJsonObject{{QStringLiteral("name"),QStringLiteral("Sử Việt")},
                    {QStringLiteral("url"),QStringLiteral("file:///unsafe")},
                    {QStringLiteral("items"),QJsonArray{QStringLiteral("legacy")}}}
    }}};
    if(!workspace::save(QStringLiteral("projects"),initial)) return 1;
    ProjectsPage page;
    page.resize(800,600);
    page.show();
    app.processEvents();
    const auto cards=page.findChildren<QFrame*>(QStringLiteral("ProjectLinkCard"));
    if(cards.size()!=3) return 2;
    int enabled=0, disabled=0;
    for(auto* card:cards) {
        const auto buttons=card->findChildren<QPushButton*>();
        if(buttons.size()!=1) return 3;
        if(buttons.first()->isEnabled()) ++enabled; else ++disabled;
        int avatars=0;
        for(auto* label:card->findChildren<QLabel*>())
            if(!label->pixmap().isNull()) ++avatars;
        if(avatars!=1) { std::fprintf(stderr,"PROJECT_UI_FAIL avatar decode\n"); return 4; }
    }
    if(enabled!=2||disabled!=1) return 5;
    page.flush();
    if(workspace::load(QStringLiteral("projects"))!=initial) return 6;
    std::puts("PROJECT_UI_PASS three cards, three avatars, safe URL, legacy data preserved");
    return 0;
}
