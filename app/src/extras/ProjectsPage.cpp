// SPDX-License-Identifier: MPL-2.0
#include "ProjectsPage.h"
#include "WorkspaceStore.h"

#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace {
struct Project { const char* name; const char* initials; const char* url; const char* color; };
const Project projects[] = {
    {"Sử Việt", "SV", "", "#387D72"},
    {"TFT Labs", "TFT", "https://tft2.id.vn/", "#5855A4"},
    {"Trending Việt Nam", "TV", "https://trend.id.vn/", "#C04B59"},
};

// Only retain an existing local avatar or Sử Việt address if it belongs to
// this named project. Never overwrite legacy user data just to simplify UI.
QJsonObject matchingProject(const QJsonArray& saved, const QString& name) {
    for (const auto& value : saved) {
        const auto entry=value.toObject();
        if(entry.value(QStringLiteral("name")).toString().trimmed().compare(name,Qt::CaseInsensitive)==0)
            return entry;
    }
    return {};
}
bool validWebsite(const QUrl& url) {
    return url.isValid() && url.scheme()==QLatin1String("https")
        && !url.host().isEmpty() && url.userInfo().isEmpty();
}
}

ProjectsPage::ProjectsPage(QWidget* parent) : QWidget(parent) {
    auto* outer=new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    auto* scroll=new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* body=new QWidget(scroll);
    scroll->setWidget(body);
    auto* v=new QVBoxLayout(body);
    v->setContentsMargins(24,24,24,24);
    v->setSpacing(16);
    auto* title=new QLabel(tr("Dự án"),body);
    title->setStyleSheet(QStringLiteral("font-size:22px;font-weight:700;color:#1C2940;"));
    v->addWidget(title);

    const auto saved=workspace::load(QStringLiteral("projects")).value(QStringLiteral("projects")).toArray();
    for(const auto& project:projects) {
        const QString name=QString::fromUtf8(project.name);
        const auto existing=matchingProject(saved,name);
        QUrl url(QString::fromLatin1(project.url),QUrl::StrictMode);
        if(!*project.url)
            url=QUrl(existing.value(QStringLiteral("url")).toString().trimmed(),QUrl::StrictMode);
        const bool available=validWebsite(url);
        auto* card=new QFrame(body);
        card->setObjectName(QStringLiteral("ProjectLinkCard"));
        card->setStyleSheet(QStringLiteral("QFrame#ProjectLinkCard{background:#fff;border:1px solid #DEE5F0;border-radius:12px;}"));
        auto* grid=new QGridLayout(card);
        grid->setContentsMargins(18,14,18,14);
        grid->setHorizontalSpacing(18);
        grid->setColumnStretch(1,1);
        auto* avatar=new QLabel(card);
        avatar->setFixedSize(76,76);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setAccessibleName(name);
        avatar->setStyleSheet(QStringLiteral("background:%1;border-radius:12px;color:white;font-size:22px;font-weight:700;").arg(QString::fromLatin1(project.color)));
        QPixmap photo;
        const auto image=existing.value(QStringLiteral("image")).toString();
        if(!image.isEmpty() && photo.load(image))
            avatar->setPixmap(photo.scaled(avatar->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        else
            avatar->setText(QString::fromLatin1(project.initials));
        grid->addWidget(avatar,0,0,2,1);
        auto* label=new QLabel(name,card);
        label->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;color:#1C2940;"));
        label->setWordWrap(true);
        grid->addWidget(label,0,1);
        auto* address=new QLabel(available?url.toDisplayString():tr("Chưa công bố website"),card);
        address->setTextFormat(Qt::PlainText);
        address->setWordWrap(true);
        address->setStyleSheet(QStringLiteral("color:#697992;"));
        grid->addWidget(address,1,1);
        auto* open=new QPushButton(tr("Mở website"),card);
        open->setEnabled(available);
        open->setAccessibleName(tr("Mở website %1").arg(name));
        open->setToolTip(available?url.toDisplayString():tr("Chưa có địa chỉ chính thức"));
        connect(open,&QPushButton::clicked,this,[this,url] {
            if(validWebsite(url) && !QDesktopServices::openUrl(url))
                QMessageBox::warning(this,tr("Dũng Office"),tr("Không mở được trình duyệt."));
        });
        grid->addWidget(open,0,2,2,1);
        v->addWidget(card);
    }
    v->addStretch(1);
}

// Compatibility with MainWindow's existing workspace flush lifecycle.
// The project directory is intentionally read-only.
void ProjectsPage::flush() {}
