#include "TableOps.h"
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QDate>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <limits>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace tbl {

// ---------------- CSV ----------------
static QVector<QStringList> parseCsv(const QString &text, QChar sep, QString &err)
{
    QVector<QStringList> rows;
    QStringList row;
    QString cell;
    enum class State { Start, Bare, Quoted, Closed };
    State state = State::Start;
    bool any = false;
    qsizetype cells=0;
    auto finishCell = [&] {
        if(row.size()>=kMaxCsvColumns || ++cells>kMaxCsvCells || cell.size()>kMaxCsvCellChars){
            err=QObject::tr("CSV vượt giới hạn 1.024 cột, 5 triệu ô hoặc 1 triệu ký tự mỗi ô; chưa nhập dữ liệu.");
            return false;
        }
        row << cell; cell.clear(); state = State::Start;return true;
    };
    auto finishRow=[&]{
        if(rows.size()>=kMaxCsvRecords){
            err=QObject::tr("CSV vượt 250.000 dòng dữ liệu; hãy chia thành các tệp nhỏ hơn.");
            return false;
        }
        if(!finishCell())return false;
        rows<<row;row.clear();any=false;return true;
    };
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if(cell.size()>kMaxCsvCellChars){
            err=QObject::tr("Ô CSV quá dài; chưa nhập dữ liệu.");return {};
        }
        if (state == State::Quoted) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) {
                    cell += ch; ++i;
                } else state = State::Closed;
            } else cell += ch;
            continue;
        }
        if (ch == sep) { if(!finishCell())return {}; any = true; }
        else if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n')) {
            if (ch == QLatin1Char('\r') && i + 1 < text.size() && text[i + 1] == QLatin1Char('\n')) ++i;
            if(!finishRow())return {};
        } else if (state == State::Closed || (state == State::Bare && ch == QLatin1Char('"'))) {
            err = QObject::tr("Dấu nháy sai vị trí tại bản ghi %1. Không đọc dữ liệu để tránh mất nội dung.").arg(rows.size() + 1);
            return {};
        } else if (ch == QLatin1Char('"')) {
            state = State::Quoted; any = true;
        } else {
            state = State::Bare; cell += ch; any = true;
        }
    }
    if (state == State::Quoted) { err = QObject::tr("Dấu nháy đóng chưa khớp trong CSV."); return {}; }
    if (any || !cell.isEmpty() || !row.isEmpty()) { if(!finishRow())return {}; }
    return rows;
}

// ---------------- Bảng mã ----------------
QString encodingName(Encoding e)
{
    switch (e) {
    case Encoding::Utf8: return QObject::tr("UTF-8");
    case Encoding::Utf8Bom: return QObject::tr("UTF-8 (có BOM)");
    case Encoding::Utf16LE: return QObject::tr("UTF-16 LE");
    case Encoding::Utf16BE: return QObject::tr("UTF-16 BE");
    case Encoding::Windows1258: return QObject::tr("Windows-1258 (tiếng Việt cũ)");
    default: return QObject::tr("Không rõ");
    }
}

// Windows-1258, nửa cao 0x80-0xFF. 0 = byte không định nghĩa. Các dấu thanh là ký tự tổ hợp (U+0300...) và được chuẩn hoá NFC sau khi giải mã.
static const char16_t kCp1258[128] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0, 0x2039, 0x0152, 0, 0, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0, 0x203A, 0x0153, 0, 0, 0x0178,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x0300, 0x00CD, 0x00CE, 0x00CF,
    0x0110, 0x00D1, 0x0309, 0x00D3, 0x00D4, 0x01A0, 0x00D6, 0x00D7, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x01AF, 0x0303, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x0301, 0x00ED, 0x00EE, 0x00EF,
    0x0111, 0x00F1, 0x0323, 0x00F3, 0x00F4, 0x01A1, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x01B0, 0x20AB, 0x00FF};

static bool strictUtf8(const QByteArray &b, QString &out)
{
    out = QString::fromUtf8(b);
    return !out.contains(QChar::ReplacementCharacter) && out.toUtf8() == b;
}

