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
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
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
    setAttribute(Qt::WA_StyledBackground, true);
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("WorkContent"));
    m_content->setAttribute(Qt::WA_StyledBackground, true);
    m_content->setFixedWidth(360);
    auto* v = new QVBoxLayout(m_content);
    v->setContentsMargins(12, 12, 12, 12);
    v->setSpacing(10);
    auto* heading = new QHBoxLayout;
    m_title = new QLabel(tr("Ghi chú"), m_content);
    m_title->setObjectName(QStringLiteral("WorkTitle"));
    heading->addWidget(m_title, 1);
    auto* close = new QPushButton(tr("×"), m_content);
    close->setObjectName(QStringLiteral("WorkClose"));
    close->setFixedSize(30, 30);
    close->setToolTip(tr("Thu gọn quản trị công việc"));
    close->setAccessibleName(close->toolTip());
    heading->addWidget(close);
    v->addLayout(heading);
    m_tabs = new QStackedWidget(m_content);
    for (QWidget* page : {buildNotes(), buildCalendar(), buildContacts()}) {
        auto* scroll = new QScrollArea(m_tabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(page);
        m_tabs->addWidget(scroll);
    }
    v->addWidget(m_tabs, 1);
    auto* foot = new QLabel(tr("Lưu cục bộ · Không gửi lên mạng"), m_content);
    foot->setObjectName(QStringLiteral("WorkHint"));
    foot->setWordWrap(true);
    v->addWidget(foot);
    outer->addWidget(m_content);

    auto* rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("WorkRail"));
    rail->setAttribute(Qt::WA_StyledBackground, true);
    rail->setFixedWidth(72);
    auto* rv = new QVBoxLayout(rail);
    rv->setContentsMargins(4, 8, 4, 8);
    rv->setSpacing(4);
    const QStringList labels{tr("Ghi chú"), tr("Lịch"), tr("Liên hệ")};
    const QStringList glyphs{QStringLiteral("✎"), QStringLiteral("▦"), QStringLiteral("♙")};
    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(glyphs[i] + QLatin1Char('\n') + labels[i], rail);
        button->setObjectName(QStringLiteral("WorkTab"));
        button->setCheckable(true);
        button->setFixedHeight(58);
        button->setToolTip(labels[i]);
        button->setAccessibleName(labels[i]);
        button->setCursor(Qt::PointingHandCursor);
        m_buttons.append(button);
        rv->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] {
            showPage(m_openPage == i ? -1 : i);
        });
    }
    rv->addStretch(1);
    outer->addWidget(rail);
    connect(close, &QPushButton::clicked, this, [this] { showPage(-1); });
    setStyleSheet(QStringLiteral(R"(
QWidget#SidePanel, QWidget#WorkRail { background: #FBFCFF; }
QWidget#WorkRail { border-left: 1px solid #DCE5F2; }
QWidget#WorkContent { background: #F4F8FF; border-left: 1px solid #DCE5F2; }
QLabel#WorkTitle { font-size: 17px; font-weight: 700; color: #17294D; }
QLabel#WorkHint { font-size: 11px; color: #6B7A99; }
QPushButton#WorkTab { background: transparent; border: none; border-right: 3px solid transparent; border-radius: 8px; font-size: 11px; color: #4A5A7C; }
QPushButton#WorkTab:hover, QPushButton#WorkTab:focus { background: #EEF4FF; }
QPushButton#WorkTab:checked { background: #E8F0FF; border-right: 3px solid #2864BA; color: #1B4A90; font-weight: 700; }
QWidget#WorkContent QPushButton { background: white; border: 1px solid #DCE5F2; border-radius: 8px; padding: 5px 8px; color: #1B4A90; }
QWidget#WorkContent QPushButton:hover, QWidget#WorkContent QPushButton:focus { background: #EEF4FF; border-color: #2864BA; }
QWidget#WorkContent QPushButton#WorkPrimary { background: #2864BA; color: white; font-weight: 600; }
QWidget#WorkContent QPushButton#WorkPrimary:hover { background: #1B4A90; }
QWidget#WorkContent QPushButton#WorkClose { padding: 0; font-size: 18px; }
QWidget#WorkContent QPlainTextEdit, QWidget#WorkContent QListWidget, QWidget#WorkContent QTableWidget { background: white; border: 1px solid #DCE5F2; border-radius: 8px; padding: 4px; selection-background-color: #E8F0FF; selection-color: #17294D; }
QWidget#WorkContent QPlainTextEdit:focus, QWidget#WorkContent QLineEdit:focus { border-color: #2864BA; }
QWidget#WorkContent QListWidget::item { padding: 6px; border-radius: 4px; }
QWidget#WorkContent QHeaderView::section { background: #EEF4FF; border: none; padding: 6px; font-weight: 600; color: #4A5A7C; }
QWidget#WorkContent QComboBox { background: white; border: 1px solid #DCE5F2; border-radius: 6px; padding: 4px; }
QWidget#WorkContent QCalendarWidget { background: white; border: 1px solid #DCE5F2; border-radius: 8px; }
QWidget#WorkContent QCalendarWidget QToolButton { color: #1B4A90; background: #EEF4FF; border: none; padding: 3px; }
QWidget#WorkContent QCalendarWidget QWidget#qt_calendar_navigationbar { background: #EEF4FF; }
QWidget#WorkContent QCalendarWidget QAbstractItemView { selection-background-color: #2864BA; selection-color: white; }
)"));
    showPage(-1);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(800);
    connect(m_saveTimer, &QTimer::timeout, this, &SidePanel::saveNow);
    load();
}

