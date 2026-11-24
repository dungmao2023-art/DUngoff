// SPDX-License-Identifier: MPL-2.0
#include "SidePanel.h"

#include "WorkspaceStore.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QVector>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QStringList kStatuses = {QObject::tr("Mới"), QObject::tr("Đang liên hệ"), QObject::tr("Chờ phản hồi"),
                               QObject::tr("Hoàn tất"), QObject::tr("Ngừng")};
}

SidePanel::SidePanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("SidePanel"));
    setMinimumWidth(300);
    setMaximumWidth(380);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(8, 8, 8, 8);
    auto* title = new QLabel(tr("Quản trị công việc"), this);
    title->setStyleSheet(QStringLiteral("font-weight:700;color:#1F50BB;font-size:14px;"));
    v->addWidget(title);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildNotes(), tr("Ghi chú"));
    m_tabs->addTab(buildCalendar(), tr("Lịch"));
    m_tabs->addTab(buildContacts(), tr("Liên hệ"));
    v->addWidget(m_tabs, 1);
    auto* foot = new QLabel(tr("Lưu trên máy bạn, không gửi lên mạng."), this);
    foot->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    v->addWidget(foot);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(800);
    connect(m_saveTimer, &QTimer::timeout, this, &SidePanel::saveNow);
    load();
}

void SidePanel::flush() {
    if (m_saveTimer->isActive()) {
        m_saveTimer->stop();
        saveNow();
    }
}

void SidePanel::scheduleSave() {
    if (!m_loading) m_saveTimer->start();
}

void SidePanel::saveNow() {
    QJsonObject o;
    o.insert(QStringLiteral("notes"), m_notes);
    o.insert(QStringLiteral("events"), m_events);
    o.insert(QStringLiteral("contacts"), m_contacts);
    o.insert(QStringLiteral("version"), 1);
    if (!workspace::save(QStringLiteral("workspace"), o))
        QMessageBox::warning(this, tr("Dũng Office"), tr("Không lưu được ghi chú/lịch/liên hệ vào thư mục dữ liệu."));
}

void SidePanel::load() {
    m_loading = true;
    const QJsonObject o = workspace::load(QStringLiteral("workspace"));
    m_notes = o.value(QStringLiteral("notes")).toArray();
    m_events = o.value(QStringLiteral("events")).toArray();
    m_contacts = o.value(QStringLiteral("contacts")).toArray();
    refreshNoteList();
    refreshEvents();
    refreshContacts();
    markCalendar();
    m_loading = false;
}

