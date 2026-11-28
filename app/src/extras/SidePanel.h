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
class QTabWidget;

class SidePanel : public QWidget {
    Q_OBJECT
public:
    explicit SidePanel(QWidget* parent = nullptr);
    void toggleContent();
    void showPage(int index);
    QJsonObject exportWorkProject() const;
    bool importWorkProject(const QJsonObject& snapshot);
    bool flush(); // ghi ngay các thay đổi đang chờ (gọi khi đóng ứng dụng)

private:
    QWidget* buildNotes();
    QWidget* buildWorkManager();
    QWidget* buildCalendar();
    void refreshProjects(const QString& select = {});
    void refreshBoard();
    void editProject(bool create);
    void editTask(const QString& id, int status = 0);
    void moveTask(const QString& id, int status);
    int projectIndex() const;
    int taskIndex(const QString& id) const;
    QJsonArray m_workProjects;
    QJsonArray m_tasks;
    QComboBox* m_projectPicker = nullptr;
    QLineEdit* m_taskSearch = nullptr;
    QComboBox* m_taskPersonFilter = nullptr;
    QPushButton* m_editProject = nullptr;
    QTabWidget* m_workViews = nullptr;
    QListWidget* m_board[4]{};
    QLabel* m_columnTitles[4]{};
    QPushButton* m_addTask[4]{};
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
    QLabel* m_saveStatus = nullptr;
    QPushButton* m_retrySave = nullptr;
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
