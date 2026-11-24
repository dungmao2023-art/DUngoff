#include "TableOps.h"
#include <QObject>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QDate>
#include <QRegularExpression>
#include <QTextStream>

namespace tbl {

// ---------------- CSV ----------------
static QVector<QStringList> parseCsv(const QString &text, QChar sep, QString &err)
{
    QVector<QStringList> rows;
    QStringList row; QString cell; bool inQ = false, any = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (inQ) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) { cell += c; ++i; } else inQ = false;
            } else cell += c;
        } else if (c == QLatin1Char('"')) { inQ = true; any = true; }
        else if (c == sep) { row << cell; cell.clear(); any = true; }
        else if (c == QLatin1Char('\r')) { /* bỏ qua, xử lý ở \n */ }
        else if (c == QLatin1Char('\n')) { row << cell; rows << row; row.clear(); cell.clear(); any = false; }
        else { cell += c; any = true; }
    }
    if (inQ) { err = QObject::tr("Dấu nháy đóng chưa khớp trong CSV."); return {}; }
    if (any || !cell.isEmpty() || !row.isEmpty()) { row << cell; rows << row; }
    return rows;
}

bool readCsv(const QString &path, Table &out, QString &error, QChar sep)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { error = QObject::tr("Không mở được tệp: ") + f.errorString(); return false; }
    QByteArray raw = f.readAll();
    if (raw.startsWith("\xEF\xBB\xBF")) raw.remove(0, 3);
    const QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter)) { error = QObject::tr("Tệp không phải UTF-8 hợp lệ; không đoán bảng mã."); return false; }
    auto rows = parseCsv(text, sep, error);
    if (!error.isEmpty()) return false;
    out = Table();
    if (rows.isEmpty()) return true;
    out.header = rows.takeFirst();
    out.rows = rows;
    return true;
}

static QString csvEscape(const QString &s, QChar sep)
{
    if (s.contains(sep) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r'))) {
        QString e = s; e.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + e + QLatin1Char('"');
    }
    return s;
}

bool writeCsvCopy(const QString &path, const Table &t, QString &error, QChar sep)
{
    if (QFile::exists(path)) { error = QObject::tr("Tệp đích đã tồn tại; không ghi đè. Hãy chọn tên khác."); return false; }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { error = f.errorString(); return false; }
    QString out;
    auto line = [&](const QStringList &r) {
        QStringList e; for (const QString &c : r) e << csvEscape(c, sep);
        out += e.join(sep) + QLatin1Char('\n');
    };
    line(t.header);
    for (const auto &r : t.rows) line(r);
    f.write("\xEF\xBB\xBF"); // BOM để Excel đọc đúng tiếng Việt
    f.write(out.toUtf8());
    if (!f.flush()) { error = f.errorString(); return false; }
    return true;
}

// ---------------- 1. Dòng trùng ----------------
static QString cellAt(const QStringList &r, int c) { return c >= 0 && c < r.size() ? r[c] : QString(); }

DedupResult removeDuplicates(const Table &t, const QList<int> &cols, bool trimAndFold)
{
    DedupResult res; res.cleaned.header = t.header;
    if (cols.isEmpty()) { res.ok = false; res.warnings << QObject::tr("Chưa chọn cột làm khoá."); res.cleaned = t; return res; }
    for (int c : cols) if (c < 0 || c >= t.header.size()) {
        res.ok = false; res.warnings << QObject::tr("Cột khoá không tồn tại: %1").arg(c); res.cleaned = t; return res; }

    QHash<QString, int> first; // khoá -> chỉ số dòng gốc
    int emptyKeys = 0;
    for (int i = 0; i < t.rows.size(); ++i) {
        QStringList parts; bool allEmpty = true;
        for (int c : cols) {
            QString v = cellAt(t.rows[i], c);
            if (trimAndFold) v = v.trimmed().toCaseFolded();
            if (!v.isEmpty()) allEmpty = false;
            parts << v;
        }
        if (allEmpty) { res.cleaned.rows << t.rows[i]; ++emptyKeys; continue; }
        const QString key = parts.join(QChar(0x1F));
        auto it = first.constFind(key);
        if (it == first.constEnd()) { first.insert(key, i); res.cleaned.rows << t.rows[i]; }
        else res.removed.push_back({i, t.rows[i], it.value()});
    }
    if (emptyKeys) res.warnings << QObject::tr("%1 dòng có khoá rỗng được giữ lại (không coi là trùng).").arg(emptyKeys);
    return res;
}

