// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QWidget>

class QListWidget;
class QLabel;
class QGridLayout;
class QFrame;

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(QWidget* parent = nullptr);
    void setRecent(const QStringList& files);
    void setEngineStatus(const QString& text);

signals:
    void newWorkbook();
    void newDocument();
    void newPresentation();
    void openPdf();
    void openFile();
    void openPath(const QString& path);
    void openTemplates();
    void openToolbox();

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void relayout();
    QListWidget* m_recent = nullptr;
    QLabel* m_status = nullptr;
    QGridLayout* m_featGrid = nullptr;
    QGridLayout* m_quickGrid = nullptr;
    QList<QWidget*> m_feat;
    QList<QWidget*> m_quick;
    int m_featCols = 0;
    int m_quickCols = 0;
};
