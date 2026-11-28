// SPDX-License-Identifier: MPL-2.0
#include "DataChecks.h"
#include <QRegularExpression>
#include <QMap>
#include <cmath>
#include <algorithm>

namespace dataassist {

QVector<QStringList> parseTsv(const QString &text)
{
    QVector<QStringList> g;
    QString t = text;
    if (t.endsWith(QLatin1Char('\n'))) t.chop(1);
    if (t.endsWith(QLatin1Char('\r'))) t.chop(1);
    if (t.isEmpty()) return g;
    for (const QString &line : t.split(QLatin1Char('\n'))) {
        QString l = line;
        if (l.endsWith(QLatin1Char('\r'))) l.chop(1);
        g.push_back(l.split(QLatin1Char('\t')));
    }
    return g;
}

NumParse parseNumber(const QString &sIn, bool vi)
{
    NumParse r;
    QString s = sIn.trimmed();
    if (s.isEmpty()) return r;
    static const QRegularExpression cur(QStringLiteral("\\s*(₫|đ|vnd|vnđ|\\$|usd)$"), QRegularExpression::CaseInsensitiveOption);
    s.remove(cur);
    bool pct = false;
    if (s.endsWith(QLatin1Char('%'))) { pct = true; s.chop(1); s = s.trimmed(); }
    bool neg = false;
    if (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')'))) {
        neg = true; s = s.mid(1, s.size() - 2);
        // Parentheses already encode the sign: do not reinterpret a second sign.
        if (s.startsWith(QLatin1Char('-')) || s.startsWith(QLatin1Char('+'))) return r;
    }
    if (s.startsWith(QLatin1Char('-'))) { neg = true; s = s.mid(1); }
    else if (s.startsWith(QLatin1Char('+'))) s = s.mid(1);
    if (s.isEmpty()) return r;
    const QChar thou = vi ? QLatin1Char('.') : QLatin1Char(',');
    const QChar dec = vi ? QLatin1Char(',') : QLatin1Char('.');
    for (QChar c : s) if (!(c.isDigit() || c == thou || c == dec)) return r;
    if (!s[0].isDigit() && s[0] != dec) return r;
    QString intPart = s, decPart;
    const int di = s.indexOf(dec);
    if (di >= 0) {
        if (di==s.size()-1 || s.indexOf(dec, di + 1) >= 0) return r;
        intPart = s.left(di); decPart = s.mid(di + 1);
        if (decPart.contains(thou)) return r;
    }
    if (intPart.contains(thou)) {
        const QStringList gr = intPart.split(thou);
        if (gr[0].isEmpty() || gr[0].size() > 3) return r;
        for (int i = 1; i < gr.size(); ++i) if (gr[i].size() != 3) return r;
        intPart = gr.join(QString());
    } else if (di < 0 && s.count(thou) == 0) {
        // số thuần
    }
    // "1.234" kiểu Việt = 1234 nhưng người dùng Anh sẽ hiểu 1,234 => đây là phép đọc theo cài đặt, nhưng đánh dấu mơ hồ khi chỉ có MỘT nhóm 3 chữ số.
    if (s.count(thou)==1 && di<0 && intPart.size()>=4 && s.size()-s.indexOf(thou)-1==3) r.ambiguous=true;
    if (di>=1 && di<=3 && !intPart.contains(thou) && !s.contains(thou) && decPart.size()==3) r.ambiguous=true;
    if (intPart.isEmpty()) intPart = QStringLiteral("0");
    // Excel numeric cells retain at most 15 significant decimal digits.
    // Preserve identifiers and amounts that would otherwise be silently rounded,
    // including when grouping or currency markers hid a long integer.
    if (intPart.size() > 15 || (intPart.size() > 1 && intPart.startsWith(QLatin1Char('0')))) return r;
    const QString digits = intPart + decPart;
    qsizetype first = 0, last = digits.size();
    while (first < last && digits[first] == QLatin1Char('0')) ++first;
    while (last > first && digits[last - 1] == QLatin1Char('0')) --last;
    if (last - first > 15) return r;
    bool ok = false;
    const double v = (intPart + (decPart.isEmpty() ? QString() : QStringLiteral(".") + decPart)).toDouble(&ok);
    if (!ok || !std::isfinite(v)) return r;
    const double result = (neg ? -v : v) / (pct ? 100.0 : 1.0);
    if (result == 0.0 && last > first) return r; // underflow, not a true zero
    r.ok = true;
    r.value = result;
    return r;
}