Table undoDedup(const DedupResult &r)
{
    Table t; t.header = r.cleaned.header;
    int ci = 0, ri = 0; const int total = r.cleaned.rows.size() + r.removed.size();
    for (int i = 0; i < total; ++i) {
        if (ri < r.removed.size() && r.removed[ri].originalIndex == i) t.rows << r.removed[ri++].cells;
        else t.rows << r.cleaned.rows[ci++];
    }
    return t;
}

// ---------------- 2. So sánh ----------------
CompareResult compareTables(const Table &a, const Table &b, const QString &keyColumn)
{
    CompareResult r;
    const int ka = a.header.indexOf(keyColumn), kb = b.header.indexOf(keyColumn);
    if (ka < 0 || kb < 0) { r.ok = false; r.warnings << QObject::tr("Cột mã '%1' không có ở cả hai bảng.").arg(keyColumn); return r; }

    QStringList common;
    for (const QString &h : a.header) if (b.header.contains(h)) common << h;
    for (const QString &h : a.header) if (!b.header.contains(h)) r.warnings << QObject::tr("Cột chỉ có ở bảng A (không so sánh): %1").arg(h);
    for (const QString &h : b.header) if (!a.header.contains(h)) r.warnings << QObject::tr("Cột chỉ có ở bảng B (không so sánh): %1").arg(h);

    auto index = [&](const Table &t, int kc, const char *name, QHash<QString, int> &m, QSet<QString> &dup) {
        for (int i = 0; i < t.rows.size(); ++i) {
            const QString k = cellAt(t.rows[i], kc);
            if (k.isEmpty()) { r.warnings << QObject::tr("Bảng %1 dòng %2 không có mã; bỏ qua khi so sánh.").arg(QLatin1String(name)).arg(i + 2); continue; }
            if (m.contains(k)) dup.insert(k); else m.insert(k, i);
        }
    };
    QHash<QString, int> ma, mb; QSet<QString> da, db;
    index(a, ka, "A", ma, da); index(b, kb, "B", mb, db);
    for (const QString &k : da) r.warnings << QObject::tr("Mã trùng trong bảng A: '%1' (chỉ so dòng đầu tiên).").arg(k);
    for (const QString &k : db) r.warnings << QObject::tr("Mã trùng trong bảng B: '%1' (chỉ so dòng đầu tiên).").arg(k);

    for (int i = 0; i < a.rows.size(); ++i) {
        const QString k = cellAt(a.rows[i], ka);
        if (k.isEmpty() || ma.value(k, -1) != i) continue;
        auto it = mb.constFind(k);
        if (it == mb.constEnd()) { r.onlyInA.push_back(a.rows[i]); continue; }
        bool diff = false;
        for (const QString &h : common) {
            if (h == keyColumn) continue;
            const QString va = cellAt(a.rows[i], a.header.indexOf(h)), vb = cellAt(b.rows[it.value()], b.header.indexOf(h));
            if (va != vb) { r.changed.push_back({k, h, va, vb}); diff = true; } // so sánh CHÍNH XÁC chuỗi, không ép kiểu
        }
        if (!diff) ++r.same;
    }
    for (int i = 0; i < b.rows.size(); ++i) {
        const QString k = cellAt(b.rows[i], kb);
        if (k.isEmpty() || mb.value(k, -1) != i) continue;
        if (!ma.contains(k)) r.onlyInB.push_back(b.rows[i]);
    }
    return r;
}

