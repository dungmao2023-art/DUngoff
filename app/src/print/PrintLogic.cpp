#include "PrintLogic.h"
#include <algorithm>
#include <QObject>
#include <QRegularExpression>

namespace printlogic {

RangeResult parseRanges(const QString& textIn, int total) {
    RangeResult r;
    const QString text = textIn.trimmed();
    if (total < 1) { r.error = QObject::tr("Tài liệu không có trang nào để in."); return r; }
    if (text.isEmpty() || text.compare(QStringLiteral("tất cả"), Qt::CaseInsensitive) == 0 ||
        text.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0) {
        for (int i = 1; i <= total; ++i) r.pages.push_back(i);
        r.ok = true;
        return r;
    }
    static const QRegularExpression part(QStringLiteral("^(\\d*)\\s*(-?)\\s*(\\d*)$"));
    QVector<bool> mark(total + 1, false);
    const QStringList items = text.split(QRegularExpression(QStringLiteral("[,;]")), Qt::KeepEmptyParts);
    for (const QString& raw : items) {
        const QString it = raw.trimmed();
        if (it.isEmpty()) { r.error = QObject::tr("Khoảng trang có chỗ trống thừa (dấu phẩy liên tiếp)."); return r; }
        const auto m = part.match(it);
        if (!m.hasMatch() || (m.captured(1).isEmpty() && m.captured(3).isEmpty())) {
            r.error = QObject::tr("Không hiểu khoảng trang \"%1\". Ví dụ đúng: 1-3, 5, 8-").arg(it);
            return r;
        }
        const bool dash = !m.captured(2).isEmpty();
        if (!dash && !m.captured(3).isEmpty()) {
            r.error = QObject::tr("Thiếu dấu phẩy hoặc dấu gạch giữa các số trong \"%1\".").arg(it);
            return r;
        }
        bool ok1 = true, ok2 = true;
        const long long a = m.captured(1).isEmpty() ? 1 : m.captured(1).toLongLong(&ok1);
        long long b;
        if (!dash) b = a;
        else b = m.captured(3).isEmpty() ? total : m.captured(3).toLongLong(&ok2);
        if (!dash && m.captured(1).isEmpty()) { r.error = QObject::tr("Không hiểu khoảng trang \"%1\".").arg(it); return r; }
        if (!ok1 || !ok2) { r.error = QObject::tr("Số trang \"%1\" quá lớn.").arg(it); return r; }
        if (a < 1 || b < 1) { r.error = QObject::tr("Số trang phải từ 1 trở lên (\"%1\").").arg(it); return r; }
        if (a > total || b > total) {
            r.error = QObject::tr("Trang %1 vượt quá tổng số %2 trang.").arg(std::max(a, b)).arg(total);
            return r;
        }
        if (a > b) { r.error = QObject::tr("Khoảng trang bị ngược: \"%1\".").arg(it); return r; }
        for (long long p = a; p <= b; ++p) mark[int(p)] = true;
    }
    for (int i = 1; i <= total; ++i) if (mark[i]) r.pages.push_back(i);
    r.ok = !r.pages.isEmpty();
    if (!r.ok) r.error = QObject::tr("Chưa chọn trang nào.");
    return r;
}

QVector<int> expandCopies(const QVector<int>& pages, int copies, bool collate) {
    QVector<int> out;
    if (copies < 1) copies = 1;
    if (collate) {
        for (int c = 0; c < copies; ++c) out += pages;
    } else {
        for (int p : pages) for (int c = 0; c < copies; ++c) out.push_back(p);
    }
    return out;
}

QString mismatchWarning(int previewPages, int displayPages) {
    if (displayPages <= 0 || previewPages == displayPages) return QString();
    return QObject::tr("Cảnh báo: bản in có %1 trang nhưng trong cửa sổ soạn thảo hiển thị %2 trang. "
                       "Hãy xem kỹ bản xem trước (đúng là nội dung sẽ in) trước khi in.")
        .arg(previewPages).arg(displayPages);
}

} // namespace printlogic
