// SPDX-License-Identifier: MPL-2.0
#include "MailMergeCore.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <algorithm>
#include <deque>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

namespace mailmerge {

// =====================================================================================================
// Gói zip (.docx / .xlsx)
// =====================================================================================================
namespace {

struct Package {
    QVector<QPair<QString, QByteArray>> entries;
    int indexOf(const QString &name) const {
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].first == name) return i;
        return -1;
    }
    QByteArray data(const QString &name) const {
        const int i = indexOf(name);
        return i < 0 ? QByteArray() : entries[i].second;
    }
};

bool readPackage(const QString &path, Package &pkg, QString *err, const QString &requiredEntry)
{
    QFileInfo fi(path);
    if (!fi.isFile()) { *err = QObject::tr("Không thấy tệp: %1").arg(path); return false; }
    {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray head = f.read(8);
            if (head.startsWith("\xD0\xCF\x11\xE0")) {
                *err = QObject::tr("Đây là tệp Office định dạng cũ (.doc/.xls) hoặc tệp có mật khẩu, không phải .docx/.xlsx. "
                                   "Hãy mở rồi lưu thành .docx/.xlsx trước (không đặt mật khẩu).");
                return false;
            }
        }
    }
    QZipReader zr(path);
    if (!zr.isReadable()) { *err = QObject::tr("Không đọc được tệp (không phải .docx/.xlsx hợp lệ hoặc tệp hỏng)."); return false; }
    const auto infos = zr.fileInfoList();
    for (const auto &e : infos) {
        if (e.isDir) continue;
        pkg.entries.append({e.filePath, zr.fileData(e.filePath)});
    }
    if (!requiredEntry.isEmpty() && pkg.indexOf(requiredEntry) < 0) {
        *err = QObject::tr("Tệp không đúng định dạng (thiếu %1).").arg(requiredEntry);
        return false;
    }
    return true;
}

// XML tối giản giữ NGUYÊN khoảng trắng (QDom làm rơi các text chỉ có khoảng trắng như <w:t> </w:t> nên không dùng).
struct Node {
    enum Kind { Elem, Text, Other } kind = Elem;
    QString name;                              // tên có tiền tố, ví dụ "w:p"
    QVector<QPair<QString, QString>> attrs;
    QString text;                              // Text: nội dung; Other: chuỗi XML thô (chú thích, PI)
    QVector<Node *> kids;
    Node *parent = nullptr;
};

struct Doc {
    std::deque<Node> arena;
    Node *root = nullptr;
    Node *make(Node::Kind k) { arena.emplace_back(); arena.back().kind = k; return &arena.back(); }
};

bool parseXml(const QByteArray &data, Doc &doc, QString *err)
{
    QXmlStreamReader r(data);
    r.setNamespaceProcessing(false);
    Node *cur = nullptr;
    while (!r.atEnd()) {
        switch (r.readNext()) {
        case QXmlStreamReader::StartElement: {
            Node *n = doc.make(Node::Elem);
            n->name = r.qualifiedName().toString();
            for (const auto &a : r.attributes()) n->attrs.append({a.qualifiedName().toString(), a.value().toString()});
            n->parent = cur;
            if (cur) cur->kids.append(n); else if (!doc.root) doc.root = n;
            cur = n;
            break;
        }
        case QXmlStreamReader::EndElement:
            if (cur) cur = cur->parent;
            break;
        case QXmlStreamReader::Characters:
            if (cur) {
                Node *n = doc.make(Node::Text);
                n->text = r.text().toString();
                n->parent = cur;
                cur->kids.append(n);
            }
            break;
        case QXmlStreamReader::Comment:
            if (cur) { Node *n = doc.make(Node::Other); n->text = QStringLiteral("<!--") + r.text().toString() + QStringLiteral("-->"); n->parent = cur; cur->kids.append(n); }
            break;
        case QXmlStreamReader::ProcessingInstruction:
            if (cur) { Node *n = doc.make(Node::Other);
                n->text = QStringLiteral("<?") + r.processingInstructionTarget().toString() + QLatin1Char(' ') + r.processingInstructionData().toString() + QStringLiteral("?>");
                n->parent = cur; cur->kids.append(n); }
            break;
        default: break;
        }
    }
    if (r.hasError() || !doc.root) {
        *err = QObject::tr("XML lỗi: %1").arg(r.hasError() ? r.errorString() : QStringLiteral("rỗng"));
        return false;
    }
    return true;
}

QString escText(const QString &s)
{
    QString o; o.reserve(s.size() + 8);
    for (QChar c : s) {
        if (c == QLatin1Char('&')) o += QStringLiteral("&amp;");
        else if (c == QLatin1Char('<')) o += QStringLiteral("&lt;");
        else if (c == QLatin1Char('>')) o += QStringLiteral("&gt;");
        else o += c;
    }
    return o;
}
QString escAttr(const QString &s)
{
    QString o; o.reserve(s.size() + 8);
    for (QChar c : s) {
        if (c == QLatin1Char('&')) o += QStringLiteral("&amp;");
        else if (c == QLatin1Char('<')) o += QStringLiteral("&lt;");
        else if (c == QLatin1Char('"')) o += QStringLiteral("&quot;");
        else if (c == QLatin1Char('\t')) o += QStringLiteral("&#9;");
        else if (c == QLatin1Char('\n')) o += QStringLiteral("&#10;");
        else if (c == QLatin1Char('\r')) o += QStringLiteral("&#13;");
        else o += c;
    }
    return o;
}

void writeNode(const Node *n, QString &out)
{
    if (n->kind == Node::Text) { out += escText(n->text); return; }
    if (n->kind == Node::Other) { out += n->text; return; }
    out += QLatin1Char('<'); out += n->name;
    for (const auto &a : n->attrs) { out += QLatin1Char(' '); out += a.first; out += QStringLiteral("=\""); out += escAttr(a.second); out += QLatin1Char('"'); }
    if (n->kids.isEmpty()) { out += QStringLiteral("/>"); return; }
    out += QLatin1Char('>');
    for (const Node *k : n->kids) writeNode(k, out);
    out += QStringLiteral("</"); out += n->name; out += QLatin1Char('>');
}

QByteArray serializeXml(const Doc &doc)
{
    QString s = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    writeNode(doc.root, s);
    return s.toUtf8();
}

bool wellFormed(const QByteArray &data, QString *err)
{
    QXmlStreamReader r(data);
    while (!r.atEnd()) r.readNext();
    if (r.hasError()) { *err = r.errorString(); return false; }
    return true;
}

bool writePackage(const QString &dst, const Package &pkg, QString *err)
{
    if (QFileInfo::exists(dst)) { *err = QObject::tr("Tệp đã tồn tại, không ghi đè: %1").arg(dst); return false; }
    QDir().mkpath(QFileInfo(dst).absolutePath());
    const QString tmp = dst + QStringLiteral(".dang-ghi");
    QFile::remove(tmp);
    {
        QZipWriter zw(tmp);
        if (zw.status() != QZipWriter::NoError) { *err = QObject::tr("Không tạo được tệp: %1").arg(dst); return false; }
        zw.setCompressionPolicy(QZipWriter::AutoCompress);
        for (const auto &e : pkg.entries) zw.addFile(e.first, e.second);
        zw.close();
        if (zw.status() != QZipWriter::NoError) { QFile::remove(tmp); *err = QObject::tr("Ghi tệp thất bại (hết dung lượng hoặc không có quyền ghi): %1").arg(dst); return false; }
    }
    // Đọc lại và kiểm tra từng phần XML trước khi đổi tên thành tệp kết quả.
    Package chk; QString e2;
    if (!readPackage(tmp, chk, &e2, QString())) { QFile::remove(tmp); *err = QObject::tr("Kiểm tra lại tệp vừa ghi thất bại: %1").arg(e2); return false; }
    if (chk.entries.size() != pkg.entries.size()) { QFile::remove(tmp); *err = QObject::tr("Tệp ghi ra thiếu mục so với dự kiến."); return false; }
    for (const auto &e : chk.entries) {
        if (e.first.endsWith(QLatin1String(".xml")) || e.first.endsWith(QLatin1String(".rels"))) {
            QString xe;
            if (!wellFormed(e.second, &xe)) { QFile::remove(tmp); *err = QObject::tr("XML trong %1 không hợp lệ: %2").arg(e.first, xe); return false; }
        }
    }
    if (!QFile::rename(tmp, dst)) { QFile::remove(tmp); *err = QObject::tr("Không đặt được tên tệp kết quả (có thể tệp đã xuất hiện): %1").arg(dst); return false; }
    return true;
}

