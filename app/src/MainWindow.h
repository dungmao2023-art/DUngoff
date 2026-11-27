// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "dung/FormatPolicy.h"

#include <QMainWindow>

class QStackedWidget;
class QTabWidget;
class QComboBox;
class QLabel;
class QSlider;
class Ribbon;
class HomePage;
class DocumentTab;
class SidePanel;
class ProjectsPage;
class TemplatesPage;
class ToolboxPage;
class QPushButton;
namespace shell { class TopBar; class QuickPalette; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void openFiles(const QStringList& paths);
    // Chế độ chụp giao diện (kiểm thử): lưu PNG các trang ở nhiều cỡ cửa sổ + nhật ký kiểm tra chồng/cắt chữ.
    int captureShots(const QString& dir);
    int checkFloatLifecycle(const QString& dir);

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    QWidget* buildSidebar();
    QWidget* buildFeedbackPage();
    void showFeedback();
    void showFloatMenu();
    void restoreFromFloat(dung::DocKind kind);
    DocumentTab* current() const;
    void openDialog(const QString& filter = QString());
    void openPath(const QString& path);
    void offerRecovery();  // hộp thoại "Khôi phục phiên trước" nếu còn bản mồ côi
    void newDoc(dung::DocKind kind);
    void addTab(DocumentTab* tab);
    bool closeTab(int index);
    void showHome();
    void showDocuments();
    void showProjects();
    void showWork(int index);
    void showTemplates();
    void showToolbox();
    void onCurrentTabChanged();
    void selectDocumentTab(int index);
    void refreshDocumentGroups();
    void onAppAction(const QString& id);
    void addRecent(const QString& path);
    QStringList recent() const;
    void refreshStatus();
    void showAbout();
    void showPalette();
    void runCommand(const QString& id);   // lệnh của bảng Ctrl+K / liên kết nhanh
    void activateKind(const QString& kind); // thẻ trên cùng: Word/Excel/PowerPoint/PDF
    void applyCompact(bool compact);

    QStackedWidget* m_stack;
    HomePage* m_home;
    QWidget* m_docArea;
    QWidget* m_feedback = nullptr;
    Ribbon* m_ribbon;
    QTabWidget* m_tabs;
    QComboBox* m_docGroup = nullptr;
    bool m_groupChanging = false;
    QLabel* m_status;
    QLabel* m_roLabel;
    QLabel* m_toolLabel;
    QLabel* m_saveLabel;
    SidePanel* m_side;
    ProjectsPage* m_projects;
    TemplatesPage* m_templates;
    ToolboxPage* m_toolbox;
    QLabel* m_zoomLabel;
    QSlider* m_zoom;
    shell::TopBar* m_top = nullptr;
    shell::QuickPalette* m_palette = nullptr;
    QWidget* m_sideBar = nullptr;
    QList<QPushButton*> m_navButtons;
    QList<QWidget*> m_navFullOnly;
    bool m_compact = false;
    QWidget* m_floatMenu = nullptr;
    bool m_floatSuppressed = false;
    bool m_restoreMaximized = false;
};
