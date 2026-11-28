// Kiểm thử in/xem trước: logic khoảng trang (QtCore), vẽ PDF ra ảnh có nội dung, in ra tệp PDF qua QPrinter.
// Không in ra máy in thật (CI không có máy in).
#include "PrintLogic.h"
#include "PrintRender.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPageSize>
#include <QPrinter>
#include <QPageLayout>
#include <QTemporaryDir>
#include <QThread>
#include <memory>
#include <cstdio>
#ifdef DUNG_HAVE_QTPDF
#include <QPdfDocument>
#endif

static int g_fail = 0, g_pass = 0;
static void check(bool ok, const QString& name, const QString& detail = QString()) {
    if (ok) { ++g_pass; printf("[ĐẠT]  %s\n", qPrintable(name)); }
    else { ++g_fail; printf("[LỖI]  %s\n", qPrintable(name)); }
    if (!detail.isEmpty()) printf("       %s\n", qPrintable(detail));
}

// PDF tối thiểu hợp lệ: mỗi trang có chữ "Trang N" lớn và một khối đen; w,h theo từng trang.
static QByteArray makePdf(const QVector<QSize>& sizes) {
    QByteArray out = "%PDF-1.4\n";
    QVector<qint64> off;
    auto obj = [&](const QByteArray& body) {
        off.push_back(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    const int n = sizes.size();
    obj("<< /Type /Catalog /Pages 2 0 R >>");
    QByteArray kids;
    for (int i = 0; i < n; ++i) kids += QByteArray::number(4 + i * 2) + " 0 R ";
    obj("<< /Type /Pages /Count " + QByteArray::number(n) + " /Kids [" + kids + "] >>");
    obj("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    for (int i = 0; i < n; ++i) {
        const int w = sizes[i].width(), h = sizes[i].height();
        obj("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + QByteArray::number(w) + " " + QByteArray::number(h) +
            "] /Resources << /Font << /F1 3 0 R >> >> /Contents " + QByteArray::number(5 + i * 2) + " 0 R >>");
        const QByteArray c = "BT /F1 48 Tf 72 " + QByteArray::number(h - 120) + " Td (Trang " + QByteArray::number(i + 1) +
                             ") Tj ET\n0 0 0 rg " + QByteArray::number(72 + i * 40) + " 100 120 80 re f\n";
        obj("<< /Length " + QByteArray::number(c.size()) + " >>\nstream\n" + c + "endstream");
    }
    const qint64 xref = out.size();
    out += "xref\n0 " + QByteArray::number(off.size() + 1) + "\n0000000000 65535 f \n";
    for (qint64 o : off) out += QByteArray::number(o).rightJustified(10, '0') + " 00000 n \n";
    out += "trailer\n<< /Size " + QByteArray::number(off.size() + 1) + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    return out;
}

static int darkPixels(const QImage& im) {
    int n = 0;
    for (int y = 0; y < im.height(); ++y)
        for (int x = 0; x < im.width(); ++x)
            if (qGray(im.pixel(x, y)) < 100) ++n;
    return n;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen") : qgetenv("QT_QPA_PLATFORM"));
    QGuiApplication app(argc, argv);
    using namespace printlogic;

    // ---- Khoảng trang ----
    auto pg = [](const QString& t, int total) { return parseRanges(t, total); };
    check(pg(QString(), 5).ok && pg(QString(), 5).pages == QVector<int>({1, 2, 3, 4, 5}), "khoảng trống = tất cả trang");
    check(pg(QStringLiteral("Tất cả"), 3).pages == QVector<int>({1, 2, 3}), "\"Tất cả\" = mọi trang");
    check(pg(QStringLiteral("1-3, 5"), 8).pages == QVector<int>({1, 2, 3, 5}), "1-3, 5");
    check(pg(QStringLiteral("8-"), 10).pages == QVector<int>({8, 9, 10}), "8- = đến hết");
    check(pg(QStringLiteral("-3"), 10).pages == QVector<int>({1, 2, 3}), "-3 = từ đầu đến 3");
    check(pg(QStringLiteral("3;1,2-3"), 5).pages == QVector<int>({1, 2, 3}), "trộn ; và , — tăng dần, không trùng");
    check(pg(QStringLiteral(" 2 - 4 "), 5).pages == QVector<int>({2, 3, 4}), "chấp nhận khoảng trắng");
    check(!pg(QStringLiteral("0"), 5).ok, "từ chối trang 0");
    check(!pg(QStringLiteral("6"), 5).ok && pg(QStringLiteral("6"), 5).error.contains(QStringLiteral("vượt")), "từ chối trang vượt tổng", pg(QStringLiteral("6"), 5).error);
    check(!pg(QStringLiteral("5-2"), 8).ok, "từ chối khoảng ngược");
    check(!pg(QStringLiteral("1,,3"), 8).ok, "từ chối dấu phẩy liên tiếp");
    check(!pg(QStringLiteral("a-b"), 8).ok, "từ chối chữ");
    check(!pg(QStringLiteral("1 3"), 8).ok, "từ chối hai số thiếu dấu ngăn");
    check(!pg(QStringLiteral("-"), 8).ok, "từ chối chỉ có dấu gạch");
    check(!pg(QStringLiteral("99999999999999999999"), 8).ok, "từ chối số quá lớn");
    check(!pg(QString(), 0).ok, "tài liệu 0 trang → lỗi");

    check(filterPages(pg("2-5",6),PageFilter::Odd).pages==QVector<int>({3,5}),"odd pages retain original page numbers");
    check(filterPages(pg("2-5",6),PageFilter::Even).pages==QVector<int>({2,4}),"even pages retain original page numbers");
    check(!filterPages(pg("1",6),PageFilter::Even).ok,"empty parity selection rejected");
    check(!filterPages(pg("9",6),PageFilter::All).ok,"invalid range remains invalid");
    check(expandCopies({1, 2, 3}, 2, true) == QVector<int>({1, 2, 3, 1, 2, 3}), "bản sao theo bộ");
    check(expandCopies({1, 2, 3}, 2, false) == QVector<int>({1, 1, 2, 2, 3, 3}), "bản sao không theo bộ");
    check(expandCopies({4}, 0, true) == QVector<int>({4}), "số bản < 1 coi là 1");

    check(mismatchWarning(5, 5).isEmpty(), "số trang khớp: không cảnh báo");
    check(!mismatchWarning(5, 4).isEmpty() && mismatchWarning(5, 4).contains(QStringLiteral("5")) && mismatchWarning(5, 4).contains(QStringLiteral("4")), "số trang lệch: có cảnh báo nêu cả hai số");
    check(mismatchWarning(5, 0).isEmpty() && mismatchWarning(5, -1).isEmpty(), "không so sánh được: không cảnh báo");

    // External viewer fallback must outlive the caller's temporary PDF,
    // and must never replace a pre-existing file.
    {
        QTemporaryDir storage;
        QTemporaryDir stage;
        const QString src=stage.filePath("print.pdf");
        const QString dest=storage.filePath("persistent.pdf");
        const QByteArray bytes=makePdf({QSize(595,842)});
        { QFile file(src); check(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(),"create fallback print fixture"); }
        QString err;
        check(copyPrintPdf(src,dest,&err),"copy print PDF to explicit destination",err);
        check(!copyPrintPdf(src,dest,&err),"fallback refuses existing destination");
        check(!copyPrintPdf(src,storage.filePath("invalid.docx"),&err),"fallback refuses non-PDF suffix");
        QFile::remove(src);
        QFile retained(dest);
        check(retained.open(QIODevice::ReadOnly) && retained.readAll()==bytes,"print copy survives temporary source deletion");
    }

    // ---- Vẽ và in PDF ----
    const bool needPdf = qEnvironmentVariableIntValue("DUNG_REQUIRE_PDF") == 1;
    if (!printrender::available()) {
        check(!needPdf, "có Qt6::Pdf để vẽ/in PDF", needPdf ? QStringLiteral("THIẾU Qt6::Pdf nhưng CI yêu cầu có") : QStringLiteral("BỎ QUA phần vẽ/in: bản dựng không có Qt6::Pdf"));
        printf("\nKẾT QUẢ: %s (đạt %d, lỗi %d)\n", g_fail ? "CÓ LỖI" : "ĐẠT PHẦN LOGIC", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
#ifdef DUNG_HAVE_QTPDF
    QTemporaryDir dir;
    check(dir.isValid(), "tạo thư mục tạm");
    const QString src = dir.filePath(QStringLiteral("mau.pdf"));
    { QFile f(src); f.open(QIODevice::WriteOnly); f.write(makePdf({QSize(595, 842), QSize(842, 595), QSize(595, 842)})); }

    const auto info = printrender::pdfInfo(src);
    check(info.ok && info.pages == 3, "đọc PDF mẫu: 3 trang", info.error);
    check(!printrender::pdfInfo(dir.filePath(QStringLiteral("khong-co.pdf"))).ok, "PDF không tồn tại → lỗi rõ ràng");
    { QFile f(dir.filePath(QStringLiteral("hong.pdf"))); f.open(QIODevice::WriteOnly); f.write("không phải pdf"); }
    check(!printrender::pdfInfo(dir.filePath(QStringLiteral("hong.pdf"))).ok, "PDF hỏng → lỗi rõ ràng");

    QString err;
    const QString display=QStringLiteral("Có có CÓ không").normalized(QString::NormalizationForm_C);
    check(printrender::highlightOffsets(display,QStringLiteral("  có "),false)==QVector<qsizetype>{0,3,6},
          "highlight NFC Vietnamese, trimmed query, case-insensitive");
    check(printrender::highlightOffsets(display,QStringLiteral("Có"),true)==QVector<qsizetype>{0},
          "highlight case-sensitive");
    check(printrender::highlightOffsets(QStringLiteral("aaaa"),QStringLiteral("aa"),false)==QVector<qsizetype>{0,2},
          "highlight non-overlapping literal matches");
    check(printrender::highlightOffsets(display,QStringLiteral("có"),false,2).size()==2&&
          printrender::highlightOffsets(display,QStringLiteral(" " ),false).isEmpty(),
          "highlight cap and empty query");
    const auto all=printrender::searchText(src,QStringLiteral("trang"),1,3);
    check(all.ok&&all.hits.size()==3&&all.textPages==3,"Tìm chữ PDF không phân biệt hoa/thường");
    const auto one=printrender::searchText(src,QStringLiteral("Trang 2"),1,3,true);
    check(one.ok&&one.hits.size()==1&&one.hits.first().page==2&&one.hits.first().occurrences==1,"Tìm đúng trang PDF");
    check(printrender::searchText(src,QStringLiteral("trang"),1,3,true).hits.isEmpty(),"Tìm phân biệt hoa/thường");
    check(printrender::pageText(src,2,&err).contains(QStringLiteral("Trang 2"))&&err.isEmpty(),"Đọc chữ thật từ trang");
    check(!printrender::searchText(src,QString(),1,3).ok&&!printrender::searchText(src,QStringLiteral("Trang"),1,4).ok,"Từ chối từ khóa hoặc khoảng sai");
    check(printrender::pageText(src,9,&err).isEmpty()&&!err.isEmpty(),"Trang chữ ngoài phạm vi");
    const auto preview=printrender::previewText(src,2,QSize(500,500),QStringLiteral("Trang 2"),true);
    check(preview.error.isEmpty()&&!preview.image.isNull()&&!preview.matches.isEmpty(),"PDF preview paints actual match geometry");
    if(!preview.matches.isEmpty()){
        const auto rect=preview.matches.first().boundingRect();
        check(rect.isValid()&&rect.left()>=0&&rect.top()>=0&&rect.right()<=1&&rect.bottom()<=1,"match geometry normalized to page");
        const auto selected=printrender::selectText(src,2,QPointF(rect.left()+0.001,rect.center().y()),QPointF(rect.right()-0.001,rect.center().y()));
        check(selected.error.isEmpty()&&!selected.text.trimmed().isEmpty()&&!selected.bounds.isEmpty(),"PDF drag coordinates return actual text and geometry",selected.text);
    }
    check(printrender::previewText(src,2,QSize(500,500),QStringLiteral("trang 2"),true).matches.isEmpty(),"case-sensitive page highlights");
    check(!printrender::selectText(src,2,QPointF(-1,0),QPointF(0,0)).error.isEmpty(),"reject out-of-page selection");
    const QImage a = printrender::renderPage(src, 1, QSize(400, 400), &err);
    const QImage b = printrender::renderPage(src, 3, QSize(400, 400), &err);
    check(!a.isNull() && a.width() <= 400 && a.height() <= 400, "vẽ trang 1 vừa khung", err);
    check(!a.isNull() && darkPixels(a) > 200, "trang 1 có nội dung (điểm tối)", QStringLiteral("điểm tối: %1").arg(a.isNull() ? 0 : darkPixels(a)));
    check(!a.isNull() && !b.isNull() && a != b, "trang 1 và trang 3 khác nhau");
    // Same independent QPdfDocument worker pattern as the comparison panel.
    QImage workerImage;
    QString workerError;
    std::unique_ptr<QThread> worker(QThread::create([&] {
        workerImage=printrender::renderPage(src,3,QSize(400,400),&workerError);
    }));
    worker->start();
    const bool workerDone=worker->wait(15000);
    check(workerDone,"PDF worker render completes within test timeout");
    if(workerDone)check(workerError.isEmpty()&&!workerImage.isNull()&&workerImage==b,
                       "PDF worker renders exact same third-page image as synchronous reference");
    worker.reset(); // join safely before local results go out of scope (Qt >= 6.4)

    check(printrender::renderPage(src,1,QSize(0,400),&err).isNull()&&!err.isEmpty(),"reject empty preview bounds");
    check(printrender::renderPage(src,1,QSize(-1,400),&err).isNull()&&!err.isEmpty(),"reject negative preview bounds");
    const QImage land = printrender::renderPage(src, 2, QSize(400, 400), &err);
    check(err.isEmpty(),"successful render clears previous error");
    check(!land.isNull() && land.width() > land.height(), "trang ngang giữ tỉ lệ ngang");
    check(printrender::renderPage(src, 4, QSize(400, 400), &err).isNull() && !err.isEmpty(), "trang 4 không tồn tại → ảnh rỗng + lỗi");

    // In ra tệp PDF bằng QPrinter (không cần máy in thật)
    auto printTo = [&](const QVector<int>& pages, bool autoOri, const QString& name) {
        QPrinter pr(QPrinter::HighResolution);
        pr.setOutputFormat(QPrinter::PdfFormat);
        pr.setOutputFileName(dir.filePath(name));
        pr.setPageSize(QPageSize(QPageSize::A4));
        QString e;
        const bool ok = printrender::printPages(src, pr, pages, autoOri, &e);
        return std::pair<bool, QString>(ok, e);
    };
    {
        auto r = printTo({1, 3}, false, QStringLiteral("in13.pdf"));
        check(r.first, "in trang {1,3} ra PDF", r.second);
        const auto i = printrender::pdfInfo(dir.filePath(QStringLiteral("in13.pdf")));
        check(i.ok && i.pages == 2, "tệp in có đúng 2 trang", QStringLiteral("số trang: %1").arg(i.pages));
        const QImage o = printrender::renderPage(dir.filePath(QStringLiteral("in13.pdf")), 1, QSize(400, 600), &err);
        check(!o.isNull() && darkPixels(o) > 200, "trang đã in có nội dung (không trắng)");
    }
    {
        auto r = printTo({1, 1, 3}, false, QStringLiteral("in113.pdf"));
        const auto i = printrender::pdfInfo(dir.filePath(QStringLiteral("in113.pdf")));
        check(r.first && i.pages == 3, "in danh sách có lặp: 3 trang", r.second);
    }
    {
        auto r = printTo({1, 2, 3}, true, QStringLiteral("in123.pdf"));
        check(r.first, "in tự chọn hướng giấy theo từng trang", r.second);
#ifdef DUNG_HAVE_QTPDF
        QPdfDocument d; d.load(dir.filePath(QStringLiteral("in123.pdf")));
        const bool ok = d.pageCount() == 3 && d.pagePointSize(0).height() > d.pagePointSize(0).width() &&
                        d.pagePointSize(1).width() > d.pagePointSize(1).height();
        check(ok, "trang ngang được in trên giấy ngang (hướng tự động)",
              QStringLiteral("kích thước trang 2: %1x%2").arg(d.pagePointSize(1).width()).arg(d.pagePointSize(1).height()));
#endif
    }
    {
        auto r = printTo({9}, false, QStringLiteral("loi.pdf"));
        check(!r.first && !r.second.isEmpty(), "in trang không tồn tại → lỗi rõ ràng", r.second);
        auto r2 = printTo({}, false, QStringLiteral("rong.pdf"));
        check(!r2.first, "danh sách trang rỗng → lỗi");
    }
#endif
    printf("\nKẾT QUẢ: %s (đạt %d, lỗi %d)\n", g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
