// SPDX-License-Identifier: MPL-2.0
// dung-margintest: kiểm thử đặt lề bằng cách vá XML (PageMargins).
//   dung-margintest                          → kiểm thử đơn vị (chỉ QtCore, không cần engine)
//   dung-margintest --docx-dir <thư mục>     → thêm: vá thử mọi .docx thật trong thư mục (đọc lại, kiểm gói ZIP)
//   dung-margintest --program <program> --out <thư mục> [--docx <tệp.docx>]
//                                            → thêm: chạy trên ENGINE THẬT: tạo tài liệu → bản phục hồi → vá lề → nạp lại vào engine
//                                              → Lưu → đọc lại lề trong tệp đã lưu và so với giá trị đặt (đúng quy trình app dùng)
#include "../../app/src/word/PageMargins.h"
#include "../../app/src/tools/filetools/ZipLite.h"

#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>

#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW
#endif

using namespace dung::word;

namespace {
int g_fail = 0, g_pass = 0;
void check(bool ok, const QString& what, const QString& ev = QString()) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what.toStdString();
    if (!ev.isEmpty()) std::cout << "  {" << ev.toStdString() << "}";
    std::cout << std::endl;
    ok ? ++g_pass : ++g_fail;
}

// ---- dựng gói ZIP thử (có mục nén, mục lưu nguyên, mục dùng "data descriptor")
struct Ent { QString name; QByteArray data; bool deflate = false; bool descriptor = false; };
void p16(QByteArray& b, quint16 v) { b.append(char(v & 0xFF)); b.append(char(v >> 8)); }
void p32(QByteArray& b, quint32 v) { for (int i = 0; i < 4; ++i) b.append(char((v >> (8 * i)) & 0xFF)); }
bool makeZip(const QString& path, const QVector<Ent>& ents, const QByteArray& comment = QByteArray()) {
    QByteArray body, cd;
    for (const Ent& e : ents) {
        QByteArray payload = e.data;
        quint16 method = 0;
        if (e.deflate) {
            const QByteArray z = qCompress(e.data, 6);
            payload = z.mid(6, z.size() - 10);
            method = 8;
        }
        const quint32 crc = ft::zipCrc32(e.data);
        const quint32 off = quint32(body.size());
        const QByteArray nm = e.name.toUtf8();
        p32(body, 0x04034b50u); p16(body, 20); p16(body, e.descriptor ? 8 : 0); p16(body, method); p16(body, 0x6000); p16(body, 0x5821);
        p32(body, e.descriptor ? 0 : crc); p32(body, e.descriptor ? 0 : quint32(payload.size())); p32(body, e.descriptor ? 0 : quint32(e.data.size()));
        p16(body, quint16(nm.size())); p16(body, 0);
        body.append(nm); body.append(payload);
        if (e.descriptor) { p32(body, 0x08074b50u); p32(body, crc); p32(body, quint32(payload.size())); p32(body, quint32(e.data.size())); }
        p32(cd, 0x02014b50u); p16(cd, 20); p16(cd, 20); p16(cd, e.descriptor ? 8 : 0); p16(cd, method); p16(cd, 0x6000); p16(cd, 0x5821);
        p32(cd, crc); p32(cd, quint32(payload.size())); p32(cd, quint32(e.data.size()));
        p16(cd, quint16(nm.size())); p16(cd, 0); p16(cd, 0); p16(cd, 0); p16(cd, 0); p32(cd, 0); p32(cd, off);
        cd.append(nm);
    }
    QByteArray all = body;
    const quint32 cdOff = quint32(all.size());
    all.append(cd);
    p32(all, 0x06054b50u); p16(all, 0); p16(all, 0); p16(all, quint16(ents.size())); p16(all, quint16(ents.size()));
    p32(all, quint32(cd.size())); p32(all, cdOff); p16(all, quint16(comment.size())); all.append(comment);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(all) == all.size();
}
QByteArray slurp(const QString& p) { QFile f(p); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }

const char* NS = "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"";
QByteArray docHead(const char* extra = "") {
    return QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<w:document ") + NS + " " + extra + "><w:body>";
}
const QByteArray kTail = "</w:body></w:document>";
const char* kSec1 =
    "<w:p><w:pPr><w:sectPr w:rsidR=\"0001\"><w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
    "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\" w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/>"
    "<w:cols w:space=\"708\"/></w:sectPr></w:pPr><w:r><w:t>Phần 1</w:t></w:r></w:p>";
const char* kComment = "<!-- <w:sectPr><w:pgMar w:left=\"1\"/></w:sectPr> -->";
const char* kSec2 =
    "<w:p><w:pPr><w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/>"
    "<w:pgMar w:top='720' w:right='720' w:bottom='720' w:left='720'/>"
    "<w:sectPrChange w:id=\"1\" w:author=\"a\"><w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/>"
    "<w:pgMar w:top=\"999\" w:right=\"999\" w:bottom=\"999\" w:left=\"999\"/></w:sectPr></w:sectPrChange></w:sectPr></w:pPr></w:p>";
const char* kSec3 =
    "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:cols w:space=\"708\"/><w:docGrid w:linePitch=\"360\"/></w:sectPr>";

QByteArray multiDoc() { return docHead() + kSec1 + kComment + kSec2 + "<w:p><w:r><w:t>x</w:t></w:r></w:p>" + kSec3 + kTail; }

MarginSpec vn() {
    MarginSpec s;
    s.left = mmToTwips(30); s.right = mmToTwips(15); s.top = mmToTwips(20); s.bottom = mmToTwips(20);
    return s;
}

// ----------------------------------------------------------------------------- kiểm thử đơn vị
void testUnits() {
    check(mmToTwips(25.4) == 1440, "25,4 mm = 1440 twip");
    check(mmToTwips(30) == 1701 && mmToTwips(15) == 850 && mmToTwips(20) == 1134, "30/15/20 mm = 1701/850/1134 twip",
          QString("%1/%2/%3").arg(mmToTwips(30)).arg(mmToTwips(15)).arg(mmToTwips(20)));
    check(mmToTwips(12.7) == 720, "12,7 mm = 720 twip");
    check(cmToTwips(2.54) == 1440 && inchToTwips(1) == 1440 && inchToTwips(0.5) == 720, "cm/inch → twip");
    check(qAbs(twipsToMm(1440) - 25.4) < 1e-9, "1440 twip = 25,4 mm");
    check(mmToTwips(0) == 0, "0 mm = 0 twip");
}

void testScan() {
    const ReadResult r = readDocumentXmlMargins(multiDoc());
    check(r.ok && r.sections.size() == 3, "đọc 3 phần, bỏ qua sectPr lồng trong sectPrChange và trong chú thích XML",
          QString("ok=%1 n=%2 %3").arg(r.ok).arg(r.sections.size()).arg(r.error));
    if (r.sections.size() == 3) {
        const auto& a = r.sections[0]; const auto& b = r.sections[1]; const auto& c = r.sections[2];
        check(a.left == 1440 && a.header == 708 && a.pageW == 11906 && a.pageH == 16838, "phần 1: lề/khổ giấy đúng");
        check(b.left == 720 && b.top == 720, "phần 2: đọc được thuộc tính nháy đơn");
        check(!c.hasPgMar && c.left == -1, "phần 3: chưa có pgMar");
    }
    check(r.hasBodySectPr, "nhận ra sectPr cuối thân tài liệu");
}