// ---------------------------------------------------------------- Ghi chú
QWidget* SidePanel::buildNotes() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("+ Ghi chú mới"), w);
    auto* del = new QPushButton(tr("Xoá"), w);
    row->addWidget(add);
    row->addWidget(del);
    v->addLayout(row);
    m_noteList = new QListWidget(w);
    m_noteList->setMaximumHeight(130);
    v->addWidget(m_noteList);
    m_noteText = new QPlainTextEdit(w);
    m_noteText->setPlaceholderText(tr("Gõ ghi chú ở đây. Dùng \"[ ] việc cần làm\" và \"[x] việc đã xong\" để lập danh sách việc."));
    v->addWidget(m_noteText, 1);

    connect(add, &QPushButton::clicked, this, [this] {
        QJsonObject n;
        n.insert(QStringLiteral("id"), QString::number(QDateTime::currentMSecsSinceEpoch()));
        n.insert(QStringLiteral("text"), QString());
        n.insert(QStringLiteral("updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
        m_notes.prepend(n);
        refreshNoteList();
        m_noteList->setCurrentRow(0);
        m_noteText->setFocus();
        scheduleSave();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const int r = m_noteList->currentRow();
        if (r < 0 || r >= m_notes.size()) return;
        if (QMessageBox::question(this, tr("Xoá ghi chú"), tr("Xoá ghi chú đang chọn?")) != QMessageBox::Yes) return;
        m_notes.removeAt(r);
        m_noteRow = -1;
        refreshNoteList();
        scheduleSave();
    });
    connect(m_noteList, &QListWidget::currentRowChanged, this, [this](int r) {
        m_noteRow = r;
        QSignalBlocker b(m_noteText);
        m_noteText->setPlainText(r >= 0 && r < m_notes.size() ? m_notes[r].toObject().value(QStringLiteral("text")).toString() : QString());
        m_noteText->setEnabled(r >= 0);
    });
    connect(m_noteText, &QPlainTextEdit::textChanged, this, [this] {
        if (m_noteRow < 0 || m_noteRow >= m_notes.size()) return;
        QJsonObject n = m_notes[m_noteRow].toObject();
        n.insert(QStringLiteral("text"), m_noteText->toPlainText());
        n.insert(QStringLiteral("updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
        m_notes[m_noteRow] = n;
        if (auto* it = m_noteList->item(m_noteRow)) {
            const QString first = m_noteText->toPlainText().section(QLatin1Char('\n'), 0, 0).trimmed();
            it->setText(first.isEmpty() ? tr("(ghi chú trống)") : first.left(40));
        }
        scheduleSave();
    });
    return w;
}

void SidePanel::refreshNoteList() {
    QSignalBlocker b(m_noteList);
    m_noteList->clear();
    for (const QJsonValue& v : m_notes) {
        const QString first = v.toObject().value(QStringLiteral("text")).toString().section(QLatin1Char('\n'), 0, 0).trimmed();
        m_noteList->addItem(first.isEmpty() ? tr("(ghi chú trống)") : first.left(40));
    }
    m_noteRow = -1;
    m_noteText->setEnabled(false);
    {
        QSignalBlocker bt(m_noteText);
        m_noteText->clear();
    }
    if (m_noteList->count() > 0) {
        m_noteList->setCurrentRow(0);
        m_noteRow = 0;
        m_noteText->setEnabled(true);
        QSignalBlocker bt(m_noteText);
        m_noteText->setPlainText(m_notes[0].toObject().value(QStringLiteral("text")).toString());
    }
}

// ---------------------------------------------------------------- Lịch
QWidget* SidePanel::buildCalendar() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    m_cal = new QCalendarWidget(w);
    m_cal->setGridVisible(true);
    m_cal->setFirstDayOfWeek(Qt::Monday);
    v->addWidget(m_cal);
    auto* row = new QHBoxLayout;
    m_eventTitle = new QLineEdit(w);
    m_eventTitle->setPlaceholderText(tr("Việc / hạn chót cho ngày đang chọn"));
    auto* add = new QPushButton(tr("Thêm"), w);
    row->addWidget(m_eventTitle, 1);
    row->addWidget(add);
    v->addLayout(row);
    m_eventList = new QListWidget(w);
    v->addWidget(m_eventList, 1);
    auto* del = new QPushButton(tr("Xoá mục đang chọn"), w);
    v->addWidget(del);
    auto* hint = new QLabel(tr("Nhấp đúp vào một việc để đánh dấu xong / chưa xong."), w);
    hint->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    v->addWidget(hint);

    auto addEvent = [this] {
        const QString t = m_eventTitle->text().trimmed();
        if (t.isEmpty()) return;
        QJsonObject e;
        e.insert(QStringLiteral("date"), m_cal->selectedDate().toString(Qt::ISODate));
        e.insert(QStringLiteral("title"), t);
        e.insert(QStringLiteral("done"), false);
        m_events.append(e);
        m_eventTitle->clear();
        refreshEvents();
        markCalendar();
        scheduleSave();
    };
    connect(add, &QPushButton::clicked, this, addEvent);
    connect(m_eventTitle, &QLineEdit::returnPressed, this, addEvent);
    connect(m_cal, &QCalendarWidget::selectionChanged, this, &SidePanel::refreshEvents);
    connect(m_eventList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        const int idx = it->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_events.size()) return;
        QJsonObject e = m_events[idx].toObject();
        e.insert(QStringLiteral("done"), !e.value(QStringLiteral("done")).toBool());
        m_events[idx] = e;
        refreshEvents();
        scheduleSave();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        auto* it = m_eventList->currentItem();
        if (!it) return;
        const int idx = it->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_events.size()) return;
        m_events.removeAt(idx);
        refreshEvents();
        markCalendar();
        scheduleSave();
    });
    return w;
}

void SidePanel::refreshEvents() {
    m_eventList->clear();
    const QString day = m_cal->selectedDate().toString(Qt::ISODate);
    for (int i = 0; i < m_events.size(); ++i) {
        const QJsonObject e = m_events[i].toObject();
        if (e.value(QStringLiteral("date")).toString() != day) continue;
        const bool done = e.value(QStringLiteral("done")).toBool();
        auto* it = new QListWidgetItem((done ? QStringLiteral("☑ ") : QStringLiteral("☐ ")) + e.value(QStringLiteral("title")).toString());
        it->setData(Qt::UserRole, i);
        if (done) {
            QFont f = it->font();
            f.setStrikeOut(true);
            it->setFont(f);
        }
        m_eventList->addItem(it);
    }
}

void SidePanel::markCalendar() {
    m_cal->setDateTextFormat(QDate(), QTextCharFormat()); // xoá toàn bộ định dạng ngày cũ
    QTextCharFormat fmt;
    fmt.setFontWeight(QFont::Bold);
    fmt.setBackground(QColor(0xDC, 0xE8, 0xFF));
    for (const QJsonValue& v : m_events) {
        const QDate d = QDate::fromString(v.toObject().value(QStringLiteral("date")).toString(), Qt::ISODate);
        if (d.isValid()) m_cal->setDateTextFormat(d, fmt);
    }
}

