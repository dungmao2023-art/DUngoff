#include "FileSearchCore.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QSemaphore>
#include <QSet>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QThread>
#include <QThreadPool>
#include <QXmlStreamReader>
#ifdef DUNG_HAVE_QTPDF
#include <QPdfDocument>
#include <QPdfSelection>
#endif
#include <algorithm>

namespace filesearch {

// ======================= Chuẩn hoá chữ =======================
QString foldText(const QString &s, bool strip, QVector<int> *map)
{
    QString out;
    out.reserve(s.size());
    if (map) { map->clear(); map->reserve(s.size()); }
    thread_local QHash<ushort, QString> cacheStrip, cacheKeep;
    QHash<ushort, QString> &cache = strip ? cacheStrip : cacheKeep;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        const ushort u = c.unicode();
        if (u < 0x80) {
            out += (u >= 'A' && u <= 'Z') ? QChar(ushort(u + 32)) : c;
            if (map) map->append(i);
            continue;
        }
        if (c.isSurrogate()) { out += c; if (map) map->append(i); continue; }
        auto it = cache.constFind(u);
        if (it == cache.constEnd()) {
            const QString d = QString(c).normalized(QString::NormalizationForm_D);
            QString r;
            for (const QChar ch : d) {
                const auto cat = ch.category();
                if (strip && (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining || cat == QChar::Mark_Enclosing))
                    continue;
                QChar l = ch.toLower();
                if (strip && l.unicode() == 0x0111) l = QLatin1Char('d'); // đ
                r += l;
            }
            it = cache.insert(u, r);
        }
        out += it.value();
        if (map) for (int k = 0; k < it.value().size(); ++k) map->append(i);
    }
    return out;
}

// ======================= Phân tích chuỗi truy vấn =======================
QStringList splitTerms(const QString &text)
{
    QStringList out;
    QString cur;
    bool inQuote = false, hadQuote = false;
    auto flush = [&] {
        if (!cur.isEmpty() || hadQuote) { if (!cur.trimmed().isEmpty()) out << cur.trimmed(); }
        cur.clear();
        hadQuote = false;
    };
    for (const QChar c : text) {
        if (c == QLatin1Char('"')) { inQuote = !inQuote; hadQuote = true; continue; }
        if (!inQuote && c.isSpace()) { flush(); continue; }
        cur += c;
    }
    flush();
    return out;
}

