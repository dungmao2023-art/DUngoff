// Dũng Office — điểm vào ứng dụng.
// SPDX-License-Identifier: MPL-2.0
#include "MainWindow.h"
#include "SingleInstance.h"
#include "Theme.h"
#include "update/UpdateChecker.h"
#include "update/UpdateDialog.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QStatusBar>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DungOffice"));
    QApplication::setApplicationDisplayName(QStringLiteral("Dũng Office"));
    QApplication::setOrganizationName(QStringLiteral("DungOffice"));
    QApplication::setApplicationVersion(QStringLiteral(DUNG_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/dungoffice.png")));
    QApplication::setStyle(QStringLiteral("Fusion"));

    QFont f(QStringLiteral("Segoe UI"));
    f.setPointSizeF(9.5);
    QApplication::setFont(f);
    app.setStyleSheet(theme::styleSheet());

    // Một phiên duy nhất: nhấp đúp tệp khi app đang chạy -> chuyển tệp sang cửa sổ cũ rồi thoát.
    const QStringList files = QApplication::arguments().mid(1);
    SingleInstance single;
    if (single.sendToRunning(files)) return 0;
    single.listen();  // không mở được kênh thì vẫn chạy bình thường, chỉ mất tính năng một phiên

    MainWindow w;
    w.show();
    QObject::connect(&single, &SingleInstance::filesReceived, &w, &MainWindow::openFiles);
    QObject::connect(&single, &SingleInstance::raiseRequested, &w, [&w] {
        if (w.isMinimized()) w.showNormal();
        w.raise();
        w.activateWindow();
    });
    if (!files.isEmpty()) w.openFiles(files);

    // Tự kiểm tra cập nhật: MẶC ĐỊNH TẮT; chỉ khi người dùng tự bật, tối đa mỗi ngày một lần, chỉ hiện thông báo nhỏ.
    if (UpdateDialog::autoCheckDue()) {
        QTimer::singleShot(8000, &w, [&w] {
            auto* uc = new UpdateChecker(&w);
            QObject::connect(uc, &UpdateChecker::checked, &w, [&w, uc](const update::Release&, bool newer, const QString&) {
                UpdateDialog::markChecked();
                if (newer) w.statusBar()->showMessage(QObject::tr("Có bản mới của Dũng Office — vào Dự án › Update để xem."), 20000);
                uc->deleteLater();
            });
            uc->checkLatest();
        });
    }
    return app.exec();
}
