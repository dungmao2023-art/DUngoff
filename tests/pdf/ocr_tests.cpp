// SPDX-License-Identifier: MPL-2.0
#include "PdfOcr.h"
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <iostream>
#ifdef DUNG_HAVE_PDFIUM_EDIT
#include <fpdfview.h>
#include <fpdf_text.h>
#include <QVector>
#endif

// Read the real OCR text layer using the same pinned PDFium SDK on both
// platforms, not a second external Windows utility.
#ifdef DUNG_HAVE_PDFIUM_EDIT
static QString readLayer(const QString& path){
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>150LL*1024*1024)return {};
    const QByteArray bytes=file.readAll();
    FPDF_InitLibrary();
    auto doc=FPDF_LoadMemDocument64(bytes.constData(),size_t(bytes.size()),nullptr);
    auto page=doc&&FPDF_GetPageCount(doc)==1?FPDF_LoadPage(doc,0):nullptr;
    auto text=page?FPDFText_LoadPage(page):nullptr;
    QString result;
    const int n=text?FPDFText_CountChars(text):0;
    if(n>0&&n<100000){
        QVector<unsigned short> data(n+1);
        const int got=FPDFText_GetText(text,0,n,data.data());
        if(got>0&&got<=n+1&&data[got-1]==0)
            result=QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData()),got-1);
    }
    if(text)FPDFText_ClosePage(text);
    if(page)FPDF_ClosePage(page);
    if(doc)FPDF_CloseDocument(doc);
    FPDF_DestroyLibrary();
    return result;
}
#endif

static bool sample(const QString& dest, const QStringList& lines) {
    QImage img(1600, 1000, QImage::Format_RGB32);
    img.fill(Qt::white);
    img.setDotsPerMeterX(11811); img.setDotsPerMeterY(11811);
    QPainter p(&img);
    p.setPen(Qt::black);
    QFont font(QStringLiteral("DejaVu Sans")); font.setPixelSize(52); p.setFont(font);
    int y = 160;
    for (const auto& line : lines) { p.drawText(100, y, line); y += 105; }
    p.end();
    return img.save(dest, "PNG");
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid() || dungpdf::findTesseract().isEmpty()) return 1;
    // Probe discovery without executing a dummy binary; actual OCR below still uses the real engine.
    const QString package=temp.filePath(QStringLiteral("offline package"));
    if(!QDir().mkpath(package+QStringLiteral("/tesseract")))return 1;
    if(!dungpdf::bundledTesseract(package).isEmpty() || !dungpdf::bundledTesseract(QStringLiteral(".")).isEmpty())return 1;
#ifdef Q_OS_WIN
    const QString dummy=package+QStringLiteral("/tesseract/tesseract.exe");
#else
    const QString dummy=package+QStringLiteral("/tesseract/tesseract");
#endif
    QFile probe(dummy);
    if(!probe.open(QIODevice::WriteOnly)||probe.write("discovery only")!=14)return 1;
    probe.close();
#ifndef Q_OS_WIN
    if(!QFile::setPermissions(dummy,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner))return 1;
#endif
    if(dungpdf::bundledTesseract(package)!=QFileInfo(dummy).absoluteFilePath())return 1;
    if(!probe.remove()||!dungpdf::bundledTesseract(package).isEmpty())return 1;
#ifdef Q_OS_WIN
    const QString programRoot=temp.filePath(QStringLiteral("Program Files test"));
    const QString installed=QDir(programRoot).filePath(QStringLiteral("Tesseract-OCR/tesseract.exe"));
    if(!QDir().mkpath(QFileInfo(installed).absolutePath()))return 1;
    if(!dungpdf::installedTesseract({QStringLiteral("."),programRoot}).isEmpty())return 1;
    QFile installedProbe(installed);
    if(!installedProbe.open(QIODevice::WriteOnly)||installedProbe.write("discovery only")!=14)return 1;
    installedProbe.close();
    if(dungpdf::installedTesseract({QStringLiteral("."),programRoot})!=QFileInfo(installed).absoluteFilePath())return 1;
    if(!installedProbe.remove()||!dungpdf::installedTesseract({programRoot}).isEmpty())return 1;