void testPatchXml() {
    QByteArray out;
    PatchResult p = patchDocumentXml(multiDoc(), vn(), &out);
    check(p.ok && p.sectionsPatched == 2 && p.sectionsCreated == 1, "vá 2 phần có sẵn + tạo 1 phần thiếu pgMar",
          QString("ok=%1 patched=%2 created=%3 %4").arg(p.ok).arg(p.sectionsPatched).arg(p.sectionsCreated).arg(p.error));
    const ReadResult r = readDocumentXmlMargins(out);
    bool all = r.ok && r.sections.size() == 3;
    for (const auto& s : r.sections)
        all = all && s.left == 1701 && s.right == 850 && s.top == 1134 && s.bottom == 1134;
    check(all, "đọc lại: cả 3 phần đều đúng lề mới (30/15/20/20 mm)");
    check(r.ok && r.sections[0].header == 708 && r.sections[0].footer == 708 && r.sections[0].gutter == 0,
          "giữ nguyên header/footer/gutter của phần 1");
    check(out.contains("w:top=\"999\" w:right=\"999\" w:bottom=\"999\" w:left=\"999\""), "KHÔNG đụng sectPr lồng trong sectPrChange");
    check(out.contains(kComment), "KHÔNG đụng nội dung chú thích XML");
    check(out.contains("<w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:top=\"1134\" w:right=\"850\" w:bottom=\"1134\" w:left=\"1701\" w:header=\"720\"") &&
              out.contains("w:gutter=\"0\"/><w:cols w:space=\"708\"/><w:docGrid"),
          "pgMar tạo mới nằm ngay sau pgSz, trước cols (đúng thứ tự lược đồ)");
    check(out.contains("w:rsidR=\"0001\"") && out.contains("Phần 1"), "giữ nguyên thuộc tính khác và nội dung chữ");
    check(out.contains("w:top='1134'") || out.contains("w:top='1134"), "giữ kiểu nháy đơn của thuộc tính gốc");
    // Chỉ khác ở các thuộc tính lề: xoá mọi số trong thuộc tính lề rồi so sánh phần còn lại không khả thi → so độ dài hợp lý
    check(out.size() > multiDoc().size(), "XML mới dài hơn (có pgMar tạo thêm)");

    // Custom dialog uses tenths of a millimeter; verify asymmetric roundtrip.
    MarginSpec custom;
    custom.left=mmToTwips(31.5);custom.right=mmToTwips(17.2);
    custom.top=mmToTwips(22.3);custom.bottom=mmToTwips(19.8);
    QByteArray customXml;
    const auto cp=patchDocumentXml(multiDoc(),custom,&customXml);
    const auto cr=readDocumentXmlMargins(customXml);
    bool customOk=cp.ok&&cr.ok&&cr.sections.size()==3;
    for(const auto& section:cr.sections)
        customOk=customOk&&section.left==*custom.left&&section.right==*custom.right&&
            section.top==*custom.top&&section.bottom==*custom.bottom;
    check(customOk&&customXml.contains("Phần 1"),"lề tùy chỉnh lẻ mm: tất cả section đúng, giữ nội dung");

    // Chỉ lề trên/dưới: giữ trái/phải
    MarginSpec ul; ul.top = 1000; ul.bottom = 1100;
    QByteArray out2;
    p = patchDocumentXml(multiDoc(), ul, &out2);
    const ReadResult r2 = readDocumentXmlMargins(out2);
    check(p.ok && r2.ok && r2.sections[0].left == 1440 && r2.sections[0].top == 1000 && r2.sections[0].bottom == 1100 &&
              r2.sections[1].left == 720 && r2.sections[1].top == 1000,
          "chỉ đặt trên/dưới: trái/phải giữ nguyên");

    // sectPr tự đóng
    QByteArray selfClose = docHead() + "<w:p/><w:sectPr w:rsidR=\"AB\"/>" + kTail, o3;
    p = patchDocumentXml(selfClose, vn(), &o3);
    const ReadResult r3 = readDocumentXmlMargins(o3);
    check(p.ok && r3.ok && r3.sections.size() == 1 && r3.sections[0].left == 1701 && o3.contains("</w:sectPr>") && !o3.contains("<w:sectPr w:rsidR=\"AB\"/>"),
          "sectPr tự đóng <w:sectPr/> được mở ra và thêm pgMar");

    // Không có sectPr nào
    QByteArray none = docHead() + "<w:p><w:r><w:t>Xin chào</w:t></w:r></w:p>" + kTail, o4;
    p = patchDocumentXml(none, vn(), &o4);
    const ReadResult r4 = readDocumentXmlMargins(o4);
    check(p.ok && r4.ok && r4.sections.size() == 1 && r4.sections[0].top == 1134 && o4.contains("Xin chào") && !p.warnings.isEmpty(),
          "tài liệu không có sectPr: tạo sectPr cuối thân kèm cảnh báo");

    // Tiền tố khác "w"
    const QByteArray nsAlt =
        "<?xml version=\"1.0\"?><ns0:document xmlns:ns0=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><ns0:body>"
        "<ns0:sectPr><ns0:pgSz ns0:w=\"11906\" ns0:h=\"16838\"/><ns0:pgMar ns0:top=\"1\" ns0:left=\"2\"/></ns0:sectPr></ns0:body></ns0:document>";
    QByteArray o5;
    p = patchDocumentXml(nsAlt, vn(), &o5);
    const ReadResult r5 = readDocumentXmlMargins(o5);
    check(p.ok && r5.ok && r5.sections.size() == 1 && r5.sections[0].left == 1701 && r5.sections[0].right == 850 && o5.contains("ns0:right=\"850\""),
          "tiền tố ns0: thuộc tính thiếu được thêm đúng tiền tố");

    // Không gian tên mặc định + tiền tố cho thuộc tính
    const QByteArray nsDef =
        "<document xmlns=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><body>"
        "<sectPr><pgSz w:w=\"11906\" w:h=\"16838\"/><pgMar w:top=\"1\" w:left=\"2\" w:right=\"3\" w:bottom=\"4\"/></sectPr></body></document>";
    QByteArray o6;
    p = patchDocumentXml(nsDef, vn(), &o6);
    const ReadResult r6 = readDocumentXmlMargins(o6);
    check(p.ok && r6.ok && r6.sections.size() == 1 && r6.sections[0].left == 1701, "không gian tên mặc định cho phần tử + w: cho thuộc tính");

    // Không gian tên mặc định không có tiền tố thuộc tính: từ chối, không đoán
    const QByteArray nsDefOnly =
        "<document xmlns=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><body><sectPr><pgMar/></sectPr></body></document>";
    QByteArray o7;
    p = patchDocumentXml(nsDefOnly, vn(), &o7);
    check(!p.ok && !p.error.isEmpty(), "không gian tên mặc định mà không có tiền tố thuộc tính: từ chối rõ ràng", p.error);

    // Không phải WordprocessingML
    p = patchDocumentXml("<root><a/></root>", vn(), &o7);
    check(!p.ok, "XML không phải WordprocessingML: từ chối");
    // XML hỏng
    p = patchDocumentXml(docHead() + "<w:p>", vn(), &o7);
    check(!p.ok, "XML thiếu thẻ đóng: từ chối", p.error);

    // Lề không hợp lệ
    MarginSpec bad = vn(); bad.left = -5;
    p = patchDocumentXml(multiDoc(), bad, &o7);
    check(!p.ok, "lề âm: từ chối");
    bad = vn(); bad.left = 9000; bad.right = 9000;
    p = patchDocumentXml(multiDoc(), bad, &o7);
    check(!p.ok && p.error.contains("quá lớn"), "lề trái+phải ≥ bề rộng giấy: từ chối kèm lý do", p.error);
    bad = MarginSpec();
    p = patchDocumentXml(multiDoc(), bad, &o7);
    check(!p.ok, "không có cạnh nào để đặt: từ chối");
    bad = vn(); bad.top = 40000;
    p = patchDocumentXml(multiDoc(), bad, &o7);
    check(!p.ok, "lề vượt 22 inch: từ chối");

    // Giá trị không phải số nguyên (vd. 2cm): đọc ra -1, vá vẫn thay bằng twip
    const QByteArray cm = docHead() + "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:left=\"2cm\" w:top=\"1440\"/></w:sectPr>" + kTail;
    QByteArray o8;
    const ReadResult rc = readDocumentXmlMargins(cm);
    p = patchDocumentXml(cm, vn(), &o8);
    const ReadResult rc2 = readDocumentXmlMargins(o8);
    check(rc.ok && rc.sections[0].left == -1 && p.ok && rc2.sections[0].left == 1701, "giá trị có đơn vị (2cm) đọc là chưa rõ; vá thay bằng twip");
}