bool isErrorText(const QString &sIn)
{
    const QString s = sIn.trimmed();
    static const QRegularExpression re(QStringLiteral("^(#(DIV/0!|N/A|NAME\\?|REF!|VALUE!|NUM!|NULL!|SPILL!|CALC!)|Err:\\d{3})$"), QRegularExpression::CaseInsensitiveOption);
    return re.match(s).hasMatch();
}

QString explainError(const QString &sIn)
{
    const QString s = sIn.trimmed().toUpper();
    if (s == QLatin1String("#DIV/0!") ) return QStringLiteral("Chia cho 0 hoặc ô trống");
    if (s == QLatin1String("#N/A")) return QStringLiteral("Không tìm thấy giá trị (VLOOKUP/MATCH...)");
    if (s == QLatin1String("#NAME?")) return QStringLiteral("Tên hàm hoặc tên vùng sai");
    if (s == QLatin1String("#REF!")) return QStringLiteral("Tham chiếu tới ô đã bị xoá");
    if (s == QLatin1String("#VALUE!")) return QStringLiteral("Kiểu dữ liệu không phù hợp (vd. cộng chữ với số)");
    if (s == QLatin1String("#NUM!")) return QStringLiteral("Số không hợp lệ hoặc quá lớn/nhỏ");
    if (s == QLatin1String("#NULL!")) return QStringLiteral("Giao của hai vùng rỗng");
    if (s == QLatin1String("#SPILL!")) return QStringLiteral("Kết quả mảng bị ô khác chặn");
    if (s == QLatin1String("#CALC!")) return QStringLiteral("Lỗi tính toán mảng");
    if (s.startsWith(QLatin1String("ERR:"))) {
        if (s == QLatin1String("ERR:508") || s == QLatin1String("ERR:509")) return QStringLiteral("Thiếu hoặc sai dấu ngoặc/toán tử");
        if (s == QLatin1String("ERR:522") || s == QLatin1String("ERR:523")) return QStringLiteral("Tham chiếu vòng hoặc không hội tụ");
        if (s == QLatin1String("ERR:504")) return QStringLiteral("Sai tham số của hàm");
        return QStringLiteral("Lỗi công thức của engine (mã %1)").arg(s.mid(4));
    }
    return QStringLiteral("Lỗi công thức");
}

QVector<FormulaErrorHit> findFormulaErrors(const QVector<QStringList> &grid)
{
    QVector<FormulaErrorHit> hits;
    for (int r = 0; r < grid.size(); ++r)
        for (int c = 0; c < grid[r].size(); ++c)
            if (isErrorText(grid[r][c])) hits.push_back({r + 1, c + 1, grid[r][c].trimmed(), explainError(grid[r][c])});
    return hits;
}

static bool looksLikeProtectedCode(const QString &s)
{
    const QString t = s.trimmed();
    if (t.size() > 1 && t.startsWith(QLatin1Char('0'))) {
        for (QChar c : t) if (!c.isDigit()) return false;
        return true;
    }
    if (t.size() > 15) { for (QChar c : t) if (!c.isDigit()) return false; return true; }
    return false;
}

RangeStats rangeStats(const QVector<QStringList> &grid, bool vi)
{
    RangeStats st;
    for (const QStringList &row : grid) for (const QString &cell : row) {
        ++st.cells;
        const QString t = cell.trimmed();
        if (t.isEmpty()) { ++st.empty; continue; }
        if (isErrorText(t)) { ++st.errors; continue; }
        if (looksLikeProtectedCode(t)) { ++st.text; continue; }
        const NumParse n = parseNumber(t, vi);
        if (!n.ok) { ++st.text; continue; }
        if (n.ambiguous) ++st.ambiguous;
        if (!st.hasNumbers) { st.min = st.max = n.value; st.hasNumbers = true; }
        st.min = std::min(st.min, n.value); st.max = std::max(st.max, n.value);
        st.sum += n.value; ++st.numeric;
    }
    if (st.numeric) st.average = st.sum / st.numeric;
    if (st.errors) st.warnings << QStringLiteral("Vùng có %1 ô lỗi công thức — tổng/trung bình chưa tính các ô này.").arg(st.errors);
    if (st.ambiguous) st.warnings << QStringLiteral("%1 ô dạng 1.234 có thể là 1234 (kiểu Việt) hoặc 1,234 (kiểu Anh) — đang đọc theo kiểu %2.").arg(st.ambiguous).arg(vi ? QStringLiteral("Việt") : QStringLiteral("Anh"));
    if (st.text && st.numeric) st.warnings << QStringLiteral("%1 ô là chữ (hoặc mã bắt đầu bằng 0) nên không được tính vào tổng.").arg(st.text);
    return st;
}

