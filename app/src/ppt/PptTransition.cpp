// SPDX-License-Identifier: MPL-2.0
#include "PptTransition.h"
#include "../tools/filetools/ZipLite.h"
#include "../word/PageMargins.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QXmlStreamReader>

namespace ppttransition {
namespace {
const QString P = QStringLiteral("http://schemas.openxmlformats.org/presentationml/2006/main");
const QString R = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
const QString PR = QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships");
const QString MC = QStringLiteral("http://schemas.openxmlformats.org/markup-compatibility/2006");
Result fail(const QString& e) { return {false, e}; }
bool read(ft::ZipReader& z, const QString& name, QByteArray* out, QString* error) {
    const auto* e = z.find(name);
    if (!e) { if (error) *error = QStringLiteral("Thiếu mục: ") + name; return false; }
    return z.read(*e, out, error, 32LL * 1024 * 1024);
}
}
QStringList slideEntries(const QString& source, QString* error) {
    auto bad = [&](const QString& e) { if (error) *error = e; return QStringList{}; };
    if (error) error->clear();
    ft::ZipReader z; QString e;
    if (!z.open(source, &e)) return bad(e);
    QSet<QString> names;
    for (const auto& item : z.entries()) {
        if (names.contains(item.name) || item.name.startsWith(QLatin1String("_xmlsignatures/"), Qt::CaseInsensitive))
            return bad(QStringLiteral("Gói có mục trùng tên hoặc chữ ký số; không chỉnh sửa."));
        names.insert(item.name);
    }
    QByteArray presentation, rels;
    if (!read(z, QStringLiteral("ppt/presentation.xml"), &presentation, &e) ||
        !read(z, QStringLiteral("ppt/_rels/presentation.xml.rels"), &rels, &e)) return bad(e);
    QHash<QString, QString> targets;
    QXmlStreamReader rr(rels);
    while (!rr.atEnd()) {
        rr.readNext();
        if (rr.isDTD()) return bad(QStringLiteral("Không hỗ trợ XML có DTD."));
        if (!rr.isStartElement() || rr.namespaceUri() != PR || rr.name() != QLatin1String("Relationship")) continue;
        const auto a = rr.attributes();
        if (a.value(QStringLiteral("Type")) != R + QStringLiteral("/slide")) continue;
        if (a.value(QStringLiteral("TargetMode")) == QLatin1String("External")) return bad(QStringLiteral("Slide liên kết bên ngoài."));
        const QString id = a.value(QStringLiteral("Id")).toString();
        const QString raw = a.value(QStringLiteral("Target")).toString();
        const QString path = QDir::cleanPath(raw.startsWith(QLatin1Char('/')) ? raw.mid(1) : QStringLiteral("ppt/") + raw);
        if (id.isEmpty() || targets.contains(id) || !path.startsWith(QLatin1String("ppt/slides/")) ||
            !path.endsWith(QLatin1String(".xml")) || path.contains(QLatin1Char('\\')) || !names.contains(path))
            return bad(QStringLiteral("Quan hệ slide không hợp lệ."));
        targets.insert(id, path);
    }
    if (rr.hasError()) return bad(rr.errorString());
    QXmlStreamReader sr(presentation); QStringList order; int depth = 0, listDepth = -1;
    bool root = false;
    while (!sr.atEnd()) {
        sr.readNext();
        if (sr.isDTD()) return bad(QStringLiteral("Không hỗ trợ XML có DTD."));
        if (sr.isStartElement()) {
            ++depth;
            if (depth == 1) {
                root = sr.namespaceUri() == P && sr.name() == QLatin1String("presentation");
                if (!root) return bad(QStringLiteral("Không phải PresentationML chuẩn transitional."));
            }
            if (depth == 2 && sr.namespaceUri() == P && sr.name() == QLatin1String("sldIdLst")) listDepth = depth;
            if (listDepth > 0 && depth == listDepth + 1 && sr.namespaceUri() == P && sr.name() == QLatin1String("sldId")) {
                const auto id = sr.attributes().value(R, QStringLiteral("id")).toString();
                if (!targets.contains(id) || order.contains(targets.value(id))) return bad(QStringLiteral("Không xác minh được thứ tự slide."));
                order << targets.value(id);
            }
        } else if (sr.isEndElement()) { if (depth == listDepth) listDepth = -1; --depth; }
    }
    if (sr.hasError() || !root || order.isEmpty()) return bad(QStringLiteral("Không đọc được danh sách slide."));
    return order;
}
Result patchXml(const QByteArray& xml, const Options& o, QByteArray* output) {
    if (!output || o.speed < 0 || o.speed > 2 || o.autoAdvanceMs < 0 || o.autoAdvanceMs > 600000 || (!o.onClick && !o.autoAdvanceMs))
        return fail(QStringLiteral("Thông số chuyển cảnh không hợp lệ."));
    // Giữ nguyên chuỗi UTF-8 gốc; không chấp nhận mã hóa khác để tránh thay đổi dữ liệu.
    const QString text = QString::fromUtf8(xml);
    if (text.toUtf8() != xml) return fail(QStringLiteral("Chỉ hỗ trợ XML UTF-8."));
    QXmlStreamReader xr(text);
    int depth = 0;
    qsizetype insertion = -1, oldStart = -1, oldEnd = -1;
    bool root = false, common = false, inTransition = false;
    auto tokenStart = [&]() -> qsizetype {
        const qsizetype off = xr.characterOffset();
        if (off <= 0 || off > text.size() || text.at(off - 1) != QLatin1Char('>')) return -1;
        const qsizetype start = text.lastIndexOf(QLatin1Char('<'), off - 1);
        return start;
    };
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isDTD()) return fail(QStringLiteral("Không hỗ trợ XML có DTD."));
        if (xr.isStartElement()) {
            ++depth;
            if (depth == 1) {
                root = xr.namespaceUri() == P && xr.name() == QLatin1String("sld");
                if (!root) return fail(QStringLiteral("Không nhận diện được slide PresentationML."));
            } else if (depth == 2) {
                const auto ns = xr.namespaceUri().toString(), name = xr.name().toString();
                if (ns == MC && name == QLatin1String("AlternateContent")) return fail(QStringLiteral("Slide có cấu trúc tương thích mở rộng; chưa hỗ trợ thay chuyển cảnh."));
                if (ns != P) return fail(QStringLiteral("Phần tử slide mở rộng chưa được hỗ trợ."));
                if (name == QLatin1String("cSld")) common = true;
                if (name == QLatin1String("transition")) {
                    if (oldStart >= 0) return fail(QStringLiteral("Có nhiều transition."));
                    oldStart = tokenStart(); if (oldStart < 0) return fail(QStringLiteral("Không xác minh được vị trí transition."));
                    inTransition = true;
                    for (const auto& a : xr.attributes())
                        if (!a.namespaceUri().isEmpty() || (a.name() != QLatin1String("spd") && a.name() != QLatin1String("advClick") && a.name() != QLatin1String("advTm")))
                            return fail(QStringLiteral("Chuyển cảnh mở rộng có thuộc tính chưa hỗ trợ; giữ nguyên."));
                } else if ((name == QLatin1String("timing") || name == QLatin1String("extLst")) && insertion < 0) {
                    insertion = tokenStart();
                    if (insertion < 0) return fail(QStringLiteral("Không xác minh được vị trí trước timing/extLst."));
                }
            } else if (inTransition) {
                const auto n = xr.name().toString();
                if (depth != 3 || xr.namespaceUri() != P || (n != QLatin1String("fade") && n != QLatin1String("push") && n != QLatin1String("wipe") && n != QLatin1String("dissolve")))
                    return fail(QStringLiteral("Chuyển cảnh cũ có âm thanh hoặc thành phần chưa hỗ trợ; giữ nguyên."));
            }
        } else if (xr.isEndElement()) {
            if (depth == 2 && inTransition) { oldEnd = xr.characterOffset(); inTransition = false; }
            if (depth == 1 && insertion < 0) insertion = tokenStart();
            --depth;
        }
    }
    if (xr.hasError() || !root || !common || insertion < 0 || (oldStart >= 0 && (oldEnd <= oldStart || oldEnd > text.size())))
        return fail(QStringLiteral("XML slide lỗi hoặc không tìm được vị trí hợp lệ."));
    QString fragment;
    if (o.effect != Effect::None || o.autoAdvanceMs > 0 || !o.onClick) {
        const QString speeds[] = {QStringLiteral("slow"), QStringLiteral("med"), QStringLiteral("fast")};
        fragment = QStringLiteral("<p:transition xmlns:p=\"%1\" spd=\"%2\" advClick=\"%3\"").arg(P, speeds[o.speed], o.onClick ? QStringLiteral("1") : QStringLiteral("0"));
        if (o.autoAdvanceMs) fragment += QStringLiteral(" advTm=\"%1\"").arg(o.autoAdvanceMs);
        fragment += QLatin1Char('>');
        switch (o.effect) {
        case Effect::None: break;
        case Effect::Fade: fragment += QStringLiteral("<p:fade/>"); break;
        case Effect::PushLeft: fragment += QStringLiteral("<p:push dir=\"l\"/>"); break;
        case Effect::WipeLeft: fragment += QStringLiteral("<p:wipe dir=\"l\"/>"); break;
        case Effect::Dissolve: fragment += QStringLiteral("<p:dissolve/>"); break;
        default: return fail(QStringLiteral("Hiệu ứng không hợp lệ."));
        }
        fragment += QStringLiteral("</p:transition>");
    }
    QString patched = text;
    if (oldStart >= 0) patched.replace(oldStart, oldEnd - oldStart, fragment);
    else patched.insert(insertion, fragment);
    // Reparse and verify direct transition count, without rewriting other bytes.
    QXmlStreamReader verify(patched); depth = 0; int count = 0;
    while (!verify.atEnd()) {
        verify.readNext();
        if (verify.isStartElement()) { ++depth; if (depth == 2 && verify.namespaceUri() == P && verify.name() == QLatin1String("transition")) ++count; }
        else if (verify.isEndElement()) --depth;
    }
    if (verify.hasError() || count != (fragment.isEmpty() ? 0 : 1)) return fail(QStringLiteral("XML sau chỉnh không qua kiểm tra."));
    *output = patched.toUtf8(); return {true, {}};
}
Result createCopy(const QString& source, const QString& destination, int slide1, const Options& options) {
    const QFileInfo dst(destination);
    if (!source.endsWith(QLatin1String(".pptx"), Qt::CaseInsensitive) || !destination.endsWith(QLatin1String(".pptx"), Qt::CaseInsensitive) ||
        dst.exists() || dst.isSymLink() || !QDir(dst.absolutePath()).exists())
        return fail(QStringLiteral("Chọn nguồn PPTX và tên PPTX mới chưa tồn tại."));
    QString error;
    const auto slides = slideEntries(source, &error);
    if (slide1 < 1 || slide1 > slides.size()) return fail(error.isEmpty() ? QStringLiteral("Slide không tồn tại.") : error);
    ft::ZipReader original;
    if (!original.open(source, &error)) return fail(error);
    const QString entry = slides.at(slide1 - 1);
    QByteArray xml, patched;
    if (!read(original, entry, &xml, &error)) return fail(error);
    const auto result = patchXml(xml, options, &patched);
    if (!result.ok) return result;
    QTemporaryDir temp(QDir(dst.absolutePath()).filePath(QStringLiteral(".DungOffice-ppt-XXXXXX")));
    if (!temp.isValid()) return fail(QStringLiteral("Không tạo được thư mục tạm."));
    const QString tmp = temp.filePath(QStringLiteral("copy.pptx"));
    if (!dung::word::replaceZipEntry(source, tmp, entry, patched, &error)) return fail(error);
    ft::ZipReader check;
    if (!check.open(tmp, &error) || check.entries().size() != original.entries().size()) return fail(QStringLiteral("Gói đầu ra không qua kiểm tra: ") + error);
    for (const auto& old : original.entries()) {
        const auto* now = check.find(old.name);
        if (!now) return fail(QStringLiteral("Mất mục: ") + old.name);
        if (old.name == entry) {
            QByteArray actual;
            if (!check.read(*now, &actual, &error, 32LL * 1024 * 1024) || actual != patched) return fail(QStringLiteral("Slide đầu ra không khớp."));
        } else if (old.crc != now->crc || old.usize != now->usize || old.csize != now->csize || old.method != now->method)
            return fail(QStringLiteral("Mục khác slide bị thay đổi: ") + old.name);
    }
    if (!QFile::rename(tmp, destination)) return fail(QStringLiteral("Không đặt được tên đích; không ghi đè."));
    return {true, {}};
}
}
