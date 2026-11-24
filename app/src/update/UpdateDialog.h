// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "UpdateCore.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QCheckBox;
class QTextBrowser;
class QProgressBar;
class UpdateChecker;

class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    static bool autoCheckEnabled();
    static bool autoCheckDue();      // bật + quá 24 giờ kể từ lần cuối
    static void markChecked();

private:
    void startCheck();
    void onChecked(const update::Release& r, bool newer, const QString& message);

    UpdateChecker* m_checker;
    update::Release m_release;
    QString m_installer;
    QLabel* m_status;
    QTextBrowser* m_notes;
    QProgressBar* m_bar;
    QPushButton *m_check, *m_download, *m_run, *m_page;
    QCheckBox* m_auto;
};