// ----------------------------------------------------------------------------- kiểm thử gói ZIP
void testZip(const QString& tmp) {
    QByteArray media(60000, 'x');
    for (int i = 0; i < media.size(); ++i) media[i] = char((i * 31 + i / 7) & 0xFF);
    QVector<Ent> ents = {
        {"[Content_Types].xml", "<Types/>", false, false},
        {"word/document.xml", multiDoc(), true, false},
        {"word/media/a.bin", media, true, true},          // nén + data descriptor
        {"word/styles.xml", QByteArray(2000, 'S'), true, false},
        {"docProps/core.xml", "<cp/>", false, false},
    };
    const QString src = tmp + "/nguon.docx", dst = tmp + "/dich.docx";
    check(makeZip(src, ents, "ghi chu zip"), "dựng gói DOCX thử");
    const QByteArray srcBytes = slurp(src);
    PatchResult p = patchDocxMargins(src, dst, vn());
    check(p.ok, "vá DOCX thử", p.error);
    check(slurp(src) == srcBytes, "tệp nguồn KHÔNG đổi một byte");

    ft::ZipReader zs, zd; QString err;
    check(zs.open(src, &err) && zd.open(dst, &err), "mở cả hai gói", err);
    bool rawSame = true, allOk = true;
    const QByteArray dstBytes = slurp(dst);
    for (const auto& e : zs.entries()) {
        const auto* n = zd.find(e.name);
        if (!n) { allOk = false; continue; }
        QByteArray a, b;
        const bool ok = zs.read(e, &a, &err) && zd.read(*n, &b, &err);
        if (e.name == "word/document.xml") continue;
        allOk = allOk && ok && a == b;
        // nguyên byte: bản ghi cục bộ (tiêu đề + dữ liệu) trùng khít
        const qint64 len = 30 + e.name.toUtf8().size() + 0 + qint64(e.csize) + (e.name.endsWith("a.bin") ? 16 : 0);
        rawSame = rawSame && srcBytes.mid(int(e.localOffset), int(len)) == dstBytes.mid(int(n->localOffset), int(len));
    }
    check(allOk, "mọi mục khác document.xml đọc lại đúng nội dung");
    check(rawSame, "mọi mục khác document.xml được sao chép NGUYÊN BYTE (kể cả mục có data descriptor)");
    check(dstBytes.endsWith("ghi chu zip"), "giữ nguyên chú thích của gói ZIP");
    const ReadResult r = readDocxMargins(dst);
    check(r.ok && r.sections.size() == 3 && r.sections[0].left == 1701, "readDocxMargins đọc lại lề từ tệp đã vá");

    // Vá lần 2 trên chính kết quả (chuỗi thao tác) với lề khác
    MarginSpec nm; nm.left = nm.right = nm.top = nm.bottom = 1440;
    const QString dst2 = tmp + "/dich2.docx";
    p = patchDocxMargins(dst, dst2, nm);
    const ReadResult r2 = readDocxMargins(dst2);
    check(p.ok && r2.ok && r2.sections[2].top == 1440 && r2.sections[0].right == 1440, "vá chuỗi: lề mới thay lề cũ", p.error);

    // Từ chối ghi đè nguồn
    p = patchDocxMargins(src, src, vn());
    check(!p.ok && slurp(src) == srcBytes, "đích trùng nguồn: từ chối, nguồn nguyên vẹn");
    // Không phải ZIP
    QFile bad(tmp + "/hong.docx"); bad.open(QIODevice::WriteOnly); bad.write("khong phai zip"); bad.close();
    p = patchDocxMargins(tmp + "/hong.docx", tmp + "/hong-out.docx", vn());
    check(!p.ok && !QFileInfo::exists(tmp + "/hong-out.docx"), "không phải ZIP: từ chối, không để lại tệp đích");
    // Thiếu document.xml
    makeZip(tmp + "/thieu.docx", {{"a.xml", "<a/>", false, false}});
    p = patchDocxMargins(tmp + "/thieu.docx", tmp + "/thieu-out.docx", vn());
    check(!p.ok && !QFileInfo::exists(tmp + "/thieu-out.docx"), "thiếu word/document.xml: từ chối, không để lại tệp đích");
    // Lề vô lý: không để lại tệp đích
    MarginSpec huge = vn(); huge.left = 9000; huge.right = 9000;
    p = patchDocxMargins(src, tmp + "/lon-out.docx", huge);
    check(!p.ok && !QFileInfo::exists(tmp + "/lon-out.docx") && !QFileInfo::exists(tmp + "/lon-out.docx.dung-tmp"),
          "lề vô lý: từ chối, không để lại tệp đích hay tệp tạm");
    // Tên có dấu cách + tiếng Việt
    const QString vnName = tmp + QString::fromUtf8("/Tài liệu thử nghiệm.docx");
    makeZip(vnName, ents);
    p = patchDocxMargins(vnName, tmp + QString::fromUtf8("/Tài liệu - lề mới.docx"), vn());
    check(p.ok, "đường dẫn có dấu tiếng Việt và dấu cách", p.error);
}

