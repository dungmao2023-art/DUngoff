// SPDX-License-Identifier: MPL-2.0
#include "PageMargins.h"

#include "../tools/filetools/ZipLite.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>

namespace dung::word {

// ------------------------------------------------------------------ đơn vị
qint32 mmToTwips(double mm) { return qint32(std::llround(mm / 25.4 * 1440.0)); }
qint32 cmToTwips(double cm) { return mmToTwips(cm * 10.0); }
qint32 inchToTwips(double inch) { return qint32(std::llround(inch * 1440.0)); }
double twipsToMm(qint32 twips) { return twips / 1440.0 * 25.4; }

namespace {

constexpr qint32 kMaxMargin = 31680;  // 22 inch: giới hạn trên của Word
constexpr qint32 kMinContent = 720;   // chừa tối thiểu 0,5 inch cho vùng chữ

const char* kWmlTransitional = "http://schemas.openxmlformats.org/wordprocessingml/2006/main";
const char* kWmlStrict = "http://purl.oclc.org/ooxml/wordprocessingml/main";

// ------------------------------------------------------------------ bộ quét thẻ XML (giữ nguyên từng byte)
struct Tag {
    int start = 0, end = 0;   // [start, end)
    bool closing = false, selfClosing = false;
    QByteArray qname;
};

struct Attr {
    QByteArray name;
    char quote = '"';
    int valStart = 0, valEnd = 0;   // [valStart, valEnd) bên trong dấu nháy
};

QByteArray localName(const QByteArray& q) {
    const int c = q.indexOf(':');
    return c < 0 ? q : q.mid(c + 1);
}
QByteArray prefixOf(const QByteArray& q) {
    const int c = q.indexOf(':');
    return c < 0 ? QByteArray() : q.left(c);
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Tìm thẻ kế tiếp từ pos. true = có thẻ; false = hết (err rỗng) hoặc lỗi (err có nội dung).
bool nextTag(const QByteArray& x, int& pos, Tag& t, QString* err) {
    const int n = x.size();
    for (;;) {
        const int lt = x.indexOf('<', pos);
        if (lt < 0) { pos = n; return false; }
        if (x.mid(lt, 4) == "<!--") {
            const int e = x.indexOf("-->", lt + 4);
            if (e < 0) { *err = QStringLiteral("chú thích XML không đóng"); return false; }
            pos = e + 3;
            continue;
        }
        if (x.mid(lt, 9) == "<![CDATA[") {
            const int e = x.indexOf("]]>", lt + 9);
            if (e < 0) { *err = QStringLiteral("CDATA không đóng"); return false; }
            pos = e + 3;
            continue;
        }
        if (x.mid(lt, 2) == "<?") {
            const int e = x.indexOf("?>", lt + 2);
            if (e < 0) { *err = QStringLiteral("chỉ thị xử lý XML không đóng"); return false; }
            pos = e + 2;
            continue;
        }
        if (x.mid(lt, 2) == "<!") {
            int depth = 0, j = lt + 2;
            for (; j < n; ++j) {
                if (x[j] == '[') ++depth;
                else if (x[j] == ']') --depth;
                else if (x[j] == '>' && depth <= 0) break;
            }
            if (j >= n) { *err = QStringLiteral("khai báo <! không đóng"); return false; }
            pos = j + 1;
            continue;
        }
        // thẻ thường
        int i = lt + 1;
        t = Tag();
        t.start = lt;
        if (i < n && x[i] == '/') { t.closing = true; ++i; }
        const int nameStart = i;
        while (i < n && !isSpace(x[i]) && x[i] != '/' && x[i] != '>') ++i;
        t.qname = x.mid(nameStart, i - nameStart);
        if (t.qname.isEmpty()) { *err = QStringLiteral("thẻ XML không có tên"); return false; }
        char q = 0;
        int j = i;
        for (; j < n; ++j) {
            const char c = x[j];
            if (q) { if (c == q) q = 0; continue; }
            if (c == '"' || c == '\'') { q = c; continue; }
            if (c == '>') break;
        }
        if (j >= n) { *err = QStringLiteral("thẻ XML không đóng"); return false; }
        t.end = j + 1;
        t.selfClosing = !t.closing && x[j - 1] == '/' && j - 1 >= i;
        pos = t.end;
        return true;
    }
}

// Các thuộc tính của thẻ mở.
QVector<Attr> parseAttrs(const QByteArray& x, const Tag& t) {
    QVector<Attr> out;
    int i = t.start + 1 + t.qname.size();
    const int stop = t.end - (t.selfClosing ? 2 : 1);
    while (i < stop) {
        while (i < stop && isSpace(x[i])) ++i;
        if (i >= stop) break;
        const int ns = i;
        while (i < stop && x[i] != '=' && !isSpace(x[i])) ++i;
        Attr a;
        a.name = x.mid(ns, i - ns);
        while (i < stop && isSpace(x[i])) ++i;
        if (i >= stop || x[i] != '=') break;
        ++i;
        while (i < stop && isSpace(x[i])) ++i;
        if (i >= stop || (x[i] != '"' && x[i] != '\'')) break;
        a.quote = x[i];
        ++i;
        a.valStart = i;
        while (i < stop && x[i] != a.quote) ++i;
        if (i >= stop) break;
        a.valEnd = i;
        ++i;
        out.push_back(a);
    }
    return out;
}

struct Edit { int pos = 0, len = 0; QByteArray repl; };

struct SectCtx {
    Tag tag;
    int depth = 0;           // kích thước ngăn xếp ngay sau khi đẩy sectPr
    bool inChange = false;
    bool parentIsBody = false;
    struct Child { QByteArray local; Tag tag; int end = -1; };
    QVector<Child> kids;
};

struct ScanOut {
    QVector<SectionMargins> secs;
    QVector<Edit> edits;
    QStringList warnings;
    int patched = 0, created = 0;
    bool hasBody = false;
    int bodyCloseStart = -1;
    QByteArray bodyPrefix;
    QString error;
    QByteArray attrPrefix;    // tiền tố của thuộc tính (vd. "w")
};

qint32 parseTwips(const QByteArray& v) {
    bool ok = false;
    const int n = v.trimmed().toInt(&ok);
    return ok ? qint32(n) : -1;
}

QByteArray pgMarXml(const QByteArray& elemPrefix, const QByteArray& attrPrefix, const MarginSpec& s) {
    const QByteArray ep = elemPrefix.isEmpty() ? QByteArray() : elemPrefix + ":";
    const QByteArray ap = attrPrefix + ":";
    auto v = [](const std::optional<qint32>& o) { return QByteArray::number(o ? *o : 1440); };
    QByteArray r = "<" + ep + "pgMar " + ap + "top=\"" + v(s.top) + "\" " + ap + "right=\"" + v(s.right) + "\" " + ap + "bottom=\"" +
                   v(s.bottom) + "\" " + ap + "left=\"" + v(s.left) + "\" " + ap + "header=\"720\" " + ap + "footer=\"720\" " + ap +
                   "gutter=\"0\"/>";
    return r;
}

// Phần tử đứng SAU w:pgMar theo thứ tự của lược đồ trong w:sectPr.
const QSet<QByteArray>& afterPgMar() {
    static const QSet<QByteArray> s = {"paperSrc",  "pgBorders", "lnNumType",  "pgNumType", "cols",       "formProt",
                                       "vAlign",    "noEndnote", "titlePg",    "textDirection", "bidi",   "rtlGutter",
                                       "docGrid",   "printerSettings", "sectPrChange"};
    return s;
}

bool scan(const QByteArray& xml, const MarginSpec* spec, ScanOut& o) {
    int pos = 0;
    QString err;
    Tag t;
    // --- thẻ gốc: nhận diện tiền tố của không gian tên WordprocessingML
    QSet<QByteArray> wml;   // các tiền tố ("" = mặc định) ánh xạ tới WordprocessingML
    QByteArray attrPfx;
    {
        int p = 0;
        bool found = false;
        while (nextTag(xml, p, t, &err)) {
            if (t.closing) continue;
            found = true;
            break;
        }
        if (!err.isEmpty()) { o.error = err; return false; }
        if (!found) { o.error = QStringLiteral("word/document.xml rỗng"); return false; }
        for (const Attr& a : parseAttrs(xml, t)) {
            const QByteArray val = xml.mid(a.valStart, a.valEnd - a.valStart);
            if (val != kWmlTransitional && val != kWmlStrict) continue;
            if (a.name == "xmlns") wml.insert(QByteArray());
            else if (a.name.startsWith("xmlns:")) {
                const QByteArray p2 = a.name.mid(6);
                wml.insert(p2);
                if (attrPfx.isEmpty()) attrPfx = p2;
            }
        }
        if (wml.isEmpty()) { o.error = QStringLiteral("không nhận ra không gian tên WordprocessingML trong word/document.xml"); return false; }
        if (attrPfx.isEmpty() && spec) { o.error = QStringLiteral("tài liệu dùng không gian tên mặc định cho phần tử và không khai báo tiền tố cho thuộc tính: chưa hỗ trợ"); return false; }
        if (attrPfx.isEmpty()) attrPfx = "w";
        o.attrPrefix = attrPfx;
    }
    auto isW = [&](const QByteArray& q) { return wml.contains(prefixOf(q)); };

    QVector<QByteArray> stack;
    QVector<SectCtx> sects;
    int changeDepth = 0;
    int index = 0;

    auto attrLocal = [&](const Attr& a) -> QByteArray {
        const int c = a.name.indexOf(':');
        if (c < 0) return QByteArray();
        return wml.contains(a.name.left(c)) ? a.name.mid(c + 1) : QByteArray();
    };

    auto finalize = [&](SectCtx& c, int closeStart) {
        // Đọc w:pgMar / w:pgSz
        const SectCtx::Child* pg = nullptr;
        const SectCtx::Child* sz = nullptr;
        for (const auto& k : c.kids) {
            if (k.local == "pgMar" && !pg) pg = &k;
            else if (k.local == "pgSz" && !sz) sz = &k;
        }
        SectionMargins sm;
        sm.inChange = c.inChange;
        if (pg) {
            sm.hasPgMar = true;
            for (const Attr& a : parseAttrs(xml, pg->tag)) {
                const QByteArray l = attrLocal(a);
                const qint32 v = parseTwips(xml.mid(a.valStart, a.valEnd - a.valStart));
                if (l == "left") sm.left = v; else if (l == "right") sm.right = v;
                else if (l == "top") sm.top = v; else if (l == "bottom") sm.bottom = v;
                else if (l == "header") sm.header = v; else if (l == "footer") sm.footer = v;
                else if (l == "gutter") sm.gutter = v;
            }
        }
        if (sz) {
            for (const Attr& a : parseAttrs(xml, sz->tag)) {
                const QByteArray l = attrLocal(a);
                const qint32 v = parseTwips(xml.mid(a.valStart, a.valEnd - a.valStart));
                if (l == "w") sm.pageW = v; else if (l == "h") sm.pageH = v;
            }
        }
        if (c.inChange) return;   // bản ghi thay đổi có theo dõi: không bao giờ vá
        sm.index = index++;
        if (c.parentIsBody) o.hasBody = true;
        o.secs.push_back(sm);
        if (!spec) return;

        const QByteArray elemPfx = prefixOf(c.tag.qname);
        const std::optional<qint32> vals[4] = {spec->left, spec->right, spec->top, spec->bottom};
        static const char* names[4] = {"left", "right", "top", "bottom"};
        if (pg) {
            const QVector<Attr> attrs = parseAttrs(xml, pg->tag);
            for (int i = 0; i < 4; ++i) {
                if (!vals[i]) continue;
                const QByteArray v = QByteArray::number(*vals[i]);
                bool replaced = false;
                for (const Attr& a : attrs) {
                    if (attrLocal(a) == names[i]) {
                        o.edits.push_back({a.valStart, a.valEnd - a.valStart, v});
                        replaced = true;
                        break;
                    }
                }
                if (!replaced) {
                    const int at = pg->tag.end - (pg->tag.selfClosing ? 2 : 1);
                    o.edits.push_back({at, 0, " " + attrPfx + ":" + names[i] + "=\"" + v + "\""});
                }
            }
            ++o.patched;
            return;
        }
        // Chưa có w:pgMar: tạo mới
        const QByteArray ins = pgMarXml(elemPfx, attrPfx, *spec);
        ++o.created;
        o.warnings << QStringLiteral("Phần %1 chưa khai báo lề: đã tạo mới (cạnh không đặt dùng mặc định 2,54 cm).").arg(sm.index + 1);
        if (c.tag.selfClosing) {
            // <w:sectPr .../>  →  <w:sectPr ...>pgMar</w:sectPr>
            const int slash = c.tag.end - 2;
            o.edits.push_back({slash, 2, ">" + ins + "</" + c.tag.qname + ">"});
            return;
        }
        int at = closeStart;
        if (sz) {
            at = sz->end >= 0 ? sz->end : sz->tag.end;
        } else {
            for (const auto& k : c.kids) {
                if (afterPgMar().contains(k.local)) { at = k.tag.start; break; }
            }
        }
        o.edits.push_back({at, 0, ins});
    };

    while (nextTag(xml, pos, t, &err)) {
        if (t.closing) {
            if (stack.isEmpty()) { o.error = QStringLiteral("thẻ đóng thừa: ") + QString::fromUtf8(t.qname); return false; }
            const QByteArray nm = stack.takeLast();
            if (nm != t.qname) { o.error = QStringLiteral("XML sai cấu trúc quanh thẻ ") + QString::fromUtf8(t.qname); return false; }
            if (isW(nm)) {
                const QByteArray loc = localName(nm);
                if (loc == "sectPrChange") --changeDepth;
                if (loc == "body" && o.bodyCloseStart < 0) { o.bodyCloseStart = t.start; o.bodyPrefix = prefixOf(nm); }
            }
            if (!sects.isEmpty()) {
                SectCtx& c = sects.last();
                if (stack.size() == c.depth && !c.kids.isEmpty() && c.kids.last().end < 0) {
                    c.kids.last().end = t.end;       // đóng một con trực tiếp của sectPr
                } else if (stack.size() == c.depth - 1) {
                    SectCtx done = sects.takeLast();  // đóng chính sectPr
                    finalize(done, t.start);
                }
            }
            continue;
        }
        const int depthBefore = stack.size();
        const QByteArray loc = localName(t.qname);
        const bool w = isW(t.qname);
        if (w && loc == "sectPr") {
            SectCtx c;
            c.tag = t;
            c.inChange = changeDepth > 0;
            c.parentIsBody = !stack.isEmpty() && isW(stack.last()) && localName(stack.last()) == "body";
            if (t.selfClosing) {
                c.depth = depthBefore + 1;
                finalize(c, t.start);
                continue;
            }
            stack.push_back(t.qname);
            c.depth = stack.size();
            sects.push_back(c);
            continue;
        }
        if (!sects.isEmpty() && depthBefore == sects.last().depth) {
            SectCtx::Child k;
            k.local = w ? loc : QByteArray();
            k.tag = t;
            k.end = t.selfClosing ? t.end : -1;
            sects.last().kids.push_back(k);
        }
        if (!t.selfClosing) {
            stack.push_back(t.qname);
            if (w && loc == "sectPrChange") ++changeDepth;
        }
    }
    if (!err.isEmpty()) { o.error = err; return false; }
    if (!stack.isEmpty()) { o.error = QStringLiteral("XML thiếu thẻ đóng: ") + QString::fromUtf8(stack.last()); return false; }
    return true;
}

QByteArray applyEdits(const QByteArray& xml, QVector<Edit> edits) {
    // Áp dụng từ cuối lên đầu; cùng vị trí thì mục thêm sau áp dụng trước để thứ tự cuối cùng là thứ tự thêm.
    QVector<int> order(edits.size());
    for (int i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        if (edits[a].pos != edits[b].pos) return edits[a].pos > edits[b].pos;
        return a > b;
    });
    QByteArray out = xml;
    for (int i : order) out.replace(edits[i].pos, edits[i].len, edits[i].repl);
    return out;
}

// ------------------------------------------------------------------ ZIP
quint16 u16(const QByteArray& b, int o) { return quint16(uchar(b[o]) | (uchar(b[o + 1]) << 8)); }
quint32 u32(const QByteArray& b, int o) {
    return quint32(uchar(b[o])) | (quint32(uchar(b[o + 1])) << 8) | (quint32(uchar(b[o + 2])) << 16) | (quint32(uchar(b[o + 3])) << 24);
}
void put16(QByteArray& b, quint16 v) { b.append(char(v & 0xFF)); b.append(char(v >> 8)); }
void put32(QByteArray& b, quint32 v) {
    for (int i = 0; i < 4; ++i) b.append(char((v >> (8 * i)) & 0xFF));
}
void set16(QByteArray& b, int o, quint16 v) { b[o] = char(v & 0xFF); b[o + 1] = char(v >> 8); }
void set32(QByteArray& b, int o, quint32 v) { for (int i = 0; i < 4; ++i) b[o + i] = char((v >> (8 * i)) & 0xFF); }

struct CdRec {
    int cdOff = 0, len = 0;      // vị trí/độ dài bản ghi trong bộ đệm thư mục trung tâm
    QString name;
    quint32 localOffset = 0;
};

} // namespace

// ------------------------------------------------------------------ API: đọc/vá XML
ReadResult readDocumentXmlMargins(const QByteArray& xml) {
    ReadResult r;
    ScanOut o;
    if (!scan(xml, nullptr, o)) { r.error = o.error; return r; }
    r.ok = true;
    r.sections = o.secs;
    r.hasBodySectPr = o.hasBody;
    return r;
}

ReadResult readDocxMargins(const QString& docxPath) {
    ReadResult r;
    ft::ZipReader z;
    QString err;
    if (!z.open(docxPath, &err)) { r.error = err; return r; }
    const ft::ZipEntryInfo* e = z.find(QStringLiteral("word/document.xml"));
    if (!e) { r.error = QStringLiteral("không có word/document.xml (không phải DOCX?)"); return r; }
    QByteArray xml;
    if (!z.read(*e, &xml, &err, 256LL * 1024 * 1024)) { r.error = err; return r; }
    return readDocumentXmlMargins(xml);
}

PatchResult patchDocumentXml(const QByteArray& xml, const MarginSpec& spec, QByteArray* out) {
    PatchResult r;
    if (!spec.any()) { r.error = QStringLiteral("không có cạnh lề nào cần đặt"); return r; }
    const std::optional<qint32> all[4] = {spec.left, spec.right, spec.top, spec.bottom};
    for (const auto& v : all)
        if (v && (*v < 0 || *v > kMaxMargin)) {
            r.error = QStringLiteral("giá trị lề %1 twip ngoài khoảng cho phép (0 đến %2)").arg(*v).arg(kMaxMargin);
            return r;
        }
    ScanOut o;
    if (!scan(xml, &spec, o)) { r.error = o.error; return r; }

    // Kiểm tra lề so với khổ giấy của từng phần: còn chỗ cho chữ.
    for (const SectionMargins& s : o.secs) {
        const qint32 l = spec.left ? *spec.left : (s.left >= 0 ? s.left : 1440);
        const qint32 rr = spec.right ? *spec.right : (s.right >= 0 ? s.right : 1440);
        const qint32 tp = spec.top ? *spec.top : (s.top >= 0 ? s.top : 1440);
        const qint32 bt = spec.bottom ? *spec.bottom : (s.bottom >= 0 ? s.bottom : 1440);
        // Gutter orientation may be set in settings.xml. Conservatively reserve it
        // in both axes rather than allow an unusable text area in either layout.
        const qint64 gutter=qMax<qint64>(0,s.gutter);
        if (s.pageW > 0 && qint64(l) + rr + gutter > qint64(s.pageW) - kMinContent) {
            r.error = QStringLiteral("lề trái + phải (%1 + %2 twip) và lề gáy quá lớn so với bề rộng giấy %3 twip ở phần %4")
                          .arg(l).arg(rr).arg(s.pageW).arg(s.index + 1);
            return r;
        }
        if (s.pageH > 0 && qint64(tp) + bt + gutter > qint64(s.pageH) - kMinContent) {
            r.error = QStringLiteral("lề trên + dưới (%1 + %2 twip) và lề gáy quá lớn so với chiều cao giấy %3 twip ở phần %4")
                          .arg(tp).arg(bt).arg(s.pageH).arg(s.index + 1);
            return r;
        }
    }

    if (o.secs.isEmpty()) {
        // Tài liệu không có w:sectPr nào (hợp lệ: Word dùng mặc định). Tạo w:sectPr cuối thân.
        if (o.bodyCloseStart < 0) { r.error = QStringLiteral("không tìm thấy w:body trong word/document.xml"); return r; }
        const QByteArray ins = pgMarXml(o.bodyPrefix, o.attrPrefix, spec);
        const QByteArray sp = o.bodyPrefix.isEmpty() ? QByteArray() : o.bodyPrefix + ":";
        o.edits.push_back({o.bodyCloseStart, 0, "<" + sp + "sectPr>" + ins + "</" + sp + "sectPr>"});
        ++o.created;
        o.warnings << QStringLiteral("Tài liệu chưa khai báo phần (section) nào: đã tạo một phần với lề mới (cạnh không đặt dùng mặc định 2,54 cm).");
    }
    *out = applyEdits(xml, o.edits);
    r.ok = true;
    r.sectionsPatched = o.patched;
    r.sectionsCreated = o.created;
    r.warnings = o.warnings;
    return r;
}

// ------------------------------------------------------------------ API: thay một mục ZIP
bool replaceZipEntry(const QString& src, const QString& dst, const QString& entryName, const QByteArray& newData, QString* error) {
    auto fail = [&](const QString& m) { if (error) *error = m; return false; };
    const QFileInfo destination(dst);
    if(destination.exists() || destination.isSymLink())
        return fail(QStringLiteral("tệp đích đã tồn tại: chọn tên mới"));
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) return fail(QStringLiteral("không mở được tệp nguồn: ") + in.errorString());
    const qint64 size = in.size();
    // --- tìm bản ghi kết thúc thư mục trung tâm
    const qint64 tailLen = qMin<qint64>(size, 65535 + 22);
    if (!in.seek(size - tailLen)) return fail(QStringLiteral("không đọc được đuôi ZIP"));
    const QByteArray tail = in.read(tailLen);
    int eocd = -1;
    for (int i = int(tail.size()) - 22; i >= 0; --i) {
        if (u32(tail, i) == 0x06054b50u && i + 22 + u16(tail, i + 20) == tail.size()) { eocd = i; break; }
    }
    if (eocd < 0) return fail(QStringLiteral("không thấy bản ghi kết thúc ZIP (tệp hỏng?)"));
    if (eocd >= 20 && u32(tail, eocd - 20) == 0x07064b50u) return fail(QStringLiteral("ZIP64 chưa được hỗ trợ"));
    const quint16 count = u16(tail, eocd + 10);
    const quint32 cdSize = u32(tail, eocd + 12);
    const quint32 cdOff = u32(tail, eocd + 16);
    if (count == 0xFFFF || cdSize == 0xFFFFFFFFu || cdOff == 0xFFFFFFFFu) return fail(QStringLiteral("ZIP64 chưa được hỗ trợ"));
    if (u16(tail, eocd + 4) != 0 || u16(tail, eocd + 6) != 0) return fail(QStringLiteral("ZIP nhiều đĩa không được hỗ trợ"));
    const QByteArray comment = tail.mid(eocd + 22);
    if (qint64(cdOff) + cdSize > size) return fail(QStringLiteral("thư mục trung tâm vượt quá tệp"));
    if (!in.seek(cdOff)) return fail(QStringLiteral("không đọc được thư mục trung tâm"));
    QByteArray cd = in.read(cdSize);
    if (quint32(cd.size()) != cdSize) return fail(QStringLiteral("thư mục trung tâm bị cắt"));

