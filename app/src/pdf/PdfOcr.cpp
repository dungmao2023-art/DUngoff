// SPDX-License-Identifier: MPL-2.0
#include "PdfOcr.h"
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
namespace dungpdf {
QString findTesseract() { return QStandardPaths::findExecutable(QStringLiteral("tesseract")); }
OcrResult recognizeImage(const QString& image, const QString& language) {
    auto fail = [](const QString& reason) { return OcrResult{false, {}, reason}; };
    if (language != QLatin1String("vie+eng") && language != QLatin1String("vie") && language != QLatin1String("eng"))
        return fail(QStringLiteral("Ngôn ngữ OCR không hợp lệ."));
    if (!QFileInfo(image).isFile() || QFileInfo(image).size() > 100LL * 1024 * 1024)
        return fail(QStringLiteral("Ảnh OCR không tồn tại hoặc quá lớn."));
    const QString exe = findTesseract();
    if (exe.isEmpty()) return fail(QStringLiteral("Chưa cài Tesseract hoặc chưa có trong PATH."));
    QProcess proc;
    proc.setProgram(exe);
    proc.setArguments({image, QStringLiteral("stdout"), QStringLiteral("-l"), language, QStringLiteral("--psm"), QStringLiteral("3")});
    proc.start();
    if (!proc.waitForStarted(3000)) return fail(proc.errorString());
    if (!proc.waitForFinished(90000)) { proc.kill(); proc.waitForFinished(3000); return fail(QStringLiteral("OCR vượt quá 90 giây.")); }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return fail(QStringLiteral("Tesseract lỗi (kiểm tra gói ngôn ngữ): ") + QString::fromUtf8(proc.readAllStandardError()).left(1500));
    return {true, QString::fromUtf8(proc.readAllStandardOutput()).trimmed(), {}};
}
} // namespace dungpdf
