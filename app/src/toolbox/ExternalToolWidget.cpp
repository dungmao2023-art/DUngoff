// SPDX-License-Identifier: MPL-2.0
#include "ExternalToolWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace toolbox {

ExternalToolWidget::ExternalToolWidget(const ToolSpec& spec, QWidget* parent) : QWidget(parent), m_spec(spec) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto* desc = new QLabel(spec.description.isEmpty() ? tr("(Tool ngoài, không có mô tả)") : spec.description, this);
    desc->setWordWrap(true);
    desc->setStyleSheet(QStringLiteral("color:#4B5B76;"));
    v->addWidget(desc);
    auto* warn = new QLabel(tr("Tool ngoài: chạy chương trình do bạn tự cài trên máy. Chỉ dùng tool từ nguồn bạn tin cậy."), this);
    warn->setWordWrap(true);
    warn->setStyleSheet(QStringLiteral("background:#FFF7E6;color:#7A5B00;border:1px solid #F0DDAD;border-radius:8px;padding:6px 10px;"));
    v->addWidget(warn);

    auto* form = new QFormLayout();
    for (const FieldSpec& f : spec.fields) {
        QWidget* w = nullptr;
        const QString label = f.label + (f.required ? QStringLiteral(" *") : QString());
        if (f.type == QLatin1String("multiline")) {
            auto* e = new QPlainTextEdit(this);
            e->setPlainText(f.def);
            e->setPlaceholderText(f.hint);
            e->setFixedHeight(110);
            w = e;
        } else if (f.type == QLatin1String("choice")) {
            auto* c = new QComboBox(this);
            c->addItems(f.choices);
            if (!f.def.isEmpty()) c->setCurrentText(f.def);
            w = c;
        } else if (f.type == QLatin1String("file") || f.type == QLatin1String("folder")) {
            auto* row = new QWidget(this);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            auto* e = new QLineEdit(f.def, row);
            e->setPlaceholderText(f.hint);
            auto* b = new QPushButton(tr("Chọn…"), row);
            const bool isFile = f.type == QLatin1String("file");
            connect(b, &QPushButton::clicked, this, [this, e, isFile] {
                const QString p = isFile ? QFileDialog::getOpenFileName(this, tr("Chọn tệp"))
                                         : QFileDialog::getExistingDirectory(this, tr("Chọn thư mục"));
                if (!p.isEmpty()) e->setText(p);
            });
            h->addWidget(e, 1);
            h->addWidget(b);
            row->setProperty("lineEdit", QVariant::fromValue<QObject*>(e));
            w = row;
        } else {
            auto* e = new QLineEdit(f.def, this); // text và number: nhập dạng chữ, chương trình kiểm tra lại khi chạy
            e->setPlaceholderText(f.hint);
            e->setMaxLength(f.maxLen);
            w = e;
        }
        m_inputs[f.id] = w;
        form->addRow(label, w);
    }
    v->addLayout(form);

    auto* bar = new QHBoxLayout();
    m_run = new QPushButton(tr("Chạy"), this);
    m_run->setStyleSheet(QStringLiteral("QPushButton{background:#275DD6;color:white;border:0;border-radius:8px;padding:8px 18px;font-weight:700;}"));
    m_stop = new QPushButton(tr("Dừng"), this);
    m_stop->setEnabled(false);
    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color:#697992;"));
    bar->addWidget(m_run);
    bar->addWidget(m_stop);
    bar->addWidget(m_status, 1);
    v->addLayout(bar);

    m_out = new QPlainTextEdit(this);
    m_out->setReadOnly(true);
    m_out->setMaximumBlockCount(5000);
    m_out->setPlaceholderText(tr("Kết quả của tool hiện ở đây."));
    v->addWidget(m_out, 1);

    m_runner = new ToolRunner(this);
    m_runner->setToolId(spec.id);
    connect(m_run, &QPushButton::clicked, this, &ExternalToolWidget::run);
    connect(m_stop, &QPushButton::clicked, this, &ExternalToolWidget::stopRun);
    connect(m_runner, &ToolRunner::output, this, [this](const QString& t, bool isErr) {
        m_out->moveCursor(QTextCursor::End);
        Q_UNUSED(isErr)
        m_out->insertPlainText(t); // lỗi và kết quả cùng một khung
        m_out->verticalScrollBar()->setValue(m_out->verticalScrollBar()->maximum());
    });
    connect(m_runner, &ToolRunner::finished, this, [this](int code, bool timedOut, bool stopped, bool crashed) {
        m_run->setEnabled(true);
        m_stop->setEnabled(false);
        if (timedOut) m_status->setText(tr("Đã dừng vì quá %1 giây.").arg(m_spec.timeoutSec));
        else if (stopped) m_status->setText(tr("Đã dừng theo yêu cầu."));
        else if (crashed) m_status->setText(tr("Tool bị lỗi và đóng đột ngột."));
        else if (code == 0) m_status->setText(tr("Xong (mã 0)."));
        else m_status->setText(tr("Kết thúc với mã lỗi %1.").arg(code));
    });
}

QMap<QString, QString> ExternalToolWidget::collect() const {
    QMap<QString, QString> v;
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        QWidget* w = it.value();
        if (auto* e = qobject_cast<QLineEdit*>(w)) v[it.key()] = e->text();
        else if (auto* p = qobject_cast<QPlainTextEdit*>(w)) v[it.key()] = p->toPlainText();
        else if (auto* c = qobject_cast<QComboBox*>(w)) v[it.key()] = c->currentText();
        else if (auto* le = qobject_cast<QLineEdit*>(w->property("lineEdit").value<QObject*>())) v[it.key()] = le->text();
    }
    return v;
}

bool ExternalToolWidget::ensureConfirmed() {
    if (isConfirmed(m_spec)) return true;
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Cho phép tool ngoài chạy?"));
    box.setText(tr("Tool \"%1\" sẽ chạy một chương trình trên máy bạn, với quyền của bạn.").arg(m_spec.name));
    box.setInformativeText(tr("Lệnh: %1\nThư mục tool: %2\n\nChỉ cho phép nếu bạn tin nguồn của tool này. Nếu tool bị sửa sau này, Dũng Office sẽ hỏi lại.")
                               .arg(describeCommand(m_spec), m_spec.dir));
    auto* ok = box.addButton(tr("Tôi hiểu, cho phép tool này"), QMessageBox::AcceptRole);
    box.addButton(tr("Không"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != ok) return false;
    setConfirmed(m_spec);
    return true;
}

void ExternalToolWidget::run() {
    if (!ensureConfirmed()) { m_status->setText(tr("Chưa chạy: bạn chưa cho phép tool này.")); return; }
    const Prepared p = prepare(m_spec, collect());
    if (!p.ok) {
        m_status->setText(p.error);
        return;
    }
    m_out->clear();
    QString err;
    if (!m_runner->start(p.cmd, &err)) { m_status->setText(err); return; }
    m_run->setEnabled(false);
    m_stop->setEnabled(true);
    m_status->setText(tr("Đang chạy…"));
}

void ExternalToolWidget::stopRun() { m_runner->stop(); }

} // namespace toolbox
