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
static OcrResult runTesseract(const QString& image, const QString& language, const QString& pdfBase = {}, int dpi = 300) {
    auto fail = [](const QString& reason) { return OcrResult{false, {}, reason}; };
    if (language != QLatin1String("vie+eng") && language != QLatin1String("vie") && language != QLatin1String("eng"))
        return fail(QStringLiteral("Ngôn ngữ OCR không hợp lệ."));
    if (!QFileInfo(image).isFile() || QFileInfo(image).size() > 100LL * 1024 * 1024)
        return fail(QStringLiteral("Ảnh OCR không tồn tại hoặc quá lớn."));
    const QString exe = findTesseract();
    if (exe.isEmpty()) return fail(QStringLiteral("Thiếu Tesseract: đặt gói OCR cạnh Dũng Office hoặc cài Tesseract vào PATH."));
    QProcess proc;
    proc.setProgram(exe);
    QStringList args{image, pdfBase.isEmpty() ? QStringLiteral("stdout") : pdfBase,
                     QStringLiteral("-l"), language, QStringLiteral("--psm"), QStringLiteral("3")};
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
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return fail(QStringLiteral("Tesseract lỗi (kiểm tra gói ngôn ngữ): ") + QString::fromUtf8(proc.readAllStandardError()).left(1500));
    return {true, QString::fromUtf8(proc.readAllStandardOutput()).trimmed(), {}};
}
OcrResult recognizeImage(const QString& image, const QString& language) {
    return runTesseract(image, language);
}

OcrResult searchableImagePdf(const QString& image, const QString& language, int dpi, const QString& destination) {
    auto fail = [](const QString& reason) { return OcrResult{false, {}, reason}; };
    const QFileInfo dest(destination);
    if (dpi < 70 || dpi > 2400 || !destination.endsWith(QLatin1String(".pdf"),Qt::CaseInsensitive)
        || dest.exists() || dest.isSymLink() || !QDir(dest.absolutePath()).exists())
        return fail(QStringLiteral("Chọn tên PDF mới và DPI hợp lệ; không ghi đè."));
    QTemporaryDir temp(QDir(dest.absolutePath()).filePath(QStringLiteral(".dung-ocr-XXXXXX")));
    if (!temp.isValid()) return fail(QStringLiteral("Không tạo được thư mục OCR tạm."));
    const QString base = temp.filePath(QStringLiteral("page"));
    const auto result = runTesseract(image,language,base,dpi);
    if (!result.ok) return result;
    if (QThread::currentThread()->isInterruptionRequested()) return fail(QStringLiteral("Đã hủy OCR."));
    const QString produced = base + QStringLiteral(".pdf");
    QFile pdf(produced);
    if (!pdf.open(QIODevice::ReadOnly) || pdf.size() < 100 || pdf.size() > 150LL*1024*1024
        || !pdf.read(5).startsWith("%PDF-") || !pdf.seek(qMax<qint64>(0,pdf.size()-2048))
        || !pdf.readAll().contains("%%EOF"))
        return fail(QStringLiteral("Tesseract không tạo được PDF hợp lệ."));
    pdf.close();
    if (!QFile::rename(produced,destination)) return fail(QStringLiteral("Không lưu được PDF mới; không ghi đè."));
    return {true,{},{}};
}
} // namespace dungpdf
