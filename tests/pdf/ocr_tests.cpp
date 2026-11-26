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
    const QString en = temp.filePath(QStringLiteral("english.png")), vi = temp.filePath(QStringLiteral("vietnamese.png"));
    if (!sample(en, {QStringLiteral("DUNG OFFICE 12345"), QStringLiteral("DOCUMENT TEST 67890"), QStringLiteral("HELLO WORLD")}) ||
        !sample(vi, {QStringLiteral("CỘNG HÒA XÃ HỘI"), QStringLiteral("ĐỘC LẬP TỰ DO"), QStringLiteral("VIỆT NAM 12345")})) return 1;
    const auto a = dungpdf::recognizeImage(en, QStringLiteral("eng"));
    const auto b = dungpdf::recognizeImage(vi, QStringLiteral("vie"));
    const QString bt = b.text.normalized(QString::NormalizationForm_C);
    std::cout << a.error.toStdString() << '\n' << a.text.toStdString() << '\n' << b.error.toStdString() << '\n' << b.text.toStdString() << '\n';
    const bool good = a.ok && a.text.contains(QStringLiteral("DUNG OFFICE"), Qt::CaseInsensitive) && a.text.contains(QStringLiteral("12345")) &&
        b.ok && bt.contains(QStringLiteral("VIỆT NAM"), Qt::CaseInsensitive) && bt.contains(QStringLiteral("12345"));
    const QString pdf = temp.filePath(QStringLiteral("searchable.pdf"));
    const auto generated = dungpdf::searchableImagePdf(vi,QStringLiteral("vie"),300,pdf);
    QFile file(pdf);
    const bool header = file.open(QIODevice::ReadOnly) && file.read(5) == "%PDF-";
    file.close();
    const bool refusesOverwrite = !dungpdf::searchableImagePdf(vi,QStringLiteral("vie"),300,pdf).ok;
    const bool refusesBadDpi = !dungpdf::searchableImagePdf(vi,QStringLiteral("vie"),0,temp.filePath(QStringLiteral("invalid.pdf"))).ok;
    bool searchable = false;
    const QString extractor = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (!extractor.isEmpty() && generated.ok) {
        QProcess proc; proc.start(extractor,{pdf,QStringLiteral("-")});
        if (proc.waitForStarted(3000) && proc.waitForFinished(15000) && proc.exitCode()==0) {
            const auto text = QString::fromUtf8(proc.readAllStandardOutput()).normalized(QString::NormalizationForm_C);
            searchable = text.contains(QStringLiteral("VIỆT NAM"),Qt::CaseInsensitive) && text.contains(QStringLiteral("12345"));
            std::cout << text.toStdString() << '\n';
        }
    }
    std::cout << "Searchable PDF: " << generated.error.toStdString() << " text layer=" << searchable << '\n';
    return good && generated.ok && header && refusesOverwrite && refusesBadDpi && searchable ? 0 : 1;
}