#endif
    // Also exercise paths with Vietnamese characters and spaces on Windows.
    const QString samples=temp.filePath(QStringLiteral("Ảnh kiểm thử"));
    if(!QDir().mkpath(samples))return 1;
    const QString en=QDir(samples).filePath(QStringLiteral("english.png")), vi=QDir(samples).filePath(QStringLiteral("vietnamese.png"));
    if (!sample(en, {QStringLiteral("DUNG OFFICE 12345"), QStringLiteral("DOCUMENT TEST 67890"), QStringLiteral("HELLO WORLD")}) ||
        !sample(vi, {QStringLiteral("CỘNG HÒA XÃ HỘI"), QStringLiteral("ĐỘC LẬP TỰ DO"), QStringLiteral("VIỆT NAM 12345")})) return 1;
    for (const int mode : {3, 6, 7, 11}) if (!dungpdf::validOcrLayout(mode)) return 1;
    for (const int mode : {-1, 0, 4, 13}) if (dungpdf::validOcrLayout(mode)) return 1;
    const auto invalid = dungpdf::recognizeImage(en, QStringLiteral("eng"), 99);
    if (invalid.ok || invalid.error.isEmpty()) return 1;
    // A compact but over-wide raster must be rejected before starting Tesseract.
    const QString wide = temp.filePath(QStringLiteral("too-wide.png"));
    QImage wideImage(12001, 2, QImage::Format_RGB32);
    wideImage.fill(Qt::white);
    if (!wideImage.save(wide, "PNG")) return 1;
    const auto oversized = dungpdf::recognizeImage(wide, QStringLiteral("eng"));
    if (oversized.ok || !oversized.error.contains(QStringLiteral("36"))) return 1;
    const QString rejectedPdf = temp.filePath(QStringLiteral("too-wide.pdf"));
    if (dungpdf::searchableImagePdf(wide, QStringLiteral("eng"), 300, rejectedPdf).ok ||
        QFileInfo::exists(rejectedPdf)) return 1;
    const QString corrupt = temp.filePath(QStringLiteral("corrupt.png"));
    QFile bad(corrupt);
    if (!bad.open(QIODevice::WriteOnly) || bad.write("not an image") != 12) return 1;
    bad.close();
    if (dungpdf::recognizeImage(corrupt, QStringLiteral("eng")).ok) return 1;
    const QString line = temp.filePath(QStringLiteral("line.png"));
    if (!sample(line, {QStringLiteral("DUNG OFFICE 12345")})) return 1;
    const auto single = dungpdf::recognizeImage(line, QStringLiteral("eng"), 7);
    const bool lineGood = single.ok && single.text.contains(QStringLiteral("DUNG OFFICE"), Qt::CaseInsensitive)
        && single.text.contains(QStringLiteral("12345"));
    const auto a = dungpdf::recognizeImage(en, QStringLiteral("eng"), 6);
    const auto b = dungpdf::recognizeImage(vi, QStringLiteral("vie"));
    const QString bt = b.text.normalized(QString::NormalizationForm_C);
    std::cout << a.error.toStdString() << '\n' << a.text.toStdString() << '\n' << b.error.toStdString() << '\n' << b.text.toStdString() << '\n';
    const bool good = a.ok && a.text.contains(QStringLiteral("DUNG OFFICE"), Qt::CaseInsensitive) && a.text.contains(QStringLiteral("12345")) &&
        b.ok && bt.contains(QStringLiteral("VIỆT NAM"), Qt::CaseInsensitive) && bt.contains(QStringLiteral("12345"));
    const QString pdf = temp.filePath(QStringLiteral("searchable.pdf"));
    const auto generated = dungpdf::searchableImagePdf(vi,QStringLiteral("vie"),300,pdf,6);
    QFile file(pdf);
    const bool header = file.open(QIODevice::ReadOnly) && file.read(5) == "%PDF-";
    file.close();
    const bool refusesOverwrite = !dungpdf::searchableImagePdf(vi,QStringLiteral("vie"),300,pdf).ok;
    const bool refusesBadDpi = !dungpdf::searchableImagePdf(vi,QStringLiteral("vie"),0,temp.filePath(QStringLiteral("invalid.pdf"))).ok;
    bool searchable = false;
#ifdef DUNG_HAVE_PDFIUM_EDIT
    if(generated.ok){
        const auto text=readLayer(pdf).normalized(QString::NormalizationForm_C);
        searchable=text.contains(QStringLiteral("VIỆT NAM"),Qt::CaseInsensitive)&&text.contains(QStringLiteral("12345"));
        std::cout<<"PDFium OCR text: "<<text.toStdString()<<std::endl;
    }
#else
    const QString extractor = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (!extractor.isEmpty() && generated.ok) {
        QProcess proc; proc.start(extractor,{pdf,QStringLiteral("-")});
        if (proc.waitForStarted(3000) && proc.waitForFinished(15000) && proc.exitCode()==0) {
            const auto text = QString::fromUtf8(proc.readAllStandardOutput()).normalized(QString::NormalizationForm_C);
            searchable = text.contains(QStringLiteral("VIỆT NAM"),Qt::CaseInsensitive) && text.contains(QStringLiteral("12345"));
            std::cout << text.toStdString() << '\n';
        }
    }
#endif
    std::cout << "Searchable PDF: " << generated.error.toStdString() << " text layer=" << searchable << '\n';
    const QString invalidPath = temp.filePath(QStringLiteral("invalid-layout.pdf"));
    const bool refusesLayout = !dungpdf::searchableImagePdf(vi,QStringLiteral("vie"),300,invalidPath,99).ok
        && !QFileInfo::exists(invalidPath);
    return good && lineGood && generated.ok && header && refusesOverwrite && refusesBadDpi && refusesLayout && searchable ? 0 : 1;
}
