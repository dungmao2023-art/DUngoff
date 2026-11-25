// Dũng Office — điểm vào ứng dụng.
// SPDX-License-Identifier: MPL-2.0
#include "MainWindow.h"
#include "EngineHost.h"
#include "ExitTrace.h"
#include "recovery/RecoveryStore.h"
#include "SingleInstance.h"
#include "Theme.h"
#include "update/UpdateChecker.h"
#include "update/UpdateDialog.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QStatusBar>
#include <QSettings>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

    // Chế độ chụp giao diện cho CI: DUNG_UI_SHOT_DIR=<thư mục> (kèm DUNG_NO_ENGINE=1) — không động tới phiên/engine.
    if (qEnvironmentVariableIsSet("DUNG_UI_SHOT_DIR")) {
        qputenv("DUNG_NO_ENGINE", "1");
        MainWindow w;
        return w.captureShots(QString::fromLocal8Bit(qgetenv("DUNG_UI_SHOT_DIR"))) == 0 ? 0 : 3;
    }

    // Một phiên duy nhất: nhấp đúp tệp khi app đang chạy -> chuyển tệp sang cửa sổ cũ rồi thoát.
    const QStringList files = QApplication::arguments().mid(1);
    SingleInstance single;
    if (single.sendToRunning(files)) return 0;
    single.listen();  // không mở được kênh thì vẫn chạy bình thường, chỉ mất tính năng một phiên

    recovery::Session::instance().begin();
    int rc = 0;
    {
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
        exittrace::log("main: vào vòng lặp sự kiện");
        rc = app.exec();
        exittrace::log("main: app.exec() trả về");
    }
    exittrace::log("main: cửa sổ và các tab đã đóng");
    recovery::Session::instance().end();  // thoát bình thường: xoá bản phục hồi của phiên này
    exittrace::log("main: recovery đã kết thúc phiên");
    QSettings().sync();
    exittrace::log("main: QSettings đã ghi");

#ifdef Q_OS_WIN
    // Thoát cứng có điều kiện. Trên Windows, phần dọn dẹp của LibreOffice (lok destroy, DLL detach) có thể
    // treo hoặc sập khi thoát, làm tiến trình sống mãi sau khi cửa sổ đã đóng (khoá tệp, chặn gỡ/cập nhật).
    // Chỉ thoát cứng khi MỌI tài liệu đã đóng (closeEvent đã hỏi người dùng lưu hay bỏ từng tài liệu chưa lưu)
    // và mọi trạng thái đã ghi xong ở trên. Còn tài liệu mở thì đi đường thoát bình thường, không ép.
    // DUNG_EXIT_CLEAN=1: bỏ thoát cứng để chẩn đoán đường thoát bình thường (xem DUNG_TRACE_EXIT).
    const bool wantClean = qEnvironmentVariableIntValue("DUNG_EXIT_CLEAN") == 1;
    if (!wantClean && dung::liveDocumentCount() == 0) {
        exittrace::log("main: thoát cứng (không còn tài liệu mở)");
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(rc));
    }
    exittrace::log(wantClean ? "main: DUNG_EXIT_CLEAN=1, thoát bình thường" : "main: còn tài liệu mở, thoát bình thường");
#endif
    return rc;
}