static bool decodeUtf16(const QByteArray &b, bool le, QString &out)
{
    if (b.size() % 2) return false;
    QString s; s.reserve(b.size() / 2);
    for (int i = 0; i < b.size(); i += 2) {
        const uchar a = uchar(b[i]), c = uchar(b[i + 1]);
        s += QChar(le ? char16_t(a | (c << 8)) : char16_t((a << 8) | c));
    }
    for (int i = 0; i < s.size(); ++i) { // surrogate lẻ => không hợp lệ
        if (s[i].isHighSurrogate()) { if (i + 1 >= s.size() || !s[i + 1].isLowSurrogate()) return false; ++i; }
        else if (s[i].isLowSurrogate()) return false;
    }
    out = s;
    return true;
}

static bool decodeCp1258(const QByteArray &b, QString &out)
{
    QString s; s.reserve(b.size());
    for (uchar c : b) {
        if (c == 0) return false;
        if (c < 0x80) { s += QChar(c); continue; }
        const char16_t u = kCp1258[c - 0x80];
        if (!u) return false;
        s += QChar(u);
    }
    out = s.normalized(QString::NormalizationForm_C);
    return true;
}

// Tỷ lệ ký tự không phải ASCII là chữ tiếng Việt (có dấu đặc trưng) sau khi giải mã Windows-1258.
static double vietnameseRatio(const QString &t, int *nonAscii)
{
    int n = 0, vi = 0;
    for (QChar c : t) {
        if (c.unicode() < 0x80) continue;
        ++n;
        const char16_t u = c.unicode();
        if ((u >= 0x1EA0 && u <= 0x1EF9) || u == 0x0102 || u == 0x0103 || u == 0x0110 || u == 0x0111 || u == 0x01A0 || u == 0x01A1 || u == 0x01AF || u == 0x01B0
            || u == 0x20AB || (u >= 0x00C0 && u <= 0x00FD && QStringLiteral("ÀÁÂÃÈÉÊÌÍÒÓÔÕÙÚÝàáâãèéêìíòóôõùúý").contains(c)))
            ++vi;
    }
    if (nonAscii) *nonAscii = n;
    return n ? double(vi) / n : 0.0;
}

EncodingGuess detectCsvEncoding(const QByteArray &raw)
{
    EncodingGuess g;
    QString t;
    if (raw.startsWith("\xEF\xBB\xBF")) {
        if (strictUtf8(raw.mid(3), t)) { g.enc = Encoding::Utf8Bom; g.confidence = 1.0; g.needsUserChoice = false; g.reason = QObject::tr("Có BOM UTF-8 và nội dung hợp lệ."); return g; }
        g.reason = QObject::tr("Có BOM UTF-8 nhưng nội dung không hợp lệ UTF-8."); g.alternatives = {Encoding::Windows1258}; return g;
    }
    if (raw.startsWith("\xFF\xFE") || raw.startsWith("\xFE\xFF")) {
        const bool le = raw.startsWith("\xFF\xFE");
        if (decodeUtf16(raw.mid(2), le, t)) { g.enc = le ? Encoding::Utf16LE : Encoding::Utf16BE; g.confidence = 1.0; g.needsUserChoice = false; g.reason = QObject::tr("Có BOM UTF-16 và nội dung hợp lệ."); return g; }
        g.reason = QObject::tr("Có BOM UTF-16 nhưng độ dài hoặc ký tự không hợp lệ."); return g;
    }
    if (!raw.contains('\0') && strictUtf8(raw, t)) { // byte 0 xen kẽ là dấu hiệu UTF-16, không phải văn bản UTF-8
        bool ascii = true; for (uchar c : raw) if (c >= 0x80) { ascii = false; break; }
        g.enc = Encoding::Utf8; g.confidence = ascii ? 1.0 : 0.97; g.needsUserChoice = false;
        g.reason = ascii ? QObject::tr("Toàn ký tự ASCII; UTF-8 và Windows-1258 cho kết quả giống nhau.")
                         : QObject::tr("Nội dung là UTF-8 hợp lệ có ký tự đa byte (rất khó trùng hợp ngẫu nhiên).");
        return g;
    }
    // Không BOM, không phải UTF-8: chỉ đưa ra GỢI Ý, luôn cần người dùng xác nhận.
    int zerosOdd = 0, zerosEven = 0;
    for (int i = 0; i < raw.size(); ++i) if (raw[i] == 0) (i % 2 ? zerosOdd : zerosEven)++;
    const int half = raw.size() / 2;
    if (half > 8 && (zerosOdd > half * 0.3 || zerosEven > half * 0.3) && decodeUtf16(raw, zerosOdd > zerosEven, t)) {
        g.enc = zerosOdd > zerosEven ? Encoding::Utf16LE : Encoding::Utf16BE; g.confidence = 0.6; g.needsUserChoice = true;
        g.reason = QObject::tr("Không có BOM nhưng có nhiều byte 0 xen kẽ, giống UTF-16. Hãy xem trước và xác nhận.");
        g.alternatives = {Encoding::Windows1258};
        return g;
    }
    if (decodeCp1258(raw, t)) {
        int n = 0; const double r = vietnameseRatio(t, &n);
        g.enc = Encoding::Windows1258; g.needsUserChoice = true;
        if (n >= 3 && r >= 0.9) { g.confidence = 0.8; g.reason = QObject::tr("Không phải UTF-8; giải mã Windows-1258 ra chữ tiếng Việt hợp lý. Hãy xem trước và xác nhận."); }
        else { g.confidence = 0.3; g.reason = QObject::tr("Không phải UTF-8; Windows-1258 giải mã được nhưng kết quả không giống tiếng Việt. Rất có thể là bảng mã khác — hãy kiểm tra kỹ."); }
        g.alternatives = {Encoding::Utf16LE, Encoding::Utf16BE};
        return g;
    }
    g.reason = QObject::tr("Không nhận ra bảng mã (không phải UTF-8/UTF-16/Windows-1258). Không đoán; hãy mở bằng phần mềm khác để xem bảng mã thật.");
    g.alternatives = {Encoding::Utf16LE, Encoding::Utf16BE};
    return g;
}