// =====================================================================================================
// Tìm chữ trong đoạn (w:p) qua nhiều run
// =====================================================================================================
const QString kWNs = QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main");

// Mỗi định dạng (.docx/.pptx/.xlsx) có "phương ngữ" XML riêng: thẻ đoạn, thẻ chữ, thẻ chắn, cách xuống dòng.
enum class Kind { Docx, Pptx, Xlsx };
enum class Brk { Element, Literal, Flatten };  // Element: chèn w:br; Literal: giữ \n trong chữ (xlsx); Flatten: đổi thành dấu cách và cảnh báo (pptx)
struct Dialect {
    Kind kind = Kind::Docx;
    QString nsAttr, nsUri, requiredEntry;
    QSet<QString> para, skip, barrier;
    QString text;
    Brk brk = Brk::Element;
};

const Dialect &dialectFor(Kind k)
{
    static const Dialect docx = [] {
        Dialect d; d.kind = Kind::Docx;
        d.nsAttr = QStringLiteral("xmlns:w"); d.nsUri = kWNs; d.requiredEntry = QStringLiteral("word/document.xml");
        d.para = {QStringLiteral("w:p")}; d.text = QStringLiteral("w:t");
        d.skip = {QStringLiteral("w:p"), QStringLiteral("w:pPr"), QStringLiteral("w:rPr")};
        d.barrier = {QStringLiteral("w:tab"), QStringLiteral("w:br"), QStringLiteral("w:cr"), QStringLiteral("w:drawing"),
            QStringLiteral("w:pict"), QStringLiteral("w:object"), QStringLiteral("w:fldChar"), QStringLiteral("w:instrText"), QStringLiteral("w:sym"),
            QStringLiteral("w:footnoteReference"), QStringLiteral("w:endnoteReference"), QStringLiteral("w:noBreakHyphen"), QStringLiteral("w:ptab"),
            QStringLiteral("w:commentReference"), QStringLiteral("w:delText")};
        d.brk = Brk::Element;
        return d; }();
    static const Dialect pptx = [] {
        Dialect d; d.kind = Kind::Pptx;
        d.nsAttr = QStringLiteral("xmlns:a"); d.nsUri = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/main");
        d.requiredEntry = QStringLiteral("ppt/presentation.xml");
        d.para = {QStringLiteral("a:p")}; d.text = QStringLiteral("a:t");
        d.skip = {QStringLiteral("a:p"), QStringLiteral("a:pPr"), QStringLiteral("a:rPr"), QStringLiteral("a:endParaRPr")};
        // a:fld (số trang, ngày tự cập nhật) và a:br là rào chắn: không thay chữ chạy qua chúng.
        d.barrier = {QStringLiteral("a:br"), QStringLiteral("a:fld")};
        d.brk = Brk::Flatten;
        return d; }();
    static const Dialect xlsx = [] {
        Dialect d; d.kind = Kind::Xlsx;
        d.nsAttr = QStringLiteral("xmlns"); d.nsUri = QStringLiteral("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
        d.requiredEntry = QStringLiteral("xl/workbook.xml");
        d.para = {QStringLiteral("si"), QStringLiteral("is")}; d.text = QStringLiteral("t");
        d.skip = {QStringLiteral("rPh"), QStringLiteral("phoneticPr"), QStringLiteral("rPr")};
        d.brk = Brk::Literal;
        return d; }();
    return k == Kind::Docx ? docx : k == Kind::Pptx ? pptx : xlsx;
}

// Dùng một "phương ngữ" cho cả lần gọi API hiện tại (giao diện gọi tuần tự, không đa luồng).
struct Ctx { const Dialect *d = &dialectFor(Kind::Docx); int flattened = 0; };
Ctx *g_ctx = nullptr;
struct ScopedCtx {
    Ctx c; Ctx *prev;
    explicit ScopedCtx(Kind k) { c.d = &dialectFor(k); prev = g_ctx; g_ctx = &c; }
    ~ScopedCtx() { g_ctx = prev; }
};
const Dialect &D() { static Ctx dflt; return g_ctx ? *g_ctx->d : *dflt.d; }

bool kindOf(const QString &path, Kind *k, QString *err)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("docx")) { *k = Kind::Docx; return true; }
    if (ext == QLatin1String("pptx")) { *k = Kind::Pptx; return true; }
    if (ext == QLatin1String("xlsx")) { *k = Kind::Xlsx; return true; }
    if (ext == QLatin1String("docm") || ext == QLatin1String("pptm") || ext == QLatin1String("xlsm") || ext == QLatin1String("dotm") || ext == QLatin1String("xltm"))
        *err = QObject::tr("Tệp có macro (.%1) chưa hỗ trợ điền hàng loạt. Hãy lưu một bản không macro (.docx/.pptx/.xlsx) làm mẫu; mẫu gốc không bị thay đổi.").arg(ext);
    else
        *err = QObject::tr("Chỉ hỗ trợ mẫu .docx, .pptx hoặc .xlsx (tệp này là .%1).").arg(ext);
    return false;
}

int slideNo(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("(\\d+)\\.xml$"));
    const auto m = re.match(name);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

bool isTextPart(const QString &name)
{
    switch (D().kind) {
    case Kind::Docx: {
        static const QRegularExpression re(QStringLiteral("^word/(document|header\\d*|footer\\d*|footnotes|endnotes)\\.xml$"));
        return re.match(name).hasMatch(); }
    case Kind::Pptx: {
        static const QRegularExpression re(QStringLiteral("^ppt/(slides/slide|notesSlides/notesSlide)\\d+\\.xml$"));
        return re.match(name).hasMatch(); }
    case Kind::Xlsx: {
        static const QRegularExpression re(QStringLiteral("^xl/(sharedStrings|worksheets/sheet\\d+)\\.xml$"));
        return re.match(name).hasMatch(); }
    }
    return false;
}

// Phần dùng cho bản xem trước (không gồm đầu/chân trang, ghi chú).
bool isPreviewPart(const QString &name)
{
    switch (D().kind) {
    case Kind::Docx: return name == QLatin1String("word/document.xml");
    case Kind::Pptx: return name.startsWith(QLatin1String("ppt/slides/"));
    case Kind::Xlsx: return name == QLatin1String("xl/sharedStrings.xml");
    }
    return false;
}

QString partLabel(const QString &name)
{
    if (D().kind == Kind::Pptx) {
        if (name.startsWith(QLatin1String("ppt/notesSlides/"))) return QObject::tr("ghi chú trang chiếu %1").arg(slideNo(name));
        return QObject::tr("trang chiếu %1").arg(slideNo(name));
    }
    if (D().kind == Kind::Xlsx) {
        if (name.contains(QLatin1String("sharedStrings"))) return QObject::tr("ô văn bản");
        return QObject::tr("ô nhập trực tiếp (sheet %1)").arg(slideNo(name));
    }
    if (name == QLatin1String("word/document.xml")) return QObject::tr("thân văn bản");
    if (name == QLatin1String("word/footnotes.xml")) return QObject::tr("chú thích chân trang");
    if (name == QLatin1String("word/endnotes.xml")) return QObject::tr("chú thích cuối");
    static const QRegularExpression h(QStringLiteral("^word/header(\\d*)\\.xml$")), f(QStringLiteral("^word/footer(\\d*)\\.xml$"));
    auto m = h.match(name); if (m.hasMatch()) return QObject::tr("đầu trang %1").arg(m.captured(1).isEmpty() ? QStringLiteral("1") : m.captured(1));
    m = f.match(name); if (m.hasMatch()) return QObject::tr("chân trang %1").arg(m.captured(1).isEmpty() ? QStringLiteral("1") : m.captured(1));
    return name;
}

struct Seg {
    Node *t = nullptr;     // phần tử w:t (null nếu là "rào chắn")
    QString text;
    QString tag;           // với rào chắn: tên phần tử (w:tab, w:br...)
    int start = 0;
    bool barrier = false;
    bool changed = false;
};

QString textOf(const Node *n)
{
    QString s;
    for (const Node *k : n->kids) if (k->kind == Node::Text) s += k->text;
    return s;
}

void setText(Doc &doc, Node *n, const QString &text)
{
    n->kids.clear();
    if (!text.isEmpty()) { Node *t = doc.make(Node::Text); t->text = text; t->parent = n; n->kids.append(t); }
    bool found = false;
    for (auto &a : n->attrs) if (a.first == QLatin1String("xml:space")) { a.second = QStringLiteral("preserve"); found = true; }
    if (!found) n->attrs.append({QStringLiteral("xml:space"), QStringLiteral("preserve")});
}