void SidePanel::showPage(int index) {
    if (index < 0 || index >= m_buttons.size()) {
        m_openPage = -1;
        m_content->hide();
    } else {
        m_openPage = m_lastPage = index;
        m_tabs->setCurrentIndex(index);
        m_title->setText(m_buttons[index]->accessibleName());
        m_content->show();
    }
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons[i]->setChecked(i == m_openPage);
    setFixedWidth(m_openPage < 0 ? 72 : 432);
}

void SidePanel::toggleContent() {
    showPage(m_openPage < 0 ? m_lastPage : -1);
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
    o.insert(QStringLiteral("dayNotes"), m_dayNotes);
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
    m_dayNotes = o.value(QStringLiteral("dayNotes")).toObject();
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
    add->setObjectName(QStringLiteral("WorkPrimary"));
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
    v->addWidget(new QLabel(tr("Ghi chú ngày đang chọn"), w));
    m_dayNote = new QPlainTextEdit(w);
    m_dayNote->setPlaceholderText(tr("Ghi chú riêng cho ngày này…"));
    m_dayNote->setMaximumHeight(100);
    v->addWidget(m_dayNote);
    connect(m_dayNote, &QPlainTextEdit::textChanged, this, [this] {
        const QString day = m_cal->selectedDate().toString(Qt::ISODate);
        const QString note = m_dayNote->toPlainText();
        if (note.isEmpty()) m_dayNotes.remove(day); else m_dayNotes.insert(day, note);
        markCalendar(); scheduleSave();
    });
    auto* row = new QHBoxLayout;
    m_eventTitle = new QLineEdit(w);
    m_eventTitle->setPlaceholderText(tr("Việc / hạn chót cho ngày đang chọn"));
    auto* add = new QPushButton(tr("Thêm"), w);
    add->setObjectName(QStringLiteral("WorkPrimary"));
    row->addWidget(m_eventTitle, 1);
    row->addWidget(add);
    v->addLayout(row);
    m_eventList = new QListWidget(w);
    v->addWidget(m_eventList, 1);
    auto* del = new QPushButton(tr("Xoá mục đang chọn"), w);
    v->addWidget(del);
    auto* hint = new QLabel(tr("Nhấp đúp vào một việc để đánh dấu xong / chưa xong."), w);
    hint->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    hint->setWordWrap(true);
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
    { QSignalBlocker b(m_dayNote); m_dayNote->setPlainText(m_dayNotes.value(day).toString()); }
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
    for (auto it = m_dayNotes.constBegin(); it != m_dayNotes.constEnd(); ++it) {
        const QDate day = QDate::fromString(it.key(), Qt::ISODate);
        if (day.isValid() && !it.value().toString().isEmpty()) m_cal->setDateTextFormat(day, fmt);
    }
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
    v->addLayout(top);
    v->addWidget(m_filter);
    m_table = new QTableWidget(0, 4, w);
    m_table->setHorizontalHeaderLabels({tr("Tên"), tr("Điện thoại"), tr("Email"), tr("Trạng thái")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    v->addWidget(m_table, 1);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("+ Liên hệ"), w);
    add->setObjectName(QStringLiteral("WorkPrimary"));
    auto* del = new QPushButton(tr("Xoá"), w);
    row->addWidget(add);
    row->addWidget(del);
    v->addLayout(row);
    auto* sum = new QLabel(w);
    sum->setObjectName(QStringLiteral("ContactSummary"));
    sum->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    sum->setWordWrap(true);
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
