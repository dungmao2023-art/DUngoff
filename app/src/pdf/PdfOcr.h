// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QString>
namespace dungpdf {
struct OcrResult { bool ok = false; QString text, error; };
constexpr int kOcrMaxPages = 20;
QString validateOcrRange(int first, int last, int pageCount);
// Only this fixed, application-relative directory is used for an optional offline package.
// An empty result permits fallback to an existing system installation.
QString bundledTesseract(const QString& applicationDir);
QString findTesseract();
// Tesseract page segmentation: 3 automatic, 6 block, 7 line, 11 sparse.
bool validOcrLayout(int layout);
OcrResult recognizeImage(const QString& image, const QString& language, int layout = 3);
// Tạo PDF raster có lớp chữ OCR ẩn; chỉ ghi tệp mới, không thay nguồn.
OcrResult searchableImagePdf(const QString& image, const QString& language, int dpi, const QString& destination, int layout = 3);
}
