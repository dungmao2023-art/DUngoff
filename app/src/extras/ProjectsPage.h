// SPDX-License-Identifier: MPL-2.0
// Trang "Dự án khác": thông tin Dũng Office (nút Update, mục đạt/chưa đạt) và 3 cột dự án khác (link + ảnh đại diện).
#pragma once
#include <QJsonArray>
#include <QWidget>

class QLabel;
class QListWidget;
class QLineEdit;

class ProjectsPage : public QWidget {
    Q_OBJECT
public:
    explicit ProjectsPage(QWidget* parent = nullptr);
    void flush();

private:
    QWidget* buildHeader();
    QWidget* buildColumn(int index);
    void load();
    void save();
    void refreshColumn(int index);
    void chooseImage(int index);

    QJsonArray m_projects; // đúng 3 phần tử
    struct Col {
        QLabel* avatar = nullptr;
        QLabel* name = nullptr;
        QLabel* link = nullptr;
        QListWidget* list = nullptr;
    };
    Col m_cols[3];
};
