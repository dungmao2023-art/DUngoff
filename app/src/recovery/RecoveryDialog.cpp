// SPDX-License-Identifier: MPL-2.0
#include "RecoveryDialog.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

RecoveryDialog::RecoveryDialog(const QList<recovery::Entry>& entries, QWidget* parent)
    : QDialog(parent), m_entries(entries) {
    setWindowTitle(tr("Khôi phục phiên trước"));
    setMinimumWidth(560);
    auto* v = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Dũng Office đã không đóng bình thường lần trước. Các bản tự lưu dưới đây còn lại.\n"
                                "Mở một bản không làm thay đổi tệp gốc: khi lưu, bạn sẽ chọn nơi lưu bản mới."), this);
    intro->setWordWrap(true);
    v->addWidget(intro);
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (const auto& e : m_entries) {
        QString line = e.displayName.isEmpty() ? tr("(không tên)") : e.displayName;
        line += QStringLiteral("\n") + tr("Tự lưu lúc %1 — %2 KB").arg(e.saved.toString(QStringLiteral("dd/MM/yyyy HH:mm")))
                                            .arg((e.size + 1023) / 1024);
        line += QStringLiteral("\n") + (e.isNew || e.originalPath.isEmpty() ? tr("Tài liệu mới, chưa lưu lần nào")
                                                                           : tr("Từ tệp: %1").arg(QDir::toNativeSeparators(e.originalPath)));
        m_list->addItem(line);
    }
    m_list->selectAll();
    v->addWidget(m_list, 1);
    auto* row = new QHBoxLayout;
    row->addStretch();
    auto* later = new QPushButton(tr("Để sau"), this);
    auto* drop = new QPushButton(tr("Bỏ các bản đã chọn"), this);
    auto* open = new QPushButton(tr("Mở các bản đã chọn"), this);
    open->setDefault(true);
    row->addWidget(later); row->addWidget(drop); row->addWidget(open);
    v->addLayout(row);
    connect(later, &QPushButton::clicked, this, &QDialog::reject);
    connect(open, &QPushButton::clicked, this, [this] { m_open = selected(); accept(); });
    connect(drop, &QPushButton::clicked, this, [this] { m_discard = selected(); accept(); });
}

QList<recovery::Entry> RecoveryDialog::selected() const {
    QList<recovery::Entry> out;
    for (const QModelIndex& i : m_list->selectionModel()->selectedRows())
        if (i.row() >= 0 && i.row() < m_entries.size()) out.append(m_entries.at(i.row()));
    return out;
}