void collectSegs(Node *parent, QVector<Seg> &segs)
{
    for (Node *e : parent->kids) {
        if (e->kind != Node::Elem) continue;
        const QString &tag = e->name;
        if (D().skip.contains(tag)) continue;
        if (tag == D().text) { Seg s; s.t = e; s.text = textOf(e); segs.append(s); continue; }
        if (D().barrier.contains(tag)) { Seg s; s.barrier = true; s.tag = tag; s.text = QString(QChar(1)); segs.append(s); continue; }
        collectSegs(e, segs);
    }
}

void collectParagraphs(Node *n, QVector<Node *> &out)
{
    if (n->kind != Node::Elem) return;
    if (D().para.contains(n->name)) out.append(n);
    for (Node *k : n->kids) collectParagraphs(k, out);
}

bool inFallback(const Node *n)
{
    for (const Node *p = n->parent; p; p = p->parent) if (p->name == QLatin1String("mc:Fallback")) return true;
    return false;
}

QString cleanValue(QString v)
{
    v.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    v.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    v.replace(QLatin1Char('\t'), QLatin1Char(' '));
    QString o; o.reserve(v.size());
    for (QChar c : v) {
        const ushort u = c.unicode();
        if (u < 0x20 && u != '\n') continue;   // ký tự điều khiển không hợp lệ trong XML
        if (u == 0xFFFE || u == 0xFFFF) continue;
        o += c;
    }
    return o;
}

using Repl = std::function<bool(const QString &full, const QRegularExpressionMatch &, QString *)>;

QString paragraphText(Node *p)
{
    QVector<Seg> segs; collectSegs(p, segs);
    QString full; for (const auto &s : segs) full += s.text;
    return full;
}

// Thay mọi chỗ khớp trong một đoạn. Giá trị đặt vào run chứa ký tự đầu của chỗ khớp (giữ định dạng).
int processParagraph(Doc &doc, Node *p, const QRegularExpression &re, const Repl &fn)
{
    QVector<Seg> segs; collectSegs(p, segs);
    QString full; int pos = 0; bool anyText = false;
    for (auto &s : segs) { s.start = pos; pos += s.text.size(); full += s.text; if (!s.barrier) anyText = true; }
    if (!anyText) return 0;
    struct M { int s, e; QString val; };
    QVector<M> ms;
    auto it = re.globalMatch(full);
    while (it.hasNext()) {
        const auto m = it.next();
        QString v;
        if (fn(full, m, &v)) ms.append({int(m.capturedStart()), int(m.capturedEnd()), v});
    }
    if (ms.isEmpty()) return 0;
    for (int k = ms.size() - 1; k >= 0; --k) {
        const int s = ms[k].s, e = ms[k].e;
        int first = -1;
        for (int i = 0; i < segs.size(); ++i)
            if (!segs[i].barrier && segs[i].start <= s && s < segs[i].start + segs[i].text.size()) { first = i; break; }
        if (first < 0) continue;
        Seg &f = segs[first];
        const int ls = s - f.start;
        const bool endsHere = e <= f.start + f.text.size();
        const QString prefix = f.text.left(ls);
        const QString suffix = endsHere ? f.text.mid(e - f.start) : QString();
        for (int i = first + 1; i < segs.size() && segs[i].start < e; ++i) {
            Seg &g = segs[i];
            if (g.barrier) continue;
            const int cut = qMin(g.text.size(), e - g.start);
            g.text.remove(0, cut);
            g.changed = true;
        }
        QStringList lines = cleanValue(ms[k].val).split(QLatin1Char('\n'));
        if (lines.size() > 1 && D().brk != Brk::Element) {
            if (D().brk == Brk::Flatten && g_ctx) g_ctx->flattened++;
            lines = QStringList{lines.join(D().brk == Brk::Literal ? QStringLiteral("\n") : QStringLiteral(" "))};
        }
        f.text = prefix + lines.first() + (lines.size() == 1 ? suffix : QString());
        f.changed = true;
        if (lines.size() > 1) {
            Node *parent = f.t->parent;
            int at = parent->kids.indexOf(f.t) + 1;
            for (int j = 1; j < lines.size(); ++j) {
                Node *br = doc.make(Node::Elem); br->name = QStringLiteral("w:br"); br->parent = parent;
                Node *t2 = doc.make(Node::Elem); t2->name = QStringLiteral("w:t"); t2->parent = parent;
                setText(doc, t2, lines[j] + (j == lines.size() - 1 ? suffix : QString()));
                parent->kids.insert(at++, br);
                parent->kids.insert(at++, t2);
            }
        }
    }
    for (auto &s : segs) if (s.changed && !s.barrier) setText(doc, s.t, s.text);
    return ms.size();
}

const QRegularExpression &placeholderRe()
{
    static const QRegularExpression re(QString::fromUtf8(
        "\\{\\{\\s*([^{}\\x01]+?)\\s*\\}\\}|\xC2\xAB\\s*([^\xC2\xAB\xC2\xBB\\x01]+?)\\s*\xC2\xBB"));
    return re;
}

QString placeholderName(const QRegularExpressionMatch &m)
{
    QString n = m.captured(1);
    if (n.isEmpty()) n = m.captured(2);
    return n.trimmed();
}

// Áp một phép thay lên các phần văn bản của gói. partFilter quyết định phần nào được xử lý.
bool transformParts(Package &pkg, const QRegularExpression &re, const Repl &fn, const std::function<bool(const QString &)> &partFilter,
                    OpResult &r, int *replaced, QStringList *partsSkipped = nullptr)
{
    int total = 0;
    for (auto &e : pkg.entries) {
        if (!isTextPart(e.first) || !partFilter(e.first)) continue;
        Doc doc; QString err;
        if (!parseXml(e.second, doc, &err)) { r.error = QObject::tr("%1: %2").arg(e.first, err); return false; }
        bool nsOk = false;
        for (const auto &a : doc.root->attrs) if (a.first == D().nsAttr && a.second == D().nsUri) nsOk = true;
        if (!nsOk) { r.warnings << QObject::tr("Bỏ qua %1: tiền tố XML khác thường, không thể xử lý an toàn.").arg(partLabel(e.first)); if (partsSkipped) partsSkipped->append(e.first); continue; }
        QVector<Node *> ps; collectParagraphs(doc.root, ps);
        int n = 0;
        for (Node *p : ps) n += processParagraph(doc, p, re, fn);
        if (n > 0) { e.second = serializeXml(doc); total += n; }
    }
    *replaced = total;
    if (g_ctx && g_ctx->flattened > 0)
        r.warnings << QObject::tr("PowerPoint: %1 giá trị có nhiều dòng đã được nối thành MỘT dòng (cách nhau bằng dấu cách) vì ô chữ không chèn được xuống dòng an toàn.").arg(g_ctx->flattened);
    return true;
}

QHash<QString, QString> normalizeValues(const Values &v)
{
    QHash<QString, QString> h;
    for (auto it = v.begin(); it != v.end(); ++it) h.insert(it.key().trimmed().toLower(), it.value());
    return h;
}

bool hasMergeField(const Package &pkg)
{
    if (D().kind != Kind::Docx) return false;
    for (const auto &e : pkg.entries)
        if (isTextPart(e.first) && e.second.contains("MERGEFIELD")) return true;
    return false;
}

// Chỗ có trường nhưng bộ thay KHÔNG xử lý được: cảnh báo để người dùng không tưởng đã thay hết.
QStringList unreachableWarnings(const Package &pkg)
{
    QStringList w;
    auto hasPh = [](const QByteArray &d) { return d.contains("{{") || d.contains("\xC2\xAB"); };
    if (D().kind == Kind::Pptx) {
        QStringList bad;
        for (const auto &e : pkg.entries)
            if ((e.first.startsWith(QLatin1String("ppt/charts/")) || e.first.startsWith(QLatin1String("ppt/diagrams/")) ||
                 e.first.startsWith(QLatin1String("ppt/slideMasters/")) || e.first.startsWith(QLatin1String("ppt/slideLayouts/"))) &&
                e.first.endsWith(QLatin1String(".xml")) && hasPh(e.second)) bad << e.first;
        if (!bad.isEmpty())
            w << QObject::tr("Có trường nằm trong biểu đồ/SmartArt/mẫu trang cái (%1 phần) — KHÔNG được thay; hãy đưa trường ra ô chữ thường trên trang chiếu.").arg(bad.size());
    } else if (D().kind == Kind::Xlsx) {
        static const QRegularExpression f(QString::fromUtf8("<f[^>]*>[^<]*(\\{\\{|\xC2\xAB)"));
        for (const auto &e : pkg.entries)
            if (e.first.startsWith(QLatin1String("xl/worksheets/sheet")) && f.match(QString::fromUtf8(e.second)).hasMatch()) {
                w << QObject::tr("Có trường nằm trong công thức — KHÔNG được thay. Hãy đặt trường vào ô chữ thường, không phải công thức.");
                break;
            }
        for (const auto &e : pkg.entries)
            if (e.first.startsWith(QLatin1String("xl/charts/")) && hasPh(e.second)) { w << QObject::tr("Có trường nằm trong biểu đồ — KHÔNG được thay."); break; }
    }
    return w;
}

}  // namespace