    // --- phân tích thư mục trung tâm
    QVector<CdRec> recs;
    int p = 0, target = -1;
    for (int i = 0; i < count; ++i) {
        if (p + 46 > cd.size() || u32(cd, p) != 0x02014b50u) return fail(QStringLiteral("bản ghi thư mục trung tâm hỏng"));
        CdRec r;
        r.cdOff = p;
        const int nl = u16(cd, p + 28), el = u16(cd, p + 30), cl = u16(cd, p + 32);
        r.len = 46 + nl + el + cl;
        if (p + r.len > cd.size()) return fail(QStringLiteral("bản ghi thư mục trung tâm bị cắt"));
        r.name = QString::fromUtf8(cd.mid(p + 46, nl));
        r.localOffset = u32(cd, p + 42);
        if (u32(cd, p + 20) == 0xFFFFFFFFu || u32(cd, p + 24) == 0xFFFFFFFFu || r.localOffset == 0xFFFFFFFFu)
            return fail(QStringLiteral("ZIP64 chưa được hỗ trợ (mục ") + r.name + QLatin1Char(')'));
        if (u16(cd, p + 8) & 1) return fail(QStringLiteral("mục ") + r.name + QStringLiteral(" được mã hoá"));
        if (r.name == entryName) {
            if (target >= 0) return fail(QStringLiteral("có nhiều mục trùng tên ") + entryName);
            target = recs.size();
        }
        if (r.localOffset >= cdOff) return fail(QStringLiteral("vị trí mục ") + r.name + QStringLiteral(" sai"));
        recs.push_back(r);
        p += r.len;
    }
    if (target < 0) return fail(QStringLiteral("không có mục ") + entryName + QStringLiteral(" trong gói"));

