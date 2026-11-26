// SPDX-License-Identifier: MPL-2.0
#include "TemplateExport.h"
#include "DataChecks.h"
#include "FormulaCatalog.h"
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QSet>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

namespace dataassist {

static QString colName(int c0) // 0 => A
{
    QString s;
    for (int c = c0 + 1; c > 0; c = (c - 1) / 26) s.prepend(QChar('A' + (c - 1) % 26));
    return s;
}
static QString esc(const QString &s)
{
    QString r;
    r.reserve(s.size() + 8);
    for (QChar c : s) {
        const ushort u = c.unicode();
        if (c == QLatin1Char('&')) r += QLatin1String("&amp;");
        else if (c == QLatin1Char('<')) r += QLatin1String("&lt;");
        else if (c == QLatin1Char('>')) r += QLatin1String("&gt;");
        else if (u < 0x20 && u != 9 && u != 10 && u != 13) continue; // ký tự điều khiển không hợp lệ trong XML
        else r += c;
    }
    return r;
}

// Tìm đường dẫn phần sheet theo thứ tự trong workbook.
static QStringList sheetParts(QZipReader &z, QStringList *names)
{
    QStringList parts;
    QMap<QString, QString> rel;
    {
        QXmlStreamReader x(z.fileData(QStringLiteral("xl/_rels/workbook.xml.rels")));
        while (!x.atEnd()) {
            x.readNext();
            if (x.isStartElement() && x.name() == QLatin1String("Relationship"))
                rel[x.attributes().value("Id").toString()] = x.attributes().value("Target").toString();
        }
    }
    QXmlStreamReader x(z.fileData(QStringLiteral("xl/workbook.xml")));
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement() && x.name() == QLatin1String("sheet")) {
            const QString id = x.attributes().value(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"), QStringLiteral("id")).toString();
            QString t = rel.value(id);
            if (t.startsWith(QLatin1Char('/'))) t = t.mid(1); else t = QStringLiteral("xl/") + t;
            parts << t;
            if (names) *names << x.attributes().value("name").toString();
        }
    }
    return parts;
}

static int colIndexFromRef(const QString &ref)
{
    int c = 0;
    for (QChar ch : ref) { if (!ch.isLetter()) break; c = c * 26 + (ch.toUpper().unicode() - 'A' + 1); }
    return c - 1;
}

// Đọc ô chữ/số của một sheet theo SỐ DÒNG THẬT (không bỏ dòng trống), kể cả chuỗi dùng chung và inlineStr.
static QMap<int, QMap<int, QString>> readSheetCells(QZipReader &z, const QString &part)
{
    QStringList shared;
    if (z.fileData(QStringLiteral("xl/sharedStrings.xml")).size()) {
        QXmlStreamReader r(z.fileData(QStringLiteral("xl/sharedStrings.xml")));
        QString cur; bool in = false; int depth = 0;
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement()) {
                if (r.name() == QLatin1String("si")) { in = true; cur.clear(); depth = 0; }
                else if (in && r.name() == QLatin1String("t")) cur += r.readElementText();
                (void)depth;
            } else if (r.isEndElement() && r.name() == QLatin1String("si")) { shared << cur; in = false; }
        }
    }
    QMap<int, QMap<int, QString>> cells;
    QXmlStreamReader r(z.fileData(part));
    int rowNum = 0;
    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement()) continue;
        if (r.name() == QLatin1String("row")) {
            rowNum = r.attributes().hasAttribute(QLatin1String("r")) ? r.attributes().value(QLatin1String("r")).toInt() : rowNum + 1;
        } else if (r.name() == QLatin1String("c")) {
            const QString ref = r.attributes().value(QLatin1String("r")).toString();
            const QString type = r.attributes().value(QLatin1String("t")).toString();
            const int col = colIndexFromRef(ref);
            QString v;
            while (!r.atEnd()) {
                r.readNext();
                if (r.isEndElement() && r.name() == QLatin1String("c")) break;
                if (!r.isStartElement()) continue;
                if (r.name() == QLatin1String("v")) v = r.readElementText();
                else if (r.name() == QLatin1String("t")) v += r.readElementText();   // trong <is>
            }
            if (type == QLatin1String("s")) { const int i = v.toInt(); v = (i >= 0 && i < shared.size()) ? shared[i] : QString(); }
            if (col >= 0 && !v.isEmpty()) cells[rowNum][col] = v;
        }
    }
    return cells;
}

