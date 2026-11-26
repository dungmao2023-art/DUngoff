// SPDX-License-Identifier: MPL-2.0
#include "VnText.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>

namespace vntext {

// ======================= 1. Số -> chữ =======================
static const char *kDigit[] = {"không", "một", "hai", "ba", "bốn", "năm", "sáu", "bảy", "tám", "chín"};
static const char *kScale[] = {"", "nghìn", "triệu", "tỷ", "nghìn tỷ", "triệu tỷ"};

static QString dg(int d) { return QString::fromUtf8(kDigit[d]); }

// g: 0..999. full: cần đọc "không trăm" khi hàng trăm = 0 (có nhóm cao hơn đứng trước).
static QString groupWords(int g, bool full, bool useLe, bool useTu) {
    const int h = g / 100, t = (g / 10) % 10, u = g % 10;
    QStringList p;
    const bool hundreds = h > 0 || full;
    if (hundreds) p << dg(h) + QStringLiteral(" trăm");
    if (t == 0) {
        if (u > 0) {
            if (hundreds) p << (useLe ? QStringLiteral("lẻ") : QStringLiteral("linh"));
            p << dg(u);                      // "linh năm", "linh một", "linh bốn": giữ dạng gốc
        }
    } else if (t == 1) {
        p << QStringLiteral("mười");
        if (u > 0) p << (u == 5 ? QStringLiteral("lăm") : dg(u)); // mười một, mười bốn, mười lăm
    } else {
        p << dg(t) + QStringLiteral(" mươi");
        if (u == 1) p << QStringLiteral("mốt");
        else if (u == 4) p << (useTu ? QStringLiteral("tư") : QStringLiteral("bốn"));
        else if (u == 5) p << QStringLiteral("lăm");
        else if (u > 0) p << dg(u);
    }
    return p.join(QLatin1Char(' '));
}

QString numberToWords(quint64 n, bool useLe, bool useTu) {
    if (n == 0) return dg(0);
    int groups[6] = {0, 0, 0, 0, 0, 0};
    quint64 v = n;
    int count = 0;
    while (v > 0 && count < 6) { groups[count++] = int(v % 1000); v /= 1000; }
    if (v > 0) return QString(); // >= 10^18: ngoài giới hạn, trả rỗng (người gọi phải kiểm tra trước)
    QStringList out;
    bool seenHigher = false;
    for (int i = count - 1; i >= 0; --i) {
        if (groups[i] == 0) continue;
        QString w = groupWords(groups[i], seenHigher, useLe, useTu);
        if (i > 0) w += QLatin1Char(' ') + QString::fromUtf8(kScale[i]);
        out << w;
        seenHigher = true;
    }
    return out.join(QLatin1Char(' '));
}

static bool allZero(const QString &s) {
    for (QChar c : s) if (c != QLatin1Char('0')) return false;
    return true;
}

MoneyResult moneyToWords(const QString &amountIn, const MoneyOptions &opt) {
    MoneyResult r;
    QString s = amountIn.trimmed();
    // bỏ hậu tố tiền tệ thường gặp
    static const QRegularExpression suffix(QStringLiteral("\\s*(?:vnđ|vnd|đồng|đ)\\.?$"),
                                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    s.remove(suffix);
    s = s.trimmed();
    bool neg = false;
    if (s.startsWith(QLatin1Char('-')) || s.startsWith(QChar(0x2212))) { neg = true; s = s.mid(1).trimmed(); }
    else if (s.startsWith(QLatin1Char('+'))) s = s.mid(1).trimmed();
    s.remove(QLatin1Char(' ')); s.remove(QChar(0x00A0));
    if (s.isEmpty()) { r.error = QObject::tr("Không có số để đọc."); return r; }
    static const QRegularExpression chars(QStringLiteral("^[0-9.,]+$"));
    if (!chars.match(s).hasMatch()) { r.error = QObject::tr("Chuỗi có ký tự không phải chữ số: không đoán."); return r; }

    // Xác định dấu nghìn / dấu thập phân.
    //  - có cả '.' và ',': dấu xuất hiện CUỐI là dấu thập phân, dấu còn lại là dấu nghìn;
    //  - chỉ một loại dấu: nếu khớp mẫu nhóm 3 chữ số (1.000 / 12,345,678) thì là dấu nghìn, ngược lại (1,5) là thập phân.
    const int lastDot = s.lastIndexOf(QLatin1Char('.')), lastComma = s.lastIndexOf(QLatin1Char(','));
    QChar dec, thou;
    if (!opt.decimalSeparator.isNull()) {
        if (opt.decimalSeparator != QLatin1Char('.') && opt.decimalSeparator != QLatin1Char(',')) {
            r.error = QObject::tr("Dấu thập phân không hợp lệ."); return r;
        }
        const QChar other = opt.decimalSeparator == QLatin1Char('.') ? QLatin1Char(',') : QLatin1Char('.');
        if (s.contains(opt.decimalSeparator)) dec = opt.decimalSeparator;
        if (s.contains(other)) thou = other;
    } else if (lastDot >= 0 && lastComma >= 0) {
        if (lastDot > lastComma) { dec = QLatin1Char('.'); thou = QLatin1Char(','); }
        else { dec = QLatin1Char(','); thou = QLatin1Char('.'); }
    } else if (lastDot >= 0 || lastComma >= 0) {
        const QChar c = lastDot >= 0 ? QLatin1Char('.') : QLatin1Char(',');
        const QRegularExpression th(QStringLiteral("^\\d{1,3}(?:%1\\d{3})+$").arg(QRegularExpression::escape(QString(c))));
        if (th.match(s).hasMatch()) thou = c; else dec = c;
    }
    QString ip = s, fp;
    if (!dec.isNull()) {
        if (s.count(dec) != 1) { r.error = QObject::tr("Dấu thập phân xuất hiện nhiều lần: không đoán."); return r; }
        const int k = s.indexOf(dec);
        ip = s.left(k); fp = s.mid(k + 1);
        if (fp.isEmpty() || ip.isEmpty()) { r.error = QObject::tr("Số tiền thiếu phần nguyên hoặc phần thập phân sau dấu."); return r; }
        if (!thou.isNull() && fp.contains(thou)) { r.error = QObject::tr("Dấu phân cách không hợp lệ."); return r; }
    }
    if (!thou.isNull()) {
        const QRegularExpression th(QStringLiteral("^\\d{1,3}(?:%1\\d{3})*$").arg(QRegularExpression::escape(QString(thou))));
        if (!th.match(ip).hasMatch()) { r.error = QObject::tr("Dấu phân cách hàng nghìn đặt sai chỗ: không đoán."); return r; }
        ip.remove(thou);
    }
    static const QRegularExpression digits(QStringLiteral("^\\d+$"));
    if (!digits.match(ip).hasMatch() || (!fp.isEmpty() && !digits.match(fp).hasMatch())) {
        r.error = QObject::tr("Số tiền không hợp lệ."); return r;
    }
    if (!fp.isEmpty() && opt.currency) {
        if (!allZero(fp)) { r.error = QObject::tr("VNĐ không có phần thập phân; không làm tròn âm thầm. Hãy tự xác nhận số nguyên đồng."); return r; }
        r.warnings << QObject::tr("Đã bỏ phần thập phân toàn số 0.");
    } else if (fp.size() > 18) {
        r.error = QObject::tr("Phần thập phân tối đa 18 chữ số; không làm tròn."); return r;
    }
    const QString before = ip;
    ip.remove(QRegularExpression(QStringLiteral("^0+(?=\\d)")));
    if (ip != before) r.warnings << QObject::tr("Đã bỏ số 0 ở đầu.");
    if (ip.size() > 18) { r.error = QObject::tr("Quá lớn: chỉ hỗ trợ tối đa 18 chữ số (dưới một tỷ tỷ)."); return r; }

    const quint64 n = ip.toULongLong();
    QString w = numberToWords(n, opt.useLe, opt.useTu);
    if (!opt.currency && !fp.isEmpty()) {
        w += QStringLiteral(" phẩy");
        for (const QChar digit : fp) w += QLatin1Char(' ') + dg(digit.digitValue());
    }
    if (neg && (n != 0 || (!opt.currency && !allZero(fp)))) w = QStringLiteral("âm ") + w;
    if (opt.currency) { w += QStringLiteral(" đồng"); if (opt.chan) w += QStringLiteral(" chẵn"); }
    if (opt.capitalize && !w.isEmpty()) w[0] = w.at(0).toUpper();
    r.ok = true; r.text = w;
    return r;
}

RangeWordsResult rangeToWords(const QString& tsv, int rows, int cols, const MoneyOptions& opt) {
    RangeWordsResult result;
    if (rows < 1 || cols < 1 || qint64(rows) * cols > 5000 || tsv.size() > 200000) {
        result.error = QObject::tr("Tối đa 5.000 ô và 200.000 ký tự mỗi lần."); return result;
    }
    QString normalized = tsv;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (normalized.contains(QLatin1Char('\r')) || normalized.contains(QLatin1Char('"'))) {
        result.error = QObject::tr("Vùng có ô nhiều dòng hoặc dấu ngoặc kép; hãy chọn vùng chỉ gồm số/ô trống."); return result;
    }
    QStringList lines = normalized.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (lines.size() == rows + 1 && lines.last().isEmpty()) lines.removeLast();
    if (lines.size() != rows) { result.error = QObject::tr("Số hàng đọc từ engine không khớp vùng chọn."); return result; }
    QStringList output;
    int converted = 0, warnings = 0;
    for (int row = 0; row < rows; ++row) {
        const auto fields = lines.at(row).split(QLatin1Char('\t'), Qt::KeepEmptyParts);
        if (fields.size() != cols) { result.error = QObject::tr("Số cột ở hàng %1 không khớp vùng chọn.").arg(row+1); return result; }
        QStringList words;
        for (int col = 0; col < cols; ++col) {
            if (fields.at(col).trimmed().isEmpty()) { words << QString(); continue; }
            const auto r = moneyToWords(fields.at(col), opt);
            if (!r.ok) { result.error = QObject::tr("Hàng %1, cột %2: %3").arg(row+1).arg(col+1).arg(r.error); return result; }
            words << r.text; ++converted; if (!r.warnings.isEmpty()) ++warnings;
        }
        output << words.join(QLatin1Char('\t'));
    }
    if (!converted) { result.error = QObject::tr("Vùng không có số."); return result; }
    result.ok = true; result.converted = converted; result.warnings = warnings;
    result.text = output.join(QLatin1Char('\n')); return result;
}

// ======================= 2. Ngày =======================
QString formatDateNumeric(const QDate &d) {
    return QStringLiteral("%1/%2/%3").arg(d.day(), 2, 10, QLatin1Char('0')).arg(d.month(), 2, 10, QLatin1Char('0')).arg(d.year(), 4, 10, QLatin1Char('0'));
}

QString formatDateVn(const QDate &d, DateStyle style) {
    if (!d.isValid()) return QString();
    auto pad = [](int v) { return QStringLiteral("%1").arg(v, 2, 10, QLatin1Char('0')); };
    QString dd, mm;
    switch (style) {
    case DateStyle::Plain:  dd = QString::number(d.day()); mm = QString::number(d.month()); break;
    case DateStyle::Padded: dd = pad(d.day()); mm = pad(d.month()); break;
    case DateStyle::Admin:  dd = d.day() < 10 ? pad(d.day()) : QString::number(d.day());
                            mm = d.month() < 3 ? pad(d.month()) : QString::number(d.month()); break;
    }
    return QStringLiteral("ngày %1 tháng %2 năm %3").arg(dd, mm).arg(d.year());
}

QString dateToSpelledWords(const QDate &d) {
    if (!d.isValid()) return QString();
    return QStringLiteral("ngày %1 tháng %2 năm %3")
        .arg(numberToWords(quint64(d.day())), numberToWords(quint64(d.month())), numberToWords(quint64(d.year())));
}

DateParse parseDateVn(const QString &text) {
    DateParse r;
    static const QRegularExpression re(
        QStringLiteral("^\\s*(?:ngày\\s+)?(\\d{1,2})\\s*(?:[/.\\-]|\\s+tháng\\s+)\\s*(\\d{1,2})\\s*(?:[/.\\-]|\\s+năm\\s+)\\s*(\\d+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    const auto m = re.match(text);
    if (!m.hasMatch()) { r.error = QObject::tr("Không đúng dạng dd/mm/yyyy hoặc \"ngày .. tháng .. năm ..\"."); return r; }
    const QString ys = m.captured(3);
    if (ys.size() != 4) { r.error = QObject::tr("Năm phải đủ 4 chữ số (năm 2 chữ số mơ hồ nên không đoán)."); return r; }
    const QDate d(ys.toInt(), m.captured(2).toInt(), m.captured(1).toInt());
    if (!d.isValid()) { r.error = QObject::tr("Ngày không tồn tại trong lịch."); return r; }
    r.ok = true; r.date = d;
    return r;
}

// ======================= 3. Bảng mã cũ =======================
// TCVN3 (ABC): mã -> chữ. Hai chuỗi song song, cùng thứ tự.
static const unsigned char kTcvnLowerCodes[] = {
    0xB5,0xB6,0xB7,0xB8,0xB9,           // à ả ã á ạ
    0xA8,0xBB,0xBC,0xBD,0xBE,0xC6,      // ă ằ ẳ ẵ ắ ặ
    0xA9,0xC7,0xC8,0xC9,0xCA,0xCB,      // â ầ ẩ ẫ ấ ậ
    0xCC,0xCE,0xCF,0xD0,0xD1,           // è ẻ ẽ é ẹ
    0xAA,0xD2,0xD3,0xD4,0xD5,0xD6,      // ê ề ể ễ ế ệ
    0xD7,0xD8,0xDC,0xDD,0xDE,           // ì ỉ ĩ í ị
    0xDF,0xE1,0xE2,0xE3,0xE4,           // ò ỏ õ ó ọ
    0xAB,0xE5,0xE6,0xE7,0xE8,0xE9,      // ô ồ ổ ỗ ố ộ
    0xAC,0xEA,0xEB,0xEC,0xED,0xEE,      // ơ ờ ở ỡ ớ ợ
    0xEF,0xF1,0xF2,0xF3,0xF4,           // ù ủ ũ ú ụ
    0xAD,0xF5,0xF6,0xF7,0xF8,0xF9,      // ư ừ ử ữ ứ ự
    0xFA,0xFB,0xFC,0xFD,0xFE,           // ỳ ỷ ỹ ý ỵ
    0xAE                                // đ
};
static const char *kTcvnLowerChars =
    "àảãáạ" "ăằẳẵắặ" "âầẩẫấậ" "èẻẽéẹ" "êềểễếệ" "ìỉĩíị" "òỏõóọ" "ôồổỗốộ" "ơờởỡớợ" "ùủũúụ" "ưừửữứự" "ỳỷỹýỵ" "đ";
static const unsigned char kTcvnUpperCodes[] = {0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7};
static const char *kTcvnUpperChars = "ĂÂÊÔƠƯĐ";

static const QHash<QChar, QChar> &tcvnMap() {
    static const QHash<QChar, QChar> m = [] {
        QHash<QChar, QChar> h;
        const QString lo = QString::fromUtf8(kTcvnLowerChars), up = QString::fromUtf8(kTcvnUpperChars);
        for (int i = 0; i < int(sizeof(kTcvnLowerCodes)); ++i) h.insert(QChar(ushort(kTcvnLowerCodes[i])), lo.at(i));
        for (int i = 0; i < int(sizeof(kTcvnUpperCodes)); ++i) h.insert(QChar(ushort(kTcvnUpperCodes[i])), up.at(i));
        return h;
    }();
    return m;
}

static bool isVietnameseUnicode(QChar c) {
    const ushort u = c.unicode();
    return (u >= 0x1EA0 && u <= 0x1EF9) || u == 0x0102 || u == 0x0103 || u == 0x0110 || u == 0x0111 ||
           u == 0x0128 || u == 0x0129 || u == 0x0168 || u == 0x0169 || u == 0x01A0 || u == 0x01A1 || u == 0x01AF || u == 0x01B0;
}

bool isPlausibleSyllable(const QString &syl) {
    if (syl.isEmpty()) return false;
    // Tách dấu thanh (5 dấu) khỏi chữ cái, giữ dấu mũ/móc/trăng.
    QString base;
    QChar tone;
    int tones = 0;
    for (QChar ch : syl) {
        if (ch == QChar(0x0111)) { base += ch; continue; } // đ
        const QString d = QString(ch).normalized(QString::NormalizationForm_D);
        QString shape;
        for (int i = 1; i < d.size(); ++i) {
            const ushort m = d.at(i).unicode();
            if (m == 0x0300 || m == 0x0301 || m == 0x0303 || m == 0x0309 || m == 0x0323) { tone = d.at(i); ++tones; }
            else if (m == 0x0302 || m == 0x0306 || m == 0x031B) shape += d.at(i);
            else return false; // dấu lạ
        }
        base += (QString(d.at(0)) + shape).normalized(QString::NormalizationForm_C);
    }
    if (tones > 1) return false;
    static const QRegularExpression re(QStringLiteral(
        "^(?:ngh|ng|nh|ch|gh|gi|kh|ph|qu|th|tr|[bcdđghklmnpqrstvx])?"
        "(uyê|uya|uyu|uây|uôi|uai|uao|uay|ươi|ươu|iêu|yêu|oai|oay|oeo|"
        "uâ|uê|uô|uơ|ươ|uy|ưa|ưi|ưu|iê|yê|ia|iu|oa|oă|oe|oi|ôi|ơi|ua|ui|"
        "ai|ao|au|ay|âu|ây|eo|êu|"
        "a|ă|â|e|ê|i|o|ô|ơ|u|ư|y)"
        "(ch|ng|nh|c|m|n|p|t)?$"));
    const auto m = re.match(base);
    if (!m.hasMatch()) return false;
    const QString coda = m.captured(2);
    if (coda == QLatin1String("c") || coda == QLatin1String("ch") || coda == QLatin1String("p") || coda == QLatin1String("t")) {
        // vần kết thúc bằng c/ch/p/t chỉ mang thanh sắc hoặc nặng
        if (tone != QChar(0x0301) && tone != QChar(0x0323)) return false;
    }
    return true;
}

static bool isRunChar(QChar c, const QHash<QChar, QChar> &map) {
    return (c.unicode() < 128 && c.isLetter()) || map.contains(c);
}

ConvertResult legacyToUnicode(const QString &text, LegacyEncoding enc, bool force) {
    ConvertResult r;
    r.text = text;
    if (enc == LegacyEncoding::VniWindows) {
        r.verdict = Verdict::Unsupported;
        r.warnings << QObject::tr("Chưa hỗ trợ VNI-Windows: bảng mã chưa được kiểm chứng với nguồn chuẩn nên không chuyển (tránh làm hỏng chữ).");
        return r;
    }
    const auto &map = tcvnMap();
    for (QChar c : text)
        if (isVietnameseUnicode(c)) {
            r.verdict = Verdict::AlreadyUnicode;
            r.warnings << QObject::tr("Văn bản đã có chữ tiếng Việt Unicode: không chuyển để tránh trộn bảng mã.");
            return r;
        }
    bool any = false;
    for (QChar c : text) if (map.contains(c)) { any = true; break; }
    if (!any) { r.verdict = Verdict::NothingToConvert; return r; }

    // Chuyển toàn bộ, đồng thời chấm điểm từng "từ" (dãy chữ cái ASCII và ký tự thuộc bảng mã).
    QString out;
    out.reserve(text.size());
    int affected = 0, plausible = 0;
    int i = 0;
    const int n = text.size();
    while (i < n) {
        if (!isRunChar(text.at(i), map)) { out += text.at(i); ++i; continue; }
        int j = i;
        QString conv;
        bool has = false;
        while (j < n && isRunChar(text.at(j), map)) {
            const auto it = map.constFind(text.at(j));
            if (it != map.constEnd()) { conv += *it; has = true; } else conv += text.at(j);
            ++j;
        }
        if (has) { ++affected; if (isPlausibleSyllable(conv.toLower())) ++plausible; }
        out += conv;
        i = j;
    }
    r.affectedWords = affected;
    r.confidence = affected > 0 ? double(plausible) / affected : 0.0;
    const bool sure = affected >= 2 && r.confidence >= 0.9;
    if (sure || force) {
        r.text = out;
        r.verdict = Verdict::Converted;
        if (!sure) r.warnings << QObject::tr("Đã ép chuyển dù độ tin cậy thấp (%1%): hãy kiểm tra lại kết quả.").arg(int(r.confidence * 100));
        r.warnings << QObject::tr("Chữ HOA CÓ DẤU của font .VnTimeH dùng bảng mã riêng, chưa được chuyển: hãy kiểm tra các tiêu đề in hoa.");
    } else {
        r.verdict = Verdict::NeedsReview;
        r.warnings << QObject::tr("Không đủ chắc đây là bảng mã TCVN3 (tin cậy %1%, %2 từ): giữ nguyên văn bản, cần người xác nhận.")
                          .arg(int(r.confidence * 100)).arg(affected);
    }
    return r;
}

} // namespace vntext
