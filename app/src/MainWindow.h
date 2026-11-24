// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "dung/FormatPolicy.h"

#include <QMainWindow>

class QStackedWidget;
class QTabWidget;
class QLabel;
class QSlider;
class Ribbon;
class HomePage;
class DocumentTab;
class SidePanel;
class ProjectsPage;
class TemplatesPage;
class ToolboxPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void openFiles(const QStringList& paths);

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    QWidget* buildSidebar();
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
    void showTemplates();
    void showToolbox();
    void onCurrentTabChanged();
    void onAppAction(const QString& id);
    void addRecent(const QString& path);
    QStringList recent() const;
    void refreshStatus();
    void showAbout();

    QStackedWidget* m_stack;
    HomePage* m_home;
    QWidget* m_docArea;
    Ribbon* m_ribbon;
    QTabWidget* m_tabs;
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
};
