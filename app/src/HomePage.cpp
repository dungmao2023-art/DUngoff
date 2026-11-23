// SPDX-License-Identifier: MPL-2.0
#include "HomePage.h"

#include "Theme.h"

#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

static QPushButton* makeTile(const QString& title, const QString& sub, const QColor& accent, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QStringLiteral("Tile"));
    b->setCursor(Qt::PointingHandCursor);
    b->setText(title + QLatin1Char('\n') + sub);
    b->setStyleSheet(QStringLiteral("QPushButton#Tile{border-left:5px solid %1;}").arg(accent.name()));
    return b;
}

HomePage::HomePage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("Home"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(40, 28, 40, 20);
    v->setSpacing(10);

    auto* title = new QLabel(tr("Dũng Office"), this);
    title->setObjectName(QStringLiteral("HomeTitle"));
    auto* sub = new QLabel(tr("Miễn phí • Ngoại tuyến • Không cần tài khoản. Tệp gốc luôn được giữ nguyên."), this);
    sub->setObjectName(QStringLiteral("HomeSubtitle"));
    v->addWidget(title);
    v->addWidget(sub);

    auto* newTitle = new QLabel(tr("Bắt đầu"), this);
    newTitle->setObjectName(QStringLiteral("SectionTitle"));
    v->addWidget(newTitle);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(14);
    auto* excel = makeTile(tr("Excel"), tr("Sổ làm việc trống"), theme::kExcel, this);
    auto* word = makeTile(tr("Word"), tr("Tài liệu trống"), theme::kWord, this);
    auto* ppt = makeTile(tr("PowerPoint"), tr("Bản trình chiếu trống"), theme::kPowerPoint, this);
    auto* pdf = makeTile(tr("PDF"), tr("Mở và xem tệp PDF"), theme::kPdf, this);
    grid->addWidget(excel, 0, 0);
    grid->addWidget(word, 0, 1);
    grid->addWidget(ppt, 0, 2);
    grid->addWidget(pdf, 0, 3);
    grid->setColumnStretch(4, 1);
    v->addLayout(grid);
    connect(excel, &QPushButton::clicked, this, &HomePage::newWorkbook);
    connect(word, &QPushButton::clicked, this, &HomePage::newDocument);
    connect(ppt, &QPushButton::clicked, this, &HomePage::newPresentation);
    connect(pdf, &QPushButton::clicked, this, &HomePage::openPdf);

    auto* row = new QHBoxLayout;
    auto* recentTitle = new QLabel(tr("Gần đây"), this);
    recentTitle->setObjectName(QStringLiteral("SectionTitle"));
    auto* open = new QPushButton(tr("Mở tệp…"), this);
    open->setObjectName(QStringLiteral("Primary"));
    open->setCursor(Qt::PointingHandCursor);
    row->addWidget(recentTitle);
    row->addStretch(1);
    row->addWidget(open);
    v->addLayout(row);
    connect(open, &QPushButton::clicked, this, &HomePage::openFile);

    auto* recent = m_recent = new QListWidget(this);
    recent->setObjectName(QStringLiteral("Recent"));
    v->addWidget(recent, 1);
    connect(recent, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) {
        emit openPath(it->data(Qt::UserRole).toString());
    });

    auto* status = m_status = new QLabel(this);
    status->setObjectName(QStringLiteral("HomeSubtitle"));
    v->addWidget(status);
}

void HomePage::setRecent(const QStringList& files) {
    QListWidget* list = m_recent;
    list->clear();
    for (const QString& f : files) {
        QFileInfo fi(f);
        auto* it = new QListWidgetItem(fi.fileName() + QStringLiteral("    —    ") + QDir::toNativeSeparators(fi.absolutePath()));
        it->setData(Qt::UserRole, f);
        it->setToolTip(QDir::toNativeSeparators(f));
        if (!fi.exists()) it->setForeground(theme::kMuted);
        list->addItem(it);
    }
    if (files.isEmpty()) {
        auto* it = new QListWidgetItem(tr("Chưa có tệp nào. Kéo thả tệp .xlsx, .docx, .pptx hoặc .pdf vào cửa sổ để mở."));
        it->setFlags(Qt::NoItemFlags);
        list->addItem(it);
    }
}

void HomePage::setEngineStatus(const QString& text) {
    m_status->setText(text);
}