Table diffAsTable(const CompareResult &r)
{
    Table t; t.header = {QObject::tr("Loại"), QObject::tr("Mã"), QObject::tr("Cột"), QObject::tr("Giá trị A"), QObject::tr("Giá trị B")};
    for (const auto &row : r.onlyInA) t.rows << QStringList{QObject::tr("Chỉ có ở A"), row.value(0), QString(), row.join(QStringLiteral(" | ")), QString()};
    for (const auto &row : r.onlyInB) t.rows << QStringList{QObject::tr("Chỉ có ở B"), row.value(0), QString(), QString(), row.join(QStringLiteral(" | "))};
    for (const auto &c : r.changed) t.rows << QStringList{QObject::tr("Khác giá trị"), c.key, c.column, c.valueA, c.valueB};
    return t;
}

// ---------------- 3. Làm sạch ----------------
bool isProtectedText(const QString &s)
{
    const QString v = s.trimmed();
    if (v.isEmpty()) return false;
    for (const QChar c : v) if (!c.isDigit()) return false;
    return (v.size() > 1 && v[0] == QLatin1Char('0')) || v.size() > 15;
}

CleanCell cleanDate(const QString &s)
{
    CleanCell r; r.value = s;
    const QString v = s.trimmed();
    if (v.isEmpty()) return r;
    static const QRegularExpression dmy(QStringLiteral("^(\\d{1,2})[/\\-.](\\d{1,2})[/\\-.](\\d{4})$"));
    static const QRegularExpression ymd(QStringLiteral("^(\\d{4})[/\\-.](\\d{1,2})[/\\-.](\\d{1,2})$"));
    static const QRegularExpression dmy2(QStringLiteral("^\\d{1,2}[/\\-.]\\d{1,2}[/\\-.]\\d{2}$"));
    int d = 0, m = 0, y = 0;
    if (auto mt = dmy.match(v); mt.hasMatch()) { d = mt.captured(1).toInt(); m = mt.captured(2).toInt(); y = mt.captured(3).toInt(); }
    else if (auto mt2 = ymd.match(v); mt2.hasMatch()) { y = mt2.captured(1).toInt(); m = mt2.captured(2).toInt(); d = mt2.captured(3).toInt(); }
    else if (dmy2.match(v).hasMatch()) { r.warning = QObject::tr("Năm 2 chữ số không rõ thế kỷ: giữ nguyên."); return r; }
    else if (isProtectedText(v) || (v.size() == 5 && v.toInt() > 0)) { r.warning = QObject::tr("Có thể là số hiệu hoặc số seri ngày của Excel, không phải ngày: giữ nguyên."); return r; }
    else { r.warning = QObject::tr("Không nhận ra định dạng ngày: giữ nguyên."); return r; }
    if (!QDate::isValid(y, m, d)) { r.warning = QObject::tr("Ngày không tồn tại trên lịch: giữ nguyên."); return r; }
    const QString out = QStringLiteral("%1/%2/%3").arg(d, 2, 10, QLatin1Char('0')).arg(m, 2, 10, QLatin1Char('0')).arg(y, 4, 10, QLatin1Char('0'));
    r.value = out; r.converted = (out != s);
    return r;
}