static bool parseSize(const QString &v, qint64 *out)
{
    static const QRegularExpression re(QStringLiteral("^(\\d+(?:[.,]\\d+)?)\\s*(b|kb|mb|gb)?$"), QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(v.trimmed());
    if (!m.hasMatch()) return false;
    QString num = m.captured(1);
    num.replace(QLatin1Char(','), QLatin1Char('.'));
    const double n = num.toDouble();
    const QString u = m.captured(2).toLower();
    double mul = 1;
    if (u == QLatin1String("kb")) mul = 1024.0;
    else if (u == QLatin1String("mb")) mul = 1024.0 * 1024.0;
    else if (u == QLatin1String("gb")) mul = 1024.0 * 1024.0 * 1024.0;
    *out = qint64(n * mul);
    return true;
}

Query parseQuery(const QString &text, QStringList *warnings)
{
    Query q;
    // Tách token nhưng giữ dấu '-' đứng trước cụm trong ngoặc kép.
    QStringList toks;
    {
        QString cur;
        bool inQuote = false;
        for (const QChar c : text) {
            if (c == QLatin1Char('"')) { inQuote = !inQuote; cur += c; continue; }
            if (!inQuote && c.isSpace()) { if (!cur.isEmpty()) toks << cur; cur.clear(); continue; }
            cur += c;
        }
        if (!cur.isEmpty()) toks << cur;
    }
    auto warn = [&](const QString &w) { if (warnings) warnings->append(w); };
    for (QString t : toks) {
        bool neg = false;
        if (t.startsWith(QLatin1Char('-')) && t.size() > 1) { neg = true; t = t.mid(1); }
        const bool quoted = t.contains(QLatin1Char('"'));
        QString plain = t;
        plain.remove(QLatin1Char('"'));
        if (plain.trimmed().isEmpty()) continue;
        const QString low = plain.toLower();
        if (!neg && !quoted && low.startsWith(QLatin1String("ext:"))) {
            for (QString e : low.mid(4).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                if (e.startsWith(QLatin1Char('.'))) e = e.mid(1);
                if (!e.isEmpty()) q.extensions << e;
            }
            continue;
        }
        if (!neg && !quoted && (low.startsWith(QLatin1String("size>")) || low.startsWith(QLatin1String("size<")))) {
            qint64 b = 0;
            if (!parseSize(low.mid(5), &b)) { warn(QObject::tr("Không hiểu điều kiện dung lượng: %1 (ví dụ size>1mb).").arg(plain)); continue; }
            if (low.at(4) == QLatin1Char('>')) q.minSize = b; else q.maxSize = b;
            continue;
        }
        if (!neg && !quoted && (low.startsWith(QLatin1String("after:")) || low.startsWith(QLatin1String("before:")))) {
            const bool after = low.startsWith(QLatin1String("after:"));
            const QDate d = QDate::fromString(plain.mid(after ? 6 : 7), QStringLiteral("yyyy-MM-dd"));
            if (!d.isValid()) { warn(QObject::tr("Không hiểu ngày: %1 (ví dụ after:2026-01-31).").arg(plain)); continue; }
            if (after) q.modifiedAfter = d.startOfDay(); else q.modifiedBefore = d.endOfDay();
            continue;
        }
        if (neg) q.excludeTerms << plain.trimmed(); else q.nameTerms << plain.trimmed();
    }
    return q;
}

// ======================= CRC32 và giải nén deflate (không phụ thuộc thư viện ngoài) =======================
quint32 crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    quint32 c = 0xFFFFFFFFu;
    for (char b : data) c = table[(c ^ quint8(b)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

namespace {
struct Huff { short count[16]; short symbol[288]; };
struct Inflater {
    const quint8 *in; qint64 inLen, inPos = 0;
    quint32 bitBuf = 0; int bitCnt = 0;
    QByteArray *out; qint64 maxOut; bool truncated = false;
    bool eof = false;

    int bits(int need)
    {
        quint32 val = bitBuf;
        while (bitCnt < need) {
            if (inPos >= inLen) { eof = true; return 0; }
            val |= quint32(in[inPos++]) << bitCnt;
            bitCnt += 8;
        }
        bitBuf = val >> need;
        bitCnt -= need;
        return int(val & ((1u << need) - 1));
    }
};
int construct(Huff *h, const short *length, int n)
{
    for (int l = 0; l <= 15; ++l) h->count[l] = 0;
    for (int s = 0; s < n; ++s) h->count[length[s]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int l = 1; l <= 15; ++l) { left <<= 1; left -= h->count[l]; if (left < 0) return left; }
    short offs[16];
    offs[1] = 0;
    for (int l = 1; l < 15; ++l) offs[l + 1] = offs[l] + h->count[l];
    for (int s = 0; s < n; ++s) if (length[s] != 0) h->symbol[offs[length[s]]++] = short(s);
    return left;
}
int decode(Inflater &s, const Huff &h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; ++len) {
        code |= s.bits(1);
        if (s.eof) return -1;
        const int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -10;
}
// 0 = xong khối, 1 = cắt bớt (đủ trần), <0 = lỗi
int codes(Inflater &s, const Huff &lencode, const Huff &distcode)
{
    static const short lens[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const short lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const short dists[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const short dext[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    for (;;) {
        int sym = decode(s, lencode);
        if (sym < 0) return -1;
        if (sym < 256) {
            if (s.out->size() >= s.maxOut) { s.truncated = true; return 1; }
            s.out->append(char(sym));
        } else if (sym == 256) {
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29) return -2;
            int len = lens[sym] + s.bits(lext[sym]);
            const int ds = decode(s, distcode);
            if (ds < 0 || ds >= 30) return -3;
            const qint64 dist = dists[ds] + s.bits(dext[ds]);
            if (s.eof) return -1;
            if (dist > s.out->size()) return -4;
            if (s.out->size() + len > s.maxOut) { len = int(s.maxOut - s.out->size()); s.truncated = true; }
            qint64 from = s.out->size() - dist;
            for (int i = 0; i < len; ++i) s.out->append(s.out->at(from + i));
            if (s.truncated) return 1;
        }
    }
}
} // namespace

QByteArray inflateRaw(const QByteArray &deflated, qint64 maxOut, int *status)
{
    QByteArray out;
    Inflater s;
    s.in = reinterpret_cast<const quint8 *>(deflated.constData());
    s.inLen = deflated.size();
    s.out = &out;
    s.maxOut = maxOut;
    int st = 0;
    int last;
    do {
        last = s.bits(1);
        const int type = s.bits(2);
        if (s.eof) { st = -1; break; }
        if (type == 0) {
            s.bitBuf = 0; s.bitCnt = 0;
            if (s.inPos + 4 > s.inLen) { st = -1; break; }
            const int len = s.in[s.inPos] | (s.in[s.inPos + 1] << 8);
            const int nlen = s.in[s.inPos + 2] | (s.in[s.inPos + 3] << 8);
            s.inPos += 4;
            if (len != (~nlen & 0xFFFF)) { st = -2; break; }
            if (s.inPos + len > s.inLen) { st = -1; break; }
            qint64 take = len;
            if (out.size() + take > maxOut) { take = maxOut - out.size(); s.truncated = true; }
            out.append(reinterpret_cast<const char *>(s.in + s.inPos), int(take));
            s.inPos += len;
            if (s.truncated) { st = 1; break; }
        } else if (type == 1 || type == 2) {
            Huff lencode, distcode;
            if (type == 1) {
                short lengths[288];
                int sym = 0;
                for (; sym < 144; ++sym) lengths[sym] = 8;
                for (; sym < 256; ++sym) lengths[sym] = 9;
                for (; sym < 280; ++sym) lengths[sym] = 7;
                for (; sym < 288; ++sym) lengths[sym] = 8;
                construct(&lencode, lengths, 288);
                for (sym = 0; sym < 30; ++sym) lengths[sym] = 5;
                construct(&distcode, lengths, 30);
            } else {
                static const short order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                short lengths[320];
                const int nlen = s.bits(5) + 257, ndist = s.bits(5) + 1, ncode = s.bits(4) + 4;
                if (s.eof || nlen > 286 || ndist > 30) { st = -3; break; }
                int index = 0;
                for (; index < ncode; ++index) lengths[order[index]] = short(s.bits(3));
                for (; index < 19; ++index) lengths[order[index]] = 0;
                if (s.eof) { st = -1; break; }
                Huff lc;
                if (construct(&lc, lengths, 19) != 0) { st = -4; break; }
                index = 0;
                bool bad = false;
                while (index < nlen + ndist) {
                    int sym = decode(s, lc);
                    if (sym < 0) { bad = true; break; }
                    if (sym < 16) { lengths[index++] = short(sym); continue; }
                    int len = 0;
                    if (sym == 16) {
                        if (index == 0) { bad = true; break; }
                        len = lengths[index - 1];
                        sym = 3 + s.bits(2);
                    } else if (sym == 17) sym = 3 + s.bits(3);
                    else sym = 11 + s.bits(7);
                    if (index + sym > nlen + ndist) { bad = true; break; }
                    while (sym--) lengths[index++] = short(len);
                }
                if (bad || s.eof) { st = -5; break; }
                if (lengths[256] == 0) { st = -6; break; }
                int err = construct(&lencode, lengths, nlen);
                if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1])) { st = -7; break; }
                err = construct(&distcode, lengths + nlen, ndist);
                if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1])) { st = -8; break; }
            }
            const int r = codes(s, lencode, distcode);
            if (r != 0) { st = r; break; }
        } else { st = -9; break; }
    } while (!last);
    if (status) *status = st;
    return out;
}