bool decodeText(const QByteArray &raw, Encoding enc, QString &text, QString &error)
{
    error.clear();
    QByteArray b = raw;
    bool ok = false;
    switch (enc) {
    case Encoding::Utf8: case Encoding::Utf8Bom:
        if (b.startsWith("\xEF\xBB\xBF")) b.remove(0, 3);
        ok = strictUtf8(b, text); break;
    case Encoding::Utf16LE: if (b.startsWith("\xFF\xFE")) b.remove(0, 2); ok = decodeUtf16(b, true, text); break;
    case Encoding::Utf16BE: if (b.startsWith("\xFE\xFF")) b.remove(0, 2); ok = decodeUtf16(b, false, text); break;
    case Encoding::Windows1258: ok = decodeCp1258(b, text); break;
    default: error = QObject::tr("Chưa chọn bảng mã."); return false;
    }
    if (!ok) { error = QObject::tr("Tệp không giải mã được bằng bảng mã %1; không thay ký tự và không đoán.").arg(encodingName(enc)); return false; }
    return true;
}

QStringList previewLines(const QString &t, int maxLines)
{
    QStringList out; int start = 0;
    while (start <= t.size() && out.size() < maxLines) {
        int e = t.indexOf(QLatin1Char('\n'), start);
        if (e < 0) { if (start < t.size()) out << t.mid(start); break; }
        QString l = t.mid(start, e - start); if (l.endsWith(QLatin1Char('\r'))) l.chop(1);
        out << l; start = e + 1;
    }
    return out;
}

QChar guessSeparator(const QString &t)
{
    const QStringList ls = previewLines(t, 5);
    int c = 0, s = 0, tb = 0;
    for (const QString &l : ls) { c += l.count(QLatin1Char(',')); s += l.count(QLatin1Char(';')); tb += l.count(QLatin1Char('\t')); }
    if (tb > c && tb > s) return QLatin1Char('\t');
    if (s > c) return QLatin1Char(';');
    return QLatin1Char(',');
}

bool readCsvBytes(const QByteArray& raw, Encoding enc, Table& out, QString& error, QChar sep)
{
    error.clear();
    if (sep != QLatin1Char(',') && sep != QLatin1Char(';') && sep != QLatin1Char('\t')) {
        error = QObject::tr("Dấu phân cách không được hỗ trợ."); return false;
    }
    if(raw.size()>kMaxCsvBytes){error=QObject::tr("CSV vượt 100 MiB; hãy chia tệp.");return false;}
    QString text;
    if (!decodeText(raw, enc, text, error)) return false;
    auto rows = parseCsv(text, sep, error);
    if (!error.isEmpty()) return false;
    Table parsed;
    if (!rows.isEmpty()) { parsed.header = rows.takeFirst(); parsed.rows = rows; }
    out = parsed;
    return true;
}

