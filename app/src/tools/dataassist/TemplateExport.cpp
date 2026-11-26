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
#include <QDir>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QSet>
#include <cmath>
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

// Excel stores at most 15 significant decimal digits. Keep digit-only IDs as text,
// including leading zeroes, even if a user accidentally selects numeric output.
static bool protectedInteger(const QString& raw) {
    static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
    const QString v=raw.trimmed();
    return digits.match(v).hasMatch() && (v.size()>15 || (v.size()>1 && v.startsWith(QLatin1Char('0'))));
}

ValidateResult validate(const TemplateInfo &t, const QStringList &sh, const QVector<QStringList> &rows, const ExportOptions &o)
{
    ValidateResult v;
    if (!t.ok) { v.errors << QStringLiteral("Chưa nạp được mẫu."); return v; }
    if (o.mapping.size() != t.headers.size()) v.errors << QStringLiteral("Bảng ghép cột không khớp số cột mẫu (%1 ≠ %2).").arg(o.mapping.size()).arg(t.headers.size());
    if (rows.isEmpty()) v.errors << QStringLiteral("Không có dòng dữ liệu để xuất.");
    if (t.headerRow<1 || t.headerRow>1048576 || t.headers.isEmpty() || t.headers.size()>16384 ||
        rows.size()>1048576-t.headerRow) v.errors << QStringLiteral("Dữ liệu vượt giới hạn dòng/cột Excel.");
    QSet<int> usedSrc;
    int mappedCols = 0;
    for (int i = 0; i < o.mapping.size() && i < t.headers.size(); ++i) {
        const int s = o.mapping[i];
        if (s < 0) { if (!t.headers[i].trimmed().isEmpty()) v.warnings << QStringLiteral("Cột mẫu \"%1\" chưa có dữ liệu — sẽ để trống; nếu là cột bắt buộc của phần mềm đích thì tệp có thể bị từ chối.").arg(t.headers[i]); continue; }
        if (s >= sh.size()) { v.errors << QStringLiteral("Cột mẫu \"%1\" ghép tới cột nguồn không tồn tại.").arg(t.headers[i]); continue; }
        if (usedSrc.contains(s)) v.errors << QStringLiteral("Cột nguồn \"%1\" được ghép vào hai cột mẫu.").arg(sh[s]);
        usedSrc.insert(s); ++mappedCols;
        if (i < o.colTypes.size() && o.colTypes[i] == ColType::Number) {
            int bad = 0, amb = 0, protectedCount = 0, firstBad = -1;
            for (int r = 0; r < rows.size(); ++r) {
                const QString c = s < rows[r].size() ? rows[r][s].trimmed() : QString();
                if (c.isEmpty()) continue;
                if (protectedInteger(c)) { ++protectedCount; continue; }
                const NumParse n = parseNumber(c, o.vietnameseNumbers);
                if (!n.ok) { ++bad; if (firstBad < 0) firstBad = r + 1; }
                else if (n.ambiguous) ++amb;
            }
            if (protectedCount) v.warnings << QStringLiteral("Cột \"%1\": %2 mã số có số 0 đầu hoặc trên 15 chữ số — giữ nguyên dạng CHỮ.").arg(t.headers[i]).arg(protectedCount);
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
    const QFileInfo dest(out);
    if (!out.endsWith(QLatin1String(".xlsx"),Qt::CaseInsensitive) || dest.exists() || dest.isSymLink() ||
        !QDir(dest.absolutePath()).exists()) {
        res.error=QStringLiteral("Chọn tên XLSX mới trong thư mục tồn tại; không ghi đè: %1").arg(out); return res;
    }
    const ValidateResult v = validate(t, sh, rows, o);
    if (!v.errors.isEmpty()) { res.error = v.errors.join(QStringLiteral("; ")); return res; }
    res.warnings = v.warnings;

    QZipReader zr(tpl);
    if (!zr.isReadable()) { res.error = QStringLiteral("Không đọc được tệp mẫu (không phải .xlsx hợp lệ?)."); return res; }
    QStringList names;
    const QStringList parts = sheetParts(zr, &names);
    if (t.sheetIndex < 0 || t.sheetIndex >= parts.size()) { res.error = QStringLiteral("Không tìm thấy phần sheet trong mẫu."); return res; }
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
                if (n.ok && !protectedInteger(raw)) {
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

    // Validate the patched worksheet before writing. Stage the ZIP in the destination
    // directory, verify all copied parts, then rename without replacing existing files.
    QXmlStreamReader checkXml(newXml);
    while(!checkXml.atEnd()) { checkXml.readNext(); if(checkXml.isDTD()) {res.error=QStringLiteral("Không hỗ trợ DTD.");return res;} }
    if(checkXml.hasError()) {res.error=QStringLiteral("XML đầu ra không hợp lệ: ")+checkXml.errorString();return res;}
    QTemporaryDir temp(QDir(dest.absolutePath()).filePath(QStringLiteral(".dung-export-XXXXXX")));
    if(!temp.isValid()){res.error=QStringLiteral("Không tạo được thư mục tạm.");return res;}
    const QString staged=temp.filePath(QStringLiteral("export.xlsx"));
    QZipWriter zw(staged);
    QSet<QString> written;
    if (zw.status() != QZipWriter::NoError) { res.error = QStringLiteral("Không tạo được tệp đích."); return res; }
    zw.setCompressionPolicy(QZipWriter::AutoCompress);
    for (const auto &fi : zr.fileInfoList()) {
        if (!fi.isFile) continue;
        if (written.contains(fi.filePath) || fi.filePath.startsWith(QLatin1String("_xmlsignatures/"),Qt::CaseInsensitive)) {
            res.error=QStringLiteral("Mẫu có mục trùng hoặc chữ ký số; không xuất.");return res;
        }
        written.insert(fi.filePath);
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
    if (zw.status() != QZipWriter::NoError) { res.error = QStringLiteral("Lỗi khi ghi tệp tạm."); return res; }
    {
        QZipReader verify(staged);
        if (!verify.isReadable() || verify.fileData(part)!=newXml.toUtf8()) {
            res.error=QStringLiteral("Tệp XLSX tạm không qua xác minh.");return res;
        }
        for(const auto& fi:zr.fileInfoList()) {
            if(!fi.isFile || fi.filePath==part || fi.filePath==QLatin1String("xl/calcChain.xml") ||
               fi.filePath==QLatin1String("[Content_Types].xml") || fi.filePath==QLatin1String("xl/_rels/workbook.xml.rels"))continue;
            if(verify.fileData(fi.filePath)!=zr.fileData(fi.filePath)) {
                res.error=QStringLiteral("Mục khác dữ liệu bị thay đổi: ")+fi.filePath;return res;
            }
        }
    }
    if (!QFile::rename(staged,out)) {res.error=QStringLiteral("Không lưu được tệp đích mới; không ghi đè.");return res;}
    res.ok = true;
    res.outPath = out;
    for (int i = 0; i < t.headers.size(); ++i)
        if (o.mapping[i] < 0 && !t.headers[i].trimmed().isEmpty()) res.unmappedTemplateColumns << t.headers[i];
    return res;
}

ExportResult exportGroupReport(const QString& outPath,const GroupSummary& summary,bool sumChart)
{
    ExportResult res;
    const QFileInfo dst(outPath);
    auto fail=[&](const QString& error){res.error=error;return res;};
    if(!outPath.endsWith(QLatin1String(".xlsx"),Qt::CaseInsensitive)||dst.exists()||dst.isSymLink()||!QDir(dst.absolutePath()).exists())
        return fail(QStringLiteral("Chọn tên báo cáo XLSX mới, không ghi đè."));
    if(!summary.ok||summary.groups.isEmpty()||summary.groups.size()>100)
        return fail(QStringLiteral("Biểu đồ báo cáo hỗ trợ từ 1 đến 100 nhóm; hãy lọc dữ liệu."));
    // All user labels are inline strings and XML text, never formula/attribute content.
    auto safeLabel=[](const QString& s){
        if(s.isEmpty()||s.size()>1024)return false;
        for(uint ch:s.toUcs4())if((ch<32&&ch!=9&&ch!=10&&ch!=13)||ch==0xfffe||ch==0xffff||(ch>=0xd800&&ch<=0xdfff))return false;
        return true;
    };
    auto num=[](double v){return QString::number(v,'g',15);};
    auto strCell=[](const QString& ref,const QString& val){
        return QStringLiteral("<c r=\"%1\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%2</t></is></c>").arg(ref,esc(val));
    };
    auto numCell=[&](const QString& ref,double v){return QStringLiteral("<c r=\"%1\"><v>%2</v></c>").arg(ref,num(v));};
    QString sheet=QStringLiteral("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><cols><col min=\"1\" max=\"1\" width=\"24\" customWidth=\"1\"/><col min=\"2\" max=\"7\" width=\"17\" customWidth=\"1\"/></cols><sheetData><row r=\"1\">");
    const QStringList headers{QStringLiteral("Nhóm"),QStringLiteral("Số dòng"),QStringLiteral("Ô số"),QStringLiteral("Tổng"),
        QStringLiteral("Trung bình"),QStringLiteral("Nhỏ nhất"),QStringLiteral("Lớn nhất")};
    for(int i=0;i<headers.size();++i)sheet+=strCell(colName(i)+QLatin1Char('1'),headers[i]);
    sheet+=QStringLiteral("</row>");
    QString labels,values;int index=0;bool hasMeasure=false;
    for(const auto& g:summary.groups){
        if(!safeLabel(g.label)||g.rows<1||g.numbers<0||g.numbers>g.rows||
           !std::isfinite(g.sum)||!std::isfinite(g.min)||!std::isfinite(g.max)||
           (g.numbers&&g.min>g.max))return fail(QStringLiteral("Nhóm có dữ liệu không hợp lệ."));
        const QString row=QString::number(index+2);
        sheet+=QStringLiteral("<row r=\"%1\">").arg(row)+strCell(QStringLiteral("A")+row,g.label)+
            numCell(QStringLiteral("B")+row,g.rows)+numCell(QStringLiteral("C")+row,g.numbers);
        if(g.numbers)sheet+=numCell(QStringLiteral("D")+row,g.sum)+numCell(QStringLiteral("E")+row,g.sum/g.numbers)+
            numCell(QStringLiteral("F")+row,g.min)+numCell(QStringLiteral("G")+row,g.max);
        sheet+=QStringLiteral("</row>");
        labels+=QStringLiteral("<c:pt idx=\"%1\"><c:v>%2</c:v></c:pt>").arg(index).arg(esc(g.label));
        if(!sumChart||g.numbers){
            values+=QStringLiteral("<c:pt idx=\"%1\"><c:v>%2</c:v></c:pt>").arg(index).arg(num(sumChart?g.sum:g.rows));
            hasMeasure=true;
        }
        ++index;
    }
    if(!hasMeasure)return fail(QStringLiteral("Không có số hợp lệ để tạo biểu đồ tổng."));
    sheet+=QStringLiteral("</sheetData><drawing r:id=\"rId1\"/></worksheet>");
    const QString last=QString::number(index+1),count=QString::number(index),col=sumChart?QStringLiteral("D"):QStringLiteral("B");
    const QString metric=sumChart?QStringLiteral("Tổng"):QStringLiteral("Số dòng");
    const QString chart=QStringLiteral(
        "<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
        "<c:chart><c:title><c:tx><c:rich><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang=\"vi-VN\"/><a:t>%1 theo nhóm</a:t></a:r></a:p></c:rich></c:tx></c:title>"
        "<c:plotArea><c:layout/><c:barChart><c:barDir val=\"col\"/><c:grouping val=\"clustered\"/>"
        "<c:ser><c:idx val=\"0\"/><c:order val=\"0\"/><c:tx><c:v>%1</c:v></c:tx>"
        "<c:cat><c:strRef><c:f>TongHop!$A$2:$A$%2</c:f><c:strCache><c:ptCount val=\"%3\"/>%4</c:strCache></c:strRef></c:cat>"
        "<c:val><c:numRef><c:f>TongHop!$%5$2:$%5$%2</c:f><c:numCache><c:formatCode>General</c:formatCode><c:ptCount val=\"%3\"/>%6</c:numCache></c:numRef></c:val>"
        "</c:ser><c:axId val=\"10\"/><c:axId val=\"20\"/></c:barChart>"
        "<c:catAx><c:axId val=\"10\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling><c:delete val=\"0\"/><c:axPos val=\"b\"/>"
        "<c:tickLblPos val=\"nextTo\"/><c:crossAx val=\"20\"/><c:crosses val=\"autoZero\"/><c:auto val=\"1\"/><c:lblAlgn val=\"ctr\"/><c:lblOffset val=\"100\"/></c:catAx>"
        "<c:valAx><c:axId val=\"20\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling><c:delete val=\"0\"/><c:axPos val=\"l\"/><c:majorGridlines/>"
        "<c:numFmt formatCode=\"General\" sourceLinked=\"1\"/><c:tickLblPos val=\"nextTo\"/><c:crossAx val=\"10\"/><c:crosses val=\"autoZero\"/><c:crossBetween val=\"between\"/></c:valAx>"
        "</c:plotArea><c:plotVisOnly val=\"1\"/><c:dispBlanksAs val=\"gap\"/></c:chart></c:chartSpace>").arg(metric,last,count,labels,col,values);
    const QMap<QString,QByteArray> parts{
        {QStringLiteral("[Content_Types].xml"),QByteArray("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/><Override PartName=\"/xl/drawings/drawing1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/><Override PartName=\"/xl/charts/chart1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawingml.chart+xml\"/></Types>")},
        {QStringLiteral("_rels/.rels"),QByteArray("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>")},
        {QStringLiteral("xl/workbook.xml"),QByteArray("<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"TongHop\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>")},
        {QStringLiteral("xl/_rels/workbook.xml.rels"),QByteArray("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/></Relationships>")},
        {QStringLiteral("xl/worksheets/sheet1.xml"),sheet.toUtf8()},
        {QStringLiteral("xl/worksheets/_rels/sheet1.xml.rels"),QByteArray("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing1.xml\"/></Relationships>")},
        {QStringLiteral("xl/drawings/drawing1.xml"),QByteArray("<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><xdr:oneCellAnchor><xdr:from><xdr:col>8</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>1</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from><xdr:ext cx=\"6400800\" cy=\"3657600\"/><xdr:graphicFrame macro=\"\"><xdr:nvGraphicFramePr><xdr:cNvPr id=\"1\" name=\"Chart 1\"/><xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr><xdr:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/></xdr:xfrm><a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\"><c:chart r:id=\"rId1\"/></a:graphicData></a:graphic></xdr:graphicFrame><xdr:clientData/></xdr:oneCellAnchor></xdr:wsDr>")},
        {QStringLiteral("xl/drawings/_rels/drawing1.xml.rels"),QByteArray("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart1.xml\"/></Relationships>")},
        {QStringLiteral("xl/charts/chart1.xml"),chart.toUtf8()}
    };
    QTemporaryDir temp(QDir(dst.absolutePath()).filePath(QStringLiteral(".dung-report-XXXXXX")));
    if(!temp.isValid())return fail(QStringLiteral("Không tạo được thư mục tạm."));
    const QString staged=temp.filePath(QStringLiteral("report.xlsx"));
    {
        QZipWriter zip(staged);
        for(auto it=parts.cbegin();it!=parts.cend();++it){
            QXmlStreamReader xml(it.value());while(!xml.atEnd())xml.readNext();
            if(xml.hasError())return fail(QStringLiteral("XML báo cáo không hợp lệ: ")+it.key());
            zip.addFile(it.key(),it.value());
        }
        zip.close();if(zip.status()!=QZipWriter::NoError)return fail(QStringLiteral("Không ghi được báo cáo."));
    }
    {
        QZipReader verify(staged);
        if(!verify.isReadable()||verify.fileInfoList().size()!=parts.size())return fail(QStringLiteral("Gói báo cáo không hợp lệ."));
        for(auto it=parts.cbegin();it!=parts.cend();++it)if(verify.fileData(it.key())!=it.value())return fail(QStringLiteral("Gói báo cáo không khớp."));
    }
    if(!QFile::rename(staged,outPath))return fail(QStringLiteral("Không lưu được báo cáo; không ghi đè."));
    res.ok=true;res.outPath=outPath;res.rowsWritten=index;res.warnings=summary.warnings;return res;
}

}  // namespace dataassist