// ======================= Đọc zip tối thiểu =======================
namespace {
inline quint16 u16(const QByteArray &b, int o) { return quint16(quint8(b[o]) | (quint8(b[o + 1]) << 8)); }
inline quint32 u32(const QByteArray &b, int o) { return quint32(quint8(b[o])) | (quint32(quint8(b[o + 1])) << 8) | (quint32(quint8(b[o + 2])) << 16) | (quint32(quint8(b[o + 3])) << 24); }

struct ZipEntry { QString name; quint16 method = 0, flags = 0; quint32 crc = 0; quint64 comp = 0, uncomp = 0, localOff = 0; };

class Zip {
public:
    QVector<ZipEntry> entries;
    QString path;
    bool open(const QString &p, QString *err)
    {
        path = p;
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) { *err = QObject::tr("không mở được tệp (%1)").arg(f.errorString()); return false; }
        const qint64 size = f.size();
        if (size < 22) { *err = QObject::tr("không phải zip hợp lệ (quá nhỏ)"); return false; }
        const qint64 tailLen = qMin<qint64>(size, 65557);
        f.seek(size - tailLen);
        const QByteArray tail = f.read(tailLen);
        int e = -1;
        for (int i = int(tail.size()) - 22; i >= 0; --i)
            if (u32(tail, i) == 0x06054b50u) { e = i; break; }
        if (e < 0) { *err = QObject::tr("không phải zip hợp lệ (thiếu bản ghi cuối)"); return false; }
        const quint16 total = u16(tail, e + 10);
        const quint32 cdSize = u32(tail, e + 12), cdOff = u32(tail, e + 16);
        if (total == 0xFFFF || cdOff == 0xFFFFFFFFu || cdSize == 0xFFFFFFFFu) { *err = QObject::tr("zip64 chưa được hỗ trợ"); return false; }
        if (cdSize > 64u * 1024 * 1024 || qint64(cdOff) + cdSize > size) { *err = QObject::tr("zip hỏng (danh mục tệp sai)"); return false; }
        f.seek(cdOff);
        const QByteArray cd = f.read(cdSize);
        if (cd.size() != qint64(cdSize)) { *err = QObject::tr("zip hỏng (không đọc đủ danh mục)"); return false; }
        int pos = 0;
        while (pos + 46 <= cd.size()) {
            if (u32(cd, pos) != 0x02014b50u) break;
            ZipEntry en;
            en.flags = u16(cd, pos + 8);
            en.method = u16(cd, pos + 10);
            en.crc = u32(cd, pos + 16);
            en.comp = u32(cd, pos + 20);
            en.uncomp = u32(cd, pos + 24);
            const int nl = u16(cd, pos + 28), xl = u16(cd, pos + 30), cl = u16(cd, pos + 32);
            en.localOff = u32(cd, pos + 42);
            if (pos + 46 + nl > cd.size()) break;
            en.name = QString::fromUtf8(cd.constData() + pos + 46, nl);
            entries.append(en);
            pos += 46 + nl + xl + cl;
        }
        if (entries.isEmpty()) { *err = QObject::tr("zip rỗng hoặc hỏng"); return false; }
        return true;
    }
    const ZipEntry *find(const QString &name) const
    {
        for (const auto &e : entries) if (e.name == name) return &e;
        return nullptr;
    }
    // Đọc một mục, tối đa maxOut byte đã giải nén. trunc=true nếu bị cắt bớt.
    QByteArray read(const ZipEntry &en, qint64 maxOut, QString *err, bool *trunc)
    {
        if (trunc) *trunc = false;
        if (en.flags & 1) { *err = QObject::tr("mục %1 được mã hoá").arg(en.name); return {}; }
        if (en.method != 0 && en.method != 8) { *err = QObject::tr("mục %1 dùng kiểu nén chưa hỗ trợ (%2)").arg(en.name).arg(en.method); return {}; }
        if (qint64(en.comp) > maxOut + 1024 && en.method == 0) {
            // stored: đọc tối đa maxOut
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) { *err = QObject::tr("không mở lại được zip"); return {}; }
        f.seek(qint64(en.localOff));
        const QByteArray lh = f.read(30);
        if (lh.size() < 30 || u32(lh, 0) != 0x04034b50u) { *err = QObject::tr("zip hỏng (đầu mục sai)"); return {}; }
        const qint64 dataOff = qint64(en.localOff) + 30 + u16(lh, 26) + u16(lh, 28);
        f.seek(dataOff);
        const qint64 compCap = en.method == 0 ? qMin<qint64>(qint64(en.comp), maxOut) : qint64(en.comp);
        if (en.method == 8 && qint64(en.comp) > 128LL * 1024 * 1024) { *err = QObject::tr("mục %1 quá lớn").arg(en.name); return {}; }
        const QByteArray comp = f.read(compCap);
        QByteArray out;
        bool truncated = false;
        if (en.method == 0) {
            out = comp;
            truncated = qint64(en.comp) > maxOut;
        } else {
            int st = 0;
            out = inflateRaw(comp, maxOut, &st);
            if (st == 1) truncated = true;
            else if (st < 0) { *err = QObject::tr("giải nén %1 lỗi (mã %2) — tệp có thể hỏng").arg(en.name).arg(st); return {}; }
        }
        if (!truncated && qint64(en.uncomp) == out.size() && crc32(out) != en.crc) {
            *err = QObject::tr("mục %1 sai mã kiểm tra CRC — tệp hỏng").arg(en.name);
            return {};
        }
        if (trunc) *trunc = truncated;
        return out;
    }
};

const QString kPdfUnavailable = QStringLiteral("\x01PDF_UNAVAILABLE");

QString xmlError(const QString &part, const QXmlStreamReader &xr)
{
    return QObject::tr("XML trong %1 hỏng: %2").arg(part, xr.errorString());
}

bool endsWithNum(const QString &name, const QString &prefix, const QString &suffix, int *num)
{
    if (!name.startsWith(prefix) || !name.endsWith(suffix)) return false;
    bool ok = false;
    *num = name.mid(prefix.size(), name.size() - prefix.size() - suffix.size()).toInt(&ok);
    return ok;
}

