// SPDX-License-Identifier: MPL-2.0
// Công cụ mẫu "Đồng hồ": bấm giờ và đếm ngược. Dùng để minh hoạ cách thêm một công cụ vào Hộp công cụ.
#include "../ToolRegistry.h"
#include "TimerFormat.h"

#include <QApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

class TimerWidget : public QWidget {
public:
    explicit TimerWidget(QWidget* parent) : QWidget(parent) {
        auto* v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        m_time = new QLabel(QStringLiteral("00:00:00.0"), this);
        m_time->setAlignment(Qt::AlignCenter);
        m_time->setStyleSheet(QStringLiteral("font-size:56px;font-weight:700;color:#1C2940;padding:18px;"));
        v->addWidget(m_time);
        m_info = new QLabel(tr("Bấm giờ: chạy từ 0. Đếm ngược: nhập số phút (90), phút:giây (5:30) hoặc giờ:phút:giây."), this);
        m_info->setWordWrap(true);
        m_info->setStyleSheet(QStringLiteral("color:#697992;"));
        v->addWidget(m_info);

        auto* row = new QHBoxLayout();
        m_input = new QLineEdit(this);
        m_input->setPlaceholderText(tr("Thời gian đếm ngược, ví dụ 5:30"));
        m_toggle = new QPushButton(tr("Bấm giờ ▶"), this);
        m_count = new QPushButton(tr("Đếm ngược ⏳"), this);
        m_reset = new QPushButton(tr("Đặt lại"), this);
        row->addWidget(m_input, 1);
        row->addWidget(m_toggle);
        row->addWidget(m_count);
        row->addWidget(m_reset);
        v->addLayout(row);
        v->addStretch(1);

        m_tick = new QTimer(this);
        m_tick->setInterval(100);
        connect(m_tick, &QTimer::timeout, this, [this] { refresh(); });
        connect(m_toggle, &QPushButton::clicked, this, [this] { toggleStopwatch(); });
        connect(m_count, &QPushButton::clicked, this, [this] { startCountdown(); });
        connect(m_reset, &QPushButton::clicked, this, [this] { reset(); });
    }

private:
    enum class Mode { Idle, Stopwatch, Countdown };

    qint64 now() const { return QDateTime::currentMSecsSinceEpoch(); }

    void toggleStopwatch() {
        if (m_mode == Mode::Stopwatch) { // tạm dừng
            m_acc += now() - m_start;
            m_mode = Mode::Idle;
            m_tick->stop();
            m_toggle->setText(tr("Tiếp tục ▶"));
            return;
        }
        if (m_mode == Mode::Countdown) return;
        m_start = now();
        m_mode = Mode::Stopwatch;
        m_tick->start();
        m_toggle->setText(tr("Tạm dừng ⏸"));
    }

    void startCountdown() {
        const qint64 ms = toolbox::parseDuration(m_input->text());
        if (ms < 0) { m_info->setText(tr("Thời gian không hợp lệ. Ví dụ: 90 (phút), 5:30 (phút:giây), 1:02:03.")); return; }
        reset();
        m_total = ms;
        m_start = now();
        m_mode = Mode::Countdown;
        m_tick->start();
        m_info->setText(tr("Đang đếm ngược…"));
    }

    void reset() {
        m_tick->stop();
        m_mode = Mode::Idle;
        m_acc = 0;
        m_total = 0;
        m_toggle->setText(tr("Bấm giờ ▶"));
        m_time->setText(QStringLiteral("00:00:00.0"));
        m_time->setStyleSheet(QStringLiteral("font-size:56px;font-weight:700;color:#1C2940;padding:18px;"));
    }

    void refresh() {
        if (m_mode == Mode::Stopwatch) {
            m_time->setText(toolbox::formatDuration(m_acc + now() - m_start, true));
        } else if (m_mode == Mode::Countdown) {
            const qint64 left = m_total - (now() - m_start);
            if (left <= 0) {
                m_tick->stop();
                m_mode = Mode::Idle;
                m_time->setText(QStringLiteral("00:00:00.0"));
                m_time->setStyleSheet(QStringLiteral("font-size:56px;font-weight:700;color:#C62828;padding:18px;"));
                m_info->setText(tr("Hết giờ!"));
                QApplication::beep();
            } else {
                m_time->setText(toolbox::formatDuration(left, true));
            }
        }
    }

    QLabel* m_time;
    QLabel* m_info;
    QLineEdit* m_input;
    QPushButton *m_toggle, *m_count, *m_reset;
    QTimer* m_tick;
    Mode m_mode = Mode::Idle;
    qint64 m_start = 0, m_acc = 0, m_total = 0;
};

QWidget* createTimerWidget(QWidget* parent) { return new TimerWidget(parent); }

} // namespace

// Toàn bộ việc "thêm công cụ" nằm ở dòng này (+ 1 dòng trong toolbox.cmake).
DUNG_REGISTER_TOOL(dong_ho, "Đồng hồ", "Bấm giờ và đếm ngược", "Tiện ích", "timer", createTimerWidget)
