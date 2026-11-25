#define LOK_USE_UNSTABLE_API
#include "PptActions.h"
#include "../DocumentTab.h"
#include "../TileView.h"
#include "../EngineHost.h"
#include "SlideShow.h"
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <QFileDialog>
#include <QInputDialog>
#include <QColorDialog>
#include "../word/WordCommands.h"
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
    if (!tab || tab->kind() != dung::DocKind::Presentation) return true;
    dung::Document* d = tab->document();
    if (id == QLatin1String("ppt.show") || id == QLatin1String("ppt.showHere")) {
        tab->setPptNotesMode(false);
        const int start = id == QLatin1String("ppt.show") ? 0 : d->currentPart();
        new SlideShow(d, start, tab);
    } else if (id == QLatin1String("ppt.notes") || id == QLatin1String("ppt.normal")) {
        tab->setPptNotesMode(id == QLatin1String("ppt.notes"));
    } else if (id == QLatin1String("ppt.fontSize")) {
        if (d->readOnly()) return true;
        bool ok = false;
        const double size = QInputDialog::getDouble(tab, QObject::tr("Cỡ chữ"), QObject::tr("Điểm (pt):"), 24, 1, 400, 1, &ok);
        if (ok) { const auto cmd = dung::word::fontSize(size); d->command(cmd.first, cmd.second); }
        tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.textColor")) {
        if (d->readOnly()) return true;
        const auto color = QColorDialog::getColor(Qt::black, tab, QObject::tr("Màu chữ"));
        if (color.isValid()) { const auto cmd = dung::word::fontColor(color.rgb()); d->command(cmd.first, cmd.second); }
        tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.goto")) {
        const int n = d->partCount();
        if (n < 1) return true;
        bool ok = false;
        const int page = QInputDialog::getInt(tab, QObject::tr("Đến trang chiếu"), QObject::tr("Trang:"), d->currentPart() + 1, 1, n, 1, &ok);
        if (ok) tab->stepPart(page - 1 - d->currentPart());
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
