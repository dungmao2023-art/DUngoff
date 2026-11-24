#include "PptActions.h"
#include "../DocumentTab.h"
#include "../EngineHost.h"
#include "SlideShow.h"
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>

static void dragShape(dung::Document* d, const char* uno) {
    d->command(uno);
    long w = 0, h = 0;
    d->documentSize(w, h);
    // Kéo một vùng ở giữa trang để tạo đối tượng
    d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, int(w * 0.30), int(h * 0.35), 1, 1, 0);
    d->postMouse(LOK_MOUSEEVENT_MOUSEMOVE, int(w * 0.50), int(h * 0.50), 1, 1, 0);
    d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, int(w * 0.70), int(h * 0.65), 1, 1, 0);
}

bool handlePptAction(DocumentTab* tab, const QString& id) {
    if (!id.startsWith(QLatin1String("ppt."))) return false;
    dung::Document* d = tab->document();
    if (id == QLatin1String("ppt.show") || id == QLatin1String("ppt.showHere")) {
        const int start = id == QLatin1String("ppt.show") ? 0 : d->currentPart();
        new SlideShow(d, start);
    } else if (id == QLatin1String("ppt.textbox")) {
        dragShape(d, ".uno:Text");
    } else if (id == QLatin1String("ppt.rect")) {
        dragShape(d, ".uno:BasicShapes.rectangle");
    } else if (id == QLatin1String("ppt.image")) {
        const QString f = QFileDialog::getOpenFileName(tab, QObject::tr("Chèn ảnh"), QString(),
                                                       QObject::tr("Ảnh (*.png *.jpg *.jpeg *.bmp *.gif)"));
        if (f.isEmpty()) return true;
        d->command(".uno:InsertGraphic", dung::Document::stringArg("FileName", ss(QUrl::fromLocalFile(f).toString())));
    } else if (id == QLatin1String("ppt.transitionNote")) {
        QMessageBox::information(tab, QObject::tr("Hiệu ứng chuyển cảnh"),
            QObject::tr("Hiệu ứng chuyển cảnh và hoạt ảnh (animation) chưa được hỗ trợ chỉnh sửa và chưa bảo đảm giữ nguyên "
                        "khi lưu. Hãy lưu thành bản mới và kiểm tra lại các hiệu ứng trong PowerPoint."));
    }
    return true;
}