    QVector<int> order(recs.size());
    for (int i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return recs[a].localOffset < recs[b].localOffset; });
    for (int i = 1; i < order.size(); ++i)
        if (recs[order[i]].localOffset == recs[order[i - 1]].localOffset) return fail(QStringLiteral("hai mục cùng vị trí (gói bất thường)"));

    // --- nén dữ liệu mới (DEFLATE thô qua qCompress: bỏ 4 byte độ dài, 2 byte zlib đầu, 4 byte Adler đuôi)
    quint16 method = 8;
    QByteArray payload;
    {
        const QByteArray z = qCompress(newData, 6);
        if (z.size() > 10) payload = z.mid(6, z.size() - 10);
        if (payload.isEmpty() || payload.size() >= newData.size()) { method = 0; payload = newData; }
    }
    const quint32 crc = ft::zipCrc32(newData);

    // --- ghi tệp tạm rồi đổi tên
    // QTemporaryFile::close may keep its native handle open on Windows.
    // Use an isolated directory and a regular QFile, which really closes before rename.
    QTemporaryDir staging(QDir(destination.absolutePath()).filePath(QStringLiteral(".DungOffice-zip-XXXXXX")));
    if(!staging.isValid())return fail(QStringLiteral("không tạo được thư mục tạm"));
    const QString tmp=staging.filePath(QStringLiteral("copy.zip"));
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly)) return fail(QStringLiteral("không tạo được tệp tạm: ") + out.errorString());
    auto bail = [&](const QString& m) { out.close(); return fail(m); };
    auto wr = [&](const QByteArray& b) { return out.write(b) == b.size(); };

    QVector<quint32> newOff(recs.size());
    for (int oi = 0; oi < order.size(); ++oi) {
        const int idx = order[oi];
        const CdRec& r = recs[idx];
        newOff[idx] = quint32(out.pos());
        if (idx == target) {
            QByteArray lh;
            put32(lh, 0x04034b50u);
            put16(lh, u16(cd, r.cdOff + 6));                 // version needed
            put16(lh, quint16(u16(cd, r.cdOff + 8) & ~8));   // flags, bỏ bit "data descriptor"
            put16(lh, method);
            put16(lh, u16(cd, r.cdOff + 12));                // time
            put16(lh, u16(cd, r.cdOff + 14));                // date
            put32(lh, crc);
            put32(lh, quint32(payload.size()));
            put32(lh, quint32(newData.size()));
            const int nl = u16(cd, r.cdOff + 28);
            put16(lh, quint16(nl));
            put16(lh, 0);
            lh.append(cd.mid(r.cdOff + 46, nl));
            if (!wr(lh) || !wr(payload)) return bail(QStringLiteral("lỗi ghi tệp đích: ") + out.errorString());
        } else {
            const quint32 endOff = oi + 1 < order.size() ? recs[order[oi + 1]].localOffset : cdOff;
            qint64 left = qint64(endOff) - r.localOffset;
            if (!in.seek(r.localOffset)) return bail(QStringLiteral("không đọc được mục ") + r.name);
            const QByteArray sig = in.read(4);
            if (sig.size() != 4 || u32(sig, 0) != 0x04034b50u) return bail(QStringLiteral("tiêu đề cục bộ của ") + r.name + QStringLiteral(" hỏng"));
            if (!in.seek(r.localOffset)) return bail(QStringLiteral("không đọc được mục ") + r.name);
            while (left > 0) {
                const QByteArray chunk = in.read(qMin<qint64>(left, 1 << 20));
                if (chunk.isEmpty()) return bail(QStringLiteral("mục ") + r.name + QStringLiteral(" bị cắt"));
                if (!wr(chunk)) return bail(QStringLiteral("lỗi ghi tệp đích: ") + out.errorString());
                left -= chunk.size();
            }
        }
    }
    const quint32 newCdOff = quint32(out.pos());
    for (int i = 0; i < recs.size(); ++i) {
        QByteArray rec = cd.mid(recs[i].cdOff, recs[i].len);
        set32(rec, 42, newOff[i]);
        if (i == target) {
            set16(rec, 8, quint16(u16(rec, 8) & ~8));
            set16(rec, 10, method);
            set32(rec, 16, crc);
            set32(rec, 20, quint32(payload.size()));
            set32(rec, 24, quint32(newData.size()));
        }
        if (!wr(rec)) return bail(QStringLiteral("lỗi ghi tệp đích: ") + out.errorString());
    }
    const quint32 newCdSize = quint32(out.pos()) - newCdOff;
    QByteArray eo;
    put32(eo, 0x06054b50u);
    put16(eo, 0); put16(eo, 0);
    put16(eo, count); put16(eo, count);
    put32(eo, newCdSize);
    put32(eo, newCdOff);
    put16(eo, quint16(comment.size()));
    eo.append(comment);
    if (!wr(eo)) return bail(QStringLiteral("lỗi ghi tệp đích: ") + out.errorString());
    if (!out.flush()) return bail(QStringLiteral("lỗi ghi tệp đích: ") + out.errorString());
    out.close();
    // QFile::rename refuses an existing destination, even if created while writing.
    if (!QFile::rename(tmp, dst)) return fail(QStringLiteral("không đặt được tên tệp đích (có thể tên đã tồn tại)"));
    return true;
}