ChartData groupForChart(const QVector<QStringList> &grid, int labelCol, int valueCol, bool hasHeader, bool vi, int maxPoints)
{
    ChartData d;
    if(maxPoints<1){d.warnings<<QStringLiteral("Giới hạn số nhóm không hợp lệ.");return d;}
    QMap<QString, int> idx;
    QVector<ChartPoint> pts;
    int bad = 0;
    for (int r = hasHeader ? 1 : 0; r < grid.size(); ++r) {
        const QStringList &row = grid[r];
        if (labelCol < 0 || labelCol >= row.size()) continue;
        const QString label = row[labelCol].trimmed();
        if (label.isEmpty()) continue;
        double add = 1;
        if (valueCol >= 0) {
            const NumParse n = valueCol < row.size() ? parseNumber(row[valueCol], vi) : NumParse();
            if (!n.ok) { ++bad; continue; }
            add = n.value;
        }
        auto it = idx.find(label);
        if (it == idx.end()) { idx.insert(label, pts.size()); pts.push_back({label, add}); }
        else pts[it.value()].value += add;
    }
    if (bad) d.warnings << QStringLiteral("%1 dòng bị bỏ vì ô giá trị không phải số.").arg(bad);
    if (pts.size() > maxPoints) {
        ChartPoint other{QStringLiteral("Khác"), 0};
        for (int i = maxPoints - 1; i < pts.size(); ++i) other.value += pts[i].value;
        d.warnings << QStringLiteral("Có %1 nhóm; %2 nhóm cuối gộp vào \"Khác\".").arg(pts.size()).arg(pts.size() - maxPoints + 1);
        pts.resize(maxPoints - 1);
        pts.push_back(other);
    }
    d.points = pts;
    return d;
}

GroupSummary summarizeGroups(const QVector<QStringList>& grid,int labelCol,int valueCol,bool hasHeader,bool vi) {
    GroupSummary result;
    if(grid.isEmpty() || labelCol<0 || valueCol< -1 || grid.size()>100001) {
        result.error=QStringLiteral("Vùng dữ liệu hoặc lựa chọn cột không hợp lệ (tối đa 100.000 dòng dữ liệu).");return result;
    }
    int width=0;
    for(const auto& row:grid)width=qMax(width,int(row.size()));
    if(labelCol>=width || valueCol>=width) {result.error=QStringLiteral("Cột không tồn tại.");return result;}
    QMap<QString,int> indices;
    int missing=0,invalid=0,ambiguous=0;
    for(int r=hasHeader?1:0;r<grid.size();++r) {
        const auto& row=grid[r];
        const QString label=row.value(labelCol).trimmed();
        if(label.isEmpty()){++missing;continue;}
        if(!indices.contains(label)){
            if(result.groups.size()>=5000){result.groups.clear();result.error=QStringLiteral("Quá 5.000 nhóm; hãy lọc bớt dữ liệu.");return result;}
            indices.insert(label,int(result.groups.size()));GroupSummaryRow entry;entry.label=label;result.groups.append(entry);
        }
        auto& group=result.groups[indices.value(label)];++group.rows;
        if(valueCol<0)continue;
        const QString raw=row.value(valueCol);
        const auto number=looksLikeProtectedCode(raw)?NumParse{}:parseNumber(raw,vi);
        if(!number.ok){++invalid;continue;}
        if(number.ambiguous)++ambiguous;
        if(!group.numbers)group.min=group.max=number.value;
        else {group.min=qMin(group.min,number.value);group.max=qMax(group.max,number.value);}
        group.sum+=number.value;++group.numbers;
        if(!std::isfinite(group.sum)){result.groups.clear();result.error=QStringLiteral("Tổng vượt phạm vi số hỗ trợ.");return result;}
    }
    if(missing)result.warnings<<QStringLiteral("%1 dòng thiếu nhãn nhóm, đã bỏ qua.").arg(missing);
    if(invalid)result.warnings<<QStringLiteral("%1 ô trống, lỗi, mã số hoặc không phải số không được cộng.").arg(invalid);
    if(ambiguous)result.warnings<<QStringLiteral("%1 ô có dấu phân cách mơ hồ; đọc theo kiểu số đang chọn.").arg(ambiguous);
    result.ok=!result.groups.isEmpty();
    if(!result.ok)result.error=QStringLiteral("Không có nhóm dữ liệu hợp lệ.");
    return result;
}

}  // namespace dataassist
