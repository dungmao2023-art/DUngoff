// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "RecoveryStore.h"

#include <QDialog>
#include <QList>

class QListWidget;

// "Khôi phục phiên trước": liệt kê bản phục hồi còn lại sau khi ứng dụng bị tắt đột ngột.
class RecoveryDialog : public QDialog {
    Q_OBJECT
public:
    explicit RecoveryDialog(const QList<recovery::Entry>& entries, QWidget* parent = nullptr);
    QList<recovery::Entry> toOpen() const { return m_open; }
    QList<recovery::Entry> toDiscard() const { return m_discard; }

private:
    QList<recovery::Entry> selected() const;
    QListWidget* m_list = nullptr;
    QList<recovery::Entry> m_entries, m_open, m_discard;
};