// ---------------------------------------------------------------- Liên hệ
QWidget* SidePanel::buildContacts() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* top = new QHBoxLayout;
    m_search = new QLineEdit(w);
    m_search->setPlaceholderText(tr("Tìm tên, số điện thoại, email…"));
    m_filter = new QComboBox(w);
    m_filter->addItem(tr("Mọi trạng thái"));
    m_filter->addItems(kStatuses);
    top->addWidget(m_search, 1);
    top->addWidget(m_filter);
    v->addLayout(top);
    m_table = new QTableWidget(0, 4, w);
    m_table->setHorizontalHeaderLabels({tr("Tên"), tr("Điện thoại"), tr("Email"), tr("Trạng thái")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    v->addWidget(m_table, 1);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("+ Liên hệ"), w);
    auto* del = new QPushButton(tr("Xoá"), w);
    row->addWidget(add);
    row->addWidget(del);
    v->addLayout(row);
    auto* sum = new QLabel(w);
    sum->setObjectName(QStringLiteral("ContactSummary"));
    sum->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    v->addWidget(sum);

    connect(add, &QPushButton::clicked, this, [this] {
        QJsonObject c;
        c.insert(QStringLiteral("name"), tr("Liên hệ mới"));
        c.insert(QStringLiteral("phone"), QString());
        c.insert(QStringLiteral("email"), QString());
        c.insert(QStringLiteral("status"), kStatuses.first());
        m_contacts.append(c);
        m_filter->setCurrentIndex(0);
        m_search->clear();
        refreshContacts();
        m_table->selectRow(m_table->rowCount() - 1);
        m_table->editItem(m_table->item(m_table->rowCount() - 1, 0));
        scheduleSave();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const int r = m_table->currentRow();
        if (r < 0) return;
        const int idx = m_table->item(r, 0)->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_contacts.size()) return;
        if (QMessageBox::question(this, tr("Xoá liên hệ"), tr("Xoá liên hệ \"%1\"?").arg(m_table->item(r, 0)->text())) != QMessageBox::Yes) return;
        m_contacts.removeAt(idx);
        refreshContacts();
        scheduleSave();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this] { refreshContacts(); });
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { refreshContacts(); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it) {
        if (m_loading || it->column() > 2) return;
        const int idx = m_table->item(it->row(), 0)->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_contacts.size()) return;
        static const char* keys[] = {"name", "phone", "email"};
        QJsonObject c = m_contacts[idx].toObject();
        c.insert(QLatin1String(keys[it->column()]), it->text());
        m_contacts[idx] = c;
        scheduleSave();
    });
    return w;
}

void SidePanel::refreshContacts() {
    const bool wasLoading = m_loading;
    m_loading = true;
    m_table->setRowCount(0);
    const QString q = m_search->text().trimmed().toLower();
    const int f = m_filter->currentIndex() - 1; // -1 = mọi trạng thái
    int shown = 0;
    QVector<int> counts(kStatuses.size(), 0);
    for (int i = 0; i < m_contacts.size(); ++i) {
        const QJsonObject c = m_contacts[i].toObject();
        const int si = qMax(0, kStatuses.indexOf(c.value(QStringLiteral("status")).toString()));
        counts[si]++;
        if (f >= 0 && si != f) continue;
        const QString name = c.value(QStringLiteral("name")).toString(), phone = c.value(QStringLiteral("phone")).toString(),
                      email = c.value(QStringLiteral("email")).toString();
        if (!q.isEmpty() && !(name + QLatin1Char(' ') + phone + QLatin1Char(' ') + email).toLower().contains(q)) continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto* n0 = new QTableWidgetItem(name);
        n0->setData(Qt::UserRole, i);
        m_table->setItem(r, 0, n0);
        m_table->setItem(r, 1, new QTableWidgetItem(phone));
        m_table->setItem(r, 2, new QTableWidgetItem(email));
        auto* cb = new QComboBox(m_table);
        cb->addItems(kStatuses);
        cb->setCurrentIndex(si);
        m_table->setCellWidget(r, 3, cb);
        connect(cb, &QComboBox::currentIndexChanged, this, [this, i](int s) {
            if (i < 0 || i >= m_contacts.size()) return;
            QJsonObject c = m_contacts[i].toObject();
            c.insert(QStringLiteral("status"), kStatuses.value(s));
            m_contacts[i] = c;
            scheduleSave();
        });
        ++shown;
    }
    m_table->resizeColumnToContents(1);
    QStringList parts;
    for (int s = 0; s < kStatuses.size(); ++s) parts << QStringLiteral("%1: %2").arg(kStatuses[s]).arg(counts[s]);
    if (auto* sum = findChild<QLabel*>(QStringLiteral("ContactSummary")))
        sum->setText(tr("Hiển thị %1/%2  •  ").arg(shown).arg(m_contacts.size()) + parts.join(QStringLiteral("  •  ")));
    m_loading = wasLoading;
}
