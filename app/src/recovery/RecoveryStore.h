// SPDX-License-Identifier: MPL-2.0
// Kho bản phục hồi (tự lưu). Chỉ dùng QtCore để kiểm thử được không cần giao diện.
//
// Quy ước:
//  - Mỗi lần chạy ứng dụng (phiên) giữ một tệp khoá `run-<runId>.lock`. Phiên bị tắt đột ngột để lại khoá cũ;
//    phiên sau nhận ra khoá "chết" (tiến trình không còn chạy) và coi các bản phục hồi của phiên đó là "mồ côi".
//  - Mỗi tài liệu có một bản phục hồi: `<runId>-<docId>.<đuôi>` (dữ liệu) và `<runId>-<docId>.json` (mô tả).
//    Dữ liệu được ghi trước, mô tả ghi sau và nguyên tử => có mô tả nghĩa là dữ liệu đã đầy đủ.
//  - Không bao giờ chạm tệp gốc của người dùng. Thoát bình thường: mọi bản phục hồi của phiên bị xoá.
#pragma once
#include <QDateTime>
#include <QList>
#include <QString>

namespace recovery {

struct Entry {
    QString id;            // "<runId>-<docId>"
    QString runId;
    QString displayName;   // tên hiển thị của tài liệu khi đang sửa
    QString originalPath;  // tệp gốc (rỗng nếu là tài liệu mới chưa lưu)
    QString ext;           // docx / xlsx / pptx
    QString dataPath;
    QString metaPath;
    QDateTime saved;
    qint64 size = 0;
    bool isNew = false;
};

// Thư mục phục hồi: <thư mục dữ liệu ứng dụng>/recovery. Có thể đổi để kiểm thử.
QString dir();
void setDirForTest(const QString& path);

class Session {
public:
    static Session& instance();
    bool begin();                 // tạo thư mục, giữ khoá của phiên này. Gọi nhiều lần vẫn an toàn.
    void end();                   // thoát bình thường: xoá bản phục hồi còn sót của phiên và nhả khoá
    bool active() const;
    QString runId() const;
    QString newDocId();           // mã tài liệu duy nhất trong phiên
    QString dataPath(const QString& docId, const QString& ext) const;
    // Gọi SAU khi tệp dữ liệu đã ghi xong: ghi mô tả (nguyên tử).
    bool commit(const QString& docId, const QString& ext, const QString& displayName, const QString& originalPath, bool isNew);
    void discard(const QString& docId);   // xoá dữ liệu + mô tả của tài liệu
    bool hasRoom(qint64 maxTotalBytes = Q_INT64_C(1) << 30) const;  // tổng thư mục còn dưới giới hạn (mặc định 1 GB)

private:
    Session() = default;
    struct Impl;
    Impl* d = nullptr;
};

// Bản phục hồi của các phiên đã chết (không phải của phiên đang chạy, không phải của phiên khác còn sống).
QList<Entry> findOrphans();
bool discardEntry(const Entry& e);
// Dọn: xoá bản mồ côi quá `maxAgeDays` ngày; nếu tổng dung lượng vẫn vượt `maxTotalBytes` thì xoá bản cũ nhất trước.
// Cũng xoá tệp dữ liệu không có mô tả (ghi dở) cũ hơn 1 ngày. Trả về số bản đã xoá.
int pruneOld(int maxAgeDays = 30, qint64 maxTotalBytes = Q_INT64_C(1) << 30);

}  // namespace recovery
