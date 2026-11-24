// Kiểm thử lõi tìm kiếm tệp (chỉ QtCore). Có đo thời gian và RAM đỉnh thật (mỗi phép đo chạy trong TIẾN TRÌNH RIÊNG
// để RAM đỉnh không lẫn nhau) và so với cách duyệt thô.
#include "FileSearchCore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QStandardPaths>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QStringDecoder>
#include <cstdio>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

using namespace filesearch;

static int g_fail = 0, g_pass = 0;
static void check(bool ok, const QString &what, const QString &detail = QString())
{
    if (ok) { ++g_pass; printf("[ĐẠT]  %s\n", qPrintable(what)); }
    else { ++g_fail; printf("[LỖI]  %s\n", qPrintable(what)); if (!detail.isEmpty()) printf("       %s\n", qPrintable(detail)); }
    fflush(stdout);
}

static qint64 peakKB()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return qint64(pmc.PeakWorkingSetSize / 1024);
    return -1;
#else
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly)) return -1;
    for (const QByteArray &l : f.readAll().split('\n'))
        if (l.startsWith("VmHWM:")) return l.mid(6).trimmed().split(' ').first().toLongLong();
    return -1;
#endif
}

static bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(data);
    return true;
}
static QByteArray u8(const QString &s) { return s.toUtf8(); }
static QString nfd(const QString &s) { return s.normalized(QString::NormalizationForm_D); }

