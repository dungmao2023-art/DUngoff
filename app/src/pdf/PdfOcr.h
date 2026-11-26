// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QString>
namespace dungpdf {
struct OcrResult { bool ok = false; QString text, error; };
constexpr int kOcrMaxPages = 20;
QString validateOcrRange(int first, int last, int pageCount);
QString findTesseract();
OcrResult recognizeImage(const QString& image, const QString& language);
}
