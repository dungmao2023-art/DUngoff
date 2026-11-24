// SPDX-License-Identifier: MPL-2.0
// Trang "Mẫu": duyệt thư viện biểu mẫu, tạo bản làm việc từ mẫu, cài/cập nhật gói mẫu (thay hẳn gói cũ).
#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class TemplatesPage : public QWidget {
    Q_OBJECT
public:
    explicit TemplatesPage(QWidget* parent = nullptr);
    void reload();

signals:
    void openPath(const QString& path); // mở bản làm việc vừa tạo

private:
    void applyFilter();
    void useSelected();
    void installPackage();
    void showSources();

    QLabel* m_info;
    QLineEdit* m_search;
    QTreeWidget* m_tree;
    QString m_root;
    QString m_pending;
};