// Vá thử mọi .docx thật
void testRealDocs(const QString& dir, const QString& tmp) {
    int n = 0, ok = 0;
    QStringList failed;
    QDirIterator it(dir, {"*.docx"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        ++n;
        const QString dst = tmp + "/real-" + QString::number(n) + ".docx";
        const ReadResult before = readDocxMargins(f);
        const PatchResult p = patchDocxMargins(f, dst, vn());
        const ReadResult after = readDocxMargins(dst);
        bool good = before.ok && p.ok && after.ok && !after.sections.isEmpty();
        for (const auto& s : after.sections) good = good && s.left == 1701 && s.right == 850 && s.top == 1134 && s.bottom == 1134;
        if (good) ++ok; else failed << QFileInfo(f).fileName() + ": " + (p.error.isEmpty() ? before.error + after.error : p.error);
        QFile::remove(dst);
    }
    check(n > 0, QString("có tệp .docx thật để thử trong %1").arg(dir), QString::number(n));
    check(ok == n, QString("vá và đọc lại đúng lề trên %1/%2 tệp .docx thật").arg(ok).arg(n), failed.join("; "));
}

// ----------------------------------------------------------------------------- kiểm thử trên engine thật
QString plainText(const QString& docx) {
    ft::ZipReader z; QString err; QByteArray xml;
    if (!z.open(docx, &err)) return {};
    const auto* e = z.find("word/document.xml");
    if (!e || !z.read(*e, &xml, &err, 256LL * 1024 * 1024)) return {};
    static const QRegularExpression re("<w:t(?:\\s[^>]*)?>([^<]*)</w:t>");
    QString t;
    auto it = re.globalMatch(QString::fromUtf8(xml));
    while (it.hasNext()) t += it.next().captured(1);
    return t;
}

// Một vòng đúng như app: bản phục hồi của tài liệu đang mở → vá → nạp lại → Lưu → đọc lề ở tệp đã lưu.
void roundTrip(dung::Engine& eng, dung::Document& src, const QString& tag, const MarginSpec& spec, const QString& out,
               const QString& expectText) {
    const QString dir = out;
    const QString rec = dir + "/rec-" + tag + ".docx", patched = dir + "/patched-" + tag + ".docx", res = dir + "/ketqua-" + tag + ".docx";
    std::string err;
    const bool saved = src.saveRecoveryCopy(dung::fromUtf8(rec.toUtf8().toStdString()), &err);
    check(saved, "[" + tag + "] bản phục hồi của tài liệu đang mở", QString::fromStdString(err));
    if (!saved) return;
    const ReadResult before = readDocxMargins(rec);
    check(before.ok && !before.sections.isEmpty(), "[" + tag + "] đọc được lề trong bản phục hồi", before.error);
    const PatchResult p = patchDocxMargins(rec, patched, spec);
    check(p.ok, "[" + tag + "] vá lề", p.error);
    if (!p.ok) return;
    auto d2 = eng.open(dung::fromUtf8(patched.toUtf8().toStdString()), &err);
    check((bool)d2, "[" + tag + "] nạp bản đã vá lại vào engine", QString::fromStdString(err));
    if (!d2) return;
    QFile::remove(res);
    dung::SaveOptions options;
    bool customXmlOnly=true,hasBlocking=false;
    for(const auto& item:d2->featureLoss())if(item.blocking){
        hasBlocking=true;customXmlOnly=customXmlOnly&&item.id=="xml-tuy-bien"&&!item.certain;
    }
    if(hasBlocking&&customXmlOnly){
        const auto refused=d2->saveAs(dung::fromUtf8(res.toUtf8().toStdString()));
        check(!refused.ok&&!QFileInfo::exists(res),"["+tag+"] chặn lưu khi chưa xác nhận XML tùy biến",QString::fromStdString(refused.message));
        options.allowFeatureLoss=true;
    }
    const auto sr = d2->saveAs(dung::fromUtf8(res.toUtf8().toStdString()),options);
    check(sr.ok, "[" + tag + "] Lưu thành DOCX từ bản đã nạp", QString::fromStdString(sr.message));
    if (!sr.ok) return;
    const ReadResult after = readDocxMargins(res);
    check(after.ok && !after.sections.isEmpty(), "[" + tag + "] đọc lề ở tệp đã lưu", after.error);
    if (!after.ok) return;
    auto close2 = [](qint32 a, std::optional<qint32> b) { return !b || qAbs(a - *b) <= 2; };   // 1/100 mm ≈ 0,57 twip: dung sai 2 twip
    bool all = true;
    QString ev;
    for (const auto& s : after.sections) {
        ev += QString("[%1: T%2 P%3 D%4 Tr%5] ").arg(s.index).arg(s.top).arg(s.right).arg(s.bottom).arg(s.left);
        all = all && close2(s.left, spec.left) && close2(s.right, spec.right) && close2(s.top, spec.top) && close2(s.bottom, spec.bottom);
    }
    QString want = QString("mong đợi T%1 P%2 D%3 Tr%4").arg(spec.top.value_or(-1)).arg(spec.right.value_or(-1)).arg(spec.bottom.value_or(-1)).arg(spec.left.value_or(-1));
    check(all, "[" + tag + "] LỀ Ở TỆP ĐÃ LƯU = lề đã đặt (dung sai 2 twip)", ev + want);
    if (!expectText.isEmpty()) {
        const QString t = plainText(res);
        check(t.contains(expectText), "[" + tag + "] chữ trong tài liệu còn nguyên sau khi đặt lề", t.left(120));
    }
    const QString t0 = plainText(rec), t1 = plainText(res);
    check(!t0.isEmpty() ? t0 == t1 : true, "[" + tag + "] toàn bộ chữ trước/sau khi đặt lề giống hệt", QString("%1 / %2 ký tự").arg(t0.size()).arg(t1.size()));
}

// Thoát ngay, bỏ qua bước dọn dẹp của các DLL: trên Windows, ExitProcess (do std::_Exit gọi) chạy DLL_PROCESS_DETACH của
// LibreOffice/Qt khi engine còn sống và làm tiến trình sập (0xC0000005) SAU khi mọi kiểm tra đã in kết quả (CI vòng 2 và 3).
[[noreturn]] void hardExit(int code) {
    std::cout.flush();
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), UINT(code));
#endif
    std::_Exit(code);
}