// =====================================================================================================
// API: quét, tạo trường, thay
// =====================================================================================================
ScanResult scanTemplate(const QString &docxPath)
{
    ScanResult r; Package pkg; QString err; Kind kind;
    if (!kindOf(docxPath, &kind, &err)) { r.error = err; return r; }
    ScopedCtx sc(kind);
    if (!readPackage(docxPath, pkg, &err, D().requiredEntry)) { r.error = err; return r; }
    QMap<QString, FieldInfo> byKey; QStringList order;
    for (const auto &e : pkg.entries) {
        if (!isTextPart(e.first)) continue;
        Doc doc;
        if (!parseXml(e.second, doc, &err)) { r.error = QObject::tr("%1: %2").arg(e.first, err); return r; }
        QVector<Node *> ps; collectParagraphs(doc.root, ps);
        for (Node *p : ps) {
            if (inFallback(p)) continue;  // bản VML dự phòng của hộp văn bản: không đếm hai lần
            const QString full = paragraphText(p);
            auto it = placeholderRe().globalMatch(full);
            while (it.hasNext()) {
                const QString name = placeholderName(it.next());
                if (name.isEmpty()) continue;
                const QString key = name.toLower();
                if (!byKey.contains(key)) { FieldInfo fi; fi.name = name; byKey.insert(key, fi); order << key; }
                FieldInfo &fi = byKey[key];
                fi.count++;
                const QString lab = partLabel(e.first);
                if (!fi.where.contains(lab)) fi.where << lab;
            }
        }
    }
    for (const QString &k : order) r.fields.append(byKey[k]);
    if (hasMergeField(pkg))
        r.warnings << QObject::tr("Tệp có trường trộn thư (MERGEFIELD) của Word. Bản xuất chỉ thay chữ hiển thị; đừng cập nhật trường (F9) trong bản xuất.");
    r.warnings << unreachableWarnings(pkg);
    r.ok = true;
    return r;
}

OpResult makeFieldTemplate(const QString &srcDocx, const QString &dstDocx, const QString &selectedText, const QString &fieldName)
{
    OpResult r;
    const QString sel = selectedText.trimmed();
    const QString name = fieldName.trimmed();
    if (sel.isEmpty()) { r.error = QObject::tr("Chưa có đoạn chữ nào được chọn."); return r; }
    if (sel.contains(QLatin1Char('\n')) || sel.contains(QLatin1Char('\r'))) { r.error = QObject::tr("Chỉ chọn chữ trong MỘT đoạn (không kéo qua nhiều dòng/đoạn)."); return r; }
    if (name.isEmpty() || name.size() > 60 || name.contains(QLatin1Char('{')) || name.contains(QLatin1Char('}')) ||
        name.contains(QChar(0x00AB)) || name.contains(QChar(0x00BB))) {
        r.error = QObject::tr("Tên trường không hợp lệ (để trống, quá 60 ký tự hoặc có ký tự { } « »)."); return r;
    }
    if (QFileInfo(srcDocx).absoluteFilePath() == QFileInfo(dstDocx).absoluteFilePath()) { r.error = QObject::tr("Không được ghi đè mẫu gốc."); return r; }
    Package pkg; QString err; Kind kind;
    if (!kindOf(srcDocx, &kind, &err)) { r.error = err; return r; }
    if (QFileInfo(dstDocx).suffix().toLower() != QFileInfo(srcDocx).suffix().toLower()) { r.error = QObject::tr("Bản mẫu mới phải cùng loại tệp với mẫu gốc."); return r; }
    ScopedCtx sc(kind);
    if (!readPackage(srcDocx, pkg, &err, D().requiredEntry)) { r.error = err; return r; }
    const QRegularExpression re(QRegularExpression::escape(sel));
    int skippedInside = 0;
    const QString repl = QStringLiteral("{{") + name + QStringLiteral("}}");
    Repl fn = [&](const QString &full, const QRegularExpressionMatch &m, QString *out) {
        auto it = placeholderRe().globalMatch(full);
        while (it.hasNext()) {
            const auto pm = it.next();
            if (m.capturedStart() < pm.capturedEnd() && pm.capturedStart() < m.capturedEnd()) { ++skippedInside; return false; }
        }
        *out = repl;
        return true;
    };
    int n = 0;
    if (!transformParts(pkg, re, fn, [](const QString &) { return true; }, r, &n)) return r;
    if (n == 0) {
        r.error = skippedInside ? QObject::tr("Đoạn chữ chỉ nằm bên trong trường đã có, không đổi gì.")
                                : QObject::tr("Không tìm thấy đoạn chữ này trong tệp (kiểm tra gõ đúng, kể cả dấu).");
        return r;
    }
    if (n > 1) r.warnings << QObject::tr("Đoạn chữ xuất hiện %1 lần; tất cả đã thành trường «%2».").arg(n).arg(name);
    if (skippedInside) r.warnings << QObject::tr("Bỏ qua %1 chỗ nằm bên trong trường đã có.").arg(skippedInside);
    if (!writePackage(dstDocx, pkg, &err)) { r.error = err; return r; }
    r.ok = true; r.replaced = n; r.outPath = dstDocx;
    return r;
}

namespace {
struct RenderCtx {
    QHash<QString, QString> norm;
    QSet<QString> missing;
    QStringList missingOrder;
};
Repl makeRenderRepl(RenderCtx &ctx)
{
    return [&ctx](const QString &, const QRegularExpressionMatch &m, QString *out) {
        const QString name = placeholderName(m);
        const QString key = name.toLower();
        auto it = ctx.norm.constFind(key);
        if (it == ctx.norm.constEnd()) {
            if (!ctx.missing.contains(key)) { ctx.missing.insert(key); ctx.missingOrder << name; }
            return false;
        }
        *out = it.value();
        return true;
    };
}
}  // namespace

OpResult renderOne(const QString &srcDocx, const QString &dstDocx, const Values &values)
{
    OpResult r;
    if (QFileInfo(srcDocx).absoluteFilePath() == QFileInfo(dstDocx).absoluteFilePath()) { r.error = QObject::tr("Không được ghi đè mẫu gốc."); return r; }
    Package pkg; QString err; Kind kind;
    if (!kindOf(srcDocx, &kind, &err)) { r.error = err; return r; }
    if (QFileInfo(dstDocx).suffix().toLower() != QFileInfo(srcDocx).suffix().toLower()) { r.error = QObject::tr("Tệp xuất phải cùng loại với mẫu."); return r; }
    ScopedCtx sc(kind);
    if (!readPackage(srcDocx, pkg, &err, D().requiredEntry)) { r.error = err; return r; }
    RenderCtx ctx; ctx.norm = normalizeValues(values);
    int n = 0;
    if (!transformParts(pkg, placeholderRe(), makeRenderRepl(ctx), [](const QString &) { return true; }, r, &n)) return r;
    r.warnings << unreachableWarnings(pkg);
    if (!ctx.missingOrder.isEmpty())
        r.warnings << QObject::tr("Trường chưa có dữ liệu, giữ nguyên chỗ giữ chữ: %1").arg(ctx.missingOrder.join(QStringLiteral(", ")));
    if (hasMergeField(pkg)) r.warnings << QObject::tr("Tệp có MERGEFIELD của Word: đừng cập nhật trường (F9) trong bản xuất.");
    if (!writePackage(dstDocx, pkg, &err)) { r.error = err; return r; }
    r.ok = true; r.replaced = n; r.outPath = dstDocx;
    return r;
}

