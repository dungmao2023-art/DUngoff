// SPDX-License-Identifier: MPL-2.0
// Giao diện tự dựng từ tool.json: biểu mẫu nhập + nút Chạy/Dừng + khung kết quả. Xin xác nhận lần đầu.
#pragma once
#include "ExternalTool.h"

#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace toolbox {

class ExternalToolWidget : public QWidget {
    Q_OBJECT
public:
    explicit ExternalToolWidget(const ToolSpec& spec, QWidget* parent = nullptr);

private:
    void run();
    void stopRun();
    bool ensureConfirmed();
    QMap<QString, QString> collect() const;

    ToolSpec m_spec;
    QMap<QString, QWidget*> m_inputs;
    ToolRunner* m_runner;
    QPlainTextEdit* m_out;
    QPushButton* m_run;
    QPushButton* m_stop;
    QLabel* m_status;
};

} // namespace toolbox
