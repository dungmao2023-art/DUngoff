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
#include <functional>
#include <QRegularExpression>

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
namespace {
struct AnimScan { QString error; QList<Shape> shapes; qsizetype insertion = -1; bool hasTiming = false; };
AnimScan scanAnimation(const QString& text) {
    AnimScan out;
    QXmlStreamReader xr(text);
    QStringList path;
    QSet<unsigned> ids;
    bool root = false, common = false;
    auto tokenStart = [&]() -> qsizetype {
        const qsizetype off = xr.characterOffset();
        if (off <= 0 || off > text.size() || text.at(off-1) != QLatin1Char('>')) return -1;
        return text.lastIndexOf(QLatin1Char('<'), off-1);
    };
    auto bad = [&](const QString& why) { out.error = why; return out; };
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isDTD()) return bad(QStringLiteral("Không hỗ trợ XML có DTD."));
        if (xr.isStartElement()) {
            const bool isP = xr.namespaceUri() == P;
            const QString n = isP ? xr.name().toString() : QStringLiteral("!");
            path << n;
            if (path.size() == 1) {
                root = n == QLatin1String("sld");
                if (!root) return bad(QStringLiteral("Không phải slide PresentationML."));
            } else if (path.size() == 2) {
                if (!isP) return bad(QStringLiteral("Slide có cấu trúc mở rộng chưa hỗ trợ."));
                if (n == QLatin1String("cSld")) common = true;
                if (n == QLatin1String("timing")) out.hasTiming = true;
                if (n == QLatin1String("extLst") && out.insertion < 0) {
                    out.insertion = tokenStart();
                    if (out.insertion < 0) return bad(QStringLiteral("Không xác minh vị trí timing."));
                }
            }
            if (n == QLatin1String("cNvPr")) {
                bool ok = false;
                const unsigned id = xr.attributes().value(QStringLiteral("id")).toUInt(&ok);
                if (!ok || !id || ids.contains(id)) return bad(QStringLiteral("ID đối tượng thiếu hoặc trùng."));
                ids.insert(id);
                if (path.size() == 6 && path[0] == QLatin1String("sld") && path[1] == QLatin1String("cSld") &&
                    path[2] == QLatin1String("spTree") &&
                    ((path[3] == QLatin1String("sp") && path[4] == QLatin1String("nvSpPr")) ||
                     (path[3] == QLatin1String("pic") && path[4] == QLatin1String("nvPicPr"))))
                    out.shapes.append({id, xr.attributes().value(QStringLiteral("name")).toString()});
            }
        } else if (xr.isEndElement()) {
            if (path.size() == 1 && out.insertion < 0) out.insertion = tokenStart();
            if (!path.isEmpty()) path.removeLast();
        }
    }
    if (xr.hasError() || !root || !common || out.insertion < 0) return bad(QStringLiteral("XML slide không hợp lệ."));
    return out;
}
}
QList<Shape> slideShapes(const QString& source, int slide1, QString* error) {
    if (error) error->clear();
    QString e;
    const auto order = slideEntries(source, &e);
    if (slide1 < 1 || slide1 > order.size()) { if (error) *error = e.isEmpty() ? QStringLiteral("Slide không tồn tại.") : e; return {}; }
    ft::ZipReader z; QByteArray xml;
    if (!z.open(source, &e) || !read(z, order.at(slide1-1), &xml, &e)) { if (error) *error = e; return {}; }
    const QString text = QString::fromUtf8(xml);
    if (text.toUtf8() != xml) { if (error) *error = QStringLiteral("Chỉ hỗ trợ XML UTF-8."); return {}; }
    const auto scan = scanAnimation(text);
    if (!scan.error.isEmpty() || scan.hasTiming) {
        if (error) *error = scan.error.isEmpty() ? QStringLiteral("Slide đã có animation/timing; chưa hỗ trợ sửa hoặc ghép hiệu ứng.") : scan.error;
        return {};
    }
    if (scan.shapes.isEmpty() && error) *error = QStringLiteral("Không có hộp chữ, hình hoặc ảnh độc lập có thể tạo hiệu ứng.");
    return scan.shapes;
}
Result planAnimation(const QList<AnimationOptions>& effects,QList<AnimationTiming>* timing) {
    if(!timing || effects.isEmpty() || effects.size()>100)return fail(QStringLiteral("Chọn từ 1 đến 100 hoạt ảnh."));
    if(effects.first().trigger!=AnimationTrigger::Click)return fail(QStringLiteral("Dòng đầu phải bắt đầu khi nhấp chuột."));
    QList<AnimationTiming> plan;QHash<unsigned,AnimationTiming> entrances;QSet<unsigned> exits;
    for(const auto& o:effects){
        if(!o.shapeId || o.durationMs<100 || o.durationMs>10000 || o.delayMs<0 || o.delayMs>30000)
            return fail(QStringLiteral("Thông số hoạt ảnh không hợp lệ."));
        if(exits.contains(o.shapeId) || (!o.exit && entrances.contains(o.shapeId)))
            return fail(QStringLiteral("Mỗi đối tượng tối đa một hiệu ứng vào rồi một hiệu ứng ra; không thêm sau hiệu ứng ra."));
        AnimationTiming t;
        if(o.trigger==AnimationTrigger::Click){t.click=plan.isEmpty()?1:plan.last().click+1;t.startMs=o.delayMs;}
        else if(o.trigger==AnimationTrigger::WithPrevious){t.click=plan.last().click;t.startMs=plan.last().startMs+o.delayMs;}
        else if(o.trigger==AnimationTrigger::AfterPrevious){t.click=plan.last().click;t.startMs=plan.last().endMs+o.delayMs;}
        else return fail(QStringLiteral("Chế độ bắt đầu không hợp lệ."));
        t.endMs=t.startMs+(o.fade?o.durationMs+(o.exit?1:0):1);
        if(t.endMs>600000)return fail(QStringLiteral("Mỗi nhóm nhấp chuột tối đa 600 giây."));
        if(o.exit){
            if(entrances.contains(o.shapeId)){
                const auto entry=entrances.value(o.shapeId);
                if(t.click<entry.click || (t.click==entry.click && t.startMs<entry.endMs))
                    return fail(QStringLiteral("Hiệu ứng ra phải bắt đầu sau khi hiệu ứng vào kết thúc."));
            }
            exits.insert(o.shapeId);
        }else entrances.insert(o.shapeId,t);
        plan.append(t);
    }
    *timing=plan;return {true,{}};
}

