// Tìm kiếm tệp: lõi chỉ cần QtCore (không giao diện, không dịch vụ nền).
// - Tìm theo TÊN: không phân biệt hoa/thường và dấu tiếng Việt, nhiều từ khoá, loại trừ, lọc đuôi/dung lượng/ngày.
// - Tìm theo NỘI DUNG: txt/csv/md/log/json/xml/html, docx/xlsx/pptx (đọc XML trong zip, không mở engine),
//   pdf (chỉ khi máy có pdftotext; nếu không thì bỏ qua và nói rõ).
// - Đa luồng có giới hạn, huỷ được, giới hạn bộ nhớ; chỉ mục tên tệp tuỳ chọn do người dùng bật và xoá được.
// - Tệp không đọc được bị bỏ qua và liệt kê lý do; không bao giờ dừng cả phiên vì một tệp lỗi.
#pragma once
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>

namespace filesearch {

// Chuẩn hoá để so khớp: chữ thường, bỏ dấu (NFD rồi bỏ dấu tổ hợp), đ→d. Tương thích cả chữ dựng sẵn lẫn chữ tổ hợp.
// map (nếu có): map[i] = vị trí trong chuỗi gốc của ký tự thứ i trong chuỗi đã chuẩn hoá.
QString foldText(const QString &s, bool stripDiacritics = true, QVector<int> *map = nullptr);

struct Query {
    QString root;                 // thư mục gốc để tìm
    QStringList nameTerms;        // tất cả phải có trong TÊN tệp (AND)
    QStringList excludeTerms;     // không được có trong tên tệp
    QStringList contentTerms;     // tất cả phải có trong NỘI DUNG tệp (AND, theo cả tệp)
    QStringList extensions;       // rỗng = mọi đuôi; không kèm dấu chấm, chữ thường
    qint64 minSize = -1, maxSize = -1;        // byte, -1 = không giới hạn
    QDateTime modifiedAfter, modifiedBefore;  // không hợp lệ = không giới hạn
    bool ignoreDiacritics = true; // true: "bao cao" khớp "Báo cáo"
    bool includeHidden = false;
    int threads = 0;              // 0 = tự chọn (nửa số lõi, tối đa 4)
    int maxResults = 5000;
    int maxMemoryMB = 256;        // trần bộ nhớ ước tính cho việc đọc nội dung
    qint64 maxTextFileBytes = 512LL * 1024 * 1024;   // tệp văn bản lớn hơn: bỏ qua và nêu lý do
    qint64 maxZipFileBytes = 256LL * 1024 * 1024;    // docx/xlsx/pptx lớn hơn: bỏ qua và nêu lý do
    QString pdftotextPath;        // rỗng = tự tìm trong PATH
    bool useIndexForNames = true; // dùng chỉ mục khi có (do người gọi truyền)
};

// Phân tích chuỗi gõ tay: từ khoá, "cụm từ", -loại_trừ, ext:docx,xlsx, size>1mb, size<10mb, after:2026-01-01, before:2026-12-31.
// Phần không hiểu được KHÔNG bị bỏ qua âm thầm: ghi vào warnings.
Query parseQuery(const QString &text, QStringList *warnings = nullptr);

// Tách chuỗi thành các từ khoá, "cụm từ trong ngoặc kép" giữ nguyên là một từ.
QStringList splitTerms(const QString &text);

struct Result {
    QString path;
    qint64 size = 0;
    QDateTime modified;
    QString snippet;   // trích đoạn có từ khoá (khi tìm nội dung)
    QString location;  // dòng N / đoạn N / slide N / Sheet!A1 / trang
};
struct Skipped { QString path, reason; };
struct Stats {
    qint64 filesSeen = 0, filesContentRead = 0, bytesRead = 0, matched = 0, skipped = 0, staleIndexEntries = 0;
    qint64 elapsedMs = 0;
    bool canceled = false, truncatedResults = false, pdfUnavailable = false;
    QHash<QString, qint64> unsupportedByExt; // đuôi → số tệp KHÔNG được tìm nội dung (vd doc, xls)
};
struct Output {
    QVector<Result> results;
    QVector<Skipped> skipped;   // tối đa 2000 mục; Stats::skipped là tổng thật
    Stats stats;
    QString error;              // không rỗng = truy vấn không chạy
};

// ---- Chỉ mục tên tệp (tuỳ chọn, lưu trong thư mục dữ liệu, xoá được) ----
struct IndexEntry { QString rel; qint64 size = 0; qint64 mtimeMs = 0; QString folded; QString foldedCase; };
struct Index {
    QString root;
    qint64 builtMs = 0;
    QVector<IndexEntry> entries;
    bool includeHidden = false;
};
bool buildIndex(const QString &root, const QString &indexDir, bool includeHidden, std::atomic_bool *cancel, QString *error, qint64 *count = nullptr);
bool loadIndex(const QString &root, const QString &indexDir, Index *out, QString *error);
bool removeIndex(const QString &root, const QString &indexDir);
QString indexFilePath(const QString &root, const QString &indexDir);

// Chạy tìm kiếm (chặn cho đến khi xong hoặc bị huỷ). onResult có thể được gọi từ nhiều luồng (đã tuần tự hoá bằng khoá).
Output search(const Query &q, const Index *index = nullptr, std::atomic_bool *cancel = nullptr,
              const std::function<void(const Result &)> &onResult = {},
              const std::function<void(qint64 filesSeen)> &onProgress = {});

// Đọc văn bản của một tệp theo từng đoạn (dòng / đoạn / slide / ô). sink trả false để dừng sớm.
// Trả về chuỗi lỗi rỗng nếu đọc được; ngược lại là lý do bỏ qua.
using ChunkSink = std::function<bool(const QString &text, const QString &location)>;
QString extractChunks(const QString &path, const Query &q, const ChunkSink &sink, bool *truncated = nullptr);

// Giải nén zip tối thiểu (stored + deflate, không zip64, không mã hoá) — dùng chung cho docx/xlsx/pptx.
// Chỉ để đọc XML, giới hạn bộ nhớ nghiêm ngặt.
QByteArray inflateRaw(const QByteArray &deflated, qint64 maxOut, int *status); // status: 0 ok, 1 cắt bớt, <0 lỗi
quint32 crc32(const QByteArray &data);

} // namespace filesearch