// ---- docx ----
QString extractDocx(Zip &zip, qint64 budget, const ChunkSink &sink, bool *truncated)
{
    QStringList parts;
    if (zip.find(QStringLiteral("word/document.xml"))) parts << QStringLiteral("word/document.xml");
    else return QObject::tr("docx không có word/document.xml");
    QStringList extra;
    for (const auto &e : zip.entries)
        if (e.name.startsWith(QLatin1String("word/")) && e.name.endsWith(QLatin1String(".xml")) &&
            (e.name.contains(QLatin1String("header")) || e.name.contains(QLatin1String("footer")) ||
             e.name.contains(QLatin1String("footnotes")) || e.name.contains(QLatin1String("endnotes"))))
            extra << e.name;
    extra.sort();
    parts << extra;
    for (const QString &part : parts) {
        QString err;
        bool tr = false;
        const QByteArray data = zip.read(*zip.find(part), budget, &err, &tr);
        if (!err.isEmpty()) return err;
        if (tr && truncated) *truncated = true;
        QXmlStreamReader xr(data);
        QString para;
        int pn = 0;
        const bool main = part == QLatin1String("word/document.xml");
        while (!xr.atEnd()) {
            const auto t = xr.readNext();
            if (t == QXmlStreamReader::StartElement) {
                const auto n = xr.name();
                if (n == QLatin1String("p")) para.clear();
                else if (n == QLatin1String("t")) para += xr.readElementText(QXmlStreamReader::IncludeChildElements);
                else if (n == QLatin1String("tab")) para += QLatin1Char('\t');
                else if (n == QLatin1String("br") || n == QLatin1String("cr")) para += QLatin1Char(' ');
            } else if (t == QXmlStreamReader::EndElement && xr.name() == QLatin1String("p")) {
                ++pn;
                if (!para.isEmpty()) {
                    const QString loc = main ? QObject::tr("đoạn %1").arg(pn)
                                             : QObject::tr("%1, đoạn %2").arg(part.mid(5, part.size() - 9), QString::number(pn));
                    if (!sink(para, loc)) return {};
                    para.clear();
                }
            }
        }
        if (xr.hasError() && !(tr)) return xmlError(part, xr);
    }
    return {};
}

// ---- pptx ----
QString extractPptx(Zip &zip, qint64 budget, const ChunkSink &sink, bool *truncated)
{
    QVector<QPair<int, QString>> slides;
    for (const auto &e : zip.entries) {
        int n = 0;
        if (endsWithNum(e.name, QStringLiteral("ppt/slides/slide"), QStringLiteral(".xml"), &n)) slides.append({n, e.name});
    }
    if (slides.isEmpty()) return QObject::tr("pptx không có slide nào");
    std::sort(slides.begin(), slides.end());
    for (const auto &sl : slides) {
        QString err;
        bool tr = false;
        const QByteArray data = zip.read(*zip.find(sl.second), budget, &err, &tr);
        if (!err.isEmpty()) return err;
        if (tr && truncated) *truncated = true;
        QXmlStreamReader xr(data);
        QString para;
        const QString loc = QObject::tr("slide %1").arg(sl.first);
        while (!xr.atEnd()) {
            const auto t = xr.readNext();
            if (t == QXmlStreamReader::StartElement) {
                const auto n = xr.name();
                if (n == QLatin1String("p")) para.clear();
                else if (n == QLatin1String("t")) para += xr.readElementText(QXmlStreamReader::IncludeChildElements);
                else if (n == QLatin1String("br")) para += QLatin1Char(' ');
            } else if (t == QXmlStreamReader::EndElement && xr.name() == QLatin1String("p")) {
                if (!para.isEmpty()) { if (!sink(para, loc)) return {}; para.clear(); }
            }
        }
        if (xr.hasError() && !tr) return xmlError(sl.second, xr);
    }
    return {};
}

// ---- xlsx ----
QString extractXlsx(Zip &zip, qint64 budget, const ChunkSink &sink, bool *truncated)
{
    QStringList shared;
    if (const ZipEntry *ss = zip.find(QStringLiteral("xl/sharedStrings.xml"))) {
        QString err;
        bool tr = false;
        const QByteArray data = zip.read(*ss, budget, &err, &tr);
        if (!err.isEmpty()) return err;
        if (tr && truncated) *truncated = true;
        QXmlStreamReader xr(data);
        QString cur;
        int rph = 0;
        qint64 chars = 0;
        while (!xr.atEnd()) {
            const auto t = xr.readNext();
            if (t == QXmlStreamReader::StartElement) {
                const auto n = xr.name();
                if (n == QLatin1String("si")) { cur.clear(); rph = 0; }
                else if (n == QLatin1String("rPh")) ++rph;
                else if (n == QLatin1String("t") && rph == 0) cur += xr.readElementText(QXmlStreamReader::IncludeChildElements);
            } else if (t == QXmlStreamReader::EndElement) {
                const auto n = xr.name();
                if (n == QLatin1String("rPh") && rph > 0) --rph;
                else if (n == QLatin1String("si")) {
                    shared.append(cur);
                    chars += cur.size();
                    if (chars * 2 > budget) { if (truncated) *truncated = true; break; }
                }
            }
        }
        if (xr.hasError() && !tr) return xmlError(ss->name, xr);
    }
    // tên sheet ↔ tệp
    QVector<QPair<QString, QString>> sheets; // tên, đường dẫn trong zip
    QHash<QString, QString> rels;
    if (const ZipEntry *re = zip.find(QStringLiteral("xl/_rels/workbook.xml.rels"))) {
        QString err;
        const QByteArray data = zip.read(*re, 4 * 1024 * 1024, &err, nullptr);
        if (err.isEmpty()) {
            QXmlStreamReader xr(data);
            while (!xr.atEnd()) {
                if (xr.readNext() == QXmlStreamReader::StartElement && xr.name() == QLatin1String("Relationship")) {
                    QString target = xr.attributes().value(QLatin1String("Target")).toString();
                    if (target.startsWith(QLatin1Char('/'))) target = target.mid(1);
                    else target = QStringLiteral("xl/") + target;
                    rels.insert(xr.attributes().value(QLatin1String("Id")).toString(), target);
                }
            }
        }
    }
    if (const ZipEntry *we = zip.find(QStringLiteral("xl/workbook.xml"))) {
        QString err;
        const QByteArray data = zip.read(*we, 4 * 1024 * 1024, &err, nullptr);
        if (err.isEmpty()) {
            QXmlStreamReader xr(data);
            while (!xr.atEnd()) {
                if (xr.readNext() == QXmlStreamReader::StartElement && xr.name() == QLatin1String("sheet")) {
                    QString rid;
                    for (const auto &a : xr.attributes())
                        if (a.name() == QLatin1String("id")) rid = a.value().toString();
                    const QString target = rels.value(rid);
                    if (!target.isEmpty()) sheets.append({xr.attributes().value(QLatin1String("name")).toString(), target});
                }
            }
        }
    }
    if (sheets.isEmpty()) { // dự phòng: theo tên tệp
        QVector<QPair<int, QString>> v;
        for (const auto &e : zip.entries) {
            int n = 0;
            if (endsWithNum(e.name, QStringLiteral("xl/worksheets/sheet"), QStringLiteral(".xml"), &n)) v.append({n, e.name});
        }
        std::sort(v.begin(), v.end());
        for (const auto &p : v) sheets.append({QObject::tr("Sheet%1").arg(p.first), p.second});
    }
    if (sheets.isEmpty()) return QObject::tr("xlsx không có sheet nào");
    for (const auto &sh : sheets) {
        const ZipEntry *se = zip.find(sh.second);
        if (!se) continue;
        QString err;
        bool tr = false;
        const QByteArray data = zip.read(*se, budget, &err, &tr);
        if (!err.isEmpty()) return err;
        if (tr && truncated) *truncated = true;
        QXmlStreamReader xr(data);
        QString ref, type, val;
        bool inIs = false;
        while (!xr.atEnd()) {
            const auto t = xr.readNext();
            if (t == QXmlStreamReader::StartElement) {
                const auto n = xr.name();
                if (n == QLatin1String("c")) {
                    ref = xr.attributes().value(QLatin1String("r")).toString();
                    type = xr.attributes().value(QLatin1String("t")).toString();
                    val.clear();
                    inIs = false;
                } else if (n == QLatin1String("is")) inIs = true;
                else if (n == QLatin1String("t") && inIs) val += xr.readElementText(QXmlStreamReader::IncludeChildElements);
                else if (n == QLatin1String("v")) {
                    const QString text = xr.readElementText(QXmlStreamReader::IncludeChildElements);
                    if (type == QLatin1String("s")) {
                        bool ok = false;
                        const int idx = text.toInt(&ok);
                        if (ok && idx >= 0 && idx < shared.size()) val = shared.at(idx);
                    } else if (type != QLatin1String("e")) val = text;
                }
            } else if (t == QXmlStreamReader::EndElement && xr.name() == QLatin1String("c")) {
                if (!val.isEmpty()) { if (!sink(val, sh.first + QLatin1Char('!') + ref)) return {}; }
                val.clear();
                inIs = false;
            }
        }
        if (xr.hasError() && !tr) return xmlError(sh.second, xr);
    }
    return {};
}