Result patchAnimationSequenceXml(const QByteArray& xml, const QList<AnimationOptions>& effects, QByteArray* output) {
    if(!output)return fail(QStringLiteral("Thiếu bộ đệm đầu ra."));
    QList<AnimationTiming> plan;
    const auto planned=planAnimation(effects,&plan);
    if(!planned.ok)return planned;
    QString text = QString::fromUtf8(xml);
    if (text.toUtf8() != xml) return fail(QStringLiteral("Chỉ hỗ trợ XML UTF-8."));
    const auto scan = scanAnimation(text);
    if (!scan.error.isEmpty()) return fail(scan.error);
    if (scan.hasTiming) return fail(QStringLiteral("Slide đã có timing; không ghi đè."));
    QSet<unsigned> valid;
    for(const auto& shape:scan.shapes)valid.insert(shape.id);
    QString sequence,children;
    int nextId=3,clickId=0;
    auto flush=[&]{
        if(children.isEmpty())return;
        sequence+=QStringLiteral("<p:par><p:cTn id=\"%1\" fill=\"hold\"><p:stCondLst><p:cond delay=\"indefinite\"/></p:stCondLst><p:childTnLst>%2</p:childTnLst></p:cTn></p:par>").arg(clickId).arg(children);
        children.clear();
    };
    for(int index=0;index<effects.size();++index) {
        const auto& o=effects[index];
        if(!valid.contains(o.shapeId))return fail(QStringLiteral("Đối tượng không còn trên slide."));
        if(o.trigger==AnimationTrigger::Click){flush();clickId=nextId++;}
        const int groupId=nextId++,effectId=nextId++,visibilityId=nextId++;
        const QString target = QStringLiteral("<p:tgtEl><p:spTgt spid=\"%1\"/></p:tgtEl>").arg(o.shapeId);
        // Entrance becomes visible at its start; exit becomes hidden at its end.
        const int visibilityDelay = o.exit && o.fade ? o.durationMs : 0;
        const QString visibility = o.exit ? QStringLiteral("hidden") : QStringLiteral("visible");
        QString behavior = QStringLiteral("<p:set><p:cBhvr><p:cTn id=\"%1\" dur=\"1\" fill=\"hold\"><p:stCondLst><p:cond delay=\"%2\"/></p:stCondLst></p:cTn>%3<p:attrNameLst><p:attrName>style.visibility</p:attrName></p:attrNameLst></p:cBhvr><p:to><p:strVal val=\"%4\"/></p:to></p:set>").arg(visibilityId).arg(visibilityDelay).arg(target,visibility);
        if (o.fade) {
            const int fadeId = nextId++;
            const QString direction = o.exit ? QStringLiteral("out") : QStringLiteral("in");
            behavior += QStringLiteral("<p:animEffect transition=\"%1\" filter=\"fade\"><p:cBhvr><p:cTn id=\"%2\" dur=\"%3\"/>%4</p:cBhvr></p:animEffect>").arg(direction).arg(fadeId).arg(o.durationMs).arg(target);
        }
        // Each click anchors a set of parallel time containers. Relative delays
        // are resolved by the plan; effect leaf node types retain editable trigger semantics.
        const QString trigger=o.trigger==AnimationTrigger::Click?QStringLiteral("clickEffect"):
            o.trigger==AnimationTrigger::WithPrevious?QStringLiteral("withEffect"):QStringLiteral("afterEffect");
        children+=QStringLiteral(
            "<p:par><p:cTn id=\"%1\" fill=\"hold\"><p:stCondLst><p:cond delay=\"%2\"/></p:stCondLst><p:childTnLst>"
            "<p:par><p:cTn id=\"%3\" presetID=\"%4\" presetClass=\"%5\" presetSubtype=\"0\" fill=\"hold\" nodeType=\"%6\">"
            "<p:stCondLst><p:cond delay=\"0\"/></p:stCondLst><p:childTnLst>%7</p:childTnLst></p:cTn></p:par>"
            "</p:childTnLst></p:cTn></p:par>")
            .arg(groupId).arg(plan[index].startMs).arg(effectId).arg(o.fade?10:1)
            .arg(o.exit ? QStringLiteral("exit") : QStringLiteral("entr"), trigger, behavior);
    }
    flush();
    const QString timing = QStringLiteral(
        "<p:timing xmlns:p=\"%1\"><p:tnLst><p:par>"
        "<p:cTn id=\"1\" dur=\"indefinite\" restart=\"never\" nodeType=\"tmRoot\"><p:childTnLst>"
        "<p:seq concurrent=\"1\" nextAc=\"seek\"><p:cTn id=\"2\" dur=\"indefinite\" nodeType=\"mainSeq\"><p:childTnLst>"
        "%2</p:childTnLst></p:cTn>"
        "<p:prevCondLst><p:cond evt=\"onPrev\" delay=\"0\"><p:tgtEl><p:sldTgt/></p:tgtEl></p:cond></p:prevCondLst>"
        "<p:nextCondLst><p:cond evt=\"onNext\" delay=\"0\"><p:tgtEl><p:sldTgt/></p:tgtEl></p:cond></p:nextCondLst>"
        "</p:seq></p:childTnLst></p:cTn></p:par></p:tnLst></p:timing>").arg(P).arg(sequence);
    text.insert(scan.insertion, timing);
    const auto after = scanAnimation(text);
    if (!after.error.isEmpty() || !after.hasTiming || after.shapes.size() != scan.shapes.size())
        return fail(QStringLiteral("XML animation đầu ra không qua kiểm tra."));
    *output = text.toUtf8(); return {true, {}};
}
Result patchAnimationXml(const QByteArray& xml, const AnimationOptions& o, QByteArray* output) {
    return patchAnimationSequenceXml(xml, {o}, output);
}
QStringList masterEntries(const QString& source, QString* error) {
    auto bad = [&](const QString& why) { if (error) *error = why; return QStringList{}; };
    QString e;
    if (slideEntries(source, &e).isEmpty()) return bad(e);
    ft::ZipReader z;
    if (!z.open(source, &e)) return bad(e);
    QByteArray pres, rel;
    if (!read(z, QStringLiteral("ppt/presentation.xml"), &pres, &e) ||
        !read(z, QStringLiteral("ppt/_rels/presentation.xml.rels"), &rel, &e)) return bad(e);
    QHash<QString,QString> map;
    QXmlStreamReader rr(rel);
    while (!rr.atEnd()) {
        rr.readNext();
        if (rr.isDTD()) return bad(QStringLiteral("DTD không hỗ trợ."));
        if (!rr.isStartElement() || rr.namespaceUri() != PR || rr.name() != QLatin1String("Relationship")) continue;
        const auto a = rr.attributes();
        if (a.value(QStringLiteral("Type")) != R + QStringLiteral("/slideMaster")) continue;
        const QString id = a.value(QStringLiteral("Id")).toString();
        const QString raw = a.value(QStringLiteral("Target")).toString();
        const QString path = QDir::cleanPath(raw.startsWith(QLatin1Char('/')) ? raw.mid(1) : QStringLiteral("ppt/") + raw);
        if (id.isEmpty() || map.contains(id) || a.value(QStringLiteral("TargetMode")) == QLatin1String("External") ||
            !path.startsWith(QLatin1String("ppt/slideMasters/")) || !path.endsWith(QLatin1String(".xml")) || !z.find(path))
            return bad(QStringLiteral("Quan hệ master không hợp lệ."));
        map.insert(id,path);
    }
    if (rr.hasError()) return bad(rr.errorString());
    QStringList entries;
    QXmlStreamReader sr(pres);
    int depth=0, listDepth=-1;
    while (!sr.atEnd()) {
        sr.readNext();
        if (sr.isDTD()) return bad(QStringLiteral("DTD không hỗ trợ."));
        if (sr.isStartElement()) {
            ++depth;
            if (depth==2 && sr.namespaceUri()==P && sr.name()==QLatin1String("sldMasterIdLst")) listDepth=depth;
            if (listDepth>0 && depth==listDepth+1 && sr.namespaceUri()==P && sr.name()==QLatin1String("sldMasterId")) {
                const auto id=sr.attributes().value(R,QStringLiteral("id")).toString();
                if (!map.contains(id) || entries.contains(map.value(id))) return bad(QStringLiteral("Không xác minh được master."));
                entries << map.value(id);
            }
        } else if (sr.isEndElement()) { if (depth==listDepth) listDepth=-1; --depth; }
    }
    if (sr.hasError() || entries.isEmpty()) return bad(QStringLiteral("Không có danh sách Slide Master hợp lệ."));
    if (error) error->clear();
    return entries;
}
Result patchMasterBackground(const QByteArray& xml, const QString& rgb, QByteArray* output) {
    static const QRegularExpression hex(QStringLiteral("^[0-9A-Fa-f]{6}$"));
    if (!output || !hex.match(rgb).hasMatch()) return fail(QStringLiteral("Màu RGB không hợp lệ."));
    QString text=QString::fromUtf8(xml);
    if (text.toUtf8()!=xml) return fail(QStringLiteral("Chỉ hỗ trợ XML UTF-8."));
    QXmlStreamReader xr(text);
    int depth=0, common=0, bgDepth=0;
    qsizetype insert=-1, oldStart=-1, oldEnd=-1;
    bool root=false, tree=false;
    auto tokenStart=[&]() -> qsizetype {
        const auto off=xr.characterOffset();
        if (off<=0 || off>text.size() || text.at(off-1)!=QLatin1Char('>')) return -1;
        return text.lastIndexOf(QLatin1Char('<'),off-1);
    };
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isDTD()) return fail(QStringLiteral("DTD không hỗ trợ."));
        if (xr.isStartElement()) {
            ++depth;
            if (depth==1) {
                root=xr.namespaceUri()==P && xr.name()==QLatin1String("sldMaster");
                if (!root) return fail(QStringLiteral("Không phải Slide Master."));
            }
            if (depth==2 && xr.namespaceUri()==P && xr.name()==QLatin1String("cSld")) common=depth;
            if (common && depth==common+1) {
                if (xr.namespaceUri()!=P) return fail(QStringLiteral("Cấu trúc master mở rộng chưa hỗ trợ."));
                if (xr.name()==QLatin1String("bg")) {
                    if (oldStart>=0) return fail(QStringLiteral("Có nhiều nền master."));
                    oldStart=tokenStart(); bgDepth=depth;
                    if (oldStart<0) return fail(QStringLiteral("Không xác minh vị trí nền."));
                }
                if (xr.name()==QLatin1String("spTree")) { tree=true; insert=tokenStart(); }
            }
        } else if (xr.isEndElement()) {
            if (bgDepth && depth==bgDepth) { oldEnd=xr.characterOffset(); bgDepth=0; }
            if (common && depth==common) common=0;
            --depth;
        }
    }
    if (xr.hasError() || !root || !tree || insert<0 || (oldStart>=0 && (oldEnd<=oldStart || oldEnd>text.size())))
        return fail(QStringLiteral("XML master không hợp lệ."));
    const QString fragment=QStringLiteral("<p:bg xmlns:p=\"%1\" xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"><p:bgPr><a:solidFill><a:srgbClr val=\"%2\"/></a:solidFill><a:effectLst/></p:bgPr></p:bg>").arg(P,rgb.toUpper());
    if (oldStart>=0) text.replace(oldStart,oldEnd-oldStart,fragment); else text.insert(insert,fragment);
    QXmlStreamReader verify(text);
    while (!verify.atEnd()) verify.readNext();
    if (verify.hasError()) return fail(QStringLiteral("XML đầu ra không hợp lệ."));
    *output=text.toUtf8(); return {true,{}};
}
static Result createPatchedEntryCopy(const QString& source, const QString& destination, const QString& entry,
                                const std::function<Result(const QByteArray&, QByteArray*)>& patch) {
    const QFileInfo dst(destination);
    if (!source.endsWith(QLatin1String(".pptx"), Qt::CaseInsensitive) || !destination.endsWith(QLatin1String(".pptx"), Qt::CaseInsensitive) ||
        dst.exists() || dst.isSymLink() || !QDir(dst.absolutePath()).exists())
        return fail(QStringLiteral("Chọn nguồn PPTX và tên PPTX mới chưa tồn tại."));
    QString error;
    ft::ZipReader original;
    if (!original.open(source, &error)) return fail(error);
    QByteArray xml, patched;
    if (!read(original, entry, &xml, &error)) return fail(error);
    const auto result = patch(xml, &patched);
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
static Result createPatchedCopy(const QString& source, const QString& destination, int slide1,
                                const std::function<Result(const QByteArray&, QByteArray*)>& patch) {
    QString error;
    const auto slides=slideEntries(source,&error);
    if (slide1<1 || slide1>slides.size()) return fail(error.isEmpty()?QStringLiteral("Slide không tồn tại."):error);
    return createPatchedEntryCopy(source,destination,slides.at(slide1-1),patch);
}
Result createMasterBackgroundCopy(const QString& source,const QString& destination,const QString& entry,const QString& rgb) {
    QString error;
    const auto masters=masterEntries(source,&error);
    if (!masters.contains(entry)) return fail(error.isEmpty()?QStringLiteral("Master không tồn tại."):error);
    return createPatchedEntryCopy(source,destination,entry,[&](const QByteArray& xml,QByteArray* out){return patchMasterBackground(xml,rgb,out);});
}
Result createCopy(const QString& source, const QString& destination, int slide1, const Options& options) {
    return createPatchedCopy(source, destination, slide1, [&](const QByteArray& xml, QByteArray* out) { return patchXml(xml, options, out); });
}
Result createAnimationCopy(const QString& source, const QString& destination, int slide1, const AnimationOptions& options) {
    return createAnimationSequenceCopy(source, destination, slide1, {options});
}
Result createAnimationSequenceCopy(const QString& source, const QString& destination, int slide1, const QList<AnimationOptions>& effects) {
    return createPatchedCopy(source, destination, slide1, [&](const QByteArray& xml, QByteArray* out) { return patchAnimationSequenceXml(xml, effects, out); });
}
}