CleanCell cleanMoney(const QString &s, MoneyLocale loc)
{
    CleanCell r; r.value = s;
    QString v = s.trimmed();
    if (v.isEmpty()) return r;
    if (isProtectedText(v)) { r.warning = QObject::tr("Chuỗi số bắt đầu bằng 0 hoặc quá dài (mã/số điện thoại?): giữ nguyên."); return r; }

    // Bỏ ký hiệu tiền ở đầu/cuối, không đụng phần giữa.
    static const QRegularExpression suffix(QStringLiteral("\\s*(vnđ|vnd|đồng|đ|₫|d)\\s*$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression prefix(QStringLiteral("^\\s*(vnđ|vnd|₫)\\s*"), QRegularExpression::CaseInsensitiveOption);
    v.remove(suffix); v.remove(prefix); v = v.trimmed();
    bool neg = false;
    if (v.startsWith(QLatin1Char('-'))) { neg = true; v.remove(0, 1); }
    else if (v.startsWith(QLatin1Char('(')) && v.endsWith(QLatin1Char(')'))) { neg = true; v = v.mid(1, v.size() - 2); }
    v.remove(QLatin1Char(' ')); v.remove(QChar(0x00A0));

    static const QRegularExpression onlyOk(QStringLiteral("^[0-9.,]+$"));
    if (v.isEmpty() || !onlyOk.match(v).hasMatch()) { r.warning = QObject::tr("Có ký tự không phải số/tiền: giữ nguyên."); return r; }

    const QChar thou = loc == MoneyLocale::VN ? QLatin1Char('.') : QLatin1Char(',');
    const QChar dec  = loc == MoneyLocale::VN ? QLatin1Char(',') : QLatin1Char('.');
    QString intPart = v, frac;
    const int di = v.indexOf(dec);
    if (di >= 0) {
        if (v.indexOf(dec, di + 1) >= 0) { r.warning = QObject::tr("Nhiều dấu thập phân: giữ nguyên."); return r; }
        intPart = v.left(di); frac = v.mid(di + 1);
        if (frac.contains(thou)) { r.warning = QObject::tr("Dấu nghìn nằm sau dấu thập phân: giữ nguyên."); return r; }
    }
    if (intPart.contains(thou)) {
        const QStringList g = intPart.split(thou);
        bool good = !g[0].isEmpty() && g[0].size() <= 3;
        for (int i = 1; i < g.size(); ++i) if (g[i].size() != 3) good = false;
        if (!good) { r.warning = QObject::tr("Dấu phân cách nghìn sai vị trí: giữ nguyên."); return r; }
        intPart = g.join(QString());
    }
    if (intPart.isEmpty()) intPart = QStringLiteral("0");
    if (intPart.size() > 1 && intPart[0] == QLatin1Char('0')) { r.warning = QObject::tr("Số có số 0 đứng đầu: giữ nguyên."); return r; }
    if (!frac.isEmpty()) {
        // Bỏ số 0 thừa cuối phần thập phân, KHÔNG làm tròn.
        while (frac.endsWith(QLatin1Char('0'))) frac.chop(1);
    }
    QString out = intPart + (frac.isEmpty() ? QString() : QLatin1Char('.') + frac);
    if (neg && out != QStringLiteral("0")) out.prepend(QLatin1Char('-'));
    r.value = out; r.converted = (out != s);
    return r;
}

CleanResult cleanTable(const Table &t, const QList<QPair<QString, ColumnKind>> &kinds, MoneyLocale loc)
{
    CleanResult res; res.cleaned = t;
    for (const auto &k : kinds) {
        const int c = t.header.indexOf(k.first);
        if (c < 0) { res.warnings << QObject::tr("Cột '%1' không tồn tại: bỏ qua.").arg(k.first); continue; }
        if (k.second == ColumnKind::Keep) continue;
        for (int i = 0; i < t.rows.size(); ++i) {
            if (c >= t.rows[i].size()) continue;
            const QString before = t.rows[i][c];
            const CleanCell cc = k.second == ColumnKind::Date ? cleanDate(before) : cleanMoney(before, loc);
            if (cc.converted) { res.cleaned.rows[i][c] = cc.value; res.changes.push_back({i, c, before, cc.value}); }
            else if (!cc.warning.isEmpty())
                res.warnings << QObject::tr("Dòng %1, cột '%2' (%3): %4").arg(i + 2).arg(k.first, before, cc.warning);
        }
    }
    return res;
}

} // namespace tbl