void watchdog(int seconds) {
    std::thread([seconds] {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        std::cout << "[LỖI]  TREO quá " << seconds << " giây\n\nKẾT QUẢ ĐẶT LỀ: CÓ LỖI (treo)" << std::endl;
        hardExit(3);
    }).detach();
}

void testEngine(const QString& program, const QString& out, const QString& realDocx) {
    watchdog(600);
    QDir().mkpath(out);
    std::string err;
    auto eng = dung::Engine::create(dung::fromUtf8(program.toUtf8().toStdString()), dung::fromUtf8((out + "/profile").toUtf8().toStdString()),
                                    dung::fromUtf8((out + "/work").toUtf8().toStdString()), &err);
    check((bool)eng, "khởi động engine", QString::fromStdString(err));
    if (!eng) return;
    {
        auto d = eng->createNew(dung::DocKind::Text, &err);
        check((bool)d, "tạo tài liệu Word mới", QString::fromStdString(err));
        if (!d) return;
        d->typeText(u"Xin chào Dũng Office — kiểm thử đặt lề");
        d->waitInputIdle(300, 4000);
        const QString text = QString::fromUtf8("Xin chào Dũng Office");
        MarginSpec normal; normal.left = normal.right = normal.top = normal.bottom = mmToTwips(25.4);
        MarginSpec narrow; narrow.left = narrow.right = narrow.top = narrow.bottom = mmToTwips(12.7);
        roundTrip(*eng, *d, "VN", vn(), out, text);
        roundTrip(*eng, *d, "binhthuong", normal, out, text);
        roundTrip(*eng, *d, "hep", narrow, out, text);
        MarginSpec ul; ul.top = mmToTwips(35); ul.bottom = mmToTwips(10);
        roundTrip(*eng, *d, "chitrenduoi", ul, out, text);
    }
    if (!realDocx.isEmpty()) {
        auto d = eng->open(dung::fromUtf8(realDocx.toUtf8().toStdString()), &err);
        check((bool)d, "mở tệp .docx thật " + QFileInfo(realDocx).fileName(), QString::fromStdString(err));
        if (d) roundTrip(*eng, *d, "tephat", vn(), out, QString());
    }
}

} // namespace