// ------------------------------------------------------------------ API: vá DOCX
PatchResult patchDocxMargins(const QString& src, const QString& dst, const MarginSpec& spec) {
    PatchResult r;
    QString err;
    if (QFileInfo(src).absoluteFilePath() == QFileInfo(dst).absoluteFilePath()) {
        r.error = QStringLiteral("tệp đích trùng tệp nguồn: không bao giờ ghi đè tệp nguồn");
        return r;
    }
    ft::ZipReader zs;
    if (!zs.open(src, &err)) { r.error = err; return r; }
    const ft::ZipEntryInfo* de = zs.find(QStringLiteral("word/document.xml"));
    if (!de) { r.error = QStringLiteral("không có word/document.xml (không phải DOCX?)"); return r; }
    QByteArray xml;
    if (!zs.read(*de, &xml, &err, 256LL * 1024 * 1024)) { r.error = err; return r; }

    QByteArray newXml;
    r = patchDocumentXml(xml, spec, &newXml);
    if (!r.ok) return r;
    r.ok = false;
    if (!replaceZipEntry(src, dst, QStringLiteral("word/document.xml"), newXml, &err)) { r.error = err; return r; }

    // --- tự kiểm bản vừa ghi
    auto bad = [&](const QString& m) { QFile::remove(dst); r.error = QStringLiteral("bản ghi không qua kiểm tra: ") + m; return r; };
    ft::ZipReader zd;
    if (!zd.open(dst, &err)) return bad(err);
    if (zd.entries().size() != zs.entries().size()) return bad(QStringLiteral("số mục khác bản gốc"));
    for (const ft::ZipEntryInfo& e : zs.entries()) {
        const ft::ZipEntryInfo* n = zd.find(e.name);
        if (!n) return bad(QStringLiteral("mất mục ") + e.name);
        QByteArray data;
        if (!zd.read(*n, &data, &err, 256LL * 1024 * 1024)) return bad(err);
        if (e.name == QLatin1String("word/document.xml")) {
            if (data != newXml) return bad(QStringLiteral("word/document.xml đọc lại khác dữ liệu đã ghi"));
        } else if (n->crc != e.crc || n->usize != e.usize || n->csize != e.csize || n->method != e.method) {
            return bad(QStringLiteral("mục ") + e.name + QStringLiteral(" bị thay đổi"));
        }
    }
    const ReadResult back = readDocumentXmlMargins(newXml);
    const ReadResult before = readDocumentXmlMargins(xml);
    if (!back.ok) return bad(back.error);
    if (!before.ok) return bad(before.error);
    if (back.sections.size() != before.sections.size() + (before.sections.isEmpty() ? 1 : 0)) return bad(QStringLiteral("số phần thay đổi"));
    for (const SectionMargins& s : back.sections) {
        if ((spec.left && s.left != *spec.left) || (spec.right && s.right != *spec.right) || (spec.top && s.top != *spec.top) ||
            (spec.bottom && s.bottom != *spec.bottom))
            return bad(QStringLiteral("đọc lại lề của phần %1 không khớp giá trị đã đặt").arg(s.index + 1));
    }
    r.ok = true;
    return r;
}

} // namespace dung::word