QString previewText(const QString &srcDocx, const Values &values, int maxChars, QString *error)
{
    Package pkg; QString err; Kind kind;
    if (!kindOf(srcDocx, &kind, &err)) { if (error) *error = err; return QString(); }
    ScopedCtx sc(kind);
    if (!readPackage(srcDocx, pkg, &err, D().requiredEntry)) { if (error) *error = err; return QString(); }
    OpResult r; RenderCtx ctx; ctx.norm = normalizeValues(values);
    int n = 0;
    if (!transformParts(pkg, placeholderRe(), makeRenderRepl(ctx), [](const QString &name) { return isPreviewPart(name); }, r, &n)) {
        if (error) *error = r.error; return QString();
    }
    QStringList parts;
    for (const auto &e : pkg.entries) if (isTextPart(e.first) && isPreviewPart(e.first)) parts << e.first;
    std::sort(parts.begin(), parts.end(), [](const QString &a, const QString &b) { return slideNo(a) != slideNo(b) ? slideNo(a) < slideNo(b) : a < b; });
    QString out;
    for (const QString &part : parts) {
        Doc doc;
        if (!parseXml(pkg.data(part), doc, &err)) { if (error) *error = err; return QString(); }
        if (D().kind == Kind::Pptx) out += QObject::tr("— Trang chiếu %1 —\n").arg(slideNo(part));
        QVector<Node *> ps; collectParagraphs(doc.root, ps);
        for (Node *p : ps) {
            if (inFallback(p)) continue;
            QVector<Seg> segs; collectSegs(p, segs);
            QString line;
            for (const auto &s : segs) {
                if (!s.barrier) line += s.text;
                else if (s.tag == QLatin1String("w:tab")) line += QLatin1Char('\t');
                else if (s.tag == QLatin1String("w:br") || s.tag == QLatin1String("w:cr") || s.tag == QLatin1String("a:br")) line += QLatin1Char('\n');
            }
            if (D().kind != Kind::Docx && line.isEmpty()) continue;
            out += line; out += QLatin1Char('\n');
            if (out.size() >= maxChars) break;
        }
        if (out.size() >= maxChars) break;
    }
    if (out.size() > maxChars) out = out.left(maxChars) + QStringLiteral("…");
    return out;
}

namespace {
Node *cloneNode(Doc &doc, const Node *src, Node *parent)
{
    Node *n = doc.make(src->kind);
    n->name = src->name; n->attrs = src->attrs; n->text = src->text; n->parent = parent;
    for (const Node *k : src->kids) n->kids.append(cloneNode(doc, k, n));
    return n;
}
void renumber(Node *n, int copy)
{
    if (n->kind != Node::Elem) return;
    if (copy > 0) {
        const bool docPr = n->name == QLatin1String("wp:docPr");
        const bool bm = n->name == QLatin1String("w:bookmarkStart") || n->name == QLatin1String("w:bookmarkEnd");
        for (int i = n->attrs.size() - 1; i >= 0; --i) {
            auto &a = n->attrs[i];
            if (a.first == QLatin1String("w14:paraId") || a.first == QLatin1String("w14:textId")) { n->attrs.removeAt(i); continue; }
            if ((docPr && a.first == QLatin1String("id")) || (bm && a.first == QLatin1String("w:id"))) {
                bool ok = false; const qlonglong v = a.second.toLongLong(&ok);
                if (ok) a.second = QString::number(v + qlonglong(copy) * 100000);
            }
            if (n->name == QLatin1String("w:bookmarkStart") && a.first == QLatin1String("w:name")) a.second += QStringLiteral("_%1").arg(copy + 1);
        }
    }
    for (Node *k : n->kids) renumber(k, copy);
}
Node *findChild(Node *n, const QString &name)
{
    for (Node *k : n->kids) if (k->kind == Node::Elem && k->name == name) return k;
    return nullptr;
}
}  // namespace

OpResult renderMerged(const QString &srcDocx, const QString &dstDocx, const QVector<Values> &records)
{
    OpResult r;
    if (records.isEmpty()) { r.error = QObject::tr("Không có bản ghi nào."); return r; }
    { Kind k; QString e0; if (!kindOf(srcDocx, &k, &e0)) { r.error = e0; return r; }
      if (k != Kind::Docx) { r.error = QObject::tr("Gộp mọi người vào MỘT tệp chỉ hỗ trợ mẫu Word (.docx). Với PowerPoint/Excel hãy xuất mỗi người một tệp."); return r; } }
    ScopedCtx sc(Kind::Docx);
    if (records.size() > 2000) { r.error = QObject::tr("Quá 2000 bản ghi cho một tệp gộp. Hãy xuất mỗi người một tệp hoặc chia nhỏ danh sách."); return r; }
    if (QFileInfo(srcDocx).absoluteFilePath() == QFileInfo(dstDocx).absoluteFilePath()) { r.error = QObject::tr("Không được ghi đè mẫu gốc."); return r; }
    Package pkg; QString err;
    if (!readPackage(srcDocx, pkg, &err, D().requiredEntry)) { r.error = err; return r; }

    // Đầu/chân trang và chú thích dùng chung cho cả tệp gộp: KHÔNG thay, chỉ cảnh báo nếu có trường.
    QStringList sharedWithFields;
    for (const auto &e : pkg.entries) {
        if (!isTextPart(e.first) || e.first == QLatin1String("word/document.xml")) continue;
        Doc d; if (!parseXml(e.second, d, &err)) { r.error = QObject::tr("%1: %2").arg(e.first, err); return r; }
        QVector<Node *> ps; collectParagraphs(d.root, ps);
        for (Node *p : ps) if (placeholderRe().match(paragraphText(p)).hasMatch()) { sharedWithFields << partLabel(e.first); break; }
    }
    if (!sharedWithFields.isEmpty())
        r.warnings << QObject::tr("Trường nằm trong %1 dùng chung cho cả tệp gộp nên KHÔNG được thay (vẫn hiện chỗ giữ chữ). "
                                  "Hãy xuất mỗi người một tệp nếu cần thay ở đó.").arg(sharedWithFields.join(QStringLiteral(", ")));

    const int di = pkg.indexOf(QStringLiteral("word/document.xml"));
    Doc doc;
    if (!parseXml(pkg.entries[di].second, doc, &err)) { r.error = err; return r; }
    bool nsOk = false;
    for (const auto &a : doc.root->attrs) if (a.first == QLatin1String("xmlns:w") && a.second == kWNs) nsOk = true;
    if (!nsOk) { r.error = QObject::tr("Tiền tố XML của tệp khác thường, không gộp được an toàn."); return r; }
    Node *body = findChild(doc.root, QStringLiteral("w:body"));
    if (!body) { r.error = QObject::tr("Tệp không có thân văn bản."); return r; }

    QVector<Node *> kids;
    Node *sect = nullptr;
    for (Node *k : body->kids) if (k->kind == Node::Elem) kids.append(k);
    if (!kids.isEmpty() && kids.last()->name == QLatin1String("w:sectPr")) { sect = kids.takeLast(); }

    QVector<Node *> out;
    QSet<QString> missingKeys; QStringList missingOrder;
    int total = 0;
    for (int i = 0; i < records.size(); ++i) {
        RenderCtx ctx; ctx.norm = normalizeValues(records[i]);
        const Repl fn = makeRenderRepl(ctx);
        QVector<Node *> copy;
        for (Node *k : kids) copy.append(cloneNode(doc, k, body));
        for (Node *c : copy) {
            QVector<Node *> ps; collectParagraphs(c, ps);
            for (Node *p : ps) total += processParagraph(doc, p, placeholderRe(), fn);
            renumber(c, i);
        }
        for (const QString &n : ctx.missingOrder) if (!missingKeys.contains(n.toLower())) { missingKeys.insert(n.toLower()); missingOrder << n; }
        if (i > 0 && !copy.isEmpty()) {
            Node *first = copy.first();
            bool done = false;
            if (first->name == QLatin1String("w:p")) {
                Node *ppr = findChild(first, QStringLiteral("w:pPr"));
                if (!ppr) {
                    ppr = doc.make(Node::Elem); ppr->name = QStringLiteral("w:pPr"); ppr->parent = first;
                    first->kids.prepend(ppr);
                }
                if (!findChild(ppr, QStringLiteral("w:pageBreakBefore"))) {
                    int at = 0;
                    while (at < ppr->kids.size() && ppr->kids[at]->kind == Node::Elem &&
                           (ppr->kids[at]->name == QLatin1String("w:pStyle") || ppr->kids[at]->name == QLatin1String("w:keepNext") ||
                            ppr->kids[at]->name == QLatin1String("w:keepLines"))) ++at;
                    Node *pb = doc.make(Node::Elem); pb->name = QStringLiteral("w:pageBreakBefore"); pb->parent = ppr;
                    ppr->kids.insert(at, pb);
                }
                done = true;
            }
            if (!done) {
                Node *p = doc.make(Node::Elem); p->name = QStringLiteral("w:p"); p->parent = body;
                Node *run = doc.make(Node::Elem); run->name = QStringLiteral("w:r"); run->parent = p; p->kids.append(run);
                Node *br = doc.make(Node::Elem); br->name = QStringLiteral("w:br"); br->parent = run; run->kids.append(br);
                br->attrs.append({QStringLiteral("w:type"), QStringLiteral("page")});
                out.append(p);
            }
        }
        out += copy;
    }
    body->kids.clear();
    for (Node *k : out) { k->parent = body; body->kids.append(k); }
    if (sect) { sect->parent = body; body->kids.append(sect); }
    pkg.entries[di].second = serializeXml(doc);
    if (!missingOrder.isEmpty())
        r.warnings << QObject::tr("Trường chưa có dữ liệu, giữ nguyên chỗ giữ chữ: %1").arg(missingOrder.join(QStringLiteral(", ")));
    if (hasMergeField(pkg)) r.warnings << QObject::tr("Tệp có MERGEFIELD của Word: đừng cập nhật trường (F9) trong bản xuất.");
    if (!writePackage(dstDocx, pkg, &err)) { r.error = err; return r; }
    r.ok = true; r.replaced = total; r.outPath = dstDocx;
    return r;
}

