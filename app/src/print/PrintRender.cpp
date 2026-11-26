#include "PrintRender.h"
#include <algorithm>
#include <QPainter>
#include <QPrinter>
#include <QPageLayout>
#include <QCoreApplication>
#ifdef DUNG_HAVE_QTPDF
#include <QPdfDocument>
#include <QPdfSelection>
#include <QThread>
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

QString pageText(const QString& path,int page,QString* error) {
    if(error)error->clear();
    QPdfDocument d;
    const auto e=d.load(path);
    if(e!=QPdfDocument::Error::None){if(error)*error=pdfErr(e);return {};}
    if(page<1 || page>d.pageCount()){if(error)*error=QObject::tr("Trang không tồn tại.");return {};}
    return d.getAllText(page-1).text();
}

TextSearch searchText(const QString& path,const QString& query,int first,int last,bool matchCase) {
    TextSearch result;
    const QString needle=query.trimmed().normalized(QString::NormalizationForm_C);
    if(needle.isEmpty()||needle.size()>256){result.error=QObject::tr("Nhập từ khóa dài 1–256 ký tự.");return result;}
    QPdfDocument d;
    const auto e=d.load(path);
    if(e!=QPdfDocument::Error::None){result.error=pdfErr(e);return result;}
    if(first<1||last<first||last>d.pageCount()||last-first+1>200){result.error=QObject::tr("Chọn khoảng hợp lệ, tối đa 200 trang mỗi lượt.");return result;}
    const auto cs=matchCase?Qt::CaseSensitive:Qt::CaseInsensitive;
    for(int page=first;page<=last;++page){
        if(QThread::currentThread()->isInterruptionRequested()){result.error=QObject::tr("Đã dừng tìm kiếm.");return result;}
        const QString text=d.getAllText(page-1).text().normalized(QString::NormalizationForm_C);
        ++result.scanned;
        if(text.trimmed().isEmpty())continue;
        ++result.textPages;
        qsizetype pos=text.indexOf(needle,0,cs);
        if(pos<0)continue;
        TextHit hit;hit.page=page;
        const auto begin=qMax<qsizetype>(0,pos-55);
        hit.snippet=text.mid(begin,needle.size()+110).simplified();
        while(pos>=0){++hit.occurrences;pos=text.indexOf(needle,pos+needle.size(),cs);}
        result.hits.append(hit);
    }
    result.ok=true;return result;
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
    p.end();
    // Record physical resolution for OCR PDF output (pixel dimensions unchanged).
    const int dpm = qRound(k * 72.0 / 0.0254);
    if (dpm > 0) { out.setDotsPerMeterX(dpm); out.setDotsPerMeterY(dpm); }
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
TextSearch searchText(const QString&,const QString&,int,int,bool) {TextSearch r;r.error=QCoreApplication::translate("printrender",kNo);return r;}
QString pageText(const QString&,int,QString* error){if(error)*error=QCoreApplication::translate("printrender",kNo);return {};}
QImage renderPage(const QString&, int, const QSize&, QString* error) { if (error) *error = QCoreApplication::translate("printrender", kNo); return {}; }
bool printPages(const QString&, QPrinter&, const QVector<int>&, bool, QString* error) { if (error) *error = QCoreApplication::translate("printrender", kNo); return false; }

#endif

} // namespace printrender
