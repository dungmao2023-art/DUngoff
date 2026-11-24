// Dũng Office — điểm vào ứng dụng.
// SPDX-License-Identifier: MPL-2.0
#include "MainWindow.h"
#include "recovery/RecoveryStore.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QIcon>

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

    recovery::Session::instance().begin();
    int rc = 0;
    {
        MainWindow w;
        w.show();
        QStringList files = QApplication::arguments().mid(1);
        if (!files.isEmpty()) w.openFiles(files);
        rc = app.exec();
    }
    recovery::Session::instance().end();  // thoát bình thường: xoá bản phục hồi của phiên này
    return rc;
}
