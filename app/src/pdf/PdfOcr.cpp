// SPDX-License-Identifier: MPL-2.0
#include "PdfOcr.h"
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QProcess>
#include <QThread>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QImageReader>
namespace dungpdf {
QString validateOcrRange(int first, int last, int pageCount) {
    if (first < 1 || last < first || last > pageCount)
        return QStringLiteral("Khoảng trang không hợp lệ.");
    if (last - first + 1 > kOcrMaxPages)
        return QStringLiteral("Mỗi lượt nhận dạng tối đa %1 trang.").arg(kOcrMaxPages);
    return {};
}
QString bundledTesseract(const QString& applicationDir) {
    if(applicationDir.isEmpty() || !QDir::isAbsolutePath(applicationDir)) return {};
#ifdef Q_OS_WIN
    const QString relative=QStringLiteral("tesseract/tesseract.exe");
#else
    const QString relative=QStringLiteral("tesseract/tesseract");
#endif
    const QFileInfo executable(QDir(applicationDir).filePath(relative));
    return executable.isFile() && executable.isExecutable() ? executable.absoluteFilePath() : QString{};
}
QString findTesseract() {
    const auto local=bundledTesseract(QCoreApplication::applicationDirPath());
    return local.isEmpty()?QStandardPaths::findExecutable(QStringLiteral("tesseract")):local;
}
bool validOcrLayout(int layout) {
    return layout == 3 || layout == 6 || layout == 7 || layout == 11;
}
static OcrResult runTesseract(const QString& image, const QString& language, int layout, const QString& pdfBase = {}, int dpi = 300) {
    auto fail = [](const QString& reason) { return OcrResult{false, {}, reason}; };
    if (!validOcrLayout(layout)) return fail(QStringLiteral("Kiểu bố cục OCR không hợp lệ."));
    if (QThread::currentThread()->isInterruptionRequested()) return fail(QStringLiteral("Đã hủy OCR."));
    if (language != QLatin1String("vie+eng") && language != QLatin1String("vie") && language != QLatin1String("eng"))
        return fail(QStringLiteral("Ngôn ngữ OCR không hợp lệ."));
    const QFileInfo input(image);
    if (!input.isFile() || input.size() <= 0 || input.size() > 100LL * 1024 * 1024)
        return fail(QStringLiteral("Ảnh OCR không tồn tại hoặc quá lớn."));
    // Reject oversized decoded images before invoking the external process.
    // File size alone cannot bound a compressed image's memory footprint.
    QImageReader reader(input.absoluteFilePath());
    const QSize dimensions = reader.size();
    if (!dimensions.isValid() || dimensions.width() > 12000 || dimensions.height() > 12000
        || qint64(dimensions.width()) * dimensions.height() > 36000000)
        return fail(QStringLiteral("Ảnh OCR không đọc được kích thước hoặc vượt giới hạn 36 triệu điểm ảnh."));
    const QString exe = findTesseract();
    if (exe.isEmpty()) return fail(QStringLiteral("Thiếu Tesseract: đặt gói OCR cạnh Dũng Office hoặc cài Tesseract vào PATH."));
    QProcess proc;
    proc.setProgram(exe);
    QStringList args{input.absoluteFilePath(), pdfBase.isEmpty() ? QStringLiteral("stdout") : pdfBase,
                     QStringLiteral("-l"), language, QStringLiteral("--psm"), QString::number(layout)};
    const auto bundled=bundledTesseract(QCoreApplication::applicationDirPath());
    if(!bundled.isEmpty() && exe==bundled) {
        const QString data=QDir(QFileInfo(exe).absolutePath()).filePath(QStringLiteral("tessdata"));
        for(const auto& lang:language.split(QLatin1Char('+'))) {
            const QFileInfo model(QDir(data).filePath(lang+QStringLiteral(".traineddata")));
            if(!model.isFile() || model.size()<=0) return fail(QStringLiteral("Gói OCR offline thiếu dữ liệu ngôn ngữ: ")+lang);
        }
        args << QStringLiteral("--tessdata-dir") << data;
        if(!pdfBase.isEmpty() && !QFileInfo(QDir(data).filePath(QStringLiteral("pdf.ttf"))).isFile())
            return fail(QStringLiteral("Gói OCR offline thiếu pdf.ttf để xuất PDF tìm kiếm được."));
    }
    if (!pdfBase.isEmpty()) args << QStringLiteral("--dpi") << QString::number(dpi) << QStringLiteral("pdf");
    proc.setArguments(args);
    proc.start();
    if (!proc.waitForStarted(3000)) return fail(proc.errorString());
    QElapsedTimer elapsed; elapsed.start();
    while (proc.state() != QProcess::NotRunning) {
        const bool cancelled = QThread::currentThread()->isInterruptionRequested();
        if (cancelled || elapsed.elapsed() >= 90000) {
            proc.kill(); proc.waitForFinished(3000);
            return fail(cancelled ? QStringLiteral("Đã hủy OCR.") : QStringLiteral("OCR vượt quá 90 giây."));
        }
        proc.waitForFinished(100);
    }
    if (QThread::currentThread()->isInterruptionRequested()) return fail(QStringLiteral("Đã hủy OCR."));
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return fail(QStringLiteral("Tesseract lỗi (kiểm tra gói ngôn ngữ): ") + QString::fromUtf8(proc.readAllStandardError()).left(1500));
    return {true, QString::fromUtf8(proc.readAllStandardOutput()).trimmed(), {}};
}
OcrResult recognizeImage(const QString& image, const QString& language, int layout) {
    return runTesseract(image, language, layout);
}

OcrResult searchableImagePdf(const QString& image, const QString& language, int dpi, const QString& destination, int layout) {
    auto fail = [](const QString& reason) { return OcrResult{false, {}, reason}; };
    const QFileInfo dest(destination);
    if (dpi < 70 || dpi > 2400 || !destination.endsWith(QLatin1String(".pdf"),Qt::CaseInsensitive)
        || dest.exists() || dest.isSymLink() || !QDir(dest.absolutePath()).exists())
        return fail(QStringLiteral("Chọn tên PDF mới và DPI hợp lệ; không ghi đè."));
    QTemporaryDir temp(QDir(dest.absolutePath()).filePath(QStringLiteral(".dung-ocr-XXXXXX")));
    if (!temp.isValid()) return fail(QStringLiteral("Không tạo được thư mục OCR tạm."));
    const QString base = temp.filePath(QStringLiteral("page"));
    const auto result = runTesseract(image,language,layout,base,dpi);
    if (!result.ok) return result;
    if (QThread::currentThread()->isInterruptionRequested()) return fail(QStringLiteral("Đã hủy OCR."));
    const QString produced = base + QStringLiteral(".pdf");
    QFile pdf(produced);
    if (!pdf.open(QIODevice::ReadOnly) || pdf.size() < 100 || pdf.size() > 150LL*1024*1024
        || !pdf.read(5).startsWith("%PDF-") || !pdf.seek(qMax<qint64>(0,pdf.size()-2048))
        || !pdf.readAll().contains("%%EOF"))
        return fail(QStringLiteral("Tesseract không tạo được PDF hợp lệ."));
    pdf.close();
    if (QThread::currentThread()->isInterruptionRequested()) return fail(QStringLiteral("Đã hủy OCR."));
    if (!QFile::rename(produced,destination)) return fail(QStringLiteral("Không lưu được PDF mới; không ghi đè."));
    return {true,{},{}};
}
} // namespace dungpdf