int main(int argc, char** argv) {
    QStringList args;
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    // Đối số dạng Unicode (argv của Windows theo bảng mã ANSI làm hỏng đường dẫn tiếng Việt).
    (void)argc; (void)argv;
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    for (int i = 0; i < wargc; ++i) args << QString::fromWCharArray(wargv[i]);
    LocalFree(wargv);
#else
    for (int i = 0; i < argc; ++i) args << QString::fromUtf8(argv[i]);
#endif
    QString docxDir, program, out, realDocx;
    for (int i = 1; i + 1 < args.size(); i += 2) {
        const QString& a = args[i];
        const QString& v = args[i + 1];
        if (a == "--docx-dir") docxDir = v;
        else if (a == "--program") program = v;
        else if (a == "--out") out = v;
        else if (a == "--docx") realDocx = v;
    }
    const QString tmp = QDir::tempPath() + "/dung-margintest-" + QString::number(qint64(std::time(nullptr)) ^ qint64(std::rand()));
    QDir(tmp).removeRecursively();
    QDir().mkpath(tmp);
    testUnits();
    testScan();
    testPatchXml();
    testZip(tmp);
    if (!docxDir.isEmpty()) testRealDocs(docxDir, tmp);
    QDir(tmp).removeRecursively();
    std::cout << "ĐƠN VỊ + GÓI ZIP: " << g_pass << " đạt, " << g_fail << " lỗi" << std::endl;
    if (!program.isEmpty() && !out.isEmpty()) {
        testEngine(program, out, realDocx);
        std::cout << (g_fail ? "\nKẾT QUẢ ĐẶT LỀ: CÓ LỖI\n" : "\nKẾT QUẢ ĐẶT LỀ: TẤT CẢ ĐẠT\n");
        std::cout.flush();
        hardExit(g_fail ? 1 : 0);   // engine (LibreOffice) có thể treo/sập khi huỷ: thoát thẳng
    }
    std::cout << (g_fail ? "\nKẾT QUẢ ĐẶT LỀ: CÓ LỖI\n" : "\nKẾT QUẢ ĐẶT LỀ: TẤT CẢ ĐẠT\n");
    return g_fail ? 1 : 0;
}