// =====================================================================================================
// Dữ liệu nguồn: dán / CSV / xlsx
// =====================================================================================================
namespace {

int countOutsideQuotes(const QString &line, QChar d)
{
    int n = 0; bool q = false;
    for (QChar c : line) { if (c == QLatin1Char('"')) q = !q; else if (!q && c == d) ++n; }
    return n;
}

QVector<QStringList> splitRecords(const QString &text, QChar d, bool none)
{
    QVector<QStringList> rows;
    QStringList row; QString cell; bool q = false, any = false;
    auto endCell = [&]() { row << cell; cell.clear(); };
    auto endRow = [&]() { endCell(); rows.append(row); row.clear(); any = false; };
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (q) {
            if (c == QLatin1Char('"')) { if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) { cell += c; ++i; } else q = false; }
            else cell += c;
            continue;
        }
        if (!none && c == QLatin1Char('"') && cell.isEmpty()) { q = true; any = true; continue; }
        if (c == QLatin1Char('\r')) { if (i + 1 < text.size() && text[i + 1] == QLatin1Char('\n')) ++i; endRow(); continue; }
        if (c == QLatin1Char('\n')) { endRow(); continue; }
        if (!none && c == d) { endCell(); any = true; continue; }
        cell += c; any = true;
    }
    if (any || !cell.isEmpty() || !row.isEmpty()) endRow();
    return rows;
}

Table finishTable(QVector<QStringList> rows, bool firstRowIsHeader, int *droppedBlank)
{
    Table t;
    // bỏ dòng trống hoàn toàn
    QVector<QStringList> kept; int dropped = 0;
    for (auto &r : rows) {
        bool empty = true; for (const QString &c : r) if (!c.trimmed().isEmpty()) { empty = false; break; }
        if (empty) ++dropped; else kept.append(r);
    }
    if (droppedBlank) *droppedBlank = dropped;
    if (kept.isEmpty()) { t.error = QObject::tr("Không có dữ liệu."); return t; }
    int cols = 0; for (const auto &r : kept) cols = qMax(cols, int(r.size()));
    int ragged = 0;
    for (auto &r : kept) { if (r.size() != cols) ++ragged; while (r.size() < cols) r << QString(); }
    if (ragged) t.warnings << QObject::tr("%1 dòng có số cột khác nhau; các ô thiếu được coi là trống.").arg(ragged);
    if (dropped) t.warnings << QObject::tr("Đã bỏ qua %1 dòng trống.").arg(dropped);
    if (firstRowIsHeader) {
        t.header = kept.takeFirst();
        for (int i = 0; i < t.header.size(); ++i) {
            t.header[i] = t.header[i].trimmed();
            if (t.header[i].isEmpty()) t.header[i] = QObject::tr("Cột %1").arg(i + 1);
        }
        t.firstDataRow = 2;
    } else {
        if (cols == 1) t.header << QObject::tr("Giá trị");
        else for (int i = 0; i < cols; ++i) t.header << QObject::tr("Cột %1").arg(i + 1);
        t.firstDataRow = 1;
    }
    QSet<QString> seen;
    for (const QString &h : t.header) { const QString k = h.toLower(); if (seen.contains(k)) { t.warnings << QObject::tr("Tên cột trùng nhau: «%1».").arg(h); break; } seen.insert(k); }
    t.rows = kept;
    if (t.rows.isEmpty()) t.warnings << QObject::tr("Chỉ có dòng tiêu đề, chưa có dòng dữ liệu.");
    return t;
}

}  // namespace

Table parsePasted(const QString &textIn, Delim delim, bool firstRowIsHeader)
{
    QString text = textIn;
    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);
    QChar d = QLatin1Char('\t'); bool none = false;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\r\n|\r|\n")));
    QStringList nonBlank; for (const QString &l : lines) if (!l.trimmed().isEmpty()) nonBlank << l;
    auto allSame = [&](QChar c) {
        if (nonBlank.isEmpty()) return false;
        const int n0 = countOutsideQuotes(nonBlank.first(), c);
        if (n0 == 0) return false;
        for (const QString &l : nonBlank) if (countOutsideQuotes(l, c) != n0) return false;
        return true;
    };
    switch (delim) {
    case Delim::Tab: d = QLatin1Char('\t'); break;
    case Delim::Semicolon: d = QLatin1Char(';'); break;
    case Delim::Comma: d = QLatin1Char(','); break;
    case Delim::None: none = true; break;
    case Delim::Auto:
        if (allSame(QLatin1Char('\t'))) d = QLatin1Char('\t');
        else if (allSame(QLatin1Char(';'))) d = QLatin1Char(';');
        else if (allSame(QLatin1Char(','))) d = QLatin1Char(',');
        else none = true;
        break;
    }
    int dropped = 0;
    Table t = finishTable(splitRecords(text, d, none), firstRowIsHeader, &dropped);
    return t;
}

Table readCsvFile(const QString &path, bool firstRowIsHeader)
{
    Table t;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { t.error = QObject::tr("Không mở được tệp: %1").arg(path); return t; }
    const QByteArray raw = f.readAll();
    QString text;
    if (raw.startsWith("\xEF\xBB\xBF")) {
        text = QString::fromUtf8(raw.mid(3));
    } else if (raw.startsWith("\xFF\xFE") || raw.startsWith("\xFE\xFF")) {
        const bool le = raw.startsWith("\xFF\xFE");
        QString s; s.reserve(raw.size() / 2);
        for (int i = 2; i + 1 < raw.size(); i += 2) {
            const uchar a = uchar(raw[i]), b = uchar(raw[i + 1]);
            s += QChar(le ? ushort(a | (b << 8)) : ushort((a << 8) | b));
        }
        text = s;
    } else {
        QByteArray probe = raw;
        const QString dec = QString::fromUtf8(probe);
        if (dec.contains(QChar(0xFFFD)) && !raw.contains("\xEF\xBF\xBD")) {
            t.error = QObject::tr("Tệp CSV không phải UTF-8 (có thể là bảng mã Windows cũ). Hãy dùng \"Tiện ích bảng\" để xem trước bảng mã và lưu lại UTF-8, rồi chọn lại.");
            return t;
        }
        text = dec;
    }
    const int nl = text.indexOf(QLatin1Char('\n'));
    const QString first = text.left(nl < 0 ? text.size() : nl);
    QChar d = QLatin1Char(','); int best = 0; bool none = true;
    for (QChar c : {QLatin1Char(','), QLatin1Char(';'), QLatin1Char('\t')}) { const int n = countOutsideQuotes(first, c); if (n > best) { best = n; d = c; none = false; } }
    int dropped = 0;
    t = finishTable(splitRecords(text, d, none), firstRowIsHeader, &dropped);
    return t;
}

namespace {

struct XlsxStyles { QVector<int> xfNumFmt; QHash<int, QString> custom; };

bool isDateFmt(int id, const XlsxStyles &st)
{
    static const QSet<int> builtin{14, 15, 16, 17, 22, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 50, 51, 52, 53, 54, 55, 56, 57, 58};
    if (builtin.contains(id)) return true;
    auto it = st.custom.constFind(id);
    if (it == st.custom.constEnd()) return false;
    QString code = it.value();
    code.remove(QRegularExpression(QStringLiteral("\"[^\"]*\"")));
    code.remove(QRegularExpression(QStringLiteral("\\[[^\\]]*\\]")));
    code.remove(QRegularExpression(QStringLiteral("\\\\.")));
    const QString l = code.toLower();
    return l.contains(QLatin1Char('d')) || l.contains(QLatin1Char('y'));
}

QString colRefToIndexString(const QString &ref, int *col)
{
    int c = 0, i = 0;
    for (; i < ref.size() && ref[i].isLetter(); ++i) c = c * 26 + (ref[i].toUpper().unicode() - 'A' + 1);
    *col = c - 1;
    return ref.mid(i);
}

}  // namespace