TemplateInfo loadTemplate(const QString &path, int sheetIndex, int headerOverride)
{
    TemplateInfo ti;
    if (!QFileInfo::exists(path)) { ti.error = QStringLiteral("Không thấy tệp mẫu: %1").arg(path); return ti; }
    if (!path.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive)) { ti.error = QStringLiteral("Chỉ hỗ trợ mẫu .xlsx (tệp .xls/.csv cần mở và lưu thành .xlsx trước)."); return ti; }
    QZipReader z(path);
    if (!z.isReadable()) { ti.error = QStringLiteral("Không mở được tệp mẫu (không phải .xlsx hợp lệ?)."); return ti; }
    const QStringList parts = sheetParts(z, &ti.sheetNames);
    if (parts.isEmpty()) { ti.error = QStringLiteral("Không đọc được danh sách sheet của mẫu."); return ti; }
    if (sheetIndex < 0 || sheetIndex >= parts.size()) { ti.error = QStringLiteral("Sheet số %1 không tồn tại.").arg(sheetIndex + 1); return ti; }
    ti.sheetIndex = sheetIndex;
    const auto cells = readSheetCells(z, parts[sheetIndex]);
    if (cells.isEmpty()) { ti.error = QStringLiteral("Sheet mẫu trống."); return ti; }
    int hdr = 0;
    if (headerOverride > 0) {
        hdr = headerOverride;
        if (!cells.contains(hdr)) { ti.error = QStringLiteral("Dòng tiêu đề %1 không có dữ liệu.").arg(headerOverride); return ti; }
    } else {
        int best = -1;
        for (auto it = cells.begin(); it != cells.end() && it.key() <= 15; ++it)
            if (int(it.value().size()) > best) { best = it.value().size(); hdr = it.key(); }
        if (best < 2) ti.warnings << QStringLiteral("Không chắc dòng tiêu đề (không dòng nào có từ 2 ô chữ trở lên): đang lấy dòng %1 — hãy chọn lại.").arg(hdr);
    }
    ti.headerRow = hdr;
    const auto &hrow = cells[hdr];
    const int lastCol = hrow.isEmpty() ? -1 : hrow.lastKey();
    for (int c = 0; c <= lastCol; ++c) ti.headers << hrow.value(c).trimmed();
    for (auto it = cells.begin(); it != cells.end(); ++it) if (it.key() > hdr) ++ti.existingDataRows;
    if (ti.existingDataRows)
        ti.warnings << QStringLiteral("Mẫu đang có %1 dòng dữ liệu/ghi chú bên dưới tiêu đề — bản xuất sẽ THAY các dòng này bằng dữ liệu của bạn (mẫu gốc không đổi).").arg(ti.existingDataRows);
    for (const auto &fi : z.fileInfoList())
        if (fi.filePath.startsWith(QLatin1String("xl/tables/"))) { ti.warnings << QStringLiteral("Mẫu có bảng định dạng (Table): vùng của bảng không tự mở rộng; phần mềm đích thường chỉ đọc theo ô nên thường không ảnh hưởng, nhưng chưa kiểm chứng."); break; }
    ti.ok = true;
    return ti;
}

