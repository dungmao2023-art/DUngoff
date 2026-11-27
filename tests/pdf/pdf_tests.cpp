// SPDX-License-Identifier: MPL-2.0
// Kiểm thử thao tác PDF (PdfOps) với qpdf thật. Không cần giao diện; tự tạo PDF mẫu.
// Chạy: dung-pdftest   (đặt DUNG_QPDF nếu qpdf không nằm trong PATH)
#include "pdf/PdfOps.h"
#include "pdf/PdfOcr.h"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QProcess>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <cstdio>
#include <iostream>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace dungpdf;

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, msg)                                                                   \
    do {                                                                                   \
        if (cond) { ++g_pass; std::cout << "[ĐẠT]  " << QString(msg).toStdString() << "\n"; } \
        else { ++g_fail; std::cout << "[LỖI]  " << QString(msg).toStdString() << "  (" #cond ")\n"; } \
    } while (0)

// PDF tối thiểu hợp lệ (bảng xref đúng), `pages` trang, chữ có nhãn từng trang.
static QByteArray makePdf(int pages, const QString& label) {
    QByteArray out = "%PDF-1.4\n";
    QList<int> offs;
    auto obj = [&](const QByteArray& body) {
        offs << out.size();
        out += QByteArray::number(offs.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    obj("<< /Type /Catalog /Pages 2 0 R >>");
    QByteArray kids;
    for (int i = 0; i < pages; ++i) kids += QByteArray::number(4 + 2 * i) + " 0 R ";
    obj("<< /Type /Pages /Count " + QByteArray::number(pages) + " /Kids [ " + kids + "] >>");
    obj("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    for (int i = 0; i < pages; ++i) {
        obj("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 3 0 R >> >> /Contents " +
            QByteArray::number(5 + 2 * i) + " 0 R >>");
        const QByteArray text = "BT /F1 24 Tf 72 720 Td (" + label.toLatin1() + " " + QByteArray::number(i + 1) + ") Tj ET";
        obj("<< /Length " + QByteArray::number(text.size()) + " >>\nstream\n" + text + "\nendstream");
    }
    const int xrefPos = out.size();
    out += "xref\n0 " + QByteArray::number(offs.size() + 1) + "\n0000000000 65535 f \n";
    for (int o : offs) {
        char line[32];
        std::snprintf(line, sizeof line, "%010d 00000 n \n", o);
        out += line;
    }
    out += "trailer\n<< /Size " + QByteArray::number(offs.size() + 1) + " /Root 1 0 R >>\nstartxref\n" +
           QByteArray::number(xrefPos) + "\n%%EOF\n";
    return out;
}

static bool writeFile(const QString& path, const QByteArray& data) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}
static QByteArray readFile(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// JPEG 16x8 thật (không cần plugin ảnh của Qt), và bản có EXIF hướng 6 (xoay 90 độ).
static const char kJpeg[] = "/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAAIABADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDkKKKK8U/TD//Z";
static const char kJpegExif6[] = "/9j/4AAQSkZJRgABAQAAAQABAAD/4QAiRXhpZgAATU0AKgAAAAgAAQESAAMAAAABAAYAAAAAAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAAIABADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDkKKKK8U/TD//Z";

// PDF tùy ý: mỗi trang một nội dung; nullptr = không có /Contents; extra = thêm khóa vào từ điển trang.
struct PageSpec { const char* content; const char* extra; };
static QByteArray makeCustomPdf(const QList<PageSpec>& specs) {
    QByteArray out = "%PDF-1.4\n";
    QList<int> offs;
    auto obj = [&](const QByteArray& body) {
        offs << out.size();
        out += QByteArray::number(offs.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    obj("<< /Type /Catalog /Pages 2 0 R >>");
    QByteArray kids;
    int next = 4;
    QList<int> pageIds;
    for (const PageSpec& sp : specs) { pageIds << next; next += sp.content ? 2 : 1; }
    for (int id : pageIds) kids += QByteArray::number(id) + " 0 R ";
    obj("<< /Type /Pages /Count " + QByteArray::number(specs.size()) + " /Kids [ " + kids + "] >>");
    obj("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    for (const PageSpec& sp : specs) {
        QByteArray page = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 3 0 R >> >> ";
        if (sp.extra) page += sp.extra;
        if (sp.content) page += " /Contents " + QByteArray::number(offs.size() + 2) + " 0 R";
        obj(page + " >>");
        if (sp.content) {
            const QByteArray c = sp.content;
            obj("<< /Length " + QByteArray::number(c.size()) + " >>\nstream\n" + c + "\nendstream");
        }
    }
    const int xrefPos = out.size();
    out += "xref\n0 " + QByteArray::number(offs.size() + 1) + "\n0000000000 65535 f \n";
    for (int o : offs) {
        char line[32];
        std::snprintf(line, sizeof line, "%010d 00000 n \n", o);
        out += line;
    }
    out += "trailer\n<< /Size " + QByteArray::number(offs.size() + 1) + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xrefPos) + "\n%%EOF\n";
    return out;
}

// Bản qpdf --qdf: luồng không nén, tìm được chuỗi trong lớp phủ/ảnh.
static QByteArray qdfOf(const QString& path) {
    const QString out = path + QStringLiteral(".qdf");
    QProcess p;
    p.start(findQpdf(), {QStringLiteral("--qdf"), QStringLiteral("--object-streams=disable"), path, out});
    p.waitForFinished(60000);
    const QByteArray b = readFile(out);
    QFile::remove(out);
    return b;
}

static int tempLeftovers(const QString& dir) {
    return QDir(dir).entryList({QStringLiteral("~dung-*")}, QDir::AllEntries | QDir::NoDotAndDotDot).size();
}

// Child mode: proxy the real qpdf, then create a competing destination after
// transformation, before PdfOps publishes. This exercises the actual write path.
static int raceProxy(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QString real=qEnvironmentVariable("DUNG_PDF_TEST_REAL_QPDF");
    const QString target=qEnvironmentVariable("DUNG_PDF_TEST_RACE_TARGET");
    if(real.isEmpty()||target.isEmpty())return 91;
    QProcess child;child.setProcessChannelMode(QProcess::ForwardedChannels);
    const auto args=app.arguments().mid(1);
    child.start(real,args);
    if(!child.waitForStarted(15000)||!child.waitForFinished(120000))return 92;
    if(child.exitStatus()!=QProcess::NormalExit)return 93;
    const int code=child.exitCode();
    if((code==0||code==3)&&!args.isEmpty()&&args.last().endsWith(QLatin1String("/result.pdf"))
        &&!args.contains(QStringLiteral("--check"))){
        QFile competing(target);
        const QByteArray bytes("competing writer");
        if(!competing.open(QIODevice::WriteOnly|QIODevice::NewOnly)||competing.write(bytes)!=bytes.size()||!competing.flush())return 94;
    }
    return code;
}

int main(int argc, char** argv) {
    if(qEnvironmentVariableIsSet("DUNG_PDF_TEST_ENCRYPTION_PROBE")){
        QCoreApplication app(argc,argv);
        const auto args=app.arguments().mid(1);
        if(args.contains(QStringLiteral("--show-npages"))){std::fputs("3\n",stdout);return 0;}
        if(args.contains(QStringLiteral("--is-encrypted"))){
            const auto mode=qgetenv("DUNG_PDF_TEST_ENCRYPTION_PROBE");
            if(mode=="diagnostic"){std::fputs("simulated qpdf inspection error\n",stderr);return 2;}
            if(mode=="unexpected")return 1;
            return 2;
        }
        return 90;
    }
    if(qEnvironmentVariableIsSet("DUNG_PDF_TEST_REAL_QPDF")&&qEnvironmentVariableIsSet("DUNG_PDF_TEST_RACE_TARGET"))
        return raceProxy(argc,argv);
    if(qgetenv("QT_QPA_PLATFORM").isEmpty()) {
#ifdef Q_OS_WIN
        // Windows native font database is necessary; no visible windows are created.
        qputenv("QT_QPA_PLATFORM","windows");
#else
        qputenv("QT_QPA_PLATFORM","offscreen");
#endif
    }
    QGuiApplication app(argc, argv);
    const QString qpdf = findQpdf();
    std::cout << "qpdf: " << (qpdf.isEmpty() ? "KHÔNG THẤY" : qpdf.toStdString()) << "\n";
    if (qpdf.isEmpty()) {
        std::cout << "KẾT QUẢ: LỖI (không có qpdf)\n";
        return 2;
    }

    QTemporaryDir tdir;
    const QString d = tdir.path();
    auto P = [&](const QString& n) { return d + QLatin1Char('/') + n; };
    const QString a = P(QStringLiteral("a.pdf")), b = P(QStringLiteral("b.pdf"));
    writeFile(a, makePdf(3, QStringLiteral("A")));
    writeFile(b, makePdf(2, QStringLiteral("B")));
    const QByteArray aBytes = readFile(a), bBytes = readFile(b);

    // --- parsePages
    QString perr;
    CHECK(parsePages(QStringLiteral("1-3,5,8-"), 9, &perr) == (QList<int>{0, 1, 2, 4, 7, 8}), "parsePages '1-3,5,8-' (9 trang)");
    CHECK(parsePages(QStringLiteral("2-"), 4) == (QList<int>{1, 2, 3}), "parsePages mở đầu cuối '2-'");
    for (const char* bad : {"0", "4", "3-1", "", "x", "1,,x", "1,,2", "1,", ",1", "1 2", "-", "+1", "1-2-3"}) {
        QString e;
        CHECK(parsePages(QString::fromLatin1(bad), 3, &e).isEmpty() && !e.isEmpty(), QStringLiteral("parsePages từ chối '%1'").arg(QString::fromLatin1(bad)));
    }

    CHECK(parsePages(QStringLiteral("1 - 3"),3,&perr)==(QList<int>{0,1,2}) && perr.isEmpty(),"page range spaces around dash");
    CHECK(parsePages(QStringLiteral("1-1000001"),1000001,&perr).isEmpty(),"reject excessive page expansion");

    // --- info
    const Info ia = info(a);
    CHECK(ia.ok && ia.pages == 3 && !ia.encrypted, "info: 3 trang, không mật khẩu");
    CHECK(!info(P(QStringLiteral("khong-co.pdf"))).ok, "info: tệp không tồn tại báo lỗi");

    // --- reorder: đủ trang, đúng thứ tự, giữ nguyên nguồn.
    const QString reordered = P(QStringLiteral("reorder.pdf"));
    CHECK(reorder({a, {}}, QStringLiteral("3,1,2"), reordered).ok && info(reordered).pages == 3, "Sắp xếp 3,1,2 đủ 3 trang");
    const QString first = P(QStringLiteral("reorder-first.pdf"));
    CHECK(extract({reordered, {}}, QStringLiteral("1"), first).ok && qdfOf(first).contains("(A 3)"), "Trang đầu bản sắp xếp là trang 3 gốc");
    CHECK(!reorder({a, {}}, QStringLiteral("1,1,3"), P(QStringLiteral("invalid-order.pdf"))).ok, "Từ chối trang trùng");
    CHECK(!reorder({a, {}}, QStringLiteral("1,3"), P(QStringLiteral("invalid-order.pdf"))).ok, "Từ chối thiếu trang");
    CHECK(!reorder({a, {}}, QStringLiteral("3,1,2"), a, true).ok && readFile(a) == aBytes, "Không ghi đè nguồn khi sắp xếp");

    // --- remove specified pages, keep original order.
    const auto plan=planRemovePages(QStringLiteral("2,2"),3);
    CHECK(plan.ok&&plan.removed==1&&plan.retained==2&&plan.keepSpec=="1,3","delete duplicates count once");
    CHECK(!planRemovePages(QStringLiteral("1-3"),3).ok,"reject deleting all pages");
    CHECK(!planRemovePages(QStringLiteral("4"),3).ok,"reject out of range deletion");
    const auto removed=P(QStringLiteral("removed.pdf"));
    CHECK(removePages({a,{}},QStringLiteral("2"),3,removed).ok&&info(removed).pages==2,"delete page 2 from real PDF");
    const auto after=P(QStringLiteral("removed-last.pdf"));
    CHECK(extract({removed,{}},QStringLiteral("2"),after).ok&&qdfOf(after).contains("(A 3)"),"retained pages in original order");
    CHECK(!removePages({a,{}},QStringLiteral("2"),4,P(QStringLiteral("changed.pdf"))).ok&&!QFile::exists(P(QStringLiteral("changed.pdf"))),"reject changed source page count");
    CHECK(!removePages({a,{}},QStringLiteral("2"),3,removed).ok,"refuse existing output");
    CHECK(!removePages({a,{}},QStringLiteral("2"),3,a).ok&&readFile(a)==aBytes,"source unchanged");
    // --- merge
    Result r = merge({{a, {}}, {b, {}}}, P(QStringLiteral("gop.pdf")));
    CHECK(r.ok && info(P(QStringLiteral("gop.pdf"))).pages == 5, "gộp 3+2 thành 5 trang");
    CHECK(readFile(a) == aBytes && readFile(b) == bBytes, "gộp: hai tệp nguồn giữ nguyên từng byte");
    r = merge({{a, {}}, {b, {}}}, P(QStringLiteral("gop.pdf")));
    CHECK(!r.ok && !r.error.isEmpty(), "gộp: từ chối khi đích đã tồn tại");
    const QByteArray oldGop = readFile(P(QStringLiteral("gop.pdf")));
    r = merge({{a, {}}, {b, {}}}, P(QStringLiteral("gop.pdf")), true);
    CHECK(r.ok && !r.backup.isEmpty() && readFile(r.backup) == oldGop, "gộp: thay thế có sao lưu đúng nội dung cũ");
    r = merge({{a, {}}, {b, {}}}, a, true);
    CHECK(!r.ok && readFile(a) == aBytes, "gộp: không được ghi đè lên chính tệp nguồn");
    r = merge({{a, {}}}, P(QStringLiteral("mot.pdf")));
    CHECK(!r.ok, "gộp: cần ít nhất hai tệp");

    const QString alias=P(QStringLiteral("source-hardlink.pdf"));
#ifdef Q_OS_WIN
    const bool linked=CreateHardLinkW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(alias).utf16()),
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(a).utf16()),nullptr)!=0;
#else
    const bool linked=::link(QFile::encodeName(a).constData(),QFile::encodeName(alias).constData())==0;
#endif
    if(linked){
        CHECK(!extract({a,{}},QStringLiteral("1"),alias,true).ok && readFile(alias)==aBytes && readFile(a)==aBytes,
              "reject hardlink destination even with replacement consent");
    }else std::cout<<"SKIP hardlink unavailable on test filesystem\n";
#ifndef Q_OS_WIN
    const QString dangling=P(QStringLiteral("dangling.pdf")), absent=P(QStringLiteral("absent.pdf"));
    if(::symlink(QFile::encodeName(absent).constData(),QFile::encodeName(dangling).constData())==0){
        CHECK(!extract({a,{}},QStringLiteral("1"),dangling,true).ok && QFileInfo(dangling).isSymLink() && !QFileInfo::exists(absent),
              "reject dangling symlink without replacing or creating target");
    }
#endif
    CHECK(!extract({a,{}},QStringLiteral("1"),d,true).ok,"reject directory destination");

    // The destination did not exist at operation start, but is created by
    // another writer after qpdf finishes. The native output must not clobber it.
    const QString raceTarget=P(QStringLiteral("race.pdf"));
    {
        const QByteArray oldQpdf=qgetenv("DUNG_QPDF");
        const bool hadQpdf=qEnvironmentVariableIsSet("DUNG_QPDF");
        qputenv("DUNG_PDF_TEST_REAL_QPDF",qpdf.toUtf8());
        qputenv("DUNG_PDF_TEST_RACE_TARGET",raceTarget.toUtf8());
        qputenv("DUNG_QPDF",QCoreApplication::applicationFilePath().toUtf8());
        const auto racing=extract({a,{}},QStringLiteral("1"),raceTarget,false);
        if(hadQpdf)qputenv("DUNG_QPDF",oldQpdf);else qunsetenv("DUNG_QPDF");
        qunsetenv("DUNG_PDF_TEST_REAL_QPDF");qunsetenv("DUNG_PDF_TEST_RACE_TARGET");
        CHECK(!racing.ok && readFile(raceTarget)=="competing writer" && readFile(a)==aBytes,
              "competing destination created during qpdf is not overwritten");
        CHECK(tempLeftovers(d)==0,"no temporary output after competing writer");
    }

    // --- tên tiếng Việt có dấu và dấu cách
    const QString vn = P(QStringLiteral("Báo cáo tháng 1 Dũng.pdf"));
    CHECK(QFile::copy(a, vn), "tạo tệp tên tiếng Việt");
    r = merge({{vn, {}}, {b, {}}}, P(QStringLiteral("Gộp tiếng Việt.pdf")));
    CHECK(r.ok && info(P(QStringLiteral("Gộp tiếng Việt.pdf"))).pages == 5, "gộp với tên tệp tiếng Việt");

    // --- extract
    r = extract({a, {}}, QStringLiteral("1,3"), P(QStringLiteral("tach.pdf")));
    CHECK(r.ok && info(P(QStringLiteral("tach.pdf"))).pages == 2, "tách trang '1,3' còn 2 trang");
    r = extract({a, {}}, QStringLiteral("9"), P(QStringLiteral("tach9.pdf")));
    CHECK(!r.ok && !QFileInfo::exists(P(QStringLiteral("tach9.pdf"))), "tách: khoảng trang sai bị từ chối, không tạo tệp");

    // --- splitEach
    QDir(d).mkdir(QStringLiteral("split"));
    r = splitEach({a, {}}, P(QStringLiteral("split")));
    CHECK(r.ok && r.files.size() == 3 && QFileInfo::exists(P(QStringLiteral("split/a-trang001.pdf"))) &&
              QFileInfo::exists(P(QStringLiteral("split/a-trang003.pdf"))),
          "tách mỗi trang một tệp: 3 tệp a-trangNNN.pdf");
    bool allOne = r.ok;
    for (const QString& f : r.files) allOne = allOne && info(f).pages == 1;
    CHECK(allOne, "mỗi tệp tách ra có đúng 1 trang");
    r = splitEach({a, {}}, P(QStringLiteral("split")));
    CHECK(!r.ok && tempLeftovers(P(QStringLiteral("split"))) == 0, "tách lần hai bị từ chối (không ghi đè), không còn thư mục tạm");

    // --- rotate
    r = rotate({a, {}}, QStringLiteral("1"), 90, P(QStringLiteral("xoay.pdf")));
    CHECK(r.ok && readFile(P(QStringLiteral("xoay.pdf"))).contains("/Rotate 90"), "xoay trang 1 thêm 90 độ");
    CHECK(readFile(a) == aBytes, "xoay: tệp nguồn giữ nguyên");
    CHECK(!rotate({a, {}}, QStringLiteral("1"), 45, P(QStringLiteral("x45.pdf"))).ok, "xoay: từ chối góc không phải bội của 90");
    CHECK(!rotate({a, {}}, QStringLiteral("1"), 360, P(QStringLiteral("x360.pdf"))).ok, "xoay: góc 360 độ không có gì để làm");
    r = rotate({a, {}}, QStringLiteral("1-"), -90, P(QStringLiteral("xoay-trai.pdf")));
    CHECK(r.ok && readFile(P(QStringLiteral("xoay-trai.pdf"))).contains("/Rotate 270"), "xoay -90 độ thành 270");

    // --- compress
    r = compress({a, {}}, P(QStringLiteral("nen.pdf")));
    CHECK(r.ok && info(P(QStringLiteral("nen.pdf"))).pages == 3, "nén giữ nguyên số trang");
    CHECK(readFile(a) == aBytes, "nén: tệp nguồn giữ nguyên");

    // --- tệp hỏng
    QByteArray cut = aBytes.left(aBytes.size() / 2);
    const QString bad = P(QStringLiteral("hong.pdf"));
    writeFile(bad, cut);
    Info ib = info(bad);
    {   // chẩn đoán: qpdf trả gì với tệp cắt cụt
        QProcess dp;
        dp.start(findQpdf(), {QStringLiteral("--show-npages"), bad});
        dp.waitForFinished(60000);
        std::printf("CHẨN ĐOÁN tệp cắt cụt: mã=%d trang=%s lỗi=%s\n", dp.exitCode(),
                    dp.readAllStandardOutput().trimmed().constData(),
                    dp.readAllStandardError().left(200).constData());
    }
    CHECK((!ib.ok && !ib.error.isEmpty()) || (ib.ok && !ib.warning.isEmpty()), "tệp bị cắt cụt: báo lỗi hoặc cảnh báo đã phục hồi");
    writeFile(P(QStringLiteral("khong-phai-pdf.pdf")), "khong phai pdf");
    CHECK(!info(P(QStringLiteral("khong-phai-pdf.pdf"))).ok, "tệp không phải PDF: báo lỗi");
    r = merge({{bad, {}}, {b, {}}}, P(QStringLiteral("gop-hong.pdf")));
    CHECK((!r.ok && !QFileInfo::exists(P(QStringLiteral("gop-hong.pdf")))) || (r.ok && !r.warnings.isEmpty()),
          "gộp với tệp hỏng: từ chối, hoặc ghi kết quả kèm cảnh báo");
    CHECK(readFile(bad) == cut, "gộp với tệp hỏng: tệp nguồn giữ nguyên");

    // Encryption inspection errors must never be treated as an unencrypted file.
    {
        const auto old=qgetenv("DUNG_QPDF");const bool had=qEnvironmentVariableIsSet("DUNG_QPDF");
        qputenv("DUNG_QPDF",QCoreApplication::applicationFilePath().toUtf8());
        qputenv("DUNG_PDF_TEST_ENCRYPTION_PROBE","unexpected");
        CHECK(!info(a).ok,"unexpected encryption probe status refused");
        qputenv("DUNG_PDF_TEST_ENCRYPTION_PROBE","diagnostic");
        CHECK(!info(a).ok,"status 2 with error diagnostics refused");
        qputenv("DUNG_PDF_TEST_ENCRYPTION_PROBE","plain");
        const auto plain=info(a);
        CHECK(plain.ok&&!plain.encrypted&&plain.pages==3,"silent status 2 is unencrypted");
        qunsetenv("DUNG_PDF_TEST_ENCRYPTION_PROBE");
        if(had)qputenv("DUNG_QPDF",old);else qunsetenv("DUNG_QPDF");
    }
    const auto ownerOnly=P(QStringLiteral("owner-only.pdf"));
    {
        QProcess p;
        p.start(qpdf,{QStringLiteral("--encrypt"),QStringLiteral(""),QStringLiteral("owner-only-test"),QStringLiteral("256"),
            QStringLiteral("--"),a,ownerOnly});
        const bool finished=p.waitForFinished(60000);
        CHECK(finished&&p.exitStatus()==QProcess::NormalExit&&p.exitCode()==0&&QFileInfo::exists(ownerOnly),"create owner-password-only fixture");
        const auto ownerInfo=info(ownerOnly);
        CHECK(ownerInfo.ok&&ownerInfo.encrypted&&ownerInfo.pages==3,"encrypted PDF with empty user password still requires encryption warning");
    }
    // --- mật khẩu
    const QString enc = P(QStringLiteral("mat-khau.pdf"));
    {
        QProcess p;
        p.start(qpdf, {QStringLiteral("--encrypt"), QStringLiteral("1234"), QStringLiteral("owner"), QStringLiteral("256"),
                       QStringLiteral("--"), a, enc});
        p.waitForFinished(60000);
        CHECK(p.exitCode() == 0 && QFileInfo::exists(enc), "tạo PDF mã hóa để thử");
    }
    const QByteArray encBytes = readFile(enc);
    {   // chẩn đoán: cách đưa mật khẩu cho qpdf
        QTemporaryFile pf;
        if (pf.open()) { pf.write("1234"); pf.close(); }
        const QStringList variants[] = {
            {QStringLiteral("--password-file=") + pf.fileName(), QStringLiteral("--show-npages"), enc},
            {QStringLiteral("--password=1234"), QStringLiteral("--show-npages"), enc},
            {QStringLiteral("--password-file=") + QDir::toNativeSeparators(pf.fileName()), QStringLiteral("--show-npages"), enc}};
        for (const QStringList& v : variants) {
            QProcess dp;
            dp.start(findQpdf(), v);
            dp.waitForFinished(60000);
            std::printf("CHẨN ĐOÁN mật khẩu [%s]: mã=%d ra=%s lỗi=%s\n", v.value(0).left(24).toUtf8().constData(),
                        dp.exitCode(), dp.readAllStandardOutput().trimmed().constData(),
                        dp.readAllStandardError().left(200).constData());
        }
    }
    Info ie = info(enc);
    CHECK(!ie.ok && ie.needPassword, "tệp mật khẩu: báo cần mật khẩu");
    CHECK(info(enc, QStringLiteral("sai")).needPassword, "tệp mật khẩu: mật khẩu sai vẫn báo cần mật khẩu");
    ie = info(enc, QStringLiteral("1234"));
    CHECK(ie.ok && ie.pages == 3 && ie.encrypted, "tệp mật khẩu: mở đúng với mật khẩu đúng");
    r = extract({enc, {}}, QStringLiteral("1"), P(QStringLiteral("giai-ma-thieu.pdf")));
    CHECK(!r.ok && r.needPassword, "tách tệp mật khẩu khi thiếu mật khẩu bị từ chối");
    r = extract({enc, QStringLiteral("1234")}, QStringLiteral("1-2"), P(QStringLiteral("giai-ma.pdf")));
    CHECK(r.ok && info(P(QStringLiteral("giai-ma.pdf"))).pages == 2 && !info(P(QStringLiteral("giai-ma.pdf"))).encrypted,
          "tách tệp mật khẩu: kết quả mở không cần mật khẩu");
    CHECK(!r.warnings.isEmpty(), "tách tệp mật khẩu: có cảnh báo kết quả không còn mật khẩu");
    CHECK(readFile(enc) == encBytes, "tệp mật khẩu nguồn giữ nguyên từng byte");


    // ======== Đánh số trang ========
    {
        NumberOptions no;
        no.format = QStringLiteral("Trang {n}/{total}");
        r = addPageNumbers({a, {}}, no, P(QStringLiteral("so.pdf")));
        const QByteArray q = r.ok ? qdfOf(P(QStringLiteral("so.pdf"))) : QByteArray();
        CHECK(r.ok && info(P(QStringLiteral("so.pdf"))).pages == 3, "đánh số trang: kết quả 3 trang");
        CHECK(q.contains("(Trang 1/3) Tj") && q.contains("(Trang 2/3) Tj") && q.contains("(Trang 3/3) Tj"), "đánh số trang: có 'Trang 1/3'..'Trang 3/3' trong nội dung");
        CHECK(readFile(a) == aBytes, "đánh số trang: tệp nguồn giữ nguyên từng byte");
        r = addPageNumbers({a, {}}, no, P(QStringLiteral("so.pdf")));
        CHECK(!r.ok && !r.error.isEmpty(), "đánh số trang: từ chối khi đích đã tồn tại");
        r = addPageNumbers({a, {}}, no, a, true);
        CHECK(!r.ok && readFile(a) == aBytes, "đánh số trang: không được ghi đè lên tệp nguồn");
        NumberOptions part;
        part.pages = QStringLiteral("2-");
        part.start = 1;
        part.pos = Position::TopRight;
        r = addPageNumbers({a, {}}, part, P(QStringLiteral("so2.pdf")));
        const QByteArray q2 = r.ok ? qdfOf(P(QStringLiteral("so2.pdf"))) : QByteArray();
        CHECK(r.ok && q2.contains("(1) Tj") && q2.contains("(2) Tj") && !q2.contains("(3) Tj"), "đánh số trang '2-': trang 2 là số 1, trang 3 là số 2, không có số 3");
        NumberOptions vn;
        vn.format = QString::fromUtf8("Trang số {n}");
        r = addPageNumbers({a, {}}, vn, P(QStringLiteral("so-vn.pdf")));
        CHECK(!r.ok && !QFileInfo::exists(P(QStringLiteral("so-vn.pdf"))), "đánh số trang: chữ có dấu bị từ chối, không tạo tệp");
        NumberOptions diag;
        diag.pos = Position::Diagonal;
        CHECK(!addPageNumbers({a, {}}, diag, P(QStringLiteral("so-cheo.pdf"))).ok, "đánh số trang: vị trí đường chéo bị từ chối");
        NumberOptions badp;
        badp.pages = QStringLiteral("9");
        CHECK(!addPageNumbers({a, {}}, badp, P(QStringLiteral("so9.pdf"))).ok, "đánh số trang: khoảng trang sai bị từ chối");
        // trang đã xoay 90 độ vẫn đánh số được (đã tạo xoay.pdf ở trên)
        r = addPageNumbers({P(QStringLiteral("xoay.pdf")), {}}, no, P(QStringLiteral("so-xoay.pdf")));
        CHECK(r.ok && info(P(QStringLiteral("so-xoay.pdf"))).pages == 3 && readFile(P(QStringLiteral("so-xoay.pdf"))).contains("/Rotate 90"),
              "đánh số trang: trang xoay 90 độ vẫn giữ xoay và có số");
        const QString ptt = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
        if (!ptt.isEmpty()) {  // chẩn đoán thêm, không dùng để kết luận
            QProcess pp;
            pp.start(ptt, {P(QStringLiteral("so.pdf")), QStringLiteral("-")});
            pp.waitForFinished(30000);
            std::printf("CHẨN ĐOÁN pdftotext so.pdf: %s\n", pp.readAllStandardOutput().simplified().left(160).constData());
        }
        // tệp mật khẩu
        r = addPageNumbers({enc, {}}, no, P(QStringLiteral("so-mk.pdf")));
        CHECK(!r.ok && r.needPassword, "đánh số trang: tệp mật khẩu thiếu mật khẩu bị từ chối");
        r = addPageNumbers({enc, QStringLiteral("1234")}, no, P(QStringLiteral("so-mk.pdf")));
        CHECK(r.ok && !r.warnings.isEmpty() && !info(P(QStringLiteral("so-mk.pdf"))).encrypted, "đánh số trang: tệp mật khẩu đúng mật khẩu, kèm cảnh báo mất mật khẩu");
    }

    // ======== Ảnh thử ========
    const QString png = P(QStringLiteral("anh rgb.png")), pngA = P(QStringLiteral("anh trong suot.png")), pngP = P(QStringLiteral("anh doc.png"));
    const QString jpg = P(QStringLiteral("anh.jpg")), jpg6 = P(QStringLiteral("anh-exif6.jpg"));
    {
        QImage i1(40, 30, QImage::Format_RGB32);
        i1.fill(QColor(10, 120, 200));
        QImage i2(20, 20, QImage::Format_ARGB32);
        i2.fill(QColor(0, 0, 255, 128));
        QImage i3(30, 40, QImage::Format_RGB32);
        i3.fill(QColor(0, 160, 0));
        CHECK(i1.save(png, "PNG") && i2.save(pngA, "PNG") && i3.save(pngP, "PNG"), "tạo ảnh PNG thử");
        CHECK(writeFile(jpg, QByteArray::fromBase64(kJpeg)) && writeFile(jpg6, QByteArray::fromBase64(kJpegExif6)), "tạo ảnh JPEG thử");
    }
    const QByteArray jpgBytes = readFile(jpg), pngBytes = readFile(png);

    // ======== Đóng dấu ========
    {
        StampOptions st;
        st.text = QStringLiteral("BAN NHAP");
        r = stamp({a, {}}, st, P(QStringLiteral("dau.pdf")));
        const QByteArray q = r.ok ? qdfOf(P(QStringLiteral("dau.pdf"))) : QByteArray();
        CHECK(r.ok && info(P(QStringLiteral("dau.pdf"))).pages == 3, "đóng dấu chữ: kết quả 3 trang");
        CHECK(q.contains("(BAN NHAP) Tj") && q.contains("/ca 0.3"), "đóng dấu chữ: có chữ và độ đậm 0.3 trong nội dung");
        CHECK(readFile(a) == aBytes, "đóng dấu: tệp nguồn giữ nguyên từng byte");
        StampOptions sv;
        sv.text = QString::fromUtf8("BẢN NHÁP");
        sv.pages=QStringLiteral("2");sv.pos=Position::Custom;sv.fontSize=28;sv.opacity=0.7;
        const QString vietnamese=P(QStringLiteral("dau-vn.pdf"));
        const auto vr=stamp({a,{}},sv,vietnamese);
        CHECK(vr.ok && info(vietnamese).pages==3,QStringLiteral("đóng dấu chữ Việt Unicode: đủ trang — ")+vr.error);
        const auto vq=vr.ok?qdfOf(vietnamese):QByteArray();
        CHECK(vq.contains("/ToUnicode") && (vq.contains("/FontFile2")||vq.contains("/FontFile3")),"lớp chữ Việt có font nhúng và bản đồ Unicode");
        CHECK(vq.contains("(A 1) Tj") && vq.contains("(A 2) Tj") && vq.contains("(A 3) Tj"),"giữ chữ vector gốc");
        const QString reader=QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
        if(!reader.isEmpty()&&vr.ok){
            // Specify encoding explicitly instead of inheriting platform/reader defaults.
            // Normalize whitespace only (never strip accents or replace characters).
            for(int page=1;page<=3;++page){
                QProcess proc;
                proc.start(reader,{QStringLiteral("-enc"),QStringLiteral("UTF-8"),QStringLiteral("-f"),QString::number(page),
                    QStringLiteral("-l"),QString::number(page),vietnamese,QStringLiteral("-")});
                const bool ok=proc.waitForStarted(3000)&&proc.waitForFinished(15000)&&proc.exitStatus()==QProcess::NormalExit&&proc.exitCode()==0;
                const auto raw=proc.readAllStandardOutput();
                const auto text=QString::fromUtf8(raw).normalized(QString::NormalizationForm_C).simplified();
                const bool correct=text.count(QStringLiteral("BẢN NHÁP"))==(page==2?1:0);
                CHECK(ok&&correct,QStringLiteral("đọc lại Unicode trang %1: %2; UTF-8 hex=%3; stderr=%4")
                    .arg(page).arg(text).arg(QString::fromLatin1(raw.toHex())).arg(QString::fromUtf8(proc.readAllStandardError())));
            }
        }
        CHECK(readFile(a)==aBytes,"lớp chữ Việt không sửa nguồn");
        sv.text=QStringLiteral("Dòng 1\nDòng 2");
        CHECK(!stamp({a,{}},sv,P(QStringLiteral("dau-vn-loi.pdf"))).ok,"từ chối dấu nhiều dòng");
        CHECK(!stamp({a, {}}, StampOptions(), P(QStringLiteral("dau-rong.pdf"))).ok, "đóng dấu: thiếu chữ và ảnh bị từ chối");
        StampOptions one;
        one.text = QStringLiteral("CHI TRANG 2");
        one.pages = QStringLiteral("2");
        one.pos = Position::TopLeft;
        r = stamp({a, {}}, one, P(QStringLiteral("dau-t2.pdf")));
        CHECK(r.ok && qdfOf(P(QStringLiteral("dau-t2.pdf"))).count("(CHI TRANG 2) Tj") == 1, "đóng dấu trang '2': chữ xuất hiện đúng một lần");
        StampOptions custom = one;
        custom.pos = Position::Custom; custom.xPercent = 0; custom.yPercent = 0; custom.opacity = 1;
        r = stamp({a, {}}, custom, P(QStringLiteral("dau-custom.pdf")));
        CHECK(r.ok && qdfOf(P(QStringLiteral("dau-custom.pdf"))).count("(CHI TRANG 2) Tj") == 1, "lớp phủ tùy vị trí đúng trang");
        custom.xPercent = 101;
        CHECK(!stamp({a, {}}, custom, P(QStringLiteral("dau-bad-pos.pdf"))).ok && !QFileInfo::exists(P(QStringLiteral("dau-bad-pos.pdf"))), "từ chối vị trí ngoài phạm vi");
        CHECK(readFile(a) == aBytes, "lớp phủ tùy vị trí giữ nguồn");
        CHECK(validateOcrRange(1, 1, 1).isEmpty(), "OCR khoảng một trang");
        CHECK(validateOcrRange(2, 4, 6).isEmpty(), "OCR khoảng nhiều trang");
        CHECK(!validateOcrRange(0, 1, 6).isEmpty() && !validateOcrRange(4, 2, 6).isEmpty() && !validateOcrRange(1, 7, 6).isEmpty(), "OCR từ chối khoảng không hợp lệ");
        CHECK(validateOcrRange(1, kOcrMaxPages, kOcrMaxPages).isEmpty() && !validateOcrRange(1, kOcrMaxPages+1, kOcrMaxPages+1).isEmpty(), "OCR giới hạn số trang mỗi lượt");
        CHECK(!recognizeImage(pngA, QStringLiteral("invalid")).ok, "OCR từ chối ngôn ngữ không hỗ trợ");
        CHECK(!recognizeImage(P(QStringLiteral("not-exist.png")), QStringLiteral("eng")).ok, "OCR từ chối ảnh không tồn tại");
        StampOptions si;
        si.imagePath = pngA;
        si.pos = Position::BottomRight;
        si.widthPercent = 20;
        r = stamp({a, {}}, si, P(QStringLiteral("dau-anh.pdf")));
        const QByteArray qi = r.ok ? qdfOf(P(QStringLiteral("dau-anh.pdf"))) : QByteArray();
        CHECK(r.ok && info(P(QStringLiteral("dau-anh.pdf"))).pages == 3 && qi.contains("/SMask"), "đóng dấu ảnh PNG trong suốt: giữ kênh trong suốt (SMask)");
        si.imagePath = jpg;
        r = stamp({a, {}}, si, P(QStringLiteral("dau-jpg.pdf")));
        CHECK(r.ok && readFile(P(QStringLiteral("dau-jpg.pdf"))).contains(jpgBytes), "đóng dấu ảnh JPEG: nhúng nguyên bản từng byte");
        si.imagePath = P(QStringLiteral("khong-co.png"));
        CHECK(!stamp({a, {}}, si, P(QStringLiteral("dau-x.pdf"))).ok, "đóng dấu: ảnh không tồn tại bị từ chối");
        si.imagePath = a;
        CHECK(!stamp({a, {}}, si, P(QStringLiteral("dau-y.pdf"))).ok, "đóng dấu: tệp không phải ảnh bị từ chối");
    }

    // ======== Ảnh -> PDF ========
    {
        ImageOptions io;
        r = imagesToPdf({png, pngA, jpg}, io, P(QStringLiteral("tu-anh.pdf")));
        CHECK(r.ok && info(P(QStringLiteral("tu-anh.pdf"))).pages == 3, "ảnh -> PDF: 3 ảnh thành 3 trang");
        const QByteArray out = readFile(P(QStringLiteral("tu-anh.pdf")));
        CHECK(out.contains(jpgBytes), "ảnh -> PDF: JPEG được nhúng nguyên bản từng byte");
        const QByteArray qd = qdfOf(P(QStringLiteral("tu-anh.pdf")));
        CHECK(qd.contains("/DCTDecode") && qd.contains("/SMask"), "ảnh -> PDF: có DCTDecode (JPEG) và SMask (PNG trong suốt)");
        CHECK(qd.contains("19.2") && qd.contains("14.4"), "ảnh -> PDF: PNG 40x30 ở 150 dpi cho trang 19.2 x 14.4 pt");
        CHECK(readFile(png) == pngBytes && readFile(jpg) == jpgBytes, "ảnh -> PDF: ảnh nguồn giữ nguyên từng byte");
        r = imagesToPdf({png}, io, P(QStringLiteral("tu-anh.pdf")));
        CHECK(!r.ok, "ảnh -> PDF: từ chối khi đích đã tồn tại");
        r = imagesToPdf({png}, io, png, true);
        CHECK(!r.ok && readFile(png) == pngBytes, "ảnh -> PDF: không được ghi đè lên chính ảnh nguồn");
        CHECK(!imagesToPdf({}, io, P(QStringLiteral("trong.pdf"))).ok, "ảnh -> PDF: danh sách rỗng bị từ chối");
        CHECK(!imagesToPdf({a}, io, P(QStringLiteral("khong-phai-anh.pdf"))).ok && !QFileInfo::exists(P(QStringLiteral("khong-phai-anh.pdf"))),
              "ảnh -> PDF: tệp không phải ảnh bị từ chối, không tạo tệp");
        CHECK(!imagesToPdf({png, P(QStringLiteral("khong-co.png"))}, io, P(QStringLiteral("mot-hong.pdf"))).ok && !QFileInfo::exists(P(QStringLiteral("mot-hong.pdf"))),
              "ảnh -> PDF: một ảnh lỗi thì hủy cả lô, không tạo tệp dở");
        r = imagesToPdf({jpg6}, io, P(QStringLiteral("exif6.pdf")));
        CHECK(r.ok && readFile(P(QStringLiteral("exif6.pdf"))).contains("/Rotate 90"), "ảnh -> PDF: EXIF hướng 6 thành trang xoay 90 độ");
        ImageOptions a4;
        a4.fitA4 = true;
        r = imagesToPdf({pngP, png}, a4, P(QStringLiteral("a4.pdf")));
        const QByteArray q4 = r.ok ? qdfOf(P(QStringLiteral("a4.pdf"))) : QByteArray();
        CHECK(r.ok && info(P(QStringLiteral("a4.pdf"))).pages == 2 && q4.contains("595.276") && q4.contains("841.89"), "ảnh -> PDF A4: trang A4 (ảnh dọc và ảnh ngang)");
        ImageOptions bad;
        bad.dpi = 5;
        CHECK(!imagesToPdf({png}, bad, P(QStringLiteral("dpi.pdf"))).ok, "ảnh -> PDF: dpi ngoài khoảng bị từ chối");
        // PDF tạo ra phải đọc lại được bằng các công cụ khác của chương trình
        r = extract({P(QStringLiteral("tu-anh.pdf")), {}}, QStringLiteral("1-2"), P(QStringLiteral("tu-anh-tach.pdf")));
        CHECK(r.ok && info(P(QStringLiteral("tu-anh-tach.pdf"))).pages == 2, "ảnh -> PDF: kết quả tách trang được bằng công cụ khác");
    }

    // ======== Trang trắng ========
    {
        const QString bl = P(QStringLiteral("trang-trang.pdf"));
        const QList<PageSpec> specs = {
            {"BT /F1 12 Tf 72 720 Td (Co chu) Tj ET", nullptr},  // 0: có chữ
            {" ", nullptr},                                        // 1: nội dung rỗng
            {"q Q", nullptr},                                      // 2: chỉ trạng thái đồ họa
            {"1 1 1 rg 0 0 100 100 re f", nullptr},                // 3: ô tô nền trắng (KHÔNG phát hiện, đúng như ghi chú)
            {" ", "/Annots [ << /Type /Annot /Subtype /Text /Rect [10 10 20 20] /Contents (ghi chu) >> ]"},  // 4: chỉ có chú thích
            {nullptr, nullptr},                                    // 5: không có /Contents
            {"% chu thich (khong ve) \n BT /F1 12 Tf (Bo qua) ET", nullptr},  // 6: chỉ có BT/ET và chú thích: không vẽ gì
        };
        writeFile(bl, makeCustomPdf(specs));
        const QByteArray blBytes = readFile(bl);
        CHECK(info(bl).ok && info(bl).pages == 7, "tạo PDF thử có trang trắng (7 trang)");
        const BlankReport br = findBlankPages({bl, {}});
        CHECK(br.ok && br.blank == (QList<int>{1, 2, 5, 6}), "trang trắng: nhận ra trang 2, 3, 6, 7 (không nhận trang có chữ, ô tô nền, chú thích)");
        CHECK(!br.notes.isEmpty(), "trang trắng: báo kèm giới hạn của phép kiểm tra");
        const BlankReport none = findBlankPages({a, {}});
        CHECK(none.ok && none.blank.isEmpty(), "trang trắng: tệp không có trang trắng cho danh sách rỗng");
        r = removeBlankPages({bl, {}}, P(QStringLiteral("het-trang-trang.pdf")));
        CHECK(r.ok && info(P(QStringLiteral("het-trang-trang.pdf"))).pages == 3, "bỏ trang trắng: còn 3 trang (7 trừ 4)");
        CHECK(readFile(bl) == blBytes, "bỏ trang trắng: tệp nguồn giữ nguyên từng byte");
        r = removeBlankPages({a, {}}, P(QStringLiteral("khong-bo.pdf")));
        CHECK(!r.ok && !QFileInfo::exists(P(QStringLiteral("khong-bo.pdf"))), "bỏ trang trắng: không có trang trắng thì không tạo tệp");
        const QString allBlank = P(QStringLiteral("toan-trang.pdf"));
        writeFile(allBlank, makeCustomPdf({{" ", nullptr}, {nullptr, nullptr}}));
        CHECK(!removeBlankPages({allBlank, {}}, P(QStringLiteral("rong.pdf"))).ok && !QFileInfo::exists(P(QStringLiteral("rong.pdf"))),
              "bỏ trang trắng: mọi trang đều trắng thì từ chối, không tạo tệp rỗng");
        const BlankReport be = findBlankPages({enc, {}});
        CHECK(!be.ok && be.needPassword, "trang trắng: tệp mật khẩu thiếu mật khẩu báo cần mật khẩu");
        const BlankReport be2 = findBlankPages({enc, QStringLiteral("1234")});
        CHECK(be2.ok && be2.blank.isEmpty(), "trang trắng: tệp mật khẩu với mật khẩu đúng đọc được");
    }

    // --- helper kiểm tra chữ ASCII (dùng trong giao diện để báo trước)
    CHECK(isAsciiText(QStringLiteral("Trang {n}/{total} - CONFIDENTIAL")), "isAsciiText: chữ không dấu hợp lệ");
    CHECK(!isAsciiText(QStringLiteral("Trang {n} - Bảo mật")), "isAsciiText: chữ có dấu bị nhận ra");
    CHECK(!isAsciiText(QStringLiteral("a\tb")), "isAsciiText: ký tự điều khiển bị nhận ra");

    // --- dọn dẹp
    CHECK(tempLeftovers(d) == 0, "không còn tệp tạm ~dung-* trong thư mục làm việc");
    CHECK(suggestOutputPath(a).endsWith(QStringLiteral("a - Dũng Office.pdf")), "đề xuất tên tệp kết quả");

    std::cout << "\nĐạt " << g_pass << ", lỗi " << g_fail << "\n";
    std::cout << (g_fail ? "KẾT QUẢ: CÓ LỖI\n" : "KẾT QUẢ: TẤT CẢ ĐẠT\n");
    return g_fail ? 1 : 0;
}