// ---- văn bản thuần ----
QString extractText(const QString &path, qint64 budget, qint64 maxFile, const ChunkSink &sink)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QObject::tr("không mở được tệp (%1)").arg(f.errorString());
    if (f.size() > maxFile) return QObject::tr("tệp văn bản lớn hơn giới hạn %1 MB").arg(maxFile / (1024 * 1024));
    const QByteArray head = f.peek(4);
    enum Enc { U8, U16LE, U16BE } enc = U8;
    int skip = 0;
    if (head.size() >= 3 && quint8(head[0]) == 0xEF && quint8(head[1]) == 0xBB && quint8(head[2]) == 0xBF) skip = 3;
    else if (head.size() >= 2 && quint8(head[0]) == 0xFF && quint8(head[1]) == 0xFE) { enc = U16LE; skip = 2; }
    else if (head.size() >= 2 && quint8(head[0]) == 0xFE && quint8(head[1]) == 0xFF) { enc = U16BE; skip = 2; }
    if (enc != U8) {
        if (f.size() > budget) return QObject::tr("tệp UTF-16 lớn hơn giới hạn bộ nhớ cho phép");
        f.seek(skip);
        const QByteArray all = f.readAll();
        QStringDecoder dec(enc == U16LE ? QStringConverter::Utf16LE : QStringConverter::Utf16BE);
        const QString text = dec(all);
        if (dec.hasError()) return QObject::tr("UTF-16 không hợp lệ");
        int line = 0, pos = 0;
        while (pos <= text.size()) {
            int nl = text.indexOf(QLatin1Char('\n'), pos);
            if (nl < 0) nl = text.size();
            ++line;
            QString l = text.mid(pos, nl - pos);
            if (l.endsWith(QLatin1Char('\r'))) l.chop(1);
            if (!l.isEmpty() && !sink(l, QObject::tr("dòng %1").arg(line))) return {};
            pos = nl + 1;
            if (nl >= text.size()) break;
        }
        return {};
    }
    // UTF-8: đọc từng dòng, bộ nhớ gần như không đổi
    f.seek(skip);
    const QByteArray probe = f.peek(8192);
    if (probe.contains('\0')) return QObject::tr("tệp nhị phân (không phải văn bản)");
    const qint64 maxLine = qMax<qint64>(64 * 1024, qMin<qint64>(budget / 4, 4 * 1024 * 1024));
    qint64 line = 1;
    bool cont = false; // đang ở giữa một dòng dài bị cắt
    while (!f.atEnd()) {
        QByteArray b = f.readLine(maxLine);
        if (b.isEmpty()) break;
        const bool full = b.endsWith('\n');
        if (!full && !f.atEnd()) { // dòng dài bị cắt: trả lại phần cuối nếu đang dở một ký tự nhiều byte
            for (int k = 1; k <= 3 && k <= b.size(); ++k) {
                const quint8 c = quint8(b[b.size() - k]);
                if ((c & 0xC0) == 0x80) continue;
                if (c >= 0xC0) {
                    const int need = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2);
                    if (need > k) { f.seek(f.pos() - k); b.chop(k); }
                }
                break;
            }
        }
        QStringDecoder dec(QStringConverter::Utf8);
        QString s = dec(b);
        if (dec.hasError())
            return QObject::tr("không phải UTF-8 hợp lệ (có thể là bảng mã cũ như TCVN3/Windows-1258) tại dòng %1").arg(line);
        while (s.endsWith(QLatin1Char('\n')) || s.endsWith(QLatin1Char('\r'))) s.chop(1);
        if (!s.isEmpty() && !sink(s, QObject::tr("dòng %1").arg(line))) return {};
        if (full || f.atEnd()) { ++line; cont = false; } else cont = true;
    }
    Q_UNUSED(cont)
    return {};
}

// ---- pdf: ưu tiên Qt6::Pdf (có sẵn khi đóng gói cùng ứng dụng), pdftotext chỉ là phương án dự phòng ----
bool qtPdfBuiltIn()
{
#ifdef DUNG_HAVE_QTPDF
    return true;
#else
    return false;
#endif
}