QVector<int> autoMap(const QStringList &th, const QStringList &sh)
{
    QVector<int> m(th.size(), -1);
    QVector<QString> ft, fs;
    for (const QString &s : th) ft << foldVietnamese(s).remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    for (const QString &s : sh) fs << foldVietnamese(s).remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    QSet<int> used;
    for (int i = 0; i < th.size(); ++i) {
        if (ft[i].isEmpty()) continue;
        QList<int> exact;
        for (int j = 0; j < sh.size(); ++j) if (!used.contains(j) && fs[j] == ft[i]) exact << j;
        if (exact.size() == 1) { m[i] = exact[0]; used.insert(exact[0]); }
    }
    for (int i = 0; i < th.size(); ++i) {
        if (m[i] >= 0 || ft[i].size() < 3) continue;
        QList<int> cand;
        for (int j = 0; j < sh.size(); ++j)
            if (!used.contains(j) && fs[j].size() >= 3 && (fs[j].contains(ft[i]) || ft[i].contains(fs[j]))) cand << j;
        if (cand.size() == 1) { m[i] = cand[0]; used.insert(cand[0]); }
    }
    return m;
}

namespace {
bool validProfileOptions(const TemplateInfo& t, const QStringList& sh, const ExportOptions& o) {
    if (!t.ok || t.headers.isEmpty() || sh.isEmpty() || t.headerRow<1 ||
        t.sheetIndex<0 || t.sheetIndex>=t.sheetNames.size() ||
        t.headers.size()>16384 || sh.size()>16384 ||
        o.mapping.size()!=t.headers.size() || o.colTypes.size()!=t.headers.size()) return false;
    QSet<int> used;
    for (int i=0;i<o.mapping.size();++i) {
        const int index=o.mapping[i];
        if (index < -1 || index >= sh.size()) return false;
        if (index>=0) { if (used.contains(index)) return false; used.insert(index); }
        if (o.colTypes[i]!=ColType::Text && o.colTypes[i]!=ColType::Number) return false;
    }
    return !used.isEmpty();
}
}
QByteArray encodeMappingProfile(const QString& vendor,const TemplateInfo& t,
                                const QStringList& sh,const ExportOptions& o) {
    if (vendor.isEmpty() || !validProfileOptions(t,sh,o)) return {};
    QJsonArray mapping,types;
    for (int index:o.mapping) mapping.append(index);
    for (auto type:o.colTypes) types.append(type==ColType::Number?QStringLiteral("number"):QStringLiteral("text"));
    const QJsonObject obj{{QStringLiteral("version"),1},{QStringLiteral("vendor"),vendor},
        {QStringLiteral("targetSheet"),t.sheetNames[t.sheetIndex]},{QStringLiteral("headerRow"),t.headerRow},
        {QStringLiteral("targetHeaders"),QJsonArray::fromStringList(t.headers)},
        {QStringLiteral("sourceHeaders"),QJsonArray::fromStringList(sh)},
        {QStringLiteral("mapping"),mapping},{QStringLiteral("types"),types},
        {QStringLiteral("vietnameseNumbers"),o.vietnameseNumbers}};
    const auto data=QJsonDocument(obj).toJson(QJsonDocument::Indented);
    return data.size()<=2*1024*1024?data:QByteArray{};
}
bool decodeMappingProfile(const QByteArray& json,const QString& vendor,const TemplateInfo& t,
                          const QStringList& sh,ExportOptions* options,QString* error) {
    auto fail=[&](const QString& why){if(error)*error=why;return false;};
    if(!options || !t.ok || t.sheetIndex<0 || t.sheetIndex>=t.sheetNames.size() ||
       json.isEmpty() || json.size()>2*1024*1024) return fail(QStringLiteral("Cấu hình không hợp lệ hoặc quá lớn."));
    QJsonParseError parse;
    const auto doc=QJsonDocument::fromJson(json,&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject()) return fail(QStringLiteral("Không đọc được JSON."));
    const auto obj=doc.object();
    if(obj.value("version").toInt(-1)!=1 || obj.value("vendor").toString()!=vendor ||
       obj.value("targetSheet").toString()!=t.sheetNames[t.sheetIndex] ||
       obj.value("headerRow").toInt(-1)!=t.headerRow ||
       obj.value("targetHeaders").toArray()!=QJsonArray::fromStringList(t.headers) ||
       obj.value("sourceHeaders").toArray()!=QJsonArray::fromStringList(sh))
        return fail(QStringLiteral("Cấu hình không khớp phần mềm, sheet hoặc tiêu đề hiện tại. Hãy ghép cột lại."));
    if(!obj.value("mapping").isArray() || !obj.value("types").isArray() ||
       !obj.value("vietnameseNumbers").isBool()) return fail(QStringLiteral("Thiếu quy tắc ghép cột."));
    ExportOptions candidate;
    candidate.vietnameseNumbers=obj.value("vietnameseNumbers").toBool();
    for(const auto value:obj.value("mapping").toArray()) {
        if(!value.isDouble() || value.toDouble() < -1 || value.toDouble()>=sh.size() ||
           value.toDouble()!=value.toInt(-2)) return fail(QStringLiteral("Chỉ số cột không hợp lệ."));
        candidate.mapping.append(value.toInt());
    }
    for(const auto value:obj.value("types").toArray()) {
        const auto type=value.toString();
        if(type!=QLatin1String("text") && type!=QLatin1String("number")) return fail(QStringLiteral("Kiểu dữ liệu không hợp lệ."));
        candidate.colTypes.append(type==QLatin1String("number")?ColType::Number:ColType::Text);
    }
    if(!validProfileOptions(t,sh,candidate)) return fail(QStringLiteral("Cấu hình ghép cột trùng hoặc sai kích thước."));
    *options=candidate;
    if(error)error->clear();
    return true;
}

