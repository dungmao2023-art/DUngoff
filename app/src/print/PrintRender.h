#pragma once
// Vẽ PDF ra ảnh và gửi PDF tới QPrinter. Cần Qt6::Pdf (DUNG_HAVE_QTPDF); nếu không có, các hàm báo lỗi rõ ràng.
#include <QImage>
#include <QString>
#include <QVector>

class QPrinter;

namespace printrender {

bool available();  // có Qt6::Pdf?

struct PdfInfo {
    bool ok = false;
    QString error;
    int pages = 0;
};

PdfInfo pdfInfo(const QString& path);

// Vẽ trang (1-based) vừa khung `box` (giữ tỉ lệ), nền trắng. Ảnh rỗng nếu lỗi.
QImage renderPage(const QString& path, int page1, const QSize& box, QString* error = nullptr);

// In các trang (1-based, đúng thứ tự cho trước, có thể lặp) bằng QPrinter. Mỗi trang vẽ vừa vùng in, giữ tỉ lệ.
// autoOrientation: mỗi trang tự chọn dọc/ngang theo tỉ lệ trang PDF.
bool printPages(const QString& path, QPrinter& printer, const QVector<int>& pages, bool autoOrientation, QString* error);

} // namespace printrender
