// SPDX-License-Identifier: MPL-2.0
#include "TopBar.h"

#include "ShellIcons.h"
#include "../Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

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
    h->setContentsMargins(24, 0, 20, 0);
    h->setSpacing(4);
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
    h->addSpacing(8);
    auto* avatar = new QLabel(QStringLiteral("D"), row1);
    avatar->setObjectName(QStringLiteral("Avatar"));
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(34, 34);
    avatar->setToolTip(tr("Dũng Office — miễn phí, ngoại tuyến, không cần tài khoản"));
    h->addWidget(avatar);
    v->addWidget(row1);

    // ---- Hàng 2: liên kết nhanh (đều là chức năng có thật) ----
    m_links = new QWidget(this);
    m_links->setObjectName(QStringLiteral("QuickLinks"));
    m_links->setAttribute(Qt::WA_StyledBackground, true);
    m_links->setFixedHeight(50);
    auto* l = new QHBoxLayout(m_links);
    l->setContentsMargins(28, 0, 24, 0);
    l->setSpacing(6);
    struct Q { const char* id; QString text; };
    const Q links[] = {{"home", tr("Trang chủ")}, {"templates", tr("Kho mẫu")}, {"tools", tr("Công cụ")},
                       {"projects", tr("Dự án")}, {"work", tr("Công việc")}, {"open", tr("Mở tệp")}};
    for (const Q& q : links) {
        auto* b = new QPushButton(q.text, m_links);
        b->setObjectName(QStringLiteral("QuickLink"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        const QString id = QString::fromLatin1(q.id);
        connect(b, &QPushButton::clicked, this, [this, id] { emit quickLinkClicked(id); });
        l->addWidget(b);
    }
    l->addStretch(1);
    m_ready = new QLabel(tr("Sẵn sàng"), m_links);
    m_ready->setObjectName(QStringLiteral("TopReady"));
    l->addWidget(m_ready);
    v->addWidget(m_links);
}

void TopBar::setActive(const QString& kind) {
    for (int i = 0; i < 5; ++i) {
        const bool on = kind == QLatin1String(kKinds[i]);
        m_tabs[i]->setProperty("active", on);
        m_tabs[i]->style()->unpolish(m_tabs[i]);
        m_tabs[i]->style()->polish(m_tabs[i]);
    }
}

void TopBar::setSaveText(const QString& html) { m_save->setText(html); }
void TopBar::setReadyText(const QString& text) { m_ready->setText(text); }

void TopBar::setCompact(bool compact) {
    m_save->setVisible(!compact);
    m_ready->setVisible(!compact);
}

} // namespace shell
