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
// Read actual PDF text layer (no OCR); 1-based pages.
struct TextHit { int page=0, occurrences=0; QString snippet; };
struct TextSearch { bool ok=false; QString error; QVector<TextHit> hits; int scanned=0, textPages=0; };
TextSearch searchText(const QString& path,const QString& query,int first,int last,bool matchCase=false);
QString pageText(const QString& path,int page,QString* error=nullptr);

// Match offsets in NFC display text, in UTF-16 units (QTextCursor).
// Caller normalizes display text first. Limit only the UI highlight count;
// the search result itself still reports the full occurrence count.
inline QVector<qsizetype> highlightOffsets(const QString& display,const QString& query,bool matchCase,int limit=1000) {
    QVector<qsizetype> hits;
    const auto needle=query.trimmed().normalized(QString::NormalizationForm_C);
    if(needle.isEmpty()||needle.size()>256||limit<=0)return hits;
    const auto cs=matchCase?Qt::CaseSensitive:Qt::CaseInsensitive;
    for(qsizetype pos=display.indexOf(needle,0,cs);pos>=0&&hits.size()<limit;
        pos=display.indexOf(needle,pos+needle.size(),cs))hits.append(pos);
    return hits;
}

// Vẽ trang (1-based) vừa khung `box` (giữ tỉ lệ), nền trắng. Ảnh rỗng nếu lỗi.
QImage renderPage(const QString& path, int page1, const QSize& box, QString* error = nullptr);

// In các trang (1-based, đúng thứ tự cho trước, có thể lặp) bằng QPrinter. Mỗi trang vẽ vừa vùng in, giữ tỉ lệ.
// autoOrientation: mỗi trang tự chọn dọc/ngang theo tỉ lệ trang PDF.
bool printPages(const QString& path, QPrinter& printer, const QVector<int>& pages, bool autoOrientation, QString* error);

} // namespace printrender
