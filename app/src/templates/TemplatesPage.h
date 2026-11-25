// SPDX-License-Identifier: MPL-2.0
// Trang "Kho mẫu": cây danh mục bên trái + lưới thẻ mẫu bên phải (theo thiết kế). Tạo bản làm việc từ mẫu
// (không ghi đè, mẫu gốc không đổi), cài/cập nhật gói mẫu (thay hẳn gói cũ).
#pragma once
#include <QList>
#include <QString>
#include <QWidget>

class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;

class TemplatesPage : public QWidget {
    Q_OBJECT
public:
    explicit TemplatesPage(QWidget* parent = nullptr);
    void reload();

signals:
    void openPath(const QString& path); // mở bản làm việc vừa tạo

protected:
    bool eventFilter(QObject* o, QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    struct Card {
        QString rel, category, name, format, status;
        QFrame* frame = nullptr;
    };
    void rebuildCards();      // tạo lại thẻ theo danh mục + ô tìm
    void relayout();          // xếp thẻ vào lưới theo bề rộng
    void select(const QString& rel);
    void useSelected();
    void installPackage();
    void showSources();

    QLabel* m_info;
    QLabel* m_count;
    QLineEdit* m_search;
    QListWidget* m_cats;
    QScrollArea* m_scroll;
    QWidget* m_gridHost;
    QGridLayout* m_grid;
    QLabel* m_empty;
    QPushButton* m_use;
    QList<Card> m_cards;
    QString m_root;
    QString m_pending;
    QString m_selected;
};
