// SPDX-License-Identifier: MPL-2.0
// Bảng quản trị công việc bên phải: Ghi chú, Lịch, Liên hệ (có trạng thái). Lưu cục bộ, không mạng.
#pragma once
#include <QDate>
#include <QJsonArray>
#include <QWidget>

class QCalendarWidget;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTableWidget;
class QTabWidget;
class QTimer;

class SidePanel : public QWidget {
    Q_OBJECT
public:
    explicit SidePanel(QWidget* parent = nullptr);
    void flush(); // ghi ngay các thay đổi đang chờ (gọi khi đóng ứng dụng)

private:
    QWidget* buildNotes();
    QWidget* buildCalendar();
    QWidget* buildContacts();
    void load();
    void scheduleSave();
    void saveNow();
    void refreshNoteList();
    void refreshEvents();
    void refreshContacts();
    void markCalendar();

    QTabWidget* m_tabs;
    QTimer* m_saveTimer;
    bool m_loading = false;

    // Ghi chú
    QJsonArray m_notes;
    QListWidget* m_noteList = nullptr;
    QPlainTextEdit* m_noteText = nullptr;
    int m_noteRow = -1;

    // Lịch
    QJsonArray m_events;
    QCalendarWidget* m_cal = nullptr;
    QListWidget* m_eventList = nullptr;
    QLineEdit* m_eventTitle = nullptr;

    // Liên hệ
    QJsonArray m_contacts;
    QTableWidget* m_table = nullptr;
    QComboBox* m_filter = nullptr;
    QLineEdit* m_search = nullptr;
};