bool readCsvFileBytes(const QString& path,QByteArray& out,QString& error){
    error.clear();
    const QFileInfo info(path);
    if(!info.isFile()){error=QObject::tr("Chọn tệp CSV thường.");return false;}
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)){error=QObject::tr("Không mở được tệp: ")+f.errorString();return false;}
    if(f.size()>kMaxCsvBytes){error=QObject::tr("CSV vượt 100 MiB; hãy chia tệp.");return false;}
    const auto bytes=f.read(kMaxCsvBytes+1);
    if(f.error()!=QFileDevice::NoError){error=f.errorString();return false;}
    if(bytes.size()>kMaxCsvBytes||!f.atEnd()){
        error=QObject::tr("CSV vượt 100 MiB hoặc chưa đọc hết; chưa nhập dữ liệu.");return false;
    }
    out=bytes;return true;
}

bool readCsvWith(const QString &path, Encoding enc, Table &out, QString &error, QChar sep)
{
    QByteArray bytes;
    return readCsvFileBytes(path,bytes,error)&&readCsvBytes(bytes,enc,out,error,sep);
}

bool readCsv(const QString &path, Table &out, QString &error, QChar sep)
{
    return readCsvWith(path, Encoding::Utf8, out, error, sep);
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
    error.clear();
    if (sep != QLatin1Char(',') && sep != QLatin1Char(';') && sep != QLatin1Char('\t')) {
        error = QObject::tr("Dấu phân cách không được hỗ trợ."); return false;
    }
    if (t.header.isEmpty()) { error = QObject::tr("Bảng không có tiêu đề."); return false; }
    // NewOnly performs the no-overwrite check atomically at open, unlike exists()+WriteOnly.
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        error = QObject::tr("Không tạo được tệp mới: %1").arg(f.errorString()); return false;
    }
    auto fail = [&] {
        error = QObject::tr("Ghi CSV không đầy đủ: %1").arg(f.errorString());
        f.close();
        if (!f.remove()) error += QObject::tr(" Không xóa được tệp kết quả lỗi; không sử dụng tệp này.");
        return false;
    };
    auto line = [&](const QStringList &r) {
        QStringList escaped;
        for (const QString &cell : r) escaped << csvEscape(cell, sep);
        const auto bytes = (escaped.join(sep) + QLatin1Char('\n')).toUtf8();
        return f.write(bytes) == bytes.size();
    };
    if (f.write("\xEF\xBB\xBF", 3) != 3 || !line(t.header)) return fail();
    for (const auto &row : t.rows) if (!line(row)) return fail();
    if (!f.flush()) return fail();
    f.close();
    return true;
}

