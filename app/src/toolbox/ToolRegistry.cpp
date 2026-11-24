// SPDX-License-Identifier: MPL-2.0
#include "ToolRegistry.h"

#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <algorithm>

namespace toolbox {
namespace {
struct Store {
    QMutex mu;
    QList<ToolInfo> list;
};
Store& store() { // đối tượng tĩnh cục bộ: an toàn thứ tự khởi tạo giữa các tệp đăng ký
    static Store s;
    return s;
}
bool lessInfo(const ToolInfo& a, const ToolInfo& b) {
    const int c = a.category.localeAwareCompare(b.category);
    if (c != 0) return c < 0;
    return a.name.localeAwareCompare(b.name) < 0;
}
} // namespace

bool isValidId(const QString& id) {
    static const QRegularExpression re(QStringLiteral("^[a-z0-9][a-z0-9_-]{1,40}$"));
    return re.match(id).hasMatch();
}

bool registerTool(const ToolInfo& info) {
    if (!isValidId(info.id) || !info.create || info.name.trimmed().isEmpty()) return false;
    Store& s = store();
    QMutexLocker lock(&s.mu);
    for (const auto& t : s.list)
        if (t.id == info.id) return false; // trùng mã: giữ công cụ đăng ký trước, không ghi đè âm thầm
    ToolInfo copy = info;
    if (copy.category.trimmed().isEmpty()) copy.category = QStringLiteral("Khác");
    s.list.append(copy);
    return true;
}

QList<ToolInfo> tools() {
    Store& s = store();
    QMutexLocker lock(&s.mu);
    QList<ToolInfo> out = s.list;
    std::sort(out.begin(), out.end(), lessInfo);
    return out;
}

QStringList categories() {
    QStringList out;
    for (const auto& t : tools())
        if (!out.contains(t.category)) out << t.category;
    out.sort(Qt::CaseInsensitive);
    return out;
}

QList<ToolInfo> byCategory(const QString& c) {
    QList<ToolInfo> out;
    for (const auto& t : tools())
        if (t.category == c) out << t;
    return out;
}

bool find(const QString& id, ToolInfo* out) {
    Store& s = store();
    QMutexLocker lock(&s.mu);
    for (const auto& t : s.list)
        if (t.id == id) {
            if (out) *out = t;
            return true;
        }
    return false;
}

QString foldVn(const QString& s) {
    QString d = s.normalized(QString::NormalizationForm_D).toLower();
    QString out;
    out.reserve(d.size());
    for (const QChar c : d) {
        if (c.category() == QChar::Mark_NonSpacing) continue; // bỏ dấu thanh/mũ
        out += (c == QChar(0x0111)) ? QChar('d') : c;         // đ → d
    }
    return out;
}

QList<ToolInfo> search(const QString& query) {
    const QStringList words = foldVn(query).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) return tools();
    QList<ToolInfo> out;
    for (const auto& t : tools()) {
        const QString hay = foldVn(t.name + QLatin1Char(' ') + t.description + QLatin1Char(' ') + t.category);
        bool all = true;
        for (const auto& w : words)
            if (!hay.contains(w)) { all = false; break; }
        if (all) out << t;
    }
    return out;
}

void clearForTest() {
    Store& s = store();
    QMutexLocker lock(&s.mu);
    s.list.clear();
}

} // namespace toolbox