// ---- ghi zip tối thiểu để tạo docx/xlsx/pptx tổng hợp ----
struct ZE { QString name; QByteArray data; bool deflate; };
static void p16(QByteArray &b, quint16 v) { b.append(char(v & 0xFF)); b.append(char(v >> 8)); }
static void p32(QByteArray &b, quint32 v) { p16(b, quint16(v & 0xFFFF)); p16(b, quint16(v >> 16)); }
static QByteArray makeZip(const QVector<ZE> &es)
{
    QByteArray out, cd;
    for (const ZE &e : es) {
        QByteArray comp = e.data;
        quint16 method = 0;
        if (e.deflate && !e.data.isEmpty()) {
            const QByteArray z = qCompress(e.data, 9); // 4 byte độ dài + zlib(2 byte đầu, deflate, 4 byte adler)
            comp = z.mid(6, z.size() - 10);
            method = 8;
        }
        const QByteArray name = e.name.toUtf8();
        const quint32 crc = crc32(e.data), off = quint32(out.size());
        p32(out, 0x04034b50); p16(out, 20); p16(out, 0x0800); p16(out, method); p16(out, 0); p16(out, 0);
        p32(out, crc); p32(out, quint32(comp.size())); p32(out, quint32(e.data.size())); p16(out, quint16(name.size())); p16(out, 0);
        out += name; out += comp;
        p32(cd, 0x02014b50); p16(cd, 20); p16(cd, 20); p16(cd, 0x0800); p16(cd, method); p16(cd, 0); p16(cd, 0);
        p32(cd, crc); p32(cd, quint32(comp.size())); p32(cd, quint32(e.data.size())); p16(cd, quint16(name.size()));
        p16(cd, 0); p16(cd, 0); p16(cd, 0); p16(cd, 0); p32(cd, 0); p32(cd, off);
        cd += name;
    }
    const quint32 cdOff = quint32(out.size());
    out += cd;
    p32(out, 0x06054b50); p16(out, 0); p16(out, 0); p16(out, quint16(es.size())); p16(out, quint16(es.size()));
    p32(out, quint32(cd.size())); p32(out, cdOff); p16(out, 0);
    return out;
}
static QByteArray makeDocx(const QStringList &paras, bool deflate = true)
{
    QString body;
    for (const QString &p : paras) body += QStringLiteral("<w:p><w:r><w:t>%1</w:t></w:r></w:p>").arg(p.toHtmlEscaped());
    const QString doc = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?><w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>%1</w:body></w:document>").arg(body);
    const QString hdr = QStringLiteral("<?xml version=\"1.0\"?><w:hdr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:p><w:r><w:t>Đầu trang bí mật</w:t></w:r></w:p></w:hdr>");
    return makeZip({{QStringLiteral("[Content_Types].xml"), "<Types/>", true}, {QStringLiteral("word/document.xml"), u8(doc), deflate}, {QStringLiteral("word/header1.xml"), u8(hdr), true}});
}
static QByteArray makeXlsx()
{
    const QString wb = QStringLiteral("<?xml version=\"1.0\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"Danh sách\" sheetId=\"1\" r:id=\"rId1\"/><sheet name=\"Quỹ\" sheetId=\"2\" r:id=\"rId2\"/></sheets></workbook>");
    const QString rels = QStringLiteral("<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Target=\"worksheets/sheet1.xml\"/><Relationship Id=\"rId2\" Target=\"worksheets/sheet2.xml\"/></Relationships>");
    const QString ss = QStringLiteral("<?xml version=\"1.0\"?><sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><si><t>Họ và tên</t></si><si><r><t>Nguyễn </t></r><r><t>Văn Dũng</t></r></si><si><t>Mã số</t><rPh><t>NGUOIPHIENAM</t></rPh></si></sst>");
    const QString s1 = QStringLiteral("<?xml version=\"1.0\"?><worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData><row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c><c r=\"B1\" t=\"s\"><v>2</v></c></row><row r=\"2\"><c r=\"A2\" t=\"s\"><v>1</v></c><c r=\"B2\"><v>00123</v></c></row></sheetData></worksheet>");
    const QString s2 = QStringLiteral("<?xml version=\"1.0\"?><worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData><row r=\"1\"><c r=\"C5\" t=\"inlineStr\"><is><t>Tiền quỹ lớp</t></is></c><c r=\"D5\"><v>1500000</v></c></row></sheetData></worksheet>");
    return makeZip({{QStringLiteral("xl/workbook.xml"), u8(wb), true}, {QStringLiteral("xl/_rels/workbook.xml.rels"), u8(rels), true},
                    {QStringLiteral("xl/sharedStrings.xml"), u8(ss), true}, {QStringLiteral("xl/worksheets/sheet1.xml"), u8(s1), true},
                    {QStringLiteral("xl/worksheets/sheet2.xml"), u8(s2), false}});
}
static QByteArray makePptx()
{
    auto slide = [](const QString &t1, const QString &t2) {
        return u8(QStringLiteral("<?xml version=\"1.0\"?><p:sld xmlns:p=\"p\" xmlns:a=\"a\"><p:cSld><p:spTree><p:sp><p:txBody><a:p><a:r><a:t>%1</a:t></a:r></a:p><a:p><a:r><a:t>%2</a:t></a:r></a:p></p:txBody></p:sp></p:spTree></p:cSld></p:sld>").arg(t1, t2));
    };
    return makeZip({{QStringLiteral("ppt/slides/slide2.xml"), slide(QStringLiteral("Kế hoạch năm"), QStringLiteral("Mục tiêu doanh thu")), true},
                    {QStringLiteral("ppt/slides/slide10.xml"), slide(QStringLiteral("Cảm ơn"), QStringLiteral("Hết")), true},
                    {QStringLiteral("ppt/slides/slide1.xml"), slide(QStringLiteral("Chào mừng"), QStringLiteral("Giới thiệu công ty")), true}});
}
static QByteArray makePdf(const QString &asciiText)
{
    QByteArray pdf = "%PDF-1.4\n";
    QVector<qint64> off;
    auto obj = [&](const QByteArray &body) { off.append(pdf.size()); pdf += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n"; };
    obj("<< /Type /Catalog /Pages 2 0 R >>");
    obj("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    obj("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>");
    const QByteArray stream = "BT /F1 14 Tf 20 100 Td (" + asciiText.toLatin1() + ") Tj ET";
    obj("<< /Length " + QByteArray::number(stream.size()) + " >>\nstream\n" + stream + "\nendstream");
    obj("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    const qint64 xref = pdf.size();
    pdf += "xref\n0 6\n0000000000 65535 f \n";
    for (qint64 o : off) pdf += QByteArray::number(o).rightJustified(10, '0') + " 00000 n \n";
    pdf += "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    return pdf;
}

static QStringList paths(const Output &o)
{
    QStringList l;
    for (const auto &r : o.results) l << QFileInfo(r.path).fileName();
    l.sort();
    return l;
}
static bool hasName(const Output &o, const QString &n) { return paths(o).contains(n); }

// ---------- kiểm thử các phần ----------
static void testFold()
{
    check(foldText(QStringLiteral("Báo cáo")) == QStringLiteral("bao cao"), "fold: Báo cáo → bao cao");
    check(foldText(QStringLiteral("ĐƯỜNG Đi")) == QStringLiteral("duong di"), "fold: ĐƯỜNG Đi → duong di (Đ, Ư, Ờ)");
    check(foldText(nfd(QStringLiteral("Phiếu đề xuất"))) == foldText(QStringLiteral("Phiếu đề xuất")), "fold: NFC và NFD cho cùng kết quả");
    check(foldText(QStringLiteral("Báo"), false) == foldText(nfd(QStringLiteral("Báo")), false), "fold giữ dấu: NFC và NFD vẫn cùng kết quả");
    check(foldText(QStringLiteral("bao"), false) != foldText(QStringLiteral("Báo"), false), "fold giữ dấu: bao ≠ báo");
    QVector<int> map;
    const QString f = foldText(nfd(QStringLiteral("Hà Nội")), true, &map);
    check(f == QStringLiteral("ha noi") && map.size() == f.size(), "fold: bảng ánh xạ vị trí đủ độ dài");
}

static void testParse()
{
    QStringList w;
    Query q = parseQuery(QStringLiteral("bao cao -nhap \"hop dong\" ext:docx,.xlsx size>1mb size<2gb after:2026-01-05 before:2026-02-01"), &w);
    check(q.nameTerms == QStringList({"bao", "cao", "hop dong"}), "parse: từ khoá và cụm trong ngoặc kép", q.nameTerms.join('|'));
    check(q.excludeTerms == QStringList({"nhap"}), "parse: loại trừ");
    check(q.extensions == QStringList({"docx", "xlsx"}), "parse: đuôi (bỏ dấu chấm)", q.extensions.join('|'));
    check(q.minSize == 1024 * 1024 && q.maxSize == 2LL * 1024 * 1024 * 1024, "parse: dung lượng");
    check(q.modifiedAfter.date() == QDate(2026, 1, 5) && q.modifiedBefore.date() == QDate(2026, 2, 1), "parse: ngày");
    check(w.isEmpty(), "parse: hợp lệ thì không có cảnh báo");
    w.clear();
    parseQuery(QStringLiteral("size>abc after:2026-13-45 ok"), &w);
    check(w.size() == 2, "parse: điều kiện sai KHÔNG bị bỏ qua âm thầm (có 2 cảnh báo)", w.join('|'));
    check(splitTerms(QStringLiteral("a \"b c\"  d")) == QStringList({"a", "b c", "d"}), "splitTerms");
}

static void testInflate()
{
    QByteArray text;
    for (int i = 0; i < 4000; ++i) text += u8(QStringLiteral("Dòng số %1: Cộng hòa xã hội chủ nghĩa Việt Nam — hợp đồng lao động\n").arg(i));
    QByteArray rnd(300000, 0);
    QRandomGenerator rg(12345);
    for (char &c : rnd) c = char(rg.bounded(256));
    const QByteArray tiny = "abc";
    for (const auto &pair : QVector<QPair<QString, QByteArray>>{{"văn bản lớn (dynamic Huffman)", text}, {"dữ liệu ngẫu nhiên (khối stored)", rnd}, {"chuỗi ngắn (fixed Huffman)", tiny}}) {
        const QByteArray z = qCompress(pair.second, 9);
        const QByteArray raw = z.mid(6, z.size() - 10);
        int st = 99;
        const QByteArray back = inflateRaw(raw, 1LL << 30, &st);
        check(st == 0 && back == pair.second, "inflate: " + pair.first, QStringLiteral("status=%1 size=%2/%3").arg(st).arg(back.size()).arg(pair.second.size()));
    }
    {
        const QByteArray z = qCompress(text, 9);
        int st = 99;
        const QByteArray part = inflateRaw(z.mid(6, z.size() - 10), 5000, &st);
        check(st == 1 && part.size() == 5000 && text.startsWith(part), "inflate: cắt đúng trần bộ nhớ, trả status=1");
        QByteArray bad = z.mid(6, z.size() - 10);
        bad = bad.left(bad.size() / 2);
        int st2 = 0;
        inflateRaw(bad, 1LL << 30, &st2);
        check(st2 < 0, "inflate: dữ liệu cụt báo lỗi, không treo/crash", QString::number(st2));
        QByteArray junk(2000, '\xFF');
        int st3 = 0;
        inflateRaw(junk, 1LL << 30, &st3);
        check(st3 != 0, "inflate: rác báo lỗi/cắt, không crash", QString::number(st3));
    }
    check(crc32("123456789") == 0xCBF43926u, "crc32 đúng giá trị chuẩn");
}

static void testNames(const QString &root)
{
    const QString d = root + "/names";
    writeFile(d + "/" + QStringLiteral("Báo cáo tháng 1.docx"), "x");                       // NFC
    writeFile(d + "/" + nfd(QStringLiteral("Báo cáo quý 2.txt")), "x");                      // NFD
    writeFile(d + "/" + QStringLiteral("BAO CAO NHAP.txt"), "xx");
    writeFile(d + "/" + QStringLiteral("Hợp đồng lao động.pdf"), "xxxx");
    writeFile(d + "/.hidden/" + QStringLiteral("Báo cáo ẩn.txt"), "x");
    const QString longName = QString(190, QLatin1Char('a')) + QStringLiteral(" dài.txt");
    check(writeFile(d + "/" + longName, "x"), "tạo được tệp tên dài 190+ ký tự");
    QString deep = d;
    for (int i = 0; i < 25; ++i) deep += QStringLiteral("/thu_muc_sau_%1").arg(i, 2, 10, QLatin1Char('0'));
    const bool deepOk = writeFile(deep + "/" + QStringLiteral("Báo cáo sâu.txt"), "x");
    check(deepOk, QStringLiteral("tạo được thư mục sâu (đường dẫn %1 ký tự)").arg(deep.size() + 20));
    QFile::setPermissions(d, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QFile old(d + "/" + QStringLiteral("Hợp đồng lao động.pdf"));
    if (old.open(QIODevice::ReadWrite)) { old.setFileTime(QDateTime(QDate(2020, 3, 1), QTime(0, 0)), QFileDevice::FileModificationTime); old.close(); }

    Query q; q.root = d; q.nameTerms = {"bao", "cao"};
    Output o = search(q);
    check(hasName(o, "Báo cáo tháng 1.docx") && hasName(o, nfd(QStringLiteral("Báo cáo quý 2.txt"))) && hasName(o, "BAO CAO NHAP.txt"), "tên: 'bao cao' khớp Báo cáo (NFC), (NFD) và BAO CAO", paths(o).join(" | "));
    check(!hasName(o, "Báo cáo ẩn.txt"), "tên: tệp trong thư mục ẩn bị bỏ qua mặc định");
    check(deepOk ? hasName(o, "Báo cáo sâu.txt") : true, "tên: tìm thấy tệp ở thư mục rất sâu");
    q.includeHidden = true;
    check(hasName(search(q), "Báo cáo ẩn.txt"), "tên: includeHidden bật thì thấy tệp ẩn");
    q.includeHidden = false;
    q.nameTerms = {nfd(QStringLiteral("Báo cáo"))};
    o = search(q);
    check(hasName(o, "Báo cáo tháng 1.docx") && hasName(o, "BAO CAO NHAP.txt"), "tên: gõ có dấu (NFD) vẫn khớp tên NFC và không dấu");
    q.ignoreDiacritics = false;
    q.nameTerms = {QStringLiteral("báo cáo")};
    o = search(q);
    check(hasName(o, "Báo cáo tháng 1.docx") && hasName(o, nfd(QStringLiteral("Báo cáo quý 2.txt"))) && !hasName(o, "BAO CAO NHAP.txt"), "tên: chế độ phân biệt dấu loại 'BAO CAO' nhưng giữ NFC/NFD", paths(o).join(" | "));
    q.nameTerms = {"bao", "cao", "thang"};
    check(search(q).results.isEmpty(), "tên: phân biệt dấu thì 'bao cao thang' không khớp 'Báo cáo tháng'");
    q.ignoreDiacritics = true;
    q.nameTerms = {"bao cao"};
    q.excludeTerms = {"nhap", "quy"};
    o = search(q);
    check(hasName(o, "Báo cáo tháng 1.docx") && !hasName(o, "BAO CAO NHAP.txt") && !hasName(o, nfd(QStringLiteral("Báo cáo quý 2.txt"))), "tên: loại trừ -nhap -quy", paths(o).join(" | "));
    q = Query(); q.root = d; q.extensions = {"pdf"};
    o = search(q);
    check(o.results.size() == 1 && hasName(o, "Hợp đồng lao động.pdf"), "lọc: đuôi pdf", paths(o).join(" | "));
    q = Query(); q.root = d; q.nameTerms = {"bao"}; q.minSize = 2;
    o = search(q);
    check(paths(o) == QStringList({"BAO CAO NHAP.txt"}), "lọc: dung lượng ≥ 2 byte", paths(o).join(" | "));
    q = Query(); q.root = d; q.modifiedBefore = QDateTime(QDate(2021, 1, 1), QTime(0, 0));
    o = search(q);
    check(paths(o) == QStringList({"Hợp đồng lao động.pdf"}), "lọc: ngày sửa trước 2021", paths(o).join(" | "));
    q = Query(); q.root = d; q.nameTerms = {QString(60, QLatin1Char('a'))};
    check(search(q).results.size() == 1, "tên: tìm thấy tên dài");
    q = Query(); q.root = d;
    check(!search(q).error.isEmpty(), "không nhập gì: báo lỗi rõ ràng thay vì liệt kê tất cả");
    q = Query(); q.root = root + "/khong_ton_tai"; q.nameTerms = {"a"};
    check(!search(q).error.isEmpty(), "thư mục không tồn tại: báo lỗi");
    q = Query(); q.root = d; q.nameTerms = {"bao"}; q.maxResults = 2;
    o = search(q);
    check(o.results.size() == 2 && o.stats.truncatedResults, "giới hạn số kết quả: cắt và báo rõ");
}

static void testContent(const QString &root)
{
    const QString d = root + "/content";
    writeFile(d + "/utf8.txt", u8(QStringLiteral("dòng một\nHợp đồng lao động số 01\nkết thúc\n")));
    writeFile(d + "/utf8bom.csv", QByteArray("\xEF\xBB\xBF") + u8(QStringLiteral("a,b\nTuyển dụng nhân sự,100\n")));
    {
        QStringEncoder enc(QStringConverter::Utf16LE, QStringConverter::Flag::WriteBom);
        writeFile(d + "/utf16le.txt", QByteArray(enc(QStringLiteral("Dòng 1\r\nBiên bản họp ban giám đốc\r\n"))));
        QStringEncoder enc2(QStringConverter::Utf16BE, QStringConverter::Flag::WriteBom);
        writeFile(d + "/utf16be.md", QByteArray(enc2(QStringLiteral("# Tiêu đề\nBiên bản họp ban giám đốc\n"))));
    }
    {   // Windows-1258 (không phải UTF-8): byte 0xE1 đơn lẻ
        QByteArray b = "Bao cao thang \xE1 nay\nhop dong\n";
        writeFile(d + "/cu1258.txt", b);
        QByteArray bin("abc\0def hop dong", 16);
        writeFile(d + "/nhiphan.log", bin);
    }
    {   // dòng rất dài (5 MB) có ký tự 2 byte nằm đúng ranh giới đọc; từ khoá ở cuối
        QByteArray big = "aa"; // 2 byte lẻ để ký tự nhiều byte rơi đúng vào ranh giới đọc
        const QByteArray ch = u8(QStringLiteral("á"));
        for (int i = 0; i < 2600000; ++i) big += ch;
        big += " cuoicungxyz\n";
        writeFile(d + "/dongdai.txt", big);
    }
    writeFile(d + "/hopdong.docx", makeDocx({QStringLiteral("HỢP ĐỒNG LAO ĐỘNG"), QStringLiteral("Bên A: Công ty Dũng Office"), QStringLiteral("Điều 1: thời hạn hợp đồng")}));
    writeFile(d + "/stored.docx", makeDocx({QStringLiteral("Nội dung lưu thô không nén")}, false));
    writeFile(d + "/bang.xlsx", makeXlsx());
    writeFile(d + "/trinhchieu.pptx", makePptx());
    {   // docx hỏng: cụt giữa chừng
        QByteArray good = makeDocx({QStringLiteral("Nội dung sẽ bị cắt cụt")});
        writeFile(d + "/cut.docx", good.left(good.size() / 2));
        // docx sai CRC: sửa một byte trong dữ liệu mục stored
        QByteArray st = makeDocx({QStringLiteral("Nội dung bị sửa một byte")}, false);
        const int p = st.indexOf("Nội dung bị");
        if (p > 0) st[p + 2] = 'X';
        writeFile(d + "/crc.docx", st);
        writeFile(d + "/khongphailazip.docx", "day khong phai la zip hop le");
    }
    writeFile(d + "/cu.doc", "binary doc");
    writeFile(d + "/cu.xls", "binary xls");
    writeFile(d + "/mau.pdf", makePdf(QStringLiteral("Hello Dung Office pdfkeyword")));

    Query q; q.root = d; q.contentTerms = {"hop dong lao dong"};
    Output o = search(q);
    check(hasName(o, "utf8.txt") && hasName(o, "hopdong.docx"), "nội dung: 'hop dong lao dong' khớp 'Hợp đồng lao động' trong txt và docx", paths(o).join(" | "));
    for (const auto &r : o.results)
        if (QFileInfo(r.path).fileName() == "utf8.txt") check(r.location == QStringLiteral("dòng 2") && r.snippet.contains(QStringLiteral("Hợp đồng lao động")), "nội dung: txt có vị trí 'dòng 2' và trích đoạn gốc có dấu", r.location + " / " + r.snippet);
    for (const auto &r : o.results)
        if (QFileInfo(r.path).fileName() == "hopdong.docx") check(r.location == QStringLiteral("đoạn 1"), "nội dung: docx có vị trí 'đoạn 1'", r.location);
    q.contentTerms = {"tuyen dung"};
    o = search(q);
    check(hasName(o, "utf8bom.csv"), "nội dung: UTF-8 có BOM (csv)", paths(o).join(" | "));
    q.contentTerms = {"bien ban hop"};
    o = search(q);
    check(hasName(o, "utf16le.txt") && hasName(o, "utf16be.md"), "nội dung: UTF-16 LE và BE có BOM", paths(o).join(" | "));
    q.contentTerms = {"cuoicungxyz"};
    o = search(q);
    check(hasName(o, "dongdai.txt"), "nội dung: dòng dài 5 MB với ký tự nhiều byte ở ranh giới không bị báo sai bảng mã", paths(o).join(" | ") + " skipped=" + QString::number(o.stats.skipped));
    q.contentTerms = {"luu tho khong nen"};
    check(hasName(search(q), "stored.docx"), "nội dung: docx dùng mục không nén (stored)");
    q.contentTerms = {"dau trang bi mat"};
    check(hasName(search(q), "hopdong.docx"), "nội dung: đọc cả đầu trang (header) của docx");
    q.contentTerms = {"nguyen van dung"};
    o = search(q);
    bool locOk = false;
    for (const auto &r : o.results) if (QFileInfo(r.path).fileName() == "bang.xlsx") locOk = r.location == QStringLiteral("Danh sách!A2");
    check(locOk, "nội dung: xlsx — chuỗi chung ghép từ nhiều run, vị trí 'Danh sách!A2'");
    q.contentTerms = {"nguoiphienam"};
    check(search(q).results.isEmpty(), "nội dung: xlsx bỏ phiên âm (rPh), không khớp nhầm");
    q.contentTerms = {"tien quy lop"};
    o = search(q);
    locOk = false;
    for (const auto &r : o.results) if (QFileInfo(r.path).fileName() == "bang.xlsx") locOk = r.location == QStringLiteral("Quỹ!C5");
    check(locOk, "nội dung: xlsx inlineStr ở sheet 2, vị trí 'Quỹ!C5'");
    q.contentTerms = {"00123"};
    check(hasName(search(q), "bang.xlsx"), "nội dung: xlsx giữ số 0 đầu của giá trị đã lưu");
    q.contentTerms = {"gioi thieu cong ty"};
    o = search(q);
    check(hasName(o, "trinhchieu.pptx") && !o.results.isEmpty() && o.results.first().location == QStringLiteral("slide 1"), "nội dung: pptx — vị trí 'slide 1'", o.results.isEmpty() ? "" : o.results.first().location);
    q.contentTerms = {"cam on"};
    o = search(q);
    check(!o.results.isEmpty() && o.results.first().location == QStringLiteral("slide 10"), "nội dung: pptx — slide 10 xếp sau slide 2 (so số, không so chuỗi)");
    q.contentTerms = {"hop dong", "dung office"}; // AND: docx có cả hai
    o = search(q);
    check(hasName(o, "hopdong.docx") && !hasName(o, "utf8.txt"), "nội dung: nhiều từ khoá là AND theo cả tệp", paths(o).join(" | "));
    q.contentTerms = {"hop dong"};
    o = search(q);
    QStringList skipped;
    for (const auto &s : o.skipped) skipped << QFileInfo(s.path).fileName() + ": " + s.reason;
    auto skipHas = [&](const QString &name, const QString &frag) { for (const QString &s : skipped) if (s.startsWith(name) && s.contains(frag)) return true; return false; };
    check(skipHas("cu1258.txt", "UTF-8"), "bỏ qua có lý do: tệp bảng mã cũ", skipped.join(" | "));
    check(skipHas("nhiphan.log", "nhị phân"), "bỏ qua có lý do: tệp nhị phân");
    check(skipHas("cut.docx", "") , "bỏ qua có lý do: docx bị cụt", skipped.join(" | "));
    check(skipHas("crc.docx", "CRC"), "bỏ qua có lý do: docx sai CRC", skipped.join(" | "));
    check(skipHas("khongphailazip.docx", "zip"), "bỏ qua có lý do: không phải zip");
    check(o.stats.unsupportedByExt.value("doc") == 1 && o.stats.unsupportedByExt.value("xls") == 1, "thống kê: .doc/.xls cũ được báo là chưa tìm nội dung");
    check(hasName(o, "utf8.txt"), "một tệp lỗi không làm dừng cả phiên (vẫn có kết quả hợp lệ)");
    // PDF
    q.contentTerms = {"pdfkeyword"};
    o = search(q);
    const bool havePdftotext = !QStandardPaths::findExecutable("pdftotext").isEmpty();
    printf("INFO  pdftotext trên máy này: %s\n", havePdftotext ? "CÓ" : "KHÔNG");
    if (havePdftotext) check(hasName(o, "mau.pdf") && o.results.first().location.startsWith("trang 1"), "nội dung: pdf qua pdftotext, vị trí trang", paths(o).join(" | ") + " skipped=" + QString::number(o.stats.skipped));
    else check(o.stats.pdfUnavailable && o.results.isEmpty(), "nội dung: không có pdftotext → báo rõ pdfUnavailable, không giả vờ đã tìm");
    // Huỷ giữa chừng và dừng sớm
    std::atomic_bool cancel{true};
    q.contentTerms = {"hop dong"};
    o = search(q, nullptr, &cancel);
    check(o.stats.canceled, "huỷ: cờ huỷ có từ đầu → dừng ngay và báo canceled");
}

static void testIndex(const QString &root)
{
    const QString d = root + "/names", idx = root + "/idx";
    QString err;
    qint64 n = 0;
    check(buildIndex(d, idx, false, nullptr, &err, &n) && n > 5, "chỉ mục: lập được", err + " n=" + QString::number(n));
    Index ix;
    check(loadIndex(d, idx, &ix, &err), "chỉ mục: đọc lại được", err);
    Query q; q.root = d; q.nameTerms = {"bao", "cao"};
    const Output a = search(q), b = search(q, &ix);
    check(paths(a) == paths(b), "chỉ mục: kết quả tìm tên giống hệt duyệt trực tiếp", paths(a).join("|") + " <> " + paths(b).join("|"));
    QFile::remove(d + "/" + QStringLiteral("BAO CAO NHAP.txt"));
    const Output c = search(q, &ix);
    check(!hasName(c, "BAO CAO NHAP.txt") && c.stats.staleIndexEntries == 1, "chỉ mục: tệp đã xoá được phát hiện là cũ, không trả về", QString::number(c.stats.staleIndexEntries));
    q.includeHidden = true;
    const Output h = search(q, &ix); // chỉ mục lập không có tệp ẩn → phải tự duyệt trực tiếp
    check(hasName(h, "Báo cáo ẩn.txt"), "chỉ mục: yêu cầu tệp ẩn mà chỉ mục không có → tự duyệt trực tiếp, không bỏ sót");
    check(removeIndex(d, idx) && !QFileInfo::exists(indexFilePath(d, idx)), "chỉ mục: xoá được");
    check(!loadIndex(d, idx, &ix, &err), "chỉ mục: sau khi xoá không còn tải được");
}

static void testTemplates(const QString &dir)
{
    if (dir.isEmpty() || !QFileInfo(dir).isDir()) { printf("INFO  bỏ qua kiểm thử mẫu thật (không có DUNG_FS_TEMPLATES)\n"); return; }
    int total = 0, errors = 0, nonEmpty = 0;
    QString firstWord, firstWordFile;
    QStringList errList;
    QDirIterator it(dir, {"*.docx", "*.xlsx", "*.pptx"}, QDir::Files, QDirIterator::Subdirectories);
    Query q;
    q.threads = 1;
    while (it.hasNext()) {
        const QString p = it.next();
        ++total;
        QString text;
        const QString e = extractChunks(p, q, [&](const QString &t, const QString &) { if (text.size() < 4000) text += t + ' '; return true; });
        if (!e.isEmpty()) { ++errors; if (errList.size() < 5) errList << p + ": " + e; }
        else if (!text.trimmed().isEmpty()) {
            ++nonEmpty;
            if (firstWord.isEmpty())
                for (const QString &w : text.split(' ', Qt::SkipEmptyParts))
                    if (w.size() >= 6 && w.at(0).isLetter()) { firstWord = w; firstWordFile = p; break; }
        }
    }
    printf("INFO  mẫu thật: %d tệp docx/xlsx/pptx, đọc lỗi %d, có chữ %d\n", total, errors, nonEmpty);
    check(total > 100 && errors == 0, "mẫu thật: đọc được toàn bộ docx/xlsx/pptx không lỗi", errList.join(" | "));
    check(nonEmpty * 100 >= total * 90, "mẫu thật: ≥90% tệp trích được chữ", QString("%1/%2").arg(nonEmpty).arg(total));
    if (!firstWord.isEmpty()) {
        Query s; s.root = dir; s.contentTerms = {firstWord};
        const Output o = search(s);
        bool inList = false;
        for (const auto &r : o.results) if (r.path == firstWordFile) inList = true;
        check(inList, "mẫu thật: tìm theo một từ lấy từ tệp thì tìm lại đúng tệp đó", firstWord + " / " + firstWordFile);
    }
    Query s; s.root = dir; s.contentTerms = {QStringLiteral("hợp đồng")};
    QElapsedTimer t; t.start();
    const Output o = search(s);
    printf("BENCH mau-that tim 'hop dong' trong %lld tep: %lld ms, %lld ket qua, %lld bo qua, RAM dinh %lld KB\n", (long long)o.stats.filesContentRead,
           (long long)t.elapsed(), (long long)o.stats.matched, (long long)o.stats.skipped, (long long)peakKB());
}

// ---------- đo hiệu năng trong tiến trình con ----------
static int benchMain(const QString &mode, const QString &dir)
{
    QElapsedTimer t;
    qint64 hits = 0, extra = 0;
    t.start();
    if (mode == "ours-name") {
        Query q; q.root = dir; q.nameTerms = {"bao", "cao", "007"};
        hits = search(q).results.size();
    } else if (mode == "raw-name") {
        QDirIterator it(dir, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); const QString n = it.fileName(); if (n.contains("Báo cáo", Qt::CaseInsensitive) && n.contains("007")) ++hits; }
    } else if (mode == "ours-content") {
        Query q; q.root = dir; q.contentTerms = {"tuyen dung"};
        Output o = search(q);
        hits = o.results.size();
        extra = o.stats.skipped;
    } else if (mode == "raw-content") {
        QDirIterator it(dir, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            if (!f.open(QIODevice::ReadOnly)) continue;
            if (QString::fromUtf8(f.readAll()).contains("tuyển dụng", Qt::CaseInsensitive)) ++hits;
        }
    } else if (mode == "ours-index") {
        QString err;
        qint64 n = 0;
        buildIndex(dir, dir + "/../bench_idx", false, nullptr, &err, &n);
        const qint64 buildMs = t.elapsed();
        t.restart();
        Index ix;
        loadIndex(dir, dir + "/../bench_idx", &ix, &err);
        const qint64 loadMs = t.elapsed();
        t.restart();
        Query q; q.root = dir; q.nameTerms = {"bao", "cao", "007"};
        hits = search(q, &ix).results.size();
        printf("BENCH ours-index build_ms=%lld load_ms=%lld entries=%lld\n", (long long)buildMs, (long long)loadMs, (long long)n);
    } else return 2;
    printf("BENCH %s ms=%lld hits=%lld extra=%lld peakKB=%lld\n", qPrintable(mode), (long long)t.elapsed(), (long long)hits, (long long)extra, (long long)peakKB());
    return 0;
}

static QString runBench(const QString &mode, const QString &dir)
{
    QProcess p;
    p.start(QCoreApplication::applicationFilePath(), {"--bench", mode, dir});
    p.waitForFinished(10 * 60 * 1000);
    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    printf("%s", qPrintable(out));
    return out;
}
static qint64 benchField(const QString &out, const QString &mode, const QString &field)
{
    for (const QString &l : out.split('\n'))
        if (l.startsWith("BENCH " + mode + " ms=")) for (const QString &tok : l.split(' ')) if (tok.startsWith(field + "=")) return tok.mid(field.size() + 1).toLongLong();
    return -1;
}

static void testBench(const QString &root)
{
    const QString d = root + "/bench";
    const int dirs = 300, perDir = 20;
    QElapsedTimer t; t.start();
    int withKey = 0;
    QByteArray filler;
    for (int i = 0; i < 18; ++i) filler += u8(QStringLiteral("Cộng hòa xã hội chủ nghĩa Việt Nam, độc lập tự do hạnh phúc, báo cáo kết quả công việc. "));
    for (int a = 0; a < dirs; ++a)
        for (int b = 0; b < perDir; ++b) {
            const int n = a * perDir + b;
            QByteArray body = filler;
            if (n % 100 == 7) { body += u8(QStringLiteral("\nThông báo tuyển dụng nhân sự mới.\n")); ++withKey; }
            writeFile(d + QStringLiteral("/nhom_%1/Báo cáo %2.txt").arg(a, 3, 10, QLatin1Char('0')).arg(n, 4, 10, QLatin1Char('0')), body);
        }
    printf("INFO  tạo bộ đo: %d tệp (~%d KB mỗi tệp), %d tệp chứa từ khoá, mất %lld ms\n", dirs * perDir, int(filler.size() / 1024), withKey, (long long)t.elapsed());
    const QString on = runBench("ours-name", d), rn = runBench("raw-name", d);
    const QString oc = runBench("ours-content", d), rc = runBench("raw-content", d);
    const QString oi = runBench("ours-index", d);
    const qint64 expName = benchField(rn, "raw-name", "hits");
    check(benchField(on, "ours-name", "hits") == expName && expName > 0, "đo: tìm tên cho số kết quả bằng cách duyệt thô", QString("%1 vs %2").arg(benchField(on, "ours-name", "hits")).arg(expName));
    check(benchField(oc, "ours-content", "hits") == withKey && benchField(rc, "raw-content", "hits") == withKey, "đo: tìm nội dung ra đúng số tệp có từ khoá", QString("ours=%1 raw=%2 want=%3").arg(benchField(oc, "ours-content", "hits")).arg(benchField(rc, "raw-content", "hits")).arg(withKey));
    check(benchField(oi, "ours-index", "hits") == expName, "đo: tìm tên bằng chỉ mục cho cùng số kết quả");
    // huỷ giữa chừng trên bộ lớn
    Query q; q.root = d; q.contentTerms = {"khong_co_tu_nay"};
    std::atomic_bool cancel{false};
    qint64 seen = 0;
    const Output o = search(q, nullptr, &cancel, {}, [&](qint64 n) { seen = n; if (n >= 512) cancel = true; });
    check(o.stats.canceled && o.stats.filesSeen < dirs * perDir, "huỷ: dừng giữa chừng khi đang duyệt bộ lớn", QString("đã duyệt %1/%2").arg(o.stats.filesSeen).arg(dirs * perDir));
    Q_UNUSED(seen)
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc >= 4 && QString(argv[1]) == "--bench") return benchMain(argv[2], argv[3]);
    QTemporaryDir tmp;
    if (!tmp.isValid()) { printf("[LỖI]  không tạo được thư mục tạm\n"); return 1; }
    const QString root = tmp.path();
    testFold();
    testParse();
    testInflate();
    testNames(root);
    testContent(root);
    testIndex(root);
    testTemplates(qEnvironmentVariable("DUNG_FS_TEMPLATES"));
    testBench(root);
    printf("\nĐạt %d, lỗi %d\nKẾT QUẢ: %s\n", g_pass, g_fail, g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT");
    return g_fail ? 1 : 0;
}