// ---------------- Exact CSV group summary ----------------
CsvGroupResult groupCsv(const Table& source,int keyCol,int valueCol)
{
    CsvGroupResult r;
    if(keyCol<0 || keyCol>=source.header.size() || valueCol<-1 || valueCol>=source.header.size() || keyCol==valueCol) {
        r.error=QObject::tr("Chọn cột nhóm và cột số hợp lệ, khác nhau."); return r;
    }
    struct Group { QString key; qsizetype rows=0,numbers=0; qint64 sum=0; };
    QList<Group> groups; QHash<QString,qsizetype> indices;
    // No thousands separators, leading-zero codes, exponents or implicit rounding.
    static const QRegularExpression canonical(QStringLiteral(R"(^-?(?:0|[1-9][0-9]{0,13})(?:\.[0-9]{1,4})?\z)"));
    for(qsizetype i=0;i<source.rows.size();++i) {
        const auto& row=source.rows[i];
        if(row.size()!=source.header.size()) { r.error=QObject::tr("Bản ghi %1 sai số cột.").arg(i+2); return r; }
        const auto& key=row[keyCol];
        if(!indices.contains(key)) {
            if(groups.size()>=5000) { r.error=QObject::tr("Quá 5.000 nhóm; hãy lọc nguồn trước."); return r; }
            indices.insert(key,groups.size()); Group g;g.key=key;groups.append(g);
        }
        auto& g=groups[indices.value(key)]; ++g.rows;
        if(valueCol<0 || row[valueCol].isEmpty()) continue;
        const auto& value=row[valueCol];
        if(!canonical.match(value).hasMatch()) {
            r.error=QObject::tr("Bản ghi %1: số không đúng dạng chuẩn (tối đa 14 chữ số nguyên và 4 chữ số thập phân). Hãy làm sạch cột số trước.").arg(i+2); return r;
        }
        const bool negative=value.startsWith(QLatin1Char('-'));
        const QString magnitude=negative?value.mid(1):value;
        const auto parts=magnitude.split(QLatin1Char('.'));
        bool ok=false;
        const qint64 integer=parts.first().toLongLong(&ok);
        if(!ok) { r.error=QObject::tr("Số không hợp lệ.");return r; }
        const qint64 fraction=parts.size()==2?parts[1].leftJustified(4,QLatin1Char('0')).toLongLong():0;
        const qint64 scaled=(integer*10000+fraction)*(negative?-1:1);
        if((scaled>0 && g.sum>(std::numeric_limits<qint64>::max)()-scaled) ||
           (scaled<0 && g.sum<(std::numeric_limits<qint64>::min)()-scaled)) {
            r.error=QObject::tr("Tổng nhóm vượt phạm vi số hỗ trợ; không xuất kết quả sai.");return r;
        }
        g.sum+=scaled;++g.numbers;
    }
    r.table.header << QObject::tr("Nhóm") << QObject::tr("Số dòng");
    if(valueCol>=0) r.table.header << QObject::tr("Số ô có số") << QObject::tr("Tổng");
    for(const auto& g:groups) {
        QStringList row{g.key,QString::number(g.rows)};
        if(valueCol>=0) {
            row << QString::number(g.numbers);
            QString sum;
            if(g.numbers) {
                const qint64 whole=g.sum/10000;
                const qint64 rem=g.sum%10000;
                sum=QString::number(whole);
                if(g.sum<0 && whole==0) sum.prepend(QLatin1Char('-'));
                if(rem) {
                    QString decimal=QString::number(rem<0?-rem:rem).rightJustified(4,QLatin1Char('0'));
                    while(decimal.endsWith(QLatin1Char('0'))) decimal.chop(1);
                    sum+=QLatin1Char('.')+decimal;
                }
            }
            row << sum;
        }
        r.table.rows.append(row);
    }
    r.ok=true;return r;
}

// ---------------- 1. Dòng trùng ----------------
static QString cellAt(const QStringList &r, int c) { return c >= 0 && c < r.size() ? r[c] : QString(); }

