// SPDX-License-Identifier: MPL-2.0
#include "TopBar.h"

#include "ShellIcons.h"
#include "../Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QApplication>

namespace shell {

static const char* kKinds[5] = {"home", "word", "excel", "ppt", "pdf"};

TopBar::TopBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("TopBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ---- Hàng 1: thẻ ứng dụng ----
    auto* row1 = new QWidget(this);
    row1->setObjectName(QStringLiteral("TopRow"));
    row1->setAttribute(Qt::WA_StyledBackground, true);
    row1->setFixedHeight(66);
    auto* h = new QHBoxLayout(row1);
    h->setContentsMargins(10, 0, 10, 0);
    h->setSpacing(3);
    h->addStretch(1);
    const QString names[5] = {tr("Trang chủ"), QStringLiteral("Word"), QStringLiteral("Excel"),
                              QStringLiteral("PowerPoint"), QStringLiteral("PDF")};
    for (int i = 0; i < 5; ++i) {
        auto* b = m_tabs[i] = new QPushButton(names[i], row1);
        b->setObjectName(QStringLiteral("TopTab"));
        b->setProperty("kind", QString::fromLatin1(kKinds[i]));
        b->setProperty("active", false);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedHeight(66);
        const QString k = QString::fromLatin1(kKinds[i]);
        connect(b, &QPushButton::clicked, this, [this, k] { emit tabClicked(k); });
        h->addWidget(b);
    }
    h->addStretch(1);
    auto* search = new QPushButton(row1);
    search->setObjectName(QStringLiteral("RoundBtn"));
    search->setIcon(icon(symbol(QStringLiteral("search"), theme::kBlue, QColor(0, 0, 0, 0), 20, devicePixelRatioF())));
    search->setIconSize(QSize(20, 20));
    search->setFixedSize(36, 36);
    search->setToolTip(tr("Tìm lệnh nhanh (Ctrl+K)"));
    search->setCursor(Qt::PointingHandCursor);
    search->setFocusPolicy(Qt::NoFocus);
    connect(search, &QPushButton::clicked, this, &TopBar::searchRequested);
    h->addWidget(search);
    h->addSpacing(8);
    m_save = new QLabel(tr("● Lưu trên máy bạn"), row1);
    m_save->setObjectName(QStringLiteral("TopSave"));
    h->addWidget(m_save);
    m_ready = new QLabel(tr("Sẵn sàng"), row1);
    m_ready->setObjectName(QStringLiteral("TopReady"));
    m_ready->hide();
    auto control=[&](QStyle::StandardPixmap iconId,const QString& tooltip,const QString& colors,auto action){
        auto* b=new QPushButton(row1);b->setFixedSize(28,28);b->setIcon(style()->standardIcon(iconId));
        b->setIconSize(QSize(14,14));b->setToolTip(tooltip);b->setAccessibleName(tooltip);
        b->setFocusPolicy(Qt::NoFocus);b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral("QPushButton{border:1px solid #CBD8EC;border-radius:5px;background:%1;} QPushButton:hover{background:%2;}").arg(colors.section('|',0,0),colors.section('|',1,1)));
        connect(b,&QPushButton::clicked,this,action);h->addWidget(b);
    };
    control(QStyle::SP_TitleBarMinButton,tr("Thu nhỏ cửa sổ"),QStringLiteral("#E8F0FF|#CEDFFF"),[this]{window()->showMinimized();});
    control(QStyle::SP_TitleBarMaxButton,tr("Phóng to / Khôi phục"),QStringLiteral("#E3F5EC|#C9E8D7"),[this]{
        if(window()->isMaximized())window()->showNormal();else window()->showMaximized();
    });
    control(QStyle::SP_TitleBarCloseButton,tr("Đóng Dũng Office"),QStringLiteral("#FBE8E8|#F4CACA"),[this]{window()->close();});
    v->addWidget(row1);
}

void TopBar::setActive(const QString& kind) {
    for (int i = 0; i < 5; ++i) {
        const bool on = kind == QLatin1String(kKinds[i]);
        const auto previous=m_tabs[i]->property("active");
        if(previous.isValid()&&previous.toBool()==on)continue;
        m_tabs[i]->setProperty("active", on);
        m_tabs[i]->style()->unpolish(m_tabs[i]);
        m_tabs[i]->style()->polish(m_tabs[i]);
    }
}

void TopBar::setSaveText(const QString& html) { if(m_save->text()!=html)m_save->setText(html); }
void TopBar::setReadyText(const QString& text) { if(m_ready->text()!=text)m_ready->setText(text); }

void TopBar::setCompact(bool compact) {
    m_save->setVisible(!compact);
    m_ready->hide();
}

} // namespace shell
