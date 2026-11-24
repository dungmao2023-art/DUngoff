// SPDX-License-Identifier: MPL-2.0
#include "ZipLite.h"
#include <QFile>
#include <QObject>
#include <vector>

namespace ft {

quint32 zipCrc32(const QByteArray& data)
{
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        init = true;
    }
    quint32 c = 0xFFFFFFFFu;
    for (char ch : data) c = table[(c ^ quint8(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// ---------------- inflate (thuật toán chuẩn RFC 1951, viết lại theo mô tả; không dùng mã của bên thứ ba) ----------------
namespace {
constexpr int kMaxBits = 15;
struct Huff { short count[kMaxBits + 1]; short symbol[288]; };
struct St {
    const uchar* in = nullptr; qint64 inlen = 0, incnt = 0;
    quint32 bitbuf = 0; int bitcnt = 0;
    QByteArray* out = nullptr; qint64 limit = 0;
    bool bad = false; QString err;
};
void fail(St& s, const QString& m) { if (!s.bad) { s.bad = true; s.err = m; } }

int getbits(St& s, int need)
{
    if (need == 0) return 0;
    qint64 val = s.bitbuf;
    while (s.bitcnt < need) {
        if (s.incnt >= s.inlen) { fail(s, QObject::tr("dữ liệu nén bị cụt")); return 0; }
        val |= qint64(s.in[s.incnt++]) << s.bitcnt;
        s.bitcnt += 8;
    }
    s.bitbuf = quint32(val >> need);
    s.bitcnt -= need;
    return int(val & ((qint64(1) << need) - 1));
}
int decodeSym(St& s, const Huff& h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= kMaxBits; ++len) {
        code |= getbits(s, 1);
        if (s.bad) return -1;
        const int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count; first += count; first <<= 1; code <<= 1;
    }
    fail(s, QObject::tr("mã Huffman không hợp lệ"));
    return -1;
}
int construct(Huff& h, const short* length, int n)
{
    short offs[kMaxBits + 1];
    for (int len = 0; len <= kMaxBits; ++len) h.count[len] = 0;
    for (int sym = 0; sym < n; ++sym) h.count[length[sym]]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len <= kMaxBits; ++len) { left <<= 1; left -= h.count[len]; if (left < 0) return left; }
    offs[1] = 0;
    for (int len = 1; len < kMaxBits; ++len) offs[len + 1] = offs[len] + h.count[len];
    for (int sym = 0; sym < n; ++sym) if (length[sym] != 0) h.symbol[offs[length[sym]]++] = short(sym);
    return left;
}
void put(St& s, char c)
{
    if (s.out->size() >= s.limit) { fail(s, QObject::tr("dữ liệu giải nén vượt kích thước khai báo")); return; }
    s.out->append(c);
}
void codes(St& s, const Huff& lc, const Huff& dc)
{
    static const short lens[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const short lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const short dists[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const short dext[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    int sym;
    do {
        sym = decodeSym(s, lc);
        if (s.bad) return;
        if (sym < 256) { put(s, char(sym)); }
        else if (sym > 256) {
            sym -= 257;
            if (sym >= 29) { fail(s, QObject::tr("mã độ dài sai")); return; }
            const int len = lens[sym] + getbits(s, lext[sym]);
            const int ds = decodeSym(s, dc);
            if (s.bad) return;
            if (ds >= 30) { fail(s, QObject::tr("mã khoảng cách sai")); return; }
            const qint64 dist = dists[ds] + getbits(s, dext[ds]);
            if (s.bad) return;
            if (dist > s.out->size()) { fail(s, QObject::tr("khoảng cách vượt quá dữ liệu đã giải nén")); return; }
            for (int i = 0; i < len; ++i) {
                const char c = s.out->at(s.out->size() - dist);
                put(s, c);
                if (s.bad) return;
            }
        }
        if (s.bad) return;
    } while (sym != 256);
}
void stored(St& s)
{
    s.bitbuf = 0; s.bitcnt = 0;
    if (s.incnt + 4 > s.inlen) { fail(s, QObject::tr("khối lưu nguyên bị cụt")); return; }
    const int len = s.in[s.incnt] | (s.in[s.incnt + 1] << 8);
    const int nlen = s.in[s.incnt + 2] | (s.in[s.incnt + 3] << 8);
    s.incnt += 4;
    if (len != (~nlen & 0xFFFF)) { fail(s, QObject::tr("khối lưu nguyên hỏng")); return; }
    if (s.incnt + len > s.inlen) { fail(s, QObject::tr("khối lưu nguyên bị cụt")); return; }
    if (s.out->size() + len > s.limit) { fail(s, QObject::tr("dữ liệu giải nén vượt kích thước khai báo")); return; }
    s.out->append(reinterpret_cast<const char*>(s.in + s.incnt), len);
    s.incnt += len;
}
void fixedBlock(St& s)
{
    static Huff lc, dc;
    static bool init = false;
    if (!init) {
        short l[288];
        int i = 0;
        for (; i < 144; ++i) l[i] = 8;
        for (; i < 256; ++i) l[i] = 9;
        for (; i < 280; ++i) l[i] = 7;
        for (; i < 288; ++i) l[i] = 8;
        construct(lc, l, 288);
        for (i = 0; i < 30; ++i) l[i] = 5;
        construct(dc, l, 30);
        init = true;
    }
    codes(s, lc, dc);
}
void dynamicBlock(St& s)
{
    static const short order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    const int nlen = getbits(s, 5) + 257, ndist = getbits(s, 5) + 1, ncode = getbits(s, 4) + 4;
    if (s.bad) return;
    if (nlen > 286 || ndist > 30) { fail(s, QObject::tr("bảng Huffman sai")); return; }
    short lengths[320];
    int idx = 0;
    for (; idx < ncode; ++idx) lengths[order[idx]] = short(getbits(s, 3));
    for (; idx < 19; ++idx) lengths[order[idx]] = 0;
    if (s.bad) return;
    Huff lc, dc;
    if (construct(lc, lengths, 19) != 0) { fail(s, QObject::tr("bảng mã độ dài chưa đầy đủ")); return; }
    idx = 0;
    while (idx < nlen + ndist) {
        int sym = decodeSym(s, lc);
        if (s.bad) return;
        if (sym < 16) lengths[idx++] = short(sym);
        else {
            int len = 0, rep;
            if (sym == 16) {
                if (idx == 0) { fail(s, QObject::tr("lặp độ dài không có giá trị trước")); return; }
                len = lengths[idx - 1]; rep = 3 + getbits(s, 2);
            } else if (sym == 17) rep = 3 + getbits(s, 3);
            else rep = 11 + getbits(s, 7);
            if (s.bad) return;
            if (idx + rep > nlen + ndist) { fail(s, QObject::tr("quá nhiều độ dài mã")); return; }
            while (rep--) lengths[idx++] = short(len);
        }
    }
    if (lengths[256] == 0) { fail(s, QObject::tr("thiếu mã kết thúc khối")); return; }
    int err = construct(lc, lengths, nlen);
    if (err && (err < 0 || nlen != lc.count[0] + lc.count[1])) { fail(s, QObject::tr("bảng mã literal sai")); return; }
    err = construct(dc, lengths + nlen, ndist);
    if (err && (err < 0 || ndist != dc.count[0] + dc.count[1])) { fail(s, QObject::tr("bảng mã khoảng cách sai")); return; }
    codes(s, lc, dc);
}
} // namespace

bool inflateRaw(const QByteArray& in, qint64 expectedSize, qint64 maxSize, QByteArray* out, QString* err)
{
    if (expectedSize < 0 || expectedSize > maxSize) { if (err) *err = QObject::tr("kích thước giải nén %1 vượt giới hạn %2").arg(expectedSize).arg(maxSize); return false; }
    St s;
    s.in = reinterpret_cast<const uchar*>(in.constData());
    s.inlen = in.size();
    out->clear();
    out->reserve(int(qMin<qint64>(expectedSize, 1 << 26)));
    s.out = out;
    s.limit = expectedSize;
    int last;
    do {
        last = getbits(s, 1);
        const int type = getbits(s, 2);
        if (s.bad) break;
        if (type == 0) stored(s);
        else if (type == 1) fixedBlock(s);
        else if (type == 2) dynamicBlock(s);
        else fail(s, QObject::tr("loại khối không hợp lệ"));
    } while (!last && !s.bad);
    if (!s.bad && out->size() != expectedSize) fail(s, QObject::tr("kích thước giải nén (%1) khác khai báo (%2)").arg(out->size()).arg(expectedSize));
    if (s.bad) { out->clear(); if (err) *err = s.err; return false; }
    return true;
}

// ---------------- ZIP ----------------
static quint32 u32(const QByteArray& b, int o) { return quint32(quint8(b[o])) | (quint32(quint8(b[o + 1])) << 8) | (quint32(quint8(b[o + 2])) << 16) | (quint32(quint8(b[o + 3])) << 24); }
static quint16 u16(const QByteArray& b, int o) { return quint16(quint8(b[o]) | (quint8(b[o + 1]) << 8)); }

bool ZipReader::open(const QString& path, QString* err)
{
    m_entries.clear();
    m_path = path;
    auto bad = [&](const QString& m) { if (err) *err = m; m_entries.clear(); return false; };
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return bad(QObject::tr("không mở được tệp: %1").arg(f.errorString()));
    const qint64 size = f.size();
    if (size < 22) return bad(QObject::tr("không phải tệp ZIP (quá nhỏ)"));
    const qint64 tailLen = qMin<qint64>(size, 22 + 65535);
    f.seek(size - tailLen);
    const QByteArray tail = f.read(tailLen);
    int eocd = -1;
    for (int i = int(tail.size()) - 22; i >= 0; --i)
        if (u32(tail, i) == 0x06054b50u) { eocd = i; break; }
    if (eocd < 0) return bad(QObject::tr("không phải tệp ZIP hợp lệ (không thấy bảng thư mục trung tâm)"));
    const quint16 total = u16(tail, eocd + 10);
    const quint32 cdSize = u32(tail, eocd + 12), cdOff = u32(tail, eocd + 16);
    if (total == 0xFFFF || cdSize == 0xFFFFFFFFu || cdOff == 0xFFFFFFFFu) return bad(QObject::tr("ZIP64 chưa được hỗ trợ"));
    if (qint64(cdOff) + cdSize > size || cdSize > 32u * 1024 * 1024) return bad(QObject::tr("bảng thư mục ZIP hỏng"));
    f.seek(cdOff);
    const QByteArray cd = f.read(cdSize);
    if (cd.size() != int(cdSize)) return bad(QObject::tr("không đọc đủ bảng thư mục ZIP"));
    int p = 0;
    for (int i = 0; i < total; ++i) {
        if (p + 46 > cd.size() || u32(cd, p) != 0x02014b50u) return bad(QObject::tr("mục thư mục ZIP hỏng"));
        ZipEntryInfo e;
        const quint16 flags = u16(cd, p + 8);
        e.method = u16(cd, p + 10);
        e.crc = u32(cd, p + 16);
        e.csize = u32(cd, p + 20);
        e.usize = u32(cd, p + 24);
        const int nlen = u16(cd, p + 28), elen = u16(cd, p + 30), clen = u16(cd, p + 32);
        e.localOffset = u32(cd, p + 42);
        e.encrypted = (flags & 1) != 0;
        e.zip64 = e.csize == 0xFFFFFFFFu || e.usize == 0xFFFFFFFFu || e.localOffset == 0xFFFFFFFFu;
        if (p + 46 + nlen + elen + clen > cd.size()) return bad(QObject::tr("mục thư mục ZIP bị cụt"));
        e.name = QString::fromUtf8(cd.constData() + p + 46, nlen);
        m_entries.push_back(e);
        p += 46 + nlen + elen + clen;
    }
    return true;
}

const ZipEntryInfo* ZipReader::find(const QString& name) const
{
    for (const auto& e : m_entries) if (e.name == name) return &e;
    return nullptr;
}

bool ZipReader::read(const ZipEntryInfo& e, QByteArray* out, QString* err, qint64 maxSize) const
{
    auto bad = [&](const QString& m) { if (err) *err = m; out->clear(); return false; };
    if (e.encrypted) return bad(QObject::tr("'%1' được mã hoá").arg(e.name));
    if (e.zip64) return bad(QObject::tr("'%1' dùng ZIP64 (chưa hỗ trợ)").arg(e.name));
    if (e.usize > quint64(maxSize)) return bad(QObject::tr("'%1' quá lớn (%2 byte)").arg(e.name).arg(e.usize));
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return bad(QObject::tr("không mở được tệp: %1").arg(f.errorString()));
    if (!f.seek(qint64(e.localOffset))) return bad(QObject::tr("vị trí mục sai"));
    const QByteArray lh = f.read(30);
    if (lh.size() != 30 || u32(lh, 0) != 0x04034b50u) return bad(QObject::tr("tiêu đề cục bộ của '%1' hỏng").arg(e.name));
    const qint64 dataOff = qint64(e.localOffset) + 30 + u16(lh, 26) + u16(lh, 28);
    if (dataOff + qint64(e.csize) > f.size()) return bad(QObject::tr("dữ liệu của '%1' vượt quá tệp").arg(e.name));
    f.seek(dataOff);
    const QByteArray raw = f.read(qint64(e.csize));
    if (raw.size() != qint64(e.csize)) return bad(QObject::tr("không đọc đủ dữ liệu '%1'").arg(e.name));
    if (e.method == 0) {
        if (e.csize != e.usize) return bad(QObject::tr("'%1': kích thước không khớp").arg(e.name));
        *out = raw;
    } else if (e.method == 8) {
        QString ie;
        if (!inflateRaw(raw, qint64(e.usize), maxSize, out, &ie)) return bad(QObject::tr("'%1': %2").arg(e.name, ie));
    } else return bad(QObject::tr("'%1' dùng phương pháp nén %2 chưa hỗ trợ").arg(e.name).arg(e.method));
    if (zipCrc32(*out) != e.crc) return bad(QObject::tr("'%1': sai CRC32 (tệp có thể hỏng)").arg(e.name));
    return true;
}

} // namespace ft