DedupResult removeDuplicates(const Table &t, const QList<int> &cols, bool trimAndFold)
{
    DedupResult res; res.cleaned.header = t.header;
    if (cols.isEmpty()) { res.ok = false; res.warnings << QObject::tr("Chưa chọn cột làm khoá."); res.cleaned = t; return res; }
    QSet<int> selected;
    for (int c : cols) {
        if (c < 0 || c >= t.header.size() || selected.contains(c)) {
            res.ok=false;res.warnings<<QObject::tr("Cột khoá không hợp lệ hoặc bị lặp: %1").arg(c);
            res.cleaned=t;return res;
        }
        selected.insert(c);
    }
    for(const auto& row:t.rows)if(row.size()!=t.header.size()){
        res.ok=false;res.warnings<<QObject::tr("Nguồn có dòng sai số cột; không loại bỏ dữ liệu.");
        res.cleaned=t;return res;
    }

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
        // Length-prefix each component; embedded delimiters, NUL and digits
        // cannot turn two distinct tuples into the same composite key.
        QString key;
        for(const auto& value:parts) {
            key+=QString::number(value.size());
            key+=QLatin1Char(':');
            key+=value;
        }
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

FilterSortResult filterSort(const Table& source, const FilterSortSpec& spec)
{
    FilterSortResult result;
    const auto valid = [&](int col) { return col == -1 || (col >= 0 && col < source.header.size()); };
    if (!valid(spec.filterCol) || !valid(spec.sortCol) || !valid(spec.cleanCol) || !valid(spec.dedupCol) || !valid(spec.groupKey) || !valid(spec.groupValue)
        || (spec.mode!=FilterMode::Equal && spec.mode!=FilterMode::Contains && spec.mode!=FilterMode::Blank && spec.mode!=FilterMode::NotBlank)
        || (spec.cleanKind!=ColumnKind::Keep && spec.cleanKind!=ColumnKind::Date && spec.cleanKind!=ColumnKind::Money)
        || (spec.moneyLocale!=MoneyLocale::VN && spec.moneyLocale!=MoneyLocale::EN)
        || ((spec.cleanCol<0)!=(spec.cleanKind==ColumnKind::Keep))) {
        result.error = QObject::tr("Cột lọc hoặc sắp xếp không hợp lệ.");
        return result;
    }
    if (spec.filterCol >= 0 && spec.mode == FilterMode::Contains && spec.value.isEmpty()) {
        result.error = QObject::tr("Nhập nội dung cần chứa, hoặc chọn lọc ô trống.");
        return result;
    }
    const bool project = !spec.outputColumns.isEmpty();
    if (spec.outputColumns.size() != spec.outputNames.size()) {
        result.error = QObject::tr("Cấu hình cột đầu ra không hợp lệ."); return result;
    }
    QSet<int> selected;
    QSet<QString> names;
    for (qsizetype i=0;i<spec.outputColumns.size();++i) {
        const int col=spec.outputColumns[i];
        const auto name=spec.outputNames[i].trimmed();
        if (col<0 || col>=source.header.size() || selected.contains(col) || name.isEmpty() || names.contains(name.toCaseFolded())) {
            result.error=QObject::tr("Cột đầu ra không hợp lệ, tên rỗng hoặc tên bị trùng."); return result;
        }
        selected.insert(col); names.insert(name.toCaseFolded());
    }
    if((spec.groupKey<0 && spec.groupValue>=0) ||
       (spec.groupKey>=0 && spec.groupKey==spec.groupValue) ||
       (project && spec.groupKey>=0 && (!selected.contains(spec.groupKey) ||
        (spec.groupValue>=0 && !selected.contains(spec.groupValue))))){
        result.error=QObject::tr("Chọn khóa nhóm và cột số khác nhau; phải giữ các cột này trong bước chọn cột.");return result;
    }
    if (project || spec.cleanCol>=0 || spec.dedupCol>=0 || spec.groupKey>=0) for (const auto& row:source.rows) if (row.size()!=source.header.size()) {
        result.error=QObject::tr("Nguồn có dòng sai số cột; chưa chọn/đổi tên cột."); return result;
    }
    const auto cs = spec.ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
    result.table.header = source.header;
    QSet<QString> seen;
    for (auto row : source.rows) {
        if(spec.cleanCol>=0) {
            const auto value=row[spec.cleanCol];
            const auto clean=spec.cleanKind==ColumnKind::Date?cleanDate(value):cleanMoney(value,spec.moneyLocale);
            if(clean.converted) {row[spec.cleanCol]=clean.value;++result.cleanedCells;}
            if(!clean.warning.isEmpty())++result.cleanWarnings;
        }
        bool keep = true;
        if (spec.filterCol >= 0) {
            const auto cell = row.value(spec.filterCol);
            switch (spec.mode) {
                case FilterMode::Equal: keep = cell.compare(spec.value, cs) == 0; break;
                case FilterMode::Contains: keep = cell.contains(spec.value, cs); break;
                case FilterMode::Blank: keep = cell.isEmpty(); break;
                case FilterMode::NotBlank: keep = !cell.isEmpty(); break;
                default: result.error = QObject::tr("Kiểu lọc không hợp lệ."); return result;
            }
        }
        if(!keep){++result.filteredRows;continue;}
        if(spec.dedupCol>=0) {
            auto key=row[spec.dedupCol];
            if(spec.dedupFold)key=key.trimmed().toCaseFolded();
            if(!key.isEmpty()){
                if(seen.contains(key)){++result.removedDuplicates;continue;}
                seen.insert(key);
            }
        }
        result.table.rows.append(row);
    }
    if (spec.sortCol >= 0)
        std::stable_sort(result.table.rows.begin(), result.table.rows.end(),
            [&](const QStringList& a, const QStringList& b) {
                const int cmp = a.value(spec.sortCol).compare(b.value(spec.sortCol), cs);
                return spec.descending ? cmp > 0 : cmp < 0;
            });
    // Filtering and sorting use source indices, then the output columns are projected.
    if (project) {
        result.table.header=spec.outputNames;
        for (auto& row:result.table.rows) {
            QStringList projected;
            for (int col:spec.outputColumns) projected << row[col];
            row=projected;
        }
    }
    if(spec.groupKey>=0){
        const int key=project?int(spec.outputColumns.indexOf(spec.groupKey)):spec.groupKey;
        const int value=spec.groupValue<0?-1:project?int(spec.outputColumns.indexOf(spec.groupValue)):spec.groupValue;
        const auto grouped=groupCsv(result.table,key,value);
        if(!grouped.ok){result.error=grouped.error;result.table={};return result;}
        result.grouped=true;result.groupedInputRows=result.table.rows.size();result.table=grouped.table;
    }
    result.ok = true;
    return result;
}

QByteArray encodeFilterSortRecipe(const QStringList& header, const FilterSortSpec& spec)
{
    QJsonObject obj{
        {"version", 1}, {"header", QJsonArray::fromStringList(header)},
        {"filterCol", spec.filterCol}, {"sortCol", spec.sortCol},
        {"mode", int(spec.mode)}, {"value", spec.value},
        {"ignoreCase", spec.ignoreCase}, {"descending", spec.descending}
    };
    if (!spec.outputColumns.isEmpty() || !spec.outputNames.isEmpty()) {
        obj.insert("version",2);
        QJsonArray columns;
        for(int col:spec.outputColumns) columns.append(col);
        obj.insert("outputColumns",columns);
        obj.insert("outputNames",QJsonArray::fromStringList(spec.outputNames));
    }
    if(spec.cleanCol>=0 || spec.dedupCol>=0 || spec.cleanKind!=ColumnKind::Keep || spec.moneyLocale!=MoneyLocale::VN || spec.dedupFold || spec.groupKey!=-1 || spec.groupValue!=-1) {
        obj.insert("version",3);
        obj.insert("cleanCol",spec.cleanCol);obj.insert("cleanKind",int(spec.cleanKind));
        obj.insert("moneyLocale",int(spec.moneyLocale));obj.insert("dedupCol",spec.dedupCol);
        obj.insert("dedupFold",spec.dedupFold);
    }
    if(spec.groupKey!=-1 || spec.groupValue!=-1){
        obj.insert("version",4);obj.insert("groupKey",spec.groupKey);obj.insert("groupValue",spec.groupValue);
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

bool decodeFilterSortRecipe(const QByteArray& json, const QStringList& header, FilterSortSpec& spec, QString& error)
{
    error.clear();
    auto fail = [&] { error = QObject::tr("Cấu hình không hợp lệ hoặc cấu trúc cột đã thay đổi."); return false; };
    if (json.size() > 1024 * 1024) return fail();
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return fail();
    const auto obj = doc.object();
    const int version=obj.value("version").toInt(-1);
    if ((version != 1 && version != 2 && version != 3 && version != 4) || !obj.value("header").isArray()
        || obj.value("header").toArray() != QJsonArray::fromStringList(header)) return fail();
    const int filter = obj.value("filterCol").toInt(-2);
    const int sort = obj.value("sortCol").toInt(-2);
    const int mode = obj.value("mode").toInt(-1);
    if (filter < -1 || filter >= header.size() || sort < -1 || sort >= header.size()
        || mode < int(FilterMode::Equal) || mode > int(FilterMode::NotBlank)
        || !obj.value("value").isString() || !obj.value("ignoreCase").isBool()
        || !obj.value("descending").isBool()) return fail();
    FilterSortSpec candidate;
    candidate.filterCol = filter; candidate.sortCol = sort; candidate.mode = FilterMode(mode);
    candidate.value = obj.value("value").toString();
    candidate.ignoreCase = obj.value("ignoreCase").toBool();
    candidate.descending = obj.value("descending").toBool();
    if(version==2 || (version>=3 && (obj.contains("outputColumns") || obj.contains("outputNames")))) {
        if(!obj.value("outputColumns").isArray() || !obj.value("outputNames").isArray()) return fail();
        const auto columns=obj.value("outputColumns").toArray(), names=obj.value("outputNames").toArray();
        if(columns.isEmpty() || columns.size()!=names.size() || columns.size()>header.size()) return fail();
        for(qsizetype i=0;i<columns.size();++i) {
            if(!columns[i].isDouble() || !names[i].isString()) return fail();
            candidate.outputColumns.append(columns[i].toInt(-1));
            candidate.outputNames.append(names[i].toString());
        }
    } else if(obj.contains("outputColumns") || obj.contains("outputNames")) return fail();
    if(version>=3) {
        const int clean=obj.value("cleanCol").toInt(-2), kind=obj.value("cleanKind").toInt(-1);
        const int locale=obj.value("moneyLocale").toInt(-1), dedup=obj.value("dedupCol").toInt(-2);
        if(clean < -1 || clean>=header.size() || dedup < -1 || dedup>=header.size()
            || kind<int(ColumnKind::Keep) || kind>int(ColumnKind::Money)
            || locale<int(MoneyLocale::VN) || locale>int(MoneyLocale::EN) || !obj.value("dedupFold").isBool())return fail();
        candidate.cleanCol=clean;candidate.cleanKind=ColumnKind(kind);candidate.moneyLocale=MoneyLocale(locale);
        candidate.dedupCol=dedup;candidate.dedupFold=obj.value("dedupFold").toBool();
    } else if(obj.contains("cleanCol")||obj.contains("cleanKind")||obj.contains("moneyLocale")||obj.contains("dedupCol")||obj.contains("dedupFold"))return fail();
    if(version==4){
        candidate.groupKey=obj.value("groupKey").toInt(-2);candidate.groupValue=obj.value("groupValue").toInt(-2);
        if(candidate.groupKey<0 || candidate.groupKey>=header.size() || candidate.groupValue<-1 || candidate.groupValue>=header.size())return fail();
    }else if(obj.contains("groupKey")||obj.contains("groupValue"))return fail();
    const auto checked = filterSort(Table{header, {}}, candidate);
    if (!checked.ok) { error = checked.error; return false; }
    spec = candidate;
    return true;
}

JoinResult joinLookup(const Table& left, const Table& right, int leftKey, int rightKey, JoinKind kind)
{
    JoinResult r;
    if (leftKey < 0 || leftKey >= left.header.size() || rightKey < 0 || rightKey >= right.header.size()
        || (kind != JoinKind::Left && kind != JoinKind::Inner)) {
        r.error = QObject::tr("Chọn cột khóa và kiểu ghép hợp lệ."); return r;
    }
    for (const auto& row : left.rows) if (row.size() != left.header.size()) {
        r.error = QObject::tr("Bảng trái có dòng sai số cột."); return r;
    }
    QHash<QString, qsizetype> index;
    for (qsizetype i=0;i<right.rows.size();++i) {
        const auto& row=right.rows[i];
        if (row.size()!=right.header.size()) { r.error=QObject::tr("Bảng phải có dòng sai số cột."); return r; }
        const auto& key=row[rightKey];
        if (key.isEmpty()) { ++r.blankRight; continue; }
        if (index.contains(key)) {
            r.error=QObject::tr("Khóa bảng phải trùng tại bản ghi %1 và %2; hãy xử lý trùng trước khi ghép.").arg(index.value(key)+2).arg(i+2);
            return r;
        }
        index.insert(key,i);
    }
    r.table.header=left.header;
    QSet<QString> used;
    for (const auto& name:left.header) used.insert(name.toCaseFolded());
    for (int i=0;i<right.header.size();++i) {
        if(i==rightKey) continue;
        const QString base=QStringLiteral("B.")+(right.header[i].isEmpty()?QString::number(i+1):right.header[i]);
        QString name=base;
        for(int suffix=2;used.contains(name.toCaseFolded());++suffix) name=base+QStringLiteral("_")+QString::number(suffix);
        used.insert(name.toCaseFolded()); r.table.header << name;
    }
    constexpr qsizetype maxCells=5000000;
    if(left.rows.size()>maxCells/r.table.header.size()) {
        r.table={}; r.error=QObject::tr("Bảng ghép vượt giới hạn 5 triệu ô; hãy lọc nguồn trước."); return r;
    }
    for(const auto& row:left.rows) {
        const auto& key=row[leftKey];
        const auto it=index.constFind(key);
        const bool matched=!key.isEmpty() && it!=index.constEnd();
        if(key.isEmpty()) ++r.blankLeft;
        if(matched) ++r.matched; else ++r.unmatched;
        if(!matched && kind==JoinKind::Inner) continue;
        QStringList joined=row;
        for(int i=0;i<right.header.size();++i) if(i!=rightKey)
            joined << (matched?right.rows[it.value()][i]:QString());
        r.table.rows.append(joined);
    }
    r.ok=true; return r;
}

} // namespace tbl