#ifdef DUNG_HAVE_QTPDF
QString extractPdfQt(const QString &path, qint64 budget, const ChunkSink &sink, bool *truncated)
{
    QPdfDocument doc;
    const QPdfDocument::Error err = doc.load(path);
    if (err == QPdfDocument::Error::IncorrectPassword || err == QPdfDocument::Error::UnsupportedSecurityScheme)
        return QObject::tr("PDF có mật khẩu hoặc mã hoá — không đọc được nội dung");
    if (err != QPdfDocument::Error::None)
        return QObject::tr("không mở được PDF (Qt6::Pdf, mã lỗi %1) — tệp có thể hỏng").arg(int(err));
    qint64 used = 0;
    const int pages = doc.pageCount();
    for (int page = 0; page < pages; ++page) {
        const QString text = doc.getAllText(page).text();
        used += text.size() * 2;
        int ln = 0;
        for (const QString &l : text.split(QRegularExpression(QStringLiteral("\\r?\\n|\\r")))) {
            ++ln;
            if (l.trimmed().isEmpty()) continue;
            if (!sink(l, QObject::tr("trang %1, dòng %2").arg(page + 1).arg(ln))) return {};
        }
        if (used > budget) { if (truncated) *truncated = true; break; }
    }
    return {};
}
#endif

QString extractPdf(const QString &path, const QString &pdftotext, qint64 budget, const ChunkSink &sink, bool *truncated)
{
#ifdef DUNG_HAVE_QTPDF
    if (pdftotext.isEmpty()) return extractPdfQt(path, budget, sink, truncated);
#endif
    QString exe = pdftotext;
    if (exe.isEmpty()) exe = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (exe.isEmpty()) return kPdfUnavailable;
    QProcess p;
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start(exe, {QStringLiteral("-q"), QStringLiteral("-enc"), QStringLiteral("UTF-8"), path, QStringLiteral("-")});
    if (!p.waitForStarted(5000)) return QObject::tr("không chạy được pdftotext");
    QByteArray out;
    QElapsedTimer tm;
    tm.start();
    bool trunc = false;
    while (p.state() != QProcess::NotRunning) {
        p.waitForReadyRead(200);
        out += p.readAllStandardOutput();
        if (out.size() > budget) { trunc = true; p.kill(); break; }
        if (tm.elapsed() > 60000) { p.kill(); p.waitForFinished(2000); return QObject::tr("pdftotext quá 60 giây"); }
    }
    p.waitForFinished(2000);
    out += p.readAllStandardOutput();
    if (!trunc && p.exitCode() != 0) {
        const QString e = QString::fromUtf8(p.readAllStandardError()).trimmed().left(120);
        return QObject::tr("pdftotext báo lỗi (%1) — PDF có thể hỏng hoặc có mật khẩu").arg(e.isEmpty() ? QString::number(p.exitCode()) : e);
    }
    if (trunc && truncated) *truncated = true;
    const QString text = QString::fromUtf8(out);
    int page = 1;
    for (const QString &pg : text.split(QLatin1Char('\f'))) {
        int ln = 0;
        for (const QString &l : pg.split(QLatin1Char('\n'))) {
            ++ln;
            if (l.trimmed().isEmpty()) continue;
            if (!sink(l, QObject::tr("trang %1, dòng %2").arg(page).arg(ln))) return {};
        }
        ++page;
    }
    return {};
}

enum class Kind { Text, Docx, Xlsx, Pptx, Pdf, Unsupported };
Kind classify(const QString &ext)
{
    static const QSet<QString> text = {
        "txt","csv","tsv","md","markdown","log","json","xml","html","htm","ini","cfg","conf","yaml","yml","srt","tex","sql",
        "py","js","ts","css","java","c","cpp","h","hpp","cs","bat","ps1","sh","vbs","bas","reg","url","rst"};
    if (text.contains(ext)) return Kind::Text;
    if (ext == QLatin1String("docx") || ext == QLatin1String("docm") || ext == QLatin1String("dotx")) return Kind::Docx;
    if (ext == QLatin1String("xlsx") || ext == QLatin1String("xlsm") || ext == QLatin1String("xltx")) return Kind::Xlsx;
    if (ext == QLatin1String("pptx") || ext == QLatin1String("pptm") || ext == QLatin1String("potx")) return Kind::Pptx;
    if (ext == QLatin1String("pdf")) return Kind::Pdf;
    return Kind::Unsupported;
}
} // namespace

QString extractChunks(const QString &path, const Query &q, const ChunkSink &sink, bool *truncated)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    const int threads = q.threads > 0 ? q.threads : 1;
    const qint64 budget = qMax<qint64>(8LL << 20, (qint64(q.maxMemoryMB) << 20) / (threads * 2));
    switch (classify(ext)) {
    case Kind::Text: return extractText(path, budget, q.maxTextFileBytes, sink);
    case Kind::Pdf: return extractPdf(path, q.pdftotextPath, budget, sink, truncated);
    case Kind::Unsupported: return QObject::tr("định dạng .%1 chưa hỗ trợ tìm nội dung").arg(ext);
    default: break;
    }
    QFileInfo fi(path);
    if (fi.size() > q.maxZipFileBytes) return QObject::tr("tệp lớn hơn giới hạn %1 MB").arg(q.maxZipFileBytes / (1024 * 1024));
    Zip zip;
    QString err;
    if (!zip.open(path, &err)) return err;
    switch (classify(ext)) {
    case Kind::Docx: return extractDocx(zip, budget, sink, truncated);
    case Kind::Xlsx: return extractXlsx(zip, budget, sink, truncated);
    case Kind::Pptx: return extractPptx(zip, budget, sink, truncated);
    default: return {};
    }
}

// ======================= Chỉ mục tên tệp =======================
static QString cleanRoot(const QString &root) { return QDir::cleanPath(QFileInfo(root).absoluteFilePath()); }

QString indexFilePath(const QString &root, const QString &indexDir)
{
    QString key = cleanRoot(root);
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    const QByteArray h = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return QDir(indexDir).filePath(QString::fromLatin1(h) + QStringLiteral(".dsidx"));
}

