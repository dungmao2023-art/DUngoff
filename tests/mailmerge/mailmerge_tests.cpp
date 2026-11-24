// SPDX-License-Identifier: MPL-2.0
// Kiểm thử lõi "Điền tên hàng loạt": quét trường, thay qua nhiều run, xlsx/pptx/docx, CSV/dán, tên tệp.
// Cách chạy: dung-mailmergetest <thư mục tests/fixtures/mailmerge>   (DUNG_MM_OUT=<thư mục> để giữ lại tệp xuất cho bước kiểm tra ngoài)
#include "MailMergeCore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>
#include <private/qzipreader_p.h>

using namespace mailmerge;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; std::cout << "[ĐẠT]  " << QString(msg).toStdString() << "\n"; } \
    else { ++g_fail; std::cout << "[LỖI]  " << QString(msg).toStdString() << "  (" #cond ")\n"; } } while (0)

static QString entry(const QString &zip, const QString &name)
{
    QZipReader r(zip);
    return QString::fromUtf8(r.fileData(name));
}
static QByteArray bytes(const QString &p) { QFile f(p); f.open(QIODevice::ReadOnly); return f.readAll(); }
static bool anyContains(const QStringList &l, const QString &s) { for (const QString &x : l) if (x.contains(s)) return true; return false; }
static const FieldInfo *field(const ScanResult &r, const QString &n) { for (const auto &f : r.fields) if (f.name == n) return &f; return nullptr; }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString fx = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("tests/fixtures/mailmerge");
    QTemporaryDir tmp;
    QString out = tmp.path();
    if (!qEnvironmentVariable("DUNG_MM_OUT").isEmpty()) { out = qEnvironmentVariable("DUNG_MM_OUT"); QDir().mkpath(out); }
    const QString docx = fx + "/giay-moi.docx", pptx = fx + "/giay-moi.pptx", xlsxT = fx + "/mau-trang-tinh.xlsx";
    for (const QString &p : {docx, pptx, xlsxT, fx + "/danh-sach.xlsx", fx + "/danh-sach.csv"})
        if (!QFileInfo::exists(p)) { std::cout << "[LỖI]  thiếu tệp mẫu " << p.toStdString() << "\n"; return 2; }

    // ================= DOCX =================
    const QByteArray docxBefore = bytes(docx);
    ScanResult sc = scanTemplate(docx);
    CHECK(sc.ok && sc.fields.size() == 5, "docx: quét ra 5 trường");
    CHECK(field(sc, "Họ tên") && field(sc, "Họ tên")->count == 2, "docx: «Họ tên» bị tách 3 run vẫn nhận ra, đếm cả thân lẫn đầu trang");
    CHECK(field(sc, "Họ tên") && field(sc, "Họ tên")->where.size() == 2, "docx: ghi nơi có trường (thân + đầu trang)");
    CHECK(field(sc, "Đơn vị") != nullptr, "docx: nhận «…» trong ô bảng");

    Values v; v["Họ tên"] = QStringLiteral("Nguyễn Văn <An> & Co\x01"); v["Chức vụ"] = "Giám đốc"; v["Đơn vị"] = "Phòng A"; v["Ghi chú"] = "d1\nd2";
    const QString o1 = out + "/docx-1.docx";
    OpResult r1 = renderOne(docx, o1, v);
    CHECK(r1.ok && r1.replaced == 5, QString("docx: renderOne thay 5 chỗ (thực tế %1) %2").arg(r1.replaced).arg(r1.error));
    CHECK(anyContains(r1.warnings, "Thiếu"), "docx: trường thiếu dữ liệu có cảnh báo");
    const QString x1 = entry(o1, "word/document.xml");
    CHECK(x1.contains("Nguyễn Văn &lt;An&gt; &amp; Co"), "docx: ký tự đặc biệt XML được thoát đúng; ký tự điều khiển bị bỏ");
    CHECK(!x1.contains(QChar(1)), "docx: không còn ký tự điều khiển");
    CHECK(x1.contains("{{Thiếu}}"), "docx: trường thiếu dữ liệu giữ nguyên chỗ giữ chữ");
    CHECK(x1.contains("<w:br/>"), "docx: giá trị nhiều dòng thành w:br");
    CHECK(x1.contains("<w:b/><w:i/>"), "docx: định dạng của các run khác vẫn còn");
    CHECK(entry(o1, "word/header1.xml").contains("Giấy mời – Nguyễn Văn"), "docx: thay cả trong đầu trang");
    CHECK(bytes(docx) == docxBefore, "docx: mẫu gốc không bị đổi byte nào");
    OpResult r1b = renderOne(docx, o1, v);
    CHECK(!r1b.ok && r1b.error.contains("đã tồn tại"), "docx: không ghi đè tệp đã có");
    OpResult r1c = renderOne(docx, docx, v);
    CHECK(!r1c.ok, "docx: không ghi đè chính mẫu");
    QString perr;
    const QString pv = previewText(docx, v, 6000, &perr);
    CHECK(perr.isEmpty() && pv.startsWith("Kính mời Nguyễn Văn <An> & Co đến dự lễ.\n"), "docx: xem trước đúng dòng đầu");
    CHECK(pv.contains("Ghi chú:\td1\nd2"), "docx: xem trước có tab và xuống dòng");

    // tạo trường từ đoạn chữ
    const QString t1 = out + "/docx-mau-moi.docx";
    OpResult mk = makeFieldTemplate(docx, t1, "Kính mời", "Lời mời");
    CHECK(mk.ok && mk.replaced == 1, QString("docx: makeFieldTemplate %1").arg(mk.error));
    ScanResult sc2 = scanTemplate(t1);
    CHECK(sc2.ok && field(sc2, "Lời mời") != nullptr && sc2.fields.size() == 6, "docx: mẫu mới có thêm trường «Lời mời»");
    CHECK(bytes(docx) == docxBefore, "docx: tạo trường không sửa mẫu gốc");
    CHECK(!makeFieldTemplate(docx, out + "/x.docx", "Họ", "A").ok, "docx: đoạn chữ chỉ nằm trong trường đã có bị từ chối");
    CHECK(!makeFieldTemplate(docx, out + "/y.docx", "không có đoạn này", "A").ok, "docx: đoạn chữ không tồn tại bị từ chối");
    CHECK(!makeFieldTemplate(docx, out + "/z.docx", "Chức", "{x}").ok, "docx: tên trường chứa { bị từ chối");
    CHECK(!makeFieldTemplate(docx, docx, "Chức", "A").ok, "docx: tạo trường không ghi đè mẫu gốc");

    // gộp
    QVector<Values> recs;
    for (const QString &n : {"An", "Bình", "Cường"}) { Values x; x["Họ tên"] = n; x["Chức vụ"] = "CV " + n; x["Đơn vị"] = "ĐV"; x["Ghi chú"] = ""; recs << x; }
    const QString om = out + "/docx-gop.docx";
    OpResult rm = renderMerged(docx, om, recs);
    CHECK(rm.ok, QString("docx: renderMerged %1").arg(rm.error));
    const QString xm = entry(om, "word/document.xml");
    CHECK(xm.count("<w:pageBreakBefore/>") == 2, "docx: 3 bản ghi → 2 lần ngắt trang");
    CHECK(xm.contains("Chức vụ: CV An") && xm.contains("Chức vụ: CV Cường"), "docx: nội dung từng bản ghi có mặt");
    CHECK(anyContains(rm.warnings, "đầu trang"), "docx: trường trong đầu trang dùng chung có cảnh báo không thay");
    CHECK(xm.count("<w:sectPr>") == 1, "docx: chỉ giữ một sectPr cuối");
    CHECK(!renderMerged(docx, out + "/rong.docx", {}).ok, "docx: gộp 0 bản ghi bị từ chối");

    // ================= PPTX =================
    const QByteArray pptxBefore = bytes(pptx);
    ScanResult sp = scanTemplate(pptx);
    CHECK(sp.ok && sp.fields.size() == 4, QString("pptx: quét 4 trường (thực tế %1) %2").arg(sp.fields.size()).arg(sp.error));
    CHECK(field(sp, "Họ tên") && field(sp, "Họ tên")->count == 2 && anyContains(field(sp, "Họ tên")->where, "ghi chú"), "pptx: «Họ tên» tách hai run + có trong ghi chú");
    CHECK(field(sp, "Đơn vị") != nullptr, "pptx: nhận «…»");
    Values pv1; pv1["Họ tên"] = "Trần Thị Bình"; pv1["Chức vụ"] = "Kế toán"; pv1["Đơn vị"] = "Phòng A & B"; pv1["Ghi chú"] = "d1\nd2";
    const QString p1 = out + "/pptx-1.pptx";
    OpResult rp = renderOne(pptx, p1, pv1);
    CHECK(rp.ok && rp.replaced == 5, QString("pptx: thay 5 chỗ (thực tế %1) %2").arg(rp.replaced).arg(rp.error));
    CHECK(anyContains(rp.warnings, "nối thành MỘT dòng"), "pptx: giá trị nhiều dòng có cảnh báo bị nối một dòng");
    const QString s1x = entry(p1, "ppt/slides/slide1.xml");
    CHECK(s1x.contains("Kính mời Trần Thị Bình") && s1x.contains("d1 d2"), "pptx: thay đúng, nhiều dòng thành dấu cách");
    CHECK(s1x.contains("<a:br/>") && s1x.contains("i=\"1\"") && s1x.contains("b=\"1\""), "pptx: giữ a:br và định dạng đậm/nghiêng của run");
    CHECK(entry(p1, "ppt/slides/slide2.xml").contains("Kế toán &amp; Phòng A &amp; B"), "pptx: thoát & đúng");
    CHECK(entry(p1, "ppt/notesSlides/notesSlide1.xml").contains("Ghi chú cho Trần Thị Bình"), "pptx: thay cả trong ghi chú");
    CHECK(bytes(pptx) == pptxBefore, "pptx: mẫu gốc không đổi");
    const QString pvp = previewText(pptx, pv1, 6000, &perr);
    CHECK(pvp.contains("— Trang chiếu 1 —") && pvp.contains("Kế toán"), "pptx: xem trước có phân trang chiếu");
    CHECK(!renderMerged(pptx, out + "/gop.pptx", recs).ok, "pptx: gộp một tệp bị từ chối rõ ràng");
    CHECK(!renderOne(pptx, out + "/sai-loai.docx", pv1).ok, "pptx: xuất khác loại với mẫu bị từ chối");

    // ================= XLSX (mẫu) =================
    const QByteArray xlsxBefore = bytes(xlsxT);
    ScanResult sx = scanTemplate(xlsxT);
    CHECK(sx.ok && sx.fields.size() == 2, QString("xlsx: quét 2 trường (thực tế %1) %2").arg(sx.fields.size()).arg(sx.error));
    CHECK(anyContains(sx.warnings, "công thức"), "xlsx: trường trong công thức có cảnh báo không thay được");
    Values xv; xv["Họ tên"] = "Lê Văn Cường"; xv["Chức vụ"] = "Phó phòng";
    const QString xo = out + "/xlsx-1.xlsx";
    OpResult rx = renderOne(xlsxT, xo, xv);
    CHECK(rx.ok && rx.replaced == 2, QString("xlsx: thay 2 chỗ (thực tế %1) %2").arg(rx.replaced).arg(rx.error));
    CHECK(entry(xo, "xl/sharedStrings.xml").contains("Kính gửi Lê Văn Cường"), "xlsx: thay trong chuỗi dùng chung (rich text tách run)");
    CHECK(entry(xo, "xl/worksheets/sheet1.xml").contains("<t xml:space=\"preserve\">Phó phòng</t>"), "xlsx: thay trong chuỗi nhập trực tiếp");
    CHECK(entry(xo, "xl/worksheets/sheet1.xml").contains("{{CôngThức}}"), "xlsx: công thức giữ nguyên, không bị đụng");
    Table back = readXlsxFile(xo, 0, false);
    CHECK(back.ok() && back.rows.size() >= 2 && back.rows[0].value(0) == "Kính gửi Lê Văn Cường", "xlsx: đọc lại tệp xuất ra đúng nội dung");
    CHECK(bytes(xlsxT) == xlsxBefore, "xlsx: mẫu gốc không đổi");
    CHECK(!renderOne(xlsxT, out + "/x.xlsm", xv).ok, "xlsx: xuất đổi đuôi khác loại bị từ chối");

    // ================= Loại tệp không hỗ trợ =================
    QFile(out + "/macro.xlsm").open(QIODevice::WriteOnly);
    ScanResult sm = scanTemplate(out + "/macro.xlsm");
    CHECK(!sm.ok && sm.error.contains("macro"), "tệp .xlsm bị từ chối kèm lý do macro");
    CHECK(!scanTemplate(out + "/a.txt").ok, "tệp .txt bị từ chối");
    { QFile f(out + "/cu.docx"); f.open(QIODevice::WriteOnly); f.write("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1 rest"); f.close(); }
    ScanResult so = scanTemplate(out + "/cu.docx");
    CHECK(!so.ok && so.error.contains("định dạng cũ"), "tệp OLE (.doc đổi đuôi/có mật khẩu) bị từ chối kèm hướng dẫn");
    { QFile f(out + "/hong.docx"); f.open(QIODevice::WriteOnly); f.write("không phải zip"); f.close(); }
    CHECK(!scanTemplate(out + "/hong.docx").ok, "tệp hỏng bị từ chối");

    // ================= Dữ liệu: dán =================
    Table pt = parsePasted(QStringLiteral("Họ tên\tChức vụ\nAn\tGiám đốc\nBình\t\n"), Delim::Auto, true);
    CHECK(pt.ok() && pt.header == QStringList({"Họ tên", "Chức vụ"}) && pt.rows.size() == 2 && pt.rows[1][1].isEmpty(), "dán: tab từ Excel, có tiêu đề");
    Table pc = parsePasted(QStringLiteral("﻿A,B\n1,\"x, y\"\n2,\"có \"\"ngoặc\"\"\"\n"), Delim::Auto, true);
    CHECK(pc.ok() && pc.rows.size() == 2 && pc.rows[0][1] == "x, y" && pc.rows[1][1] == "có \"ngoặc\"" && pc.header[0] == "A", "dán: dấu phẩy + ngoặc kép + BOM");
    Table ps = parsePasted(QStringLiteral("a;b\n1;2\n"), Delim::Auto, false);
    CHECK(ps.ok() && ps.rows.size() == 2 && ps.header.size() == 2 && ps.header[0] == "Cột 1", "dán: chấm phẩy, không tiêu đề");
    Table p1c = parsePasted(QStringLiteral("An\nBình\n\nCường\n"), Delim::Auto, false);
    CHECK(p1c.ok() && p1c.rows.size() == 3 && p1c.header == QStringList({"Giá trị"}) && anyContains(p1c.warnings, "dòng trống"), "dán: một cột gõ tay, dòng trống được bỏ và báo");
    Table prg = parsePasted(QStringLiteral("a\tb\tc\n1\t2\n"), Delim::Tab, true);
    CHECK(prg.ok() && anyContains(prg.warnings, "số cột khác nhau") && prg.rows[0].size() == 3, "dán: dòng thiếu cột có cảnh báo");
    CHECK(!parsePasted(QStringLiteral("  \n\n"), Delim::Auto, true).ok(), "dán: rỗng báo lỗi");
    Table pdup = parsePasted(QStringLiteral("A\ta\n1\t2\n"), Delim::Tab, true);
    CHECK(anyContains(pdup.warnings, "trùng"), "dán: tên cột trùng có cảnh báo");

    // ================= Dữ liệu: CSV / xlsx =================
    Table cs = readCsvFile(fx + "/danh-sach.csv", true);
    CHECK(cs.ok() && cs.header == QStringList({"Họ tên", "Chức vụ"}) && cs.rows.size() == 2, QString("csv: BOM + chấm phẩy %1").arg(cs.error));
    CHECK(cs.ok() && cs.rows[0][0] == "Nguyễn, Văn An" && cs.rows[1][0] == "Trần \"B\"" && cs.rows[1][1].contains("phòng"), "csv: ngoặc kép, dấu phẩy và xuống dòng trong ô");
    { QFile f(out + "/cp.csv"); f.open(QIODevice::WriteOnly); f.write("T\xEA" "n;Ch\xFA" "c v\xFA\n"); f.close(); }
    CHECK(!readCsvFile(out + "/cp.csv", true).ok(), "csv: bảng mã Windows cũ bị từ chối thay vì đọc sai chữ");
    QString xe;
    CHECK(xlsxSheetNames(fx + "/danh-sach.xlsx", &xe) == QStringList({"Danh sách"}), "xlsx dữ liệu: tên sheet");
    Table xt = readXlsxFile(fx + "/danh-sach.xlsx", 0, true);
    CHECK(xt.ok() && xt.header.size() == 5 && xt.rows.size() == 4, QString("xlsx dữ liệu: 4 dòng (thực tế %1) %2").arg(xt.rows.size()).arg(xt.error));
    CHECK(xt.ok() && xt.rows[0][3] == "20/11/2026" && anyContains(xt.warnings, "ô ngày"), "xlsx dữ liệu: ngày đọc thành dd/mm/yyyy và có cảnh báo");
    CHECK(xt.ok() && xt.rows[0][4] == "Dòng 1\nDòng 2" && xt.rows[1][1].isEmpty() && xt.rows[3][0] == "Phạm Thị Dung", "xlsx dữ liệu: xuống dòng trong ô, ô trống, dòng cuối");
    CHECK(!readXlsxFile(fx + "/danh-sach.xlsx", 5, true).ok(), "xlsx dữ liệu: sheet không tồn tại báo lỗi");

    // ================= Ánh xạ, kiểm tra, bản ghi =================
    Mapping m = autoMap(sc.fields, xt);
    CHECK(m["Họ tên"] == 0 && m["Chức vụ"] == 1 && m["Đơn vị"] == 2 && m["Thiếu"] == -1 && m["Ghi chú"] == 4, "autoMap: khớp tên cột không phân biệt hoa/thường, trường lạ = chưa chọn");
    Check ck = validate(sc.fields, xt, m);
    CHECK(!ck.errors.isEmpty() && anyContains(ck.errors, "Thiếu"), "validate: trường chưa chọn cột bị chặn xuất");
    m["Thiếu"] = 3;
    ck = validate(sc.fields, xt, m, 5);
    CHECK(ck.errors.isEmpty() && ck.usableRecords == 4, "validate: đủ ánh xạ thì cho xuất");
    CHECK(anyContains(ck.warnings, "trống ở 1 dòng") && anyContains(ck.warnings, "dài hơn 5 ký tự"), "validate: cảnh báo ô trống và giá trị quá dài kèm số dòng");
    QVector<int> srcRows;
    auto br = buildRecords(sc.fields, xt, m, &srcRows);
    CHECK(br.size() == 4 && srcRows.size() == 4 && br[1]["Chức vụ"].isEmpty(), "buildRecords: giữ ô trống, không bỏ dòng có dữ liệu");
    Table dupT = parsePasted(QStringLiteral("Họ tên\nAn\nAn\n \n"), Delim::Auto, true);
    ScanResult one; one.ok = true; FieldInfo fi; fi.name = "Họ tên"; one.fields << fi;
    Check dk = validate(one.fields, dupT, autoMap(one.fields, dupT));
    CHECK(dk.errors.isEmpty() && anyContains(dk.warnings, "trùng nội dung"), "validate: dòng trùng có cảnh báo");
    CHECK(!validate({}, xt, {}).errors.isEmpty(), "validate: mẫu không có trường bị chặn kèm hướng dẫn");

    // ================= Tên tệp =================
    CHECK(sanitizeFileName("a/b:c*?\"<>|") == "a_b_c______", "tên tệp: ký tự cấm Windows thành _");
    CHECK(sanitizeFileName("CON") == "CON_" && sanitizeFileName("nul.txt") == "nul.txt_", "tên tệp: tên dành riêng Windows");
    CHECK(sanitizeFileName("  tên cuối.  ") == "tên cuối", "tên tệp: bỏ dấu chấm/khoảng trắng cuối");
    CHECK(sanitizeFileName(QString(300, 'x')).size() <= 100, "tên tệp: cắt ở 100 ký tự");
    QDir().mkpath(out + "/ten");
    { QFile f(out + "/ten/An.docx"); f.open(QIODevice::WriteOnly); f.close(); }
    QVector<Values> nr; for (const QString &n : {"An", "An", "B/C"}) { Values x; x["Họ tên"] = n; nr << x; }
    QStringList w;
    const QStringList names = planFileNames(nr, "{Họ tên}", ".docx", out + "/ten", &w);
    CHECK(names.size() == 3 && QFileInfo(names[0]).fileName() == "An (2).docx" && QFileInfo(names[1]).fileName() == "An (3).docx" && QFileInfo(names[2]).fileName() == "B_C.docx",
          "tên tệp: trùng nhau hoặc trùng tệp đã có thì thêm (n), không ghi đè");
    CHECK(anyContains(w, "trùng"), "tên tệp: có cảnh báo khi phải đổi tên");
    QStringList w2;
    planFileNames(nr, "{KhongCo}-{#}", ".pdf", out + "/ten", &w2);
    CHECK(anyContains(w2, "KhongCo"), "tên tệp: mẫu tên dùng trường không có thì cảnh báo");
    CHECK(QFileInfo(planFileNames(nr, "{#}", ".pdf", out + "/ten", nullptr)[0]).fileName() == "001.pdf", "tên tệp: số thứ tự có đệm 0");

    std::cout << "\nTổng: " << g_pass << " đạt, " << g_fail << " lỗi\n";
    return g_fail ? 1 : 0;
}
