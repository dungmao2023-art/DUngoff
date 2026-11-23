// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QWidget>

class QListWidget;
class QLabel;

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

private:
    QListWidget* m_recent = nullptr;
    QLabel* m_status = nullptr;
};
