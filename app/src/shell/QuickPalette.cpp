// SPDX-License-Identifier: MPL-2.0
#include "QuickPalette.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace shell {

QString foldVietnamese(const QString& s) {
    QString out;
    const QString d = s.toLower().normalized(QString::NormalizationForm_D);
    for (QChar c : d) {
        if (c.category() == QChar::Mark_NonSpacing) continue;
        if (c == QChar(0x0111)) c = QLatin1Char('d'); // đ
        out.append(c);
    }
    return out;
}

QuickPalette::QuickPalette(QWidget* parent) : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint) {
    setObjectName(QStringLiteral("QuickPalette"));
    setAttribute(Qt::WA_StyledBackground, true);
    setModal(true);
    setFixedWidth(560);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(14, 14, 14, 10);
    v->setSpacing(8);
    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("PaletteInput"));
    m_edit->setPlaceholderText(tr("Gõ lệnh hoặc tên chức năng… (không dấu cũng được)"));
    v->addWidget(m_edit);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("PaletteList"));
    m_list->setFixedHeight(320);
    v->addWidget(m_list);
    auto* foot = new QLabel(tr("Enter: chạy · Esc: đóng · ↑↓: chọn"), this);
    foot->setObjectName(QStringLiteral("PaletteHint"));
    v->addWidget(foot);
    connect(m_edit, &QLineEdit::textChanged, this, [this] { refilter(); });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) {
        const QString id = it->data(Qt::UserRole).toString();
        accept();
        emit triggered(id);
    });
    m_edit->installEventFilter(this);
}

void QuickPalette::setEntries(const QList<PaletteEntry>& entries) {
    m_entries = entries;
    refilter();
}

void QuickPalette::refilter() {
    const QStringList words = foldVietnamese(m_edit->text()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    m_list->clear();
    for (const PaletteEntry& e : m_entries) {
        const QString hay = foldVietnamese(e.title + QLatin1Char(' ') + e.hint);
        bool ok = true;
        for (const QString& w : words) if (!hay.contains(w)) { ok = false; break; }
        if (!ok) continue;
        auto* it = new QListWidgetItem(e.hint.isEmpty() ? e.title : e.title + QStringLiteral("    ·    ") + e.hint);
        it->setData(Qt::UserRole, e.id);
        m_list->addItem(it);
    }
    if (m_list->count() == 0) {
        auto* it = new QListWidgetItem(tr("Không có lệnh phù hợp"));
        it->setFlags(Qt::NoItemFlags);
        m_list->addItem(it);
    } else {
        m_list->setCurrentRow(0);
    }
}

bool QuickPalette::eventFilter(QObject* o, QEvent* e) {
    if (o == m_edit && e->type() == QEvent::KeyPress) {
        const int k = static_cast<QKeyEvent*>(e)->key();
        if (k == Qt::Key_Down || k == Qt::Key_Up) {
            const int n = m_list->count();
            if (n > 0) m_list->setCurrentRow((m_list->currentRow() + (k == Qt::Key_Down ? 1 : n - 1)) % n);
            return true;
        }
        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            if (auto* it = m_list->currentItem()) if (it->flags() & Qt::ItemIsEnabled) emit m_list->itemActivated(it);
            return true;
        }
    }
    return QDialog::eventFilter(o, e);
}

void QuickPalette::popup() {
    m_edit->clear();
    refilter();
    if (QWidget* p = parentWidget()) {
        const QPoint c = p->mapToGlobal(QPoint(p->width() / 2, 0));
        move(c.x() - width() / 2, p->mapToGlobal(QPoint(0, 0)).y() + qMin(120, p->height() / 6));
    }
    show();
    m_edit->setFocus();
}

} // namespace shell