static void fillFolds(IndexEntry &e)
{
    const QString name = e.rel.mid(e.rel.lastIndexOf(QLatin1Char('/')) + 1);
    e.folded = foldText(name, true);
    e.foldedCase = foldText(name, false);
}

bool buildIndex(const QString &rootIn, const QString &indexDir, bool includeHidden, std::atomic_bool *cancel, QString *error, qint64 *count)
{
    const QString root = cleanRoot(rootIn);
    if (!QFileInfo(root).isDir()) { if (error) *error = QObject::tr("Thư mục không tồn tại."); return false; }
    QDir().mkpath(indexDir);
    QDir::Filters fl = QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot;
    if (includeHidden) fl |= QDir::Hidden;
    QDirIterator it(root, fl, QDirIterator::Subdirectories);
    QVector<IndexEntry> entries;
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
    while (it.hasNext()) {
        if (cancel && cancel->load()) { if (error) *error = QObject::tr("Đã huỷ."); return false; }
        const QString p = it.next();
        const QFileInfo fi = it.fileInfo();
        IndexEntry e;
        e.rel = p.startsWith(prefix) ? p.mid(prefix.size()) : p;
        e.size = fi.size();
        e.mtimeMs = fi.lastModified().toMSecsSinceEpoch();
        entries.append(e);
    }
    QByteArray raw;
    {
        QDataStream ds(&raw, QIODevice::WriteOnly);
        ds.setVersion(QDataStream::Qt_6_0);
        ds << root << qint64(QDateTime::currentMSecsSinceEpoch()) << includeHidden << qint32(entries.size());
        for (const auto &e : entries) ds << e.rel << e.size << e.mtimeMs;
    }
    QSaveFile out(indexFilePath(root, indexDir));
    if (!out.open(QIODevice::WriteOnly)) { if (error) *error = QObject::tr("Không ghi được chỉ mục: %1").arg(out.errorString()); return false; }
    out.write("DSIDX1\n");
    out.write(qCompress(raw, 3));
    if (!out.commit()) { if (error) *error = QObject::tr("Không lưu được chỉ mục."); return false; }
    if (count) *count = entries.size();
    return true;
}

bool loadIndex(const QString &rootIn, const QString &indexDir, Index *outIdx, QString *error)
{
    const QString root = cleanRoot(rootIn);
    QFile f(indexFilePath(root, indexDir));
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = QObject::tr("Chưa có chỉ mục cho thư mục này."); return false; }
    const QByteArray all = f.readAll();
    if (!all.startsWith("DSIDX1\n")) { if (error) *error = QObject::tr("Chỉ mục hỏng hoặc không đúng phiên bản."); return false; }
    const QByteArray raw = qUncompress(all.mid(7));
    if (raw.isEmpty()) { if (error) *error = QObject::tr("Chỉ mục hỏng."); return false; }
    QDataStream ds(raw);
    ds.setVersion(QDataStream::Qt_6_0);
    Index idx;
    qint32 n = 0;
    ds >> idx.root >> idx.builtMs >> idx.includeHidden >> n;
    if (ds.status() != QDataStream::Ok || n < 0) { if (error) *error = QObject::tr("Chỉ mục hỏng."); return false; }
    idx.entries.reserve(n);
    for (qint32 i = 0; i < n; ++i) {
        IndexEntry e;
        ds >> e.rel >> e.size >> e.mtimeMs;
        if (ds.status() != QDataStream::Ok) { if (error) *error = QObject::tr("Chỉ mục hỏng (cụt)."); return false; }
        fillFolds(e);
        idx.entries.append(std::move(e));
    }
    *outIdx = std::move(idx);
    return true;
}

bool removeIndex(const QString &root, const QString &indexDir)
{
    const QString p = indexFilePath(root, indexDir);
    return !QFileInfo::exists(p) || QFile::remove(p);
}

// ======================= Bộ chạy tìm kiếm =======================
namespace {
struct ContentOutcome { bool matched = false; QString snippet, location, error; bool truncated = false; };

ContentOutcome matchContent(const QString &path, const Query &q, const QStringList &terms, const std::function<bool()> &cancelled)
{
    ContentOutcome oc;
    QVector<bool> found(terms.size(), false);
    int remaining = terms.size();
    QVector<int> map;
    const ChunkSink sink = [&](const QString &text, const QString &loc) {
        if (cancelled()) return false;
        const QString folded = foldText(text, q.ignoreDiacritics, &map);
        for (int i = 0; i < terms.size(); ++i) {
            if (found[i]) continue;
            const int pos = folded.indexOf(terms.at(i));
            if (pos < 0) continue;
            found[i] = true;
            --remaining;
            if (oc.snippet.isEmpty()) {
                const int last = qMin(pos + terms.at(i).size() - 1, int(map.size()) - 1);
                const int op = map.at(pos), oe = map.at(last) + 1;
                const int a = qMax(0, op - 40), b = qMin(int(text.size()), oe + 80);
                oc.snippet = (a > 0 ? QStringLiteral("…") : QString()) + text.mid(a, b - a).simplified() + (b < text.size() ? QStringLiteral("…") : QString());
                oc.location = loc;
            }
        }
        return remaining > 0;
    };
    bool trunc = false;
    const QString err = extractChunks(path, q, sink, &trunc);
    oc.truncated = trunc;
    oc.matched = remaining == 0;
    if (!oc.matched) {
        if (!err.isEmpty()) oc.error = err;
        else if (trunc) oc.error = QObject::tr("chỉ đọc được một phần (vượt giới hạn bộ nhớ) — có thể bỏ sót");
    }
    return oc;
}
} // namespace

