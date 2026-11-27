// SPDX-License-Identifier: MPL-2.0
// Bảng quản trị công việc bên phải: Ghi chú, Lịch, Liên hệ (có trạng thái). Lưu cục bộ, không mạng.
#pragma once
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>
#include <QList>

class QCalendarWidget;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTableWidget;
class QStackedWidget;
class QPushButton;
class QLabel;
class QTimer;

class SidePanel : public QWidget {
    Q_OBJECT
public:
    explicit SidePanel(QWidget* parent = nullptr);
    void toggleContent();
    void showPage(int index);
    bool flush(); // ghi ngay các thay đổi đang chờ (gọi khi đóng ứng dụng)

private:
    QWidget* buildNotes();
    QWidget* buildCalendar();
    QWidget* buildContacts();
    void load();
    void scheduleSave();
    bool saveNow();
    void refreshNoteList();
    void refreshEvents();
    void refreshContacts();
    void markCalendar();

    QWidget* m_content = nullptr;
    QStackedWidget* m_tabs = nullptr;
    QLabel* m_title = nullptr;
    QList<QPushButton*> m_buttons;
    int m_openPage = -1;
    int m_lastPage = 0;
    QTimer* m_saveTimer;
    bool m_loading = false;
    bool m_dirty = false;
    bool m_readable = false;
    QJsonObject m_workspaceBase;

    // Ghi chú
    QJsonArray m_notes;
    QListWidget* m_noteList = nullptr;
    QPlainTextEdit* m_noteText = nullptr;
    int m_noteRow = -1;

    // Lịch
    QJsonArray m_events;
    QJsonObject m_dayNotes;
    QPlainTextEdit* m_dayNote = nullptr;
    QCalendarWidget* m_cal = nullptr;
    QLabel* m_dateTitle = nullptr;
    QListWidget* m_eventList = nullptr;
    QLineEdit* m_eventTitle = nullptr;

    // Liên hệ
    QJsonArray m_contacts;
    QTableWidget* m_table = nullptr;
    QComboBox* m_filter = nullptr;
    QLineEdit* m_search = nullptr;
};
