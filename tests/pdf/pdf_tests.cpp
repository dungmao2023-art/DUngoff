// SPDX-License-Identifier: MPL-2.0
// Kiểm thử thao tác PDF (PdfOps) với qpdf thật. Không cần giao diện; tự tạo PDF mẫu.
// Chạy: dung-pdftest   (đặt DUNG_QPDF nếu qpdf không nằm trong PATH)
#include "pdf/PdfOps.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <cstdio>
#include <iostream>

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
static int tempLeftovers(const QString& dir) {
    return QDir(dir).entryList({QStringLiteral("~dung-*")}, QDir::AllEntries | QDir::NoDotAndDotDot).size();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
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
    for (const char* bad : {"0", "4", "3-1", "", "x", "1,,x"}) {
        QString e;
        CHECK(parsePages(QString::fromLatin1(bad), 3, &e).isEmpty() && !e.isEmpty(), QStringLiteral("parsePages từ chối '%1'").arg(QString::fromLatin1(bad)));
    }

    // --- info
    const Info ia = info(a);
    CHECK(ia.ok && ia.pages == 3 && !ia.encrypted, "info: 3 trang, không mật khẩu");
    CHECK(!info(P(QStringLiteral("khong-co.pdf"))).ok, "info: tệp không tồn tại báo lỗi");

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
    CHECK(!ib.ok && !ib.error.isEmpty(), "tệp bị cắt cụt: báo lỗi");
    writeFile(P(QStringLiteral("khong-phai-pdf.pdf")), "khong phai pdf");
    CHECK(!info(P(QStringLiteral("khong-phai-pdf.pdf"))).ok, "tệp không phải PDF: báo lỗi");
    r = merge({{bad, {}}, {b, {}}}, P(QStringLiteral("gop-hong.pdf")));
    CHECK(!r.ok && !QFileInfo::exists(P(QStringLiteral("gop-hong.pdf"))), "gộp với tệp hỏng: không tạo tệp kết quả");

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

    // --- dọn dẹp
    CHECK(tempLeftovers(d) == 0, "không còn tệp tạm ~dung-* trong thư mục làm việc");
    CHECK(suggestOutputPath(a).endsWith(QStringLiteral("a - Dũng Office.pdf")), "đề xuất tên tệp kết quả");

    std::cout << "\nĐạt " << g_pass << ", lỗi " << g_fail << "\n";
    std::cout << (g_fail ? "KẾT QUẢ: CÓ LỖI\n" : "KẾT QUẢ: TẤT CẢ ĐẠT\n");
    return g_fail ? 1 : 0;
}