ValidateResult validate(const TemplateInfo &t, const QStringList &sh, const QVector<QStringList> &rows, const ExportOptions &o)
{
    ValidateResult v;
    if (!t.ok) { v.errors << QStringLiteral("Chưa nạp được mẫu."); return v; }
    if (o.mapping.size() != t.headers.size()) v.errors << QStringLiteral("Bảng ghép cột không khớp số cột mẫu (%1 ≠ %2).").arg(o.mapping.size()).arg(t.headers.size());
    if (rows.isEmpty()) v.errors << QStringLiteral("Không có dòng dữ liệu để xuất.");
    QSet<int> usedSrc;
    int mappedCols = 0;
    for (int i = 0; i < o.mapping.size() && i < t.headers.size(); ++i) {
        const int s = o.mapping[i];
        if (s < 0) { if (!t.headers[i].trimmed().isEmpty()) v.warnings << QStringLiteral("Cột mẫu \"%1\" chưa có dữ liệu — sẽ để trống; nếu là cột bắt buộc của phần mềm đích thì tệp có thể bị từ chối.").arg(t.headers[i]); continue; }
        if (s >= sh.size()) { v.errors << QStringLiteral("Cột mẫu \"%1\" ghép tới cột nguồn không tồn tại.").arg(t.headers[i]); continue; }
        if (usedSrc.contains(s)) v.errors << QStringLiteral("Cột nguồn \"%1\" được ghép vào hai cột mẫu.").arg(sh[s]);
        usedSrc.insert(s); ++mappedCols;
        if (i < o.colTypes.size() && o.colTypes[i] == ColType::Number) {
            int bad = 0, amb = 0, firstBad = -1;
            for (int r = 0; r < rows.size(); ++r) {
                const QString c = s < rows[r].size() ? rows[r][s].trimmed() : QString();
                if (c.isEmpty()) continue;
                const NumParse n = parseNumber(c, o.vietnameseNumbers);
                if (!n.ok) { ++bad; if (firstBad < 0) firstBad = r + 1; }
                else if (n.ambiguous) ++amb;
            }
            if (bad) v.warnings << QStringLiteral("Cột \"%1\": %2 ô không đọc được thành số (dòng dữ liệu đầu tiên: %3) — các ô này sẽ được ghi dạng CHỮ, không tự đổi.").arg(t.headers[i]).arg(bad).arg(firstBad);
            if (amb) v.warnings << QStringLiteral("Cột \"%1\": %2 ô dạng 1.234 mơ hồ giữa kiểu Việt và Anh — đang đọc theo kiểu %3.").arg(t.headers[i]).arg(amb).arg(o.vietnameseNumbers ? QStringLiteral("Việt") : QStringLiteral("Anh"));
        }
    }
    if (mappedCols == 0 && !rows.isEmpty()) v.errors << QStringLiteral("Chưa ghép cột nào.");
    for (int j = 0; j < sh.size(); ++j)
        if (!usedSrc.contains(j) && !sh[j].trimmed().isEmpty()) v.warnings << QStringLiteral("Cột nguồn \"%1\" không được dùng — dữ liệu cột này sẽ KHÔNG nằm trong tệp xuất.").arg(sh[j]);
    return v;
}

