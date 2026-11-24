// SPDX-License-Identifier: MPL-2.0
// Lõi công cụ tệp/thư mục/liên kết (chỉ QtCore, kiểm thử được không cần giao diện).
// Nguyên tắc: không bao giờ xoá vĩnh viễn; thao tác phá huỷ luôn có xem trước, nhật ký và hoàn tác.
#pragma once
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace ft {

// Gọi định kỳ khi chạy lâu; trả false để HUỶ. done/total có thể là số tệp hoặc số byte tuỳ hàm.
using Progress = std::function<bool(qint64 done, qint64 total)>;

// ---------------- Nhật ký (đổi tên / di chuyển) ----------------
struct JournalEntry { QString from, to; };
struct OpResult {
    bool ok = false;
    QString error;           // lý do thất bại (tiếng Việt)
    QStringList warnings;
    QString journalPath;     // nhật ký đã ghi (rỗng nếu chưa ghi)
    int done = 0;
};
// Hoàn tác theo nhật ký: đưa `to` về lại `from`. Bỏ qua (và báo) mục không an toàn: `to` không còn, hoặc `from` đã bị chiếm.
OpResult undoJournal(const QString& journalPath);

// ---------------- Đổi tên hàng loạt ----------------
enum class CaseMode { Keep, Lower, Upper, Title };
struct RenameOptions {
    // Mẫu tên mới (không gồm đuôi nếu tệp có đuôi và keepExtension). Mã: {name} tên cũ, {n} số thứ tự,
    // {date} ngày sửa yyyy-MM-dd, {today} hôm nay, {parent} tên thư mục cha, {ext} đuôi (không dấu chấm).
    QString pattern = QStringLiteral("{name}");
    int start = 1, step = 1, pad = 3;
    QString find, replace;               // thay chữ trong tên (sau khi áp mẫu); find rỗng = bỏ qua
    bool caseSensitive = false;
    bool removeDiacritics = false;       // bỏ dấu tiếng Việt (đ→d)
    bool normalizeSpaces = false;        // gộp khoảng trắng, bỏ khoảng trắng đầu/cuối, "_" giữ nguyên
    CaseMode caseMode = CaseMode::Keep;
    bool keepExtension = true;           // giữ nguyên đuôi tệp
};
enum class RenameStatus { Ok, Unchanged, Conflict, Invalid };
struct RenameItem {
    QString dir, oldName, newName, note;
    RenameStatus status = RenameStatus::Ok;
};
struct RenamePlan {
    QVector<RenameItem> items;
    bool hasProblems() const;            // có mục Conflict/Invalid → không áp dụng
    int changeCount() const;
};
QString removeVietnameseDiacritics(const QString& s);
// Tạo kế hoạch (xem trước đầy đủ). Không đụng tới đĩa ngoài việc đọc thông tin tệp.
RenamePlan planRename(const QStringList& paths, const RenameOptions& o);
// Áp dụng: từ chối nếu có xung đột; dùng 2 pha (tên tạm) nên hoán đổi/chuỗi tên vẫn an toàn; lỗi giữa chừng thì tự quay lại.
OpResult applyRename(const RenamePlan& plan, const QString& journalDir);

// ---------------- Tìm tệp trùng ----------------
struct DupOptions {
    qint64 minSize = 1;                  // bỏ qua tệp nhỏ hơn (mặc định bỏ tệp rỗng)
    bool recursive = true;
};
struct DupGroup { qint64 size = 0; QString sha256; QStringList files; };
struct DupResult {
    QVector<DupGroup> groups;
    QStringList skipped;                 // "đường dẫn: lý do" — tệp không đọc được (bị khoá, thiếu quyền...)
    qint64 wastedBytes = 0;              // dung lượng có thể giải phóng nếu giữ 1 bản mỗi nhóm
    bool cancelled = false;
};
DupResult findDuplicates(const QStringList& roots, const DupOptions& o, const Progress& p = {});
// Chuyển các tệp được chọn vào thư mục `destDir` (vd "<gốc>/_to_delete"), KHÔNG xoá. Từ chối nếu chọn hết mọi bản trong một nhóm.
OpResult moveDuplicates(const DupResult& r, const QStringList& selected, const QString& destDir, const QString& journalDir);

// ---------------- So sánh hai thư mục ----------------
enum class DiffState { Same, OnlyA, OnlyB, Different, NewerA, NewerB };
struct DiffEntry { QString rel; DiffState state = DiffState::Same; qint64 sizeA = -1, sizeB = -1; QDateTime timeA, timeB; };
struct CompareOptions { bool compareContent = false; /* true: băm nội dung khi kích thước bằng nhau */ };
struct CompareResult { QVector<DiffEntry> entries; QStringList skipped; bool cancelled = false; };
CompareResult compareFolders(const QString& a, const QString& b, const CompareOptions& o, const Progress& p = {});
QString diffStateText(DiffState s);
// Xuất báo cáo CSV (UTF-8 có BOM). Từ chối ghi đè tệp đã có.
OpResult exportCompareCsv(const QString& path, const CompareResult& r, const QString& a, const QString& b);

// ---------------- Dung lượng thư mục ----------------
struct UsageNode { QString name, path; qint64 size = 0; qint64 files = 0; bool isDir = false; };
struct UsageResult {
    qint64 total = 0, totalFiles = 0;
    QVector<UsageNode> children;         // con trực tiếp, sắp xếp giảm dần theo dung lượng
    QVector<UsageNode> largestFiles;     // N tệp lớn nhất toàn cây
    QStringList skipped;
    bool cancelled = false;
};
UsageResult diskUsage(const QString& root, int topFiles = 50, const Progress& p = {});
QString humanSize(qint64 bytes);

// ---------------- Liên kết ----------------
struct LinkOptions {
    bool stripTracking = true;           // bỏ utm_*, fbclid, gclid...
    bool dropFragment = false;           // bỏ phần #...
    bool lowerHost = true;
};
QStringList extractUrls(const QString& text);      // gom http/https/www từ văn bản, bỏ dấu câu cuối
QString cleanUrl(const QString& url, const LinkOptions& o, QStringList* removedParams = nullptr);
struct LinkClean { QStringList urls; int duplicates = 0; int cleaned = 0; QStringList warnings; };
LinkClean cleanLinks(const QStringList& in, const LinkOptions& o);   // làm sạch + bỏ trùng, giữ thứ tự
// Xuất danh sách (.txt một dòng/URL hoặc .csv có BOM). Từ chối ghi đè.
OpResult exportLinks(const QString& path, const QStringList& urls);

// ---------------- Xuất CSV dùng chung ----------------
// UTF-8 có BOM, chống chèn công thức. Từ chối ghi đè tệp đã có.
OpResult exportCsvRows(const QString& path, const QStringList& header, const QVector<QStringList>& rows);

// ---------------- Sắp xếp tệp vào thư mục ----------------
enum class OrganizeMode { ByCategory, ByExtension, ByYear, ByYearMonth };
struct OrganizeItem { QString from, to, note; };
struct OrganizePlan {
    QVector<OrganizeItem> items;         // tệp sẽ được chuyển
    QStringList skipped;                 // "đường dẫn: lý do" (đã đúng chỗ, tên quá dài, ...)
    int renamedForConflict = 0;          // số tệp phải thêm " (2)" vì trùng tên ở đích
};
QString fileCategory(const QString& suffix);   // "Tài liệu", "Bảng tính", "Ảnh", ... hoặc "Khác"
// Chỉ lập kế hoạch (không đụng tới đĩa). recursive=false: chỉ tệp nằm trực tiếp trong srcDir.
// Tệp ẩn và tệp đang nằm trong destRoot bị bỏ qua. Không bao giờ ghi đè: trùng tên thì thêm " (2)".
OrganizePlan planOrganize(const QString& srcDir, bool recursive, OrganizeMode m, const QString& destRoot);
// Chuyển theo kế hoạch, có nhật ký; lỗi giữa chừng thì khôi phục những tệp đã chuyển. Hoàn tác bằng undoJournal.
OpResult applyOrganize(const OrganizePlan& plan, const QString& journalDir);

// ---------------- Đường dẫn ----------------
constexpr int kWinMaxPath = 260;          // MAX_PATH của Windows (gồm ký tự kết thúc) — đường dẫn từ 260 ký tự trở lên dễ lỗi
constexpr int kPathNearLimit = 240;
struct PathForms {
    QString full;        // dạng đầy đủ, dấu phân cách theo hệ điều hành
    QString quoted;      // có dấu ngoặc kép (dán vào dòng lệnh)
    QString slashes;     // dùng dấu /
    QString fileUrl;     // file:///...
    QString folder;      // thư mục chứa
    QString name;        // tên tệp
    QString extended;    // dạng \\?\ (đường dẫn dài của Windows)
    int length = 0;      // số ký tự UTF-16 của `full`
    bool tooLong = false, nearLimit = false;
};
PathForms pathForms(const QString& path);
struct LongPathResult { QStringList paths; QStringList skipped; int scanned = 0; bool cancelled = false; };
// Liệt kê tệp/thư mục có đường dẫn đầy đủ dài từ `limit` ký tự trở lên (mặc định 260).
LongPathResult findLongPaths(const QString& root, int limit = kWinMaxPath, const Progress& p = {});

} // namespace ft