QStringList xlsxSheetNames(const QString &path, QString *error)
{
    Package pkg; QString err;
    if (!readPackage(path, pkg, &err, QStringLiteral("xl/workbook.xml"))) { if (error) *error = err; return {}; }
    QStringList names;
    QXmlStreamReader r(pkg.data(QStringLiteral("xl/workbook.xml")));
    while (!r.atEnd()) { if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("sheet")) names << r.attributes().value(QLatin1String("name")).toString(); }
    if (r.hasError()) { if (error) *error = r.errorString(); return {}; }
    return names;
}

Table readXlsxFile(const QString &path, int sheetIndex, bool firstRowIsHeader)
{
    Table t; Package pkg; QString err;
    if (!readPackage(path, pkg, &err, QStringLiteral("xl/workbook.xml"))) { t.error = err; return t; }
    // workbook.xml → danh sách sheet (r:id)
    QStringList rids;
    {
        QXmlStreamReader r(pkg.data(QStringLiteral("xl/workbook.xml")));
        while (!r.atEnd()) {
            if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("sheet"))
                rids << r.attributes().value(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"), QStringLiteral("id")).toString();
        }
    }
    if (sheetIndex < 0 || sheetIndex >= rids.size()) { t.error = QObject::tr("Không có sheet số %1.").arg(sheetIndex + 1); return t; }
    QString target;
    {
        QXmlStreamReader r(pkg.data(QStringLiteral("xl/_rels/workbook.xml.rels")));
        while (!r.atEnd()) {
            if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("Relationship") &&
                r.attributes().value(QLatin1String("Id")).toString() == rids[sheetIndex])
                target = r.attributes().value(QLatin1String("Target")).toString();
        }
    }
    if (target.isEmpty()) { t.error = QObject::tr("Không tìm thấy dữ liệu của sheet."); return t; }
    target = target.startsWith(QLatin1Char('/')) ? target.mid(1) : QStringLiteral("xl/") + target;
    if (pkg.indexOf(target) < 0) { t.error = QObject::tr("Thiếu phần sheet trong tệp."); return t; }

    // shared strings
    QStringList shared;
    if (pkg.indexOf(QStringLiteral("xl/sharedStrings.xml")) >= 0) {
        QXmlStreamReader r(pkg.data(QStringLiteral("xl/sharedStrings.xml")));
        QString cur; bool inSi = false; int phDepth = 0;
        while (!r.atEnd()) {
            const auto tok = r.readNext();
            if (tok == QXmlStreamReader::StartElement) {
                if (r.name() == QLatin1String("si")) { inSi = true; cur.clear(); phDepth = 0; }
                else if (r.name() == QLatin1String("rPh")) ++phDepth;
                else if (r.name() == QLatin1String("t") && inSi && phDepth == 0) cur += r.readElementText();
            } else if (tok == QXmlStreamReader::EndElement) {
                if (r.name() == QLatin1String("rPh")) --phDepth;
                else if (r.name() == QLatin1String("si")) { shared << cur; inSi = false; }
            }
        }
    }
    // styles → ô nào là ngày
    XlsxStyles st;
    if (pkg.indexOf(QStringLiteral("xl/styles.xml")) >= 0) {
        QXmlStreamReader r(pkg.data(QStringLiteral("xl/styles.xml")));
        bool inCellXfs = false;
        while (!r.atEnd()) {
            const auto tok = r.readNext();
            if (tok == QXmlStreamReader::StartElement) {
                if (r.name() == QLatin1String("numFmt")) st.custom.insert(r.attributes().value(QLatin1String("numFmtId")).toInt(), r.attributes().value(QLatin1String("formatCode")).toString());
                else if (r.name() == QLatin1String("cellXfs")) inCellXfs = true;
                else if (inCellXfs && r.name() == QLatin1String("xf")) st.xfNumFmt << r.attributes().value(QLatin1String("numFmtId")).toInt();
            } else if (tok == QXmlStreamReader::EndElement && r.name() == QLatin1String("cellXfs")) inCellXfs = false;
        }
    }

    QMap<int, QMap<int, QString>> cells;
    int dateCells = 0, formattedNumbers = 0, errorCells = 0, rowCount = 0;
    const int kMaxRows = 50000;
    {
        QXmlStreamReader r(pkg.data(target));
        int rowNum = 0;
        while (!r.atEnd()) {
            if (r.readNext() != QXmlStreamReader::StartElement) continue;
            if (r.name() == QLatin1String("row")) {
                rowNum = r.attributes().hasAttribute(QLatin1String("r")) ? r.attributes().value(QLatin1String("r")).toInt() - 1 : rowNum + 1;
                if (++rowCount > kMaxRows) { t.warnings << QObject::tr("Chỉ đọc %1 dòng đầu của sheet.").arg(kMaxRows); break; }
            } else if (r.name() == QLatin1String("c")) {
                const QString ref = r.attributes().value(QLatin1String("r")).toString();
                const QString type = r.attributes().value(QLatin1String("t")).toString();
                const int sIdx = r.attributes().value(QLatin1String("s")).toInt();
                int col = 0; colRefToIndexString(ref, &col);
                QString v; bool hasV = false;
                while (!r.atEnd()) {
                    const auto tok = r.readNext();
                    if (tok == QXmlStreamReader::EndElement && r.name() == QLatin1String("c")) break;
                    if (tok != QXmlStreamReader::StartElement) continue;
                    if (r.name() == QLatin1String("v")) { v = r.readElementText(); hasV = true; }
                    else if (r.name() == QLatin1String("is")) {
                        int depth = 1;
                        while (!r.atEnd() && depth > 0) {
                            const auto t2 = r.readNext();
                            if (t2 == QXmlStreamReader::StartElement) { if (r.name() == QLatin1String("t")) { v += r.readElementText(); hasV = true; } else ++depth; }
                            else if (t2 == QXmlStreamReader::EndElement) --depth;
                        }
                    }
                }
                if (!hasV) continue;
                QString out;
                if (type == QLatin1String("s")) { const int i = v.toInt(); out = (i >= 0 && i < shared.size()) ? shared[i] : QString(); }
                else if (type == QLatin1String("b")) out = (v == QLatin1String("1")) ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
                else if (type == QLatin1String("e")) { out = v; ++errorCells; }
                else if (type == QLatin1String("str") || type == QLatin1String("inlineStr")) out = v;
                else {
                    out = v;
                    const int fmt = (sIdx >= 0 && sIdx < st.xfNumFmt.size()) ? st.xfNumFmt[sIdx] : 0;
                    bool ok = false; const double dv = v.toDouble(&ok);
                    if (ok && fmt != 0 && isDateFmt(fmt, st) && dv >= 1) {
                        const QDate d = QDate(1899, 12, 30).addDays(int(dv));
                        const int mins = int((dv - int(dv)) * 1440.0 + 0.5);
                        out = d.toString(QStringLiteral("dd/MM/yyyy"));
                        if (mins > 0) out += QStringLiteral(" %1:%2").arg(mins / 60, 2, 10, QLatin1Char('0')).arg(mins % 60, 2, 10, QLatin1Char('0'));
                        ++dateCells;
                    } else if (ok && fmt != 0) {
                        ++formattedNumbers;
                    }
                }
                cells[rowNum][col] = out;
            }
        }
        if (r.hasError()) { t.error = QObject::tr("Đọc sheet lỗi: %1").arg(r.errorString()); return t; }
    }
    if (cells.isEmpty()) { t.error = QObject::tr("Sheet không có dữ liệu."); return t; }
    int maxCol = 0; for (auto it = cells.begin(); it != cells.end(); ++it) maxCol = qMax(maxCol, it.value().isEmpty() ? 0 : it.value().lastKey());
    QVector<QStringList> rows;
    const int firstRow = cells.firstKey();
    for (auto it = cells.begin(); it != cells.end(); ++it) {
        // giữ nguyên khoảng trống giữa các dòng có dữ liệu: dòng trống hoàn toàn sẽ bị finishTable bỏ và báo
        QStringList row; for (int c = 0; c <= maxCol; ++c) row << it.value().value(c);
        rows.append(row);
    }
    int dropped = 0;
    Table res = finishTable(rows, firstRowIsHeader, &dropped);
    res.firstDataRow = firstRow + (firstRowIsHeader ? 2 : 1);
    if (dateCells) res.warnings << QObject::tr("%1 ô ngày được hiển thị dạng dd/mm/yyyy (Excel lưu ngày dạng số).").arg(dateCells);
    if (formattedNumbers) res.warnings << QObject::tr("%1 ô số được lấy theo giá trị gốc, không theo định dạng hiển thị (ví dụ số thập phân, phần trăm, tiền). Hãy đổi cột sang dạng văn bản trong Excel nếu cần đúng như đang hiện.").arg(formattedNumbers);
    if (errorCells) res.warnings << QObject::tr("%1 ô đang báo lỗi công thức (#N/A, #REF!...).").arg(errorCells);
    return res;
}