static QString buildRow(int rowNum, const QVector<QString> &texts, const QVector<bool> &isNum)
{
    QString r = QStringLiteral("<row r=\"%1\">").arg(rowNum);
    for (int c = 0; c < texts.size(); ++c) {
        if (texts[c].isEmpty()) continue;
        const QString ref = colName(c) + QString::number(rowNum);
        if (isNum[c]) r += QStringLiteral("<c r=\"%1\"><v>%2</v></c>").arg(ref, texts[c]);
        else r += QStringLiteral("<c r=\"%1\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%2</t></is></c>").arg(ref, esc(texts[c]));
    }
    r += QStringLiteral("</row>");
    return r;
}

ExportResult exportToTemplate(const QString &tpl, const QString &out, const TemplateInfo &t, const QStringList &sh,
                              const QVector<QStringList> &rows, const ExportOptions &o)
{
    ExportResult res;
    if (!t.ok) { res.error = QStringLiteral("Chưa nạp được mẫu."); return res; }
    if (QFileInfo(out).absoluteFilePath() == QFileInfo(tpl).absoluteFilePath()) { res.error = QStringLiteral("Không được ghi đè lên tệp mẫu."); return res; }
    if (QFileInfo::exists(out)) { res.error = QStringLiteral("Tệp đích đã tồn tại, không ghi đè: %1").arg(out); return res; }
    const ValidateResult v = validate(t, sh, rows, o);
    if (!v.errors.isEmpty()) { res.error = v.errors.join(QStringLiteral("; ")); return res; }
    res.warnings = v.warnings;

    QZipReader zr(tpl);
    if (!zr.isReadable()) { res.error = QStringLiteral("Không đọc được tệp mẫu (không phải .xlsx hợp lệ?)."); return res; }
    QStringList names;
    const QStringList parts = sheetParts(zr, &names);
    if (t.sheetIndex >= parts.size()) { res.error = QStringLiteral("Không tìm thấy phần sheet trong mẫu."); return res; }
    const QString part = parts[t.sheetIndex];
    QString xml = QString::fromUtf8(zr.fileData(part));
    if (xml.isEmpty()) { res.error = QStringLiteral("Không đọc được %1 trong mẫu.").arg(part); return res; }

    int sdStart = xml.indexOf(QStringLiteral("<sheetData"));
    if (sdStart < 0) { res.error = QStringLiteral("Sheet mẫu không có <sheetData>."); return res; }
    int sdTagEnd = xml.indexOf(QLatin1Char('>'), sdStart);
    QString head, keepRows, tail;
    if (xml.mid(sdStart, sdTagEnd - sdStart + 1).endsWith(QLatin1String("/>"))) {
        head = xml.left(sdStart); tail = xml.mid(sdTagEnd + 1);
    } else {
        const int sdEnd = xml.indexOf(QStringLiteral("</sheetData>"), sdTagEnd);
        if (sdEnd < 0) { res.error = QStringLiteral("Sheet mẫu hỏng (thiếu </sheetData>)."); return res; }
        head = xml.left(sdStart); tail = xml.mid(sdEnd + int(QStringLiteral("</sheetData>").size()));
        const QString body = xml.mid(sdTagEnd + 1, sdEnd - sdTagEnd - 1);
        static const QRegularExpression rowRe(QStringLiteral("<row\\b[^>]*?(?:/>|>.*?</row>)"), QRegularExpression::DotMatchesEverythingOption);
        static const QRegularExpression rAttr(QStringLiteral("^<row\\b[^>]*?\\br=\"(\\d+)\""));
        auto it = rowRe.globalMatch(body);
        int ordinal = 0;
        while (it.hasNext()) {
            const QString row = it.next().captured(0);
            ++ordinal;
            const auto m = rAttr.match(row);
            const int rn = m.hasMatch() ? m.captured(1).toInt() : ordinal;
            if (rn <= t.headerRow) keepRows += row;
        }
    }
    // Cột tối đa cần ghi
    const int ncol = t.headers.size();
    QString newRows;
    int rowNum = t.headerRow;
    int numberFallback = 0;
    for (const QStringList &src : rows) {
        bool any = false;
        for (int s : o.mapping) if (s >= 0 && s < src.size() && !src[s].trimmed().isEmpty()) any = true;
        if (!any) continue;
        ++rowNum;
        QVector<QString> txt(ncol); QVector<bool> isNum(ncol, false);
        for (int c = 0; c < ncol; ++c) {
            const int s = o.mapping[c];
            if (s < 0 || s >= src.size()) continue;
            const QString raw = src[s];
            if (c < o.colTypes.size() && o.colTypes[c] == ColType::Number && !raw.trimmed().isEmpty()) {
                const NumParse n = parseNumber(raw, o.vietnameseNumbers);
                if (n.ok && !(raw.trimmed().size() > 1 && raw.trimmed().startsWith(QLatin1Char('0')) && !raw.contains(QLatin1Char(',')) && !raw.contains(QLatin1Char('.')))) {
                    txt[c] = QString::number(n.value, 'g', 17); isNum[c] = true; continue;
                }
                ++numberFallback;
            }
            txt[c] = raw;
        }
        newRows += buildRow(rowNum, txt, isNum);
        ++res.rowsWritten;
    }
    if (numberFallback) res.warnings << QStringLiteral("%1 ô ở cột số được ghi dạng chữ vì không đọc được chắc chắn thành số.").arg(numberFallback);
    // cập nhật <dimension>
    const QString dim = QStringLiteral("<dimension ref=\"A1:%1%2\"/>").arg(colName(qMax(0, ncol - 1))).arg(qMax(rowNum, t.headerRow));
    static const QRegularExpression dimRe(QStringLiteral("<dimension\\b[^>]*/>"));
    if (dimRe.match(head).hasMatch()) head.replace(dimRe, dim);
    QString newXml = head + QStringLiteral("<sheetData>") + keepRows + newRows + QStringLiteral("</sheetData>") + tail;
    // vùng định dạng có điều kiện/xác thực nằm ở tail: giữ nguyên.

    QZipWriter zw(out);
    if (zw.status() != QZipWriter::NoError) { res.error = QStringLiteral("Không tạo được tệp đích."); return res; }
    zw.setCompressionPolicy(QZipWriter::AutoCompress);
    for (const auto &fi : zr.fileInfoList()) {
        if (!fi.isFile) continue;
        if (fi.filePath == QLatin1String("xl/calcChain.xml")) continue; // chuỗi tính toán cũ có thể trỏ tới ô không còn; Excel/engine tự dựng lại
        QByteArray data = fi.filePath == part ? newXml.toUtf8() : zr.fileData(fi.filePath);
        if (fi.filePath == QLatin1String("[Content_Types].xml")) {
            static const QRegularExpression re(QStringLiteral("<Override\\b[^>]*calcChain[^>]*/>"));
            QString x = QString::fromUtf8(data); x.remove(re); data = x.toUtf8();
        } else if (fi.filePath == QLatin1String("xl/_rels/workbook.xml.rels")) {
            static const QRegularExpression re(QStringLiteral("<Relationship\\b[^>]*calcChain[^>]*/>"));
            QString x = QString::fromUtf8(data); x.remove(re); data = x.toUtf8();
        }
        zw.addFile(fi.filePath, data);
    }
    zw.close();
    if (zw.status() != QZipWriter::NoError) { QFile::remove(out); res.error = QStringLiteral("Lỗi khi ghi tệp đích."); return res; }
    res.ok = true;
    res.outPath = out;
    for (int i = 0; i < t.headers.size(); ++i)
        if (o.mapping[i] < 0 && !t.headers[i].trimmed().isEmpty()) res.unmappedTemplateColumns << t.headers[i];
    return res;
}

}  // namespace dataassist
