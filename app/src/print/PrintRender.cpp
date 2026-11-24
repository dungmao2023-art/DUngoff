#include "PrintRender.h"
#include <algorithm>
#include <QPainter>
#include <QPrinter>
#include <QPageLayout>
#include <QCoreApplication>
#ifdef DUNG_HAVE_QTPDF
#include <QPdfDocument>
#endif

namespace printrender {

#ifdef DUNG_HAVE_QTPDF

bool available() { return true; }

static QString pdfErr(QPdfDocument::Error e) {
    switch (e) {
    case QPdfDocument::Error::None: return {};
    case QPdfDocument::Error::FileNotFound: return QObject::tr("Không thấy tệp PDF.");
    case QPdfDocument::Error::InvalidFileFormat: return QObject::tr("Tệp PDF không hợp lệ hoặc bị hỏng.");
    case QPdfDocument::Error::IncorrectPassword: return QObject::tr("PDF có mật khẩu.");
    case QPdfDocument::Error::UnsupportedSecurityScheme: return QObject::tr("PDF dùng kiểu mã hoá không được hỗ trợ.");
    default: return QObject::tr("Không đọc được PDF.");
    }
}

PdfInfo pdfInfo(const QString& path) {
    PdfInfo r;
    QPdfDocument d;
    const auto e = d.load(path);
    if (e != QPdfDocument::Error::None) { r.error = pdfErr(e); return r; }
    r.pages = d.pageCount();
    r.ok = r.pages > 0;
    if (!r.ok) r.error = QObject::tr("PDF không có trang nào.");
    return r;
}

QImage renderPage(const QString& path, int page1, const QSize& box, QString* error) {
    QPdfDocument d;
    const auto e = d.load(path);
    if (e != QPdfDocument::Error::None) { if (error) *error = pdfErr(e); return {}; }
    if (page1 < 1 || page1 > d.pageCount()) { if (error) *error = QObject::tr("Trang %1 không tồn tại.").arg(page1); return {}; }
    const QSizeF pt = d.pagePointSize(page1 - 1);
    if (pt.isEmpty()) { if (error) *error = QObject::tr("Trang có kích thước bằng 0."); return {}; }
    const double k = std::min(box.width() / pt.width(), box.height() / pt.height());
    const QSize px(std::max(1, int(pt.width() * k)), std::max(1, int(pt.height() * k)));
    QImage img = d.render(page1 - 1, px);
    if (img.isNull()) { if (error) *error = QObject::tr("Không vẽ được trang %1.").arg(page1); return {}; }
    // Nền trong suốt → nền trắng để xem/in đúng như giấy.
    QImage out(img.size(), QImage::Format_ARGB32);
    out.fill(Qt::white);
    QPainter p(&out);
    p.drawImage(0, 0, img);
    return out;
}

bool printPages(const QString& path, QPrinter& printer, const QVector<int>& pages, bool autoOrientation, QString* error) {
    QPdfDocument d;
    const auto e = d.load(path);
    if (e != QPdfDocument::Error::None) { if (error) *error = pdfErr(e); return false; }
    if (pages.isEmpty()) { if (error) *error = QObject::tr("Chưa chọn trang để in."); return false; }
    for (int pg : pages)
        if (pg < 1 || pg > d.pageCount()) { if (error) *error = QObject::tr("Trang %1 không tồn tại.").arg(pg); return false; }

    auto layoutFor = [&](int pg) {
        const QSizeF pt = d.pagePointSize(pg - 1);
        if (autoOrientation)
            printer.setPageOrientation(pt.width() > pt.height() ? QPageLayout::Landscape : QPageLayout::Portrait);
    };
    layoutFor(pages.first());
    QPainter painter;
    if (!painter.begin(&printer)) { if (error) *error = QObject::tr("Không mở được máy in hoặc tệp đích."); return false; }
    bool first = true;
    for (int pg : pages) {
        if (!first) {
            layoutFor(pg);
            if (!printer.newPage()) { painter.end(); if (error) *error = QObject::tr("Máy in từ chối trang tiếp theo."); return false; }
        }
        first = false;
        const QRect area = printer.pageLayout().paintRectPixels(printer.resolution());
        const QSizeF pt = d.pagePointSize(pg - 1);
        const double k = std::min(area.width() / pt.width(), area.height() / pt.height());
        const QSize px(std::max(1, int(pt.width() * k)), std::max(1, int(pt.height() * k)));
        QImage img = d.render(pg - 1, px);
        if (img.isNull()) { painter.end(); if (error) *error = QObject::tr("Không vẽ được trang %1.").arg(pg); return false; }
        QImage white(img.size(), QImage::Format_ARGB32);
        white.fill(Qt::white);
        { QPainter q(&white); q.drawImage(0, 0, img); }
        painter.drawImage(QPoint((area.width() - px.width()) / 2, (area.height() - px.height()) / 2), white);
    }
    painter.end();
    return true;
}

#else  // Không có Qt6::Pdf

bool available() { return false; }
static const char* kNo = QT_TRANSLATE_NOOP("printrender", "Bản dựng này không có bộ vẽ PDF (Qt6::Pdf).");
PdfInfo pdfInfo(const QString&) { PdfInfo r; r.error = QCoreApplication::translate("printrender", kNo); return r; }
QImage renderPage(const QString&, int, const QSize&, QString* error) { if (error) *error = QCoreApplication::translate("printrender", kNo); return {}; }
bool printPages(const QString&, QPrinter&, const QVector<int>&, bool, QString* error) { if (error) *error = QCoreApplication::translate("printrender", kNo); return false; }

#endif

} // namespace printrender