Output search(const Query &q, const Index *index, std::atomic_bool *extCancel,
              const std::function<void(const Result &)> &onResult,
              const std::function<void(qint64)> &onProgress)
{
    Output out;
    QElapsedTimer tm;
    tm.start();
    const QString root = cleanRoot(q.root);
    if (q.root.trimmed().isEmpty() || !QFileInfo(root).isDir()) { out.error = QObject::tr("Hãy chọn một thư mục tồn tại để tìm."); return out; }
    QStringList nameF, exclF, contF;
    for (const QString &t : q.nameTerms) { const QString f = foldText(t, q.ignoreDiacritics); if (!f.isEmpty()) nameF << f; }
    for (const QString &t : q.excludeTerms) { const QString f = foldText(t, q.ignoreDiacritics); if (!f.isEmpty()) exclF << f; }
    for (const QString &t : q.contentTerms) { const QString f = foldText(t, q.ignoreDiacritics); if (!f.isEmpty()) contF << f; }
    QSet<QString> exts;
    for (QString e : q.extensions) { e = e.toLower(); if (e.startsWith(QLatin1Char('.'))) e = e.mid(1); if (!e.isEmpty()) exts.insert(e); }
    const bool wantContent = !contF.isEmpty();
    const bool hasFilter = !nameF.isEmpty() || !exclF.isEmpty() || wantContent || !exts.isEmpty() || q.minSize >= 0 || q.maxSize >= 0 ||
                           q.modifiedAfter.isValid() || q.modifiedBefore.isValid();
    if (!hasFilter) { out.error = QObject::tr("Hãy nhập từ khoá hoặc chọn bộ lọc."); return out; }

    const int threads = q.threads > 0 ? q.threads : qMax(1, qMin(4, QThread::idealThreadCount() / 2));
    Query qq = q;
    qq.threads = threads;
    std::atomic_bool localCancel{false};
    auto cancelled = [&]() { return localCancel.load() || (extCancel && extCancel->load()); };

    QMutex mu;
    bool pdfWarned = false;
    auto addSkip = [&](const QString &path, const QString &why) {
        QMutexLocker l(&mu);
        out.stats.skipped++;
        if (out.skipped.size() < 2000) out.skipped.append({path, why});
    };
    auto addResult = [&](const Result &r) {
        QMutexLocker l(&mu);
        if (out.results.size() >= q.maxResults) { out.stats.truncatedResults = true; localCancel = true; return; }
        out.results.append(r);
        out.stats.matched++;
        if (onResult) onResult(r);
    };

    QThreadPool pool;
    pool.setMaxThreadCount(threads);
    QSemaphore slotSem(threads * 4);

    auto consider = [&](const QString &path, const QString &foldedName, qint64 size, const QDateTime &mtime, const QString &ext) {
        out.stats.filesSeen++;
        if (onProgress && (out.stats.filesSeen & 255) == 0) onProgress(out.stats.filesSeen);
        for (const QString &t : nameF) if (!foldedName.contains(t)) return;
        for (const QString &t : exclF) if (foldedName.contains(t)) return;
        if (!exts.isEmpty() && !exts.contains(ext)) return;
        if (q.minSize >= 0 && size < q.minSize) return;
        if (q.maxSize >= 0 && size > q.maxSize) return;
        if (q.modifiedAfter.isValid() && mtime < q.modifiedAfter) return;
        if (q.modifiedBefore.isValid() && mtime > q.modifiedBefore) return;
        if (!wantContent) {
            if (index && !QFileInfo::exists(path)) { out.stats.staleIndexEntries++; return; }
            Result r;
            r.path = path; r.size = size; r.modified = mtime;
            addResult(r);
            return;
        }
        const Kind kind = classify(ext);
        if (kind == Kind::Unsupported) { out.stats.unsupportedByExt[ext.isEmpty() ? QStringLiteral("(không đuôi)") : ext]++; return; }
        if (kind == Kind::Pdf && !pdfWarned) {
            const QString exe = q.pdftotextPath.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("pdftotext")) : q.pdftotextPath;
            // Có Qt6::Pdf trong bản dựng thì luôn đọc được (trừ khi người dùng chỉ định pdftotext riêng mà không có).
            if (exe.isEmpty() && !(qtPdfBuiltIn() && q.pdftotextPath.isEmpty())) { out.stats.pdfUnavailable = true; pdfWarned = true; }
        }
        if (kind == Kind::Pdf && out.stats.pdfUnavailable) { out.stats.unsupportedByExt[QStringLiteral("pdf")]++; return; }
        slotSem.acquire();
        if (cancelled()) { slotSem.release(); return; }
        pool.start(QRunnable::create([&, path, size, mtime]() {
            if (!cancelled()) {
                const ContentOutcome oc = matchContent(path, qq, contF, cancelled);
                {
                    QMutexLocker l(&mu);
                    out.stats.filesContentRead++;
                    out.stats.bytesRead += size;
                }
                if (oc.matched) {
                    Result r;
                    r.path = path; r.size = size; r.modified = mtime; r.snippet = oc.snippet; r.location = oc.location;
                    addResult(r);
                } else if (!oc.error.isEmpty() && !cancelled()) {
                    addSkip(path, oc.error);
                }
            }
            slotSem.release();
        }));
    };

    const bool useIndex = index && q.useIndexForNames && cleanRoot(index->root) == root && (index->includeHidden || !q.includeHidden);
    if (useIndex) {
        const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
        for (const IndexEntry &e : index->entries) {
            if (cancelled()) break;
            if (!q.includeHidden) { // bỏ tệp nằm trong thư mục/tên ẩn
                bool hidden = false;
                for (const QString &part : e.rel.split(QLatin1Char('/'))) if (part.startsWith(QLatin1Char('.'))) { hidden = true; break; }
                if (hidden) continue;
            }
            const QString name = e.rel.mid(e.rel.lastIndexOf(QLatin1Char('/')) + 1);
            consider(prefix + e.rel, q.ignoreDiacritics ? e.folded : e.foldedCase, e.size, QDateTime::fromMSecsSinceEpoch(e.mtimeMs),
                     QFileInfo(name).suffix().toLower());
        }
    } else {
        QDir::Filters fl = QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot;
        if (q.includeHidden) fl |= QDir::Hidden;
        QDirIterator it(root, fl, QDirIterator::Subdirectories);
        while (it.hasNext() && !cancelled()) {
            const QString p = it.next();
            const QFileInfo fi = it.fileInfo();
            const QString name = fi.fileName();
            consider(p, foldText(name, q.ignoreDiacritics), fi.size(), fi.lastModified(), fi.suffix().toLower());
        }
    }
    pool.waitForDone();
    out.stats.canceled = extCancel && extCancel->load();
    out.stats.elapsedMs = tm.elapsed();
    std::sort(out.results.begin(), out.results.end(), [](const Result &a, const Result &b) { return a.path.compare(b.path, Qt::CaseInsensitive) < 0; });
    return out;
}

} // namespace filesearch
