// SPDX-License-Identifier: MPL-2.0
// Trang "Công cụ": tìm kiếm, nhóm, thẻ công cụ. Mở công cụ ngay trong trang (có nút quay lại).
#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

class ToolboxPage : public QWidget {
    Q_OBJECT
public:
    explicit ToolboxPage(QWidget* parent = nullptr);
    void reload(); // quét lại tool ngoài và vẽ lại danh sách

private:
    void rebuild();
    void openTool(const QString& id);
    void closeTool();

    QLineEdit* m_search;
    QStackedWidget* m_stack;
    QWidget* m_listPage;
    QWidget* m_listBody = nullptr;
    QScrollArea* m_scroll;
    QWidget* m_toolPage;
    QVBoxLayout* m_toolLayout;
    QLabel* m_toolTitle;
    QWidget* m_toolWidget = nullptr;
    QLabel* m_errors;
};
