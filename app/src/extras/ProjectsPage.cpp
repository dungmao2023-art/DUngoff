// SPDX-License-Identifier: MPL-2.0
#include "ProjectsPage.h"
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>
#include <QMessageBox>

namespace {
struct Project { const char* title; const char* url; const char* image; const char* mark; const char* color; };
constexpr Project kProjects[] = {
    {"TFT Labs","https://tft2.id.vn/",":/projects/ornn.webp","TFT","#4C62A2"},
    {"Trending Việt Nam","https://trend.id.vn/",":/projects/trend.webp","TVN","#AD4962"},
    {"Sử Việt","https://suviet.id.vn/",":/projects/sv.webp","SV","#287B6C"}
};
}
ProjectsPage::ProjectsPage(QWidget* parent):QWidget(parent){
    auto* outer=new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    auto* scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* body=new QWidget(scroll);scroll->setWidget(body);
    auto* v=new QVBoxLayout(body);v->setContentsMargins(24,22,24,22);v->setSpacing(16);
    auto* title=new QLabel(tr("Các dự án"),body);title->setStyleSheet(QStringLiteral("font-size:24px;font-weight:700;color:#1C2940;"));
    v->addWidget(title);
    for(const Project& project:kProjects){
        auto* card=new QFrame(body);card->setObjectName(QStringLiteral("ProjectLinkCard"));
        card->setStyleSheet(QStringLiteral("QFrame#ProjectLinkCard{background:white;border:1px solid #DEE5F0;border-radius:12px;}"));
        auto* row=new QHBoxLayout(card);row->setContentsMargins(16,14,16,14);row->setSpacing(16);
        auto* avatar=new QLabel(card);avatar->setObjectName(QStringLiteral("ProjectAvatar"));
        avatar->setFixedSize(84,84);avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(QStringLiteral("background:#EEF4FA;color:%1;border-radius:12px;font-size:22px;font-weight:700;").arg(QString::fromLatin1(project.color)));
        const QPixmap pm(QString::fromLatin1(project.image));
        avatar->setProperty("loadedOriginalAvatar",!pm.isNull());
        if(!pm.isNull())avatar->setPixmap(pm.scaled(avatar->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        else avatar->setText(QString::fromLatin1(project.mark));
        row->addWidget(avatar);
        auto* labels=new QVBoxLayout;
        auto* name=new QLabel(QString::fromUtf8(project.title),card);
        name->setStyleSheet(QStringLiteral("font-size:17px;font-weight:700;color:#17294D;"));name->setWordWrap(true);
        labels->addWidget(name);
        const QUrl url(QString::fromLatin1(project.url));
        auto* link=new QLabel(url.toString(),card);link->setTextFormat(Qt::PlainText);link->setWordWrap(true);
        link->setStyleSheet(QStringLiteral("color:#697992;"));labels->addWidget(link);
        row->addLayout(labels,1);
        auto* visit=new QPushButton(tr("Truy cập ↗"),card);visit->setObjectName(QStringLiteral("ProjectVisit"));
        visit->setAccessibleName(tr("Truy cập %1").arg(name->text()));visit->setToolTip(url.toString());
        visit->setCursor(Qt::PointingHandCursor);
        visit->setStyleSheet(QStringLiteral("QPushButton{background:#2864BA;color:white;border:0;border-radius:8px;padding:9px 14px;font-weight:600;} QPushButton:hover{background:#1B4A90;}"));
        connect(visit,&QPushButton::clicked,this,[this,url]{
            if(!QDesktopServices::openUrl(url))QMessageBox::warning(this,tr("Mở website"),tr("Không mở được trình duyệt. Địa chỉ: %1").arg(url.toString()));
        });
        row->addWidget(visit);v->addWidget(card);
    }
    v->addStretch(1);
}