// =====================================================================================================
// Ánh xạ, kiểm tra, tạo bản ghi
// =====================================================================================================
Mapping autoMap(const QVector<FieldInfo> &fields, const Table &t)
{
    Mapping m;
    for (const auto &f : fields) {
        int col = -1;
        for (int i = 0; i < t.header.size(); ++i)
            if (t.header[i].trimmed().toLower() == f.name.trimmed().toLower()) { col = i; break; }
        m.insert(f.name, col);
    }
    if (fields.size() == 1 && t.header.size() == 1) m[fields.first().name] = 0;
    return m;
}

namespace {
QString rowList(const QVector<int> &rows, int firstDataRow)
{
    QStringList s;
    for (int i = 0; i < rows.size() && i < 8; ++i) s << QString::number(rows[i] + firstDataRow);
    QString o = s.join(QStringLiteral(", "));
    if (rows.size() > 8) o += QObject::tr(" … (+%1 dòng nữa)").arg(rows.size() - 8);
    return o;
}
}  // namespace

Check validate(const QVector<FieldInfo> &fields, const Table &t, const Mapping &m, int maxLen)
{
    Check c;
    if (fields.isEmpty()) {
        c.errors << QObject::tr("Mẫu chưa có trường nào ({{Tên}} hoặc «Tên»). Hãy bôi chọn chữ cần thay trong mẫu và bấm \"Biến đoạn này thành trường\".");
        return c;
    }
    if (!t.ok()) { c.errors << t.error; return c; }
    if (t.rows.isEmpty()) { c.errors << QObject::tr("Chưa có dòng dữ liệu."); return c; }
    for (const auto &f : fields) {
        const int col = m.value(f.name, -1);
        if (col < 0 || col >= t.header.size()) c.errors << QObject::tr("Trường «%1» chưa chọn cột dữ liệu.").arg(f.name);
    }
    if (!c.errors.isEmpty()) return c;

    QVector<int> blankRows;
    QHash<QString, int> seen; QVector<int> dupRows;
    QMap<QString, QVector<int>> emptyBy, longBy;
    for (int r = 0; r < t.rows.size(); ++r) {
        bool allEmpty = true; QStringList key;
        for (const auto &f : fields) {
            const QString v = t.rows[r].value(m.value(f.name));
            if (!v.trimmed().isEmpty()) allEmpty = false;
            key << v.trimmed();
        }
        if (allEmpty) { blankRows << r; continue; }
        for (const auto &f : fields) {
            const QString v = t.rows[r].value(m.value(f.name));
            if (v.trimmed().isEmpty()) emptyBy[f.name] << r;
            else if (v.size() > maxLen) longBy[f.name] << r;
        }
        const QString k = key.join(QChar(0x1F));
        if (seen.contains(k)) dupRows << r; else seen.insert(k, r);
    }
    c.usableRecords = t.rows.size() - blankRows.size();
    if (c.usableRecords == 0) { c.errors << QObject::tr("Mọi dòng dữ liệu đều trống ở các cột đã chọn."); return c; }
    if (!blankRows.isEmpty()) c.warnings << QObject::tr("Bỏ qua %1 dòng trống hoàn toàn (dòng %2).").arg(blankRows.size()).arg(rowList(blankRows, t.firstDataRow));
    for (const auto &f : fields) {
        if (emptyBy.contains(f.name))
            c.warnings << QObject::tr("Trường «%1» trống ở %2 dòng (dòng %3): chỗ đó sẽ để trống trong bản xuất.").arg(f.name).arg(emptyBy[f.name].size()).arg(rowList(emptyBy[f.name], t.firstDataRow));
        if (longBy.contains(f.name))
            c.warnings << QObject::tr("Trường «%1» dài hơn %2 ký tự ở %3 dòng (dòng %4): có thể tràn dòng hoặc xuống dòng ngoài ý muốn, hãy xem trước.").arg(f.name).arg(maxLen).arg(longBy[f.name].size()).arg(rowList(longBy[f.name], t.firstDataRow));
    }
    if (!dupRows.isEmpty()) c.warnings << QObject::tr("%1 dòng trùng nội dung với dòng trước (dòng %2): sẽ tạo bản trùng.").arg(dupRows.size()).arg(rowList(dupRows, t.firstDataRow));
    c.warnings << t.warnings;
    return c;
}

QVector<Values> buildRecords(const QVector<FieldInfo> &fields, const Table &t, const Mapping &m, QVector<int> *sourceRows)
{
    QVector<Values> out;
    for (int r = 0; r < t.rows.size(); ++r) {
        Values v; bool allEmpty = true;
        for (const auto &f : fields) {
            const int col = m.value(f.name, -1);
            const QString s = (col >= 0) ? t.rows[r].value(col) : QString();
            if (!s.trimmed().isEmpty()) allEmpty = false;
            v.insert(f.name, s);
        }
        if (allEmpty) continue;
        out.append(v);
        if (sourceRows) sourceRows->append(r);
    }
    return out;
}

// =====================================================================================================
// Tên tệp
// =====================================================================================================
QString sanitizeFileName(const QString &nameIn)
{
    QString s;
    for (QChar c : nameIn) {
        const ushort u = c.unicode();
        if (u < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(c)) s += QLatin1Char('_'); else s += c;
    }
    s = s.simplified();
    while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' '))) s.chop(1);
    while (s.startsWith(QLatin1Char(' '))) s.remove(0, 1);
    static const QRegularExpression reserved(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\\..*)?$"), QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(s).hasMatch()) s += QLatin1Char('_');
    if (s.size() > 100) {
        s = s.left(100);
        if (s.back().isHighSurrogate()) s.chop(1);
        while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' '))) s.chop(1);
    }
    return s;
}

QStringList planFileNames(const QVector<Values> &records, const QString &patternIn, const QString &ext, const QString &outDir, QStringList *warnings)
{
    const QString pattern = patternIn.trimmed().isEmpty() ? QStringLiteral("{#}") : patternIn;
    QStringList result; QSet<QString> used;
    QSet<QString> missingWarned;
    const int width = qMax(3, QString::number(records.size()).size());
    static const QRegularExpression tok(QStringLiteral("\\{([^{}]+)\\}"));
    for (int i = 0; i < records.size(); ++i) {
        QString base = pattern; QString outS; int last = 0;
        auto it = tok.globalMatch(pattern);
        while (it.hasNext()) {
            const auto m = it.next();
            outS += pattern.mid(last, m.capturedStart() - last);
            const QString key = m.captured(1).trimmed();
            if (key == QLatin1String("#")) outS += QString::number(i + 1).rightJustified(width, QLatin1Char('0'));
            else {
                bool found = false;
                for (auto vi = records[i].begin(); vi != records[i].end(); ++vi)
                    if (vi.key().trimmed().compare(key, Qt::CaseInsensitive) == 0) { outS += vi.value(); found = true; break; }
                if (!found && warnings && !missingWarned.contains(key.toLower())) {
                    missingWarned.insert(key.toLower());
                    *warnings << QObject::tr("Mẫu tên tệp dùng {%1} nhưng không có trường này.").arg(key);
                }
            }
            last = m.capturedEnd();
        }
        outS += pattern.mid(last);
        base = sanitizeFileName(outS);
        if (base.isEmpty()) base = QStringLiteral("tep_%1").arg(i + 1, width, 10, QLatin1Char('0'));
        QString name = base; int n = 1;
        auto taken = [&](const QString &nm) { return used.contains(nm.toLower()) || QFileInfo::exists(QDir(outDir).filePath(nm + ext)); };
        while (taken(name)) name = QStringLiteral("%1 (%2)").arg(base).arg(++n);
        if (n > 1 && warnings) *warnings << QObject::tr("Tên tệp trùng nên đổi thành «%1».").arg(name);
        used.insert(name.toLower());
        const QString full = QDir(outDir).filePath(name + ext);
        if (full.size() > 240 && warnings) *warnings << QObject::tr("Đường dẫn dài hơn 240 ký tự có thể lỗi trên Windows: %1").arg(full.left(60) + QStringLiteral("…"));
        result << full;
    }
    return result;
}

}  // namespace mailmerge
