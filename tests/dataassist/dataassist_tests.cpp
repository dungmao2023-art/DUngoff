// SPDX-License-Identifier: MPL-2.0
// Kiểm thử lõi tiện ích dữ liệu: danh mục hàm, dựng công thức, thống kê/lỗi/biểu đồ, xuất theo mẫu.
// Cách chạy: dung-dataassisttest   (DUNG_DA_OUT=<thư mục> để giữ tệp xuất cho bước kiểm tra ngoài bằng LibreOffice)
#include "DataChecks.h"
#include "FormulaCatalog.h"
#include "TemplateExport.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QSet>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

using namespace dataassist;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; std::cout << "[ĐẠT]  " << QString(msg).toStdString() << "\n"; } \
    else { ++g_fail; std::cout << "[LỖI]  " << QString(msg).toStdString() << "  (" #cond ")\n"; } } while (0)

static QByteArray sha(const QString &p) { QFile f(p); f.open(QIODevice::ReadOnly); return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex(); }

static QString mkTemplate(const QString &path, bool withCalcChain)
{
    QZipWriter z(path);
    z.addFile("[Content_Types].xml", QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/sharedStrings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/>"
        + QByteArray(withCalcChain ? "<Override PartName=\"/xl/calcChain.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.calcChain+xml\"/>" : "") + "</Types>"));
    z.addFile("_rels/.rels", QByteArray("<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>"));
    z.addFile("xl/workbook.xml", QByteArray("<?xml version=\"1.0\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"Mẫu nhập\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>"));
    z.addFile("xl/_rels/workbook.xml.rels", QByteArray("<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>"
        + QByteArray(withCalcChain ? "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/calcChain\" Target=\"calcChain.xml\"/>" : "") + "</Relationships>"));
    // dòng 1: ghi chú; dòng 2: tiêu đề (Mã hàng, Tên hàng, Số lượng, Đơn giá, Ghi chú); dòng 3: dòng mẫu có sẵn
    z.addFile("xl/sharedStrings.xml", QString::fromUtf8("<?xml version=\"1.0\" encoding=\"UTF-8\"?><sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"7\" uniqueCount=\"7\">"
        "<si><t>MẪU NHẬP HÀNG</t></si><si><t>Mã hàng</t></si><si><t>Tên hàng</t></si><si><t>Số lượng</t></si><si><t>Đơn giá</t></si><si><t>Ghi chú</t></si><si><t>VD-001</t></si></sst>").toUtf8());
    z.addFile("xl/worksheets/sheet1.xml", QString::fromUtf8("<?xml version=\"1.0\" encoding=\"UTF-8\"?><worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><dimension ref=\"A1:E3\"/>"
        "<sheetData><row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c></row>"
        "<row r=\"2\"><c r=\"A2\" t=\"s\"><v>1</v></c><c r=\"B2\" t=\"s\"><v>2</v></c><c r=\"C2\" t=\"s\"><v>3</v></c><c r=\"D2\" t=\"s\"><v>4</v></c><c r=\"E2\" t=\"s\"><v>5</v></c></row>"
        "<row r=\"3\"><c r=\"A3\" t=\"s\"><v>6</v></c></row></sheetData><mergeCells count=\"1\"><mergeCell ref=\"A1:E1\"/></mergeCells></worksheet>").toUtf8());
    if (withCalcChain) z.addFile("xl/calcChain.xml", QByteArray("<calcChain xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"/>"));
    z.close();
    return path;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    QString out = tmp.path();
    if (!qEnvironmentVariable("DUNG_DA_OUT").isEmpty()) { out = qEnvironmentVariable("DUNG_DA_OUT"); QDir().mkpath(out); }

    // ---------------- Danh mục hàm ----------------
    CHECK(catalog().size() >= 45, "Danh mục có từ 45 hàm: " + QString::number(catalog().size()));
    QSet<QString> seen; bool dup = false, allHave = true;
    for (const FuncInfo &f : catalog()) { if (seen.contains(f.name)) dup = true; seen.insert(f.name); if (f.intent.isEmpty() || f.example.isEmpty() || f.description.isEmpty()) allHave = false; }
    CHECK(!dup, "Không trùng tên hàm"); CHECK(allHave, "Mọi hàm có ý định, mô tả, ví dụ");
    for (const char* n : {"COUNTBLANK", "PRODUCT", "AVERAGEIF", "AVERAGEIFS", "NOT", "LOOKUP", "SQRT", "INT", "MOD", "POWER", "CONCAT"})
        CHECK(findFunction(QString::fromLatin1(n)) != nullptr, QString("Hàm trợ lý theo HTML: ") + n);
    CHECK(allCategories().size() == 6, "6 nhóm hàm");
    for (Category c : allCategories()) CHECK(!byCategory(c).isEmpty(), "Nhóm không rỗng: " + categoryName(c));
    for (const char *n : {"FILTER", "SORT", "UNIQUE", "TEXT", "DATE", "NETWORKDAYS", "PMT", "NPV", "IRR", "ROUNDUP", "ROUNDDOWN"})
        CHECK(findFunction(QString::fromLatin1(n)) != nullptr, QString("Có hàm mà bộ tính JS của bản HTML không hỗ trợ: ") + n);
    CHECK(foldVietnamese(QStringLiteral("Đếm Tổng ĐÃ")) == QStringLiteral("dem tong da"), "Bỏ dấu tiếng Việt kể cả đ/Đ");
    auto top = [](const QString &q) { auto r = search(q, 5); return r.isEmpty() ? QString() : r[0]->name; };
    CHECK(top(QStringLiteral("cộng tổng")) == QLatin1String("SUM"), "Tìm “cộng tổng” => " + top(QStringLiteral("cộng tổng")));
    CHECK(top(QStringLiteral("tong")) == QLatin1String("SUM"), "Tìm không dấu “tong” => " + top(QStringLiteral("tong")));
    CHECK(top(QStringLiteral("tìm theo mã")) == QLatin1String("VLOOKUP"), "Tìm “tìm theo mã” => " + top(QStringLiteral("tìm theo mã")));
    CHECK(top(QStringLiteral("đếm có điều kiện")) == QLatin1String("COUNTIF"), "Tìm “đếm có điều kiện” => " + top(QStringLiteral("đếm có điều kiện")));
    CHECK(top(QStringLiteral("làm tròn lên")) == QLatin1String("ROUNDUP"), "Tìm “làm tròn lên” => " + top(QStringLiteral("làm tròn lên")));
    CHECK(top(QStringLiteral("xoa khoang trang")) == QLatin1String("TRIM"), "Tìm “xoa khoang trang” => " + top(QStringLiteral("xoa khoang trang")));
    CHECK(top(QStringLiteral("vlookup")) == QLatin1String("VLOOKUP"), "Tìm theo tên hàm");
    CHECK(search(QStringLiteral("zzzzqq")).isEmpty(), "Không khớp => rỗng, không đoán");

    // ---------------- Dựng công thức ----------------
    const FuncInfo *vl = findFunction(QStringLiteral("vlookup"));
    auto b1 = buildFormula(*vl, {QStringLiteral("A2"), QStringLiteral("F2:H100"), QStringLiteral("2"), QStringLiteral("FALSE")});
    CHECK(b1.ok && b1.formula == QLatin1String("=VLOOKUP(A2,F2:H100,2,FALSE)"), "Dựng VLOOKUP: " + b1.formula);
    auto b2 = buildFormula(*vl, {QStringLiteral("A2"), QString(), QStringLiteral("2"), QStringLiteral("FALSE")});
    CHECK(!b2.ok && b2.error.contains(QStringLiteral("Bảng")), "Thiếu đối số giữa => báo tên đối số: " + b2.error);
    auto b3 = buildFormula(*findFunction(QStringLiteral("SUMIF")), {QStringLiteral("A2:A9"), QStringLiteral("\"x\"")});
    CHECK(b3.ok && b3.formula == QLatin1String("=SUMIF(A2:A9,\"x\")"), "Đối số tuỳ chọn cuối bỏ trống được: " + b3.formula);
    auto b4 = buildFormula(*findFunction(QStringLiteral("SUM")), {QStringLiteral("A1:A3")}, QLatin1Char(';'));
    CHECK(b4.formula == QLatin1String("=SUM(A1:A3)"), "Dấu ngăn ';' cho 1 đối số");
    auto b5 = buildFormula(*findFunction(QStringLiteral("IF")), {QStringLiteral("A1>1"), QStringLiteral("\"a\""), QStringLiteral("\"b\"")}, QLatin1Char(';'));
    CHECK(b5.formula == QLatin1String("=IF(A1>1;\"a\";\"b\")"), "Dấu ngăn ';' : " + b5.formula);
    auto b6 = buildFormula(*findFunction(QStringLiteral("TODAY")), {});
    CHECK(b6.ok && b6.formula == QLatin1String("=TODAY()"), "Hàm không đối số: " + b6.formula);
    CHECK(!quickSyntaxCheck(QStringLiteral("SUM(A1)")).ok, "Thiếu = bị báo");
    CHECK(!quickSyntaxCheck(QStringLiteral("=SUM(A1")).ok, "Thiếu ) bị báo");
    CHECK(!quickSyntaxCheck(QStringLiteral("=SUM(A1))")).ok, "Thừa ) bị báo");
    CHECK(!quickSyntaxCheck(QStringLiteral("=IF(A1,\"a)")).ok, "Thiếu nháy đóng bị báo");
    CHECK(quickSyntaxCheck(QStringLiteral("=IF(A1,\")\",1)")).ok, "Ngoặc trong chuỗi không tính");

    // ---------------- Đọc số ----------------
    CHECK(parseNumber(QStringLiteral("1.234.567,5"), true).value == 1234567.5, "Số Việt 1.234.567,5");
    CHECK(parseNumber(QStringLiteral("1,234,567.5"), false).value == 1234567.5, "Số Anh 1,234,567.5");
    CHECK(parseNumber(QStringLiteral("1.234"), true).ambiguous, "1.234 mơ hồ được đánh dấu");
    CHECK(!parseNumber(QStringLiteral("12.34.5"), true).ok, "12.34.5 không phải số");
    CHECK(!parseNumber(QStringLiteral("1.23,4.5"), true).ok, "1.23,4.5 không phải số");
    CHECK(parseNumber(QStringLiteral("(1.500)"), true).value == -1500, "Số âm trong ngoặc");
    CHECK(parseNumber(QStringLiteral("15%"), true).value == 0.15, "Phần trăm");
    CHECK(parseNumber(QStringLiteral("2.500.000 ₫"), true).value == 2500000, "Có ký hiệu tiền");
    CHECK(!parseNumber(QStringLiteral("abc"), true).ok && !parseNumber(QString(), true).ok, "Chữ/rỗng không phải số");

    // ---------------- Thống kê / lỗi / biểu đồ ----------------
    const auto grid = parseTsv(QStringLiteral("Tên\tSố\tMã\nA\t10\t001\nB\t20,5\t002\nC\t#DIV/0!\t003\nD\t\t004\r\n"));
    CHECK(grid.size() == 5 && grid[0].size() == 3, "Tách TSV: 5 dòng × 3 cột");
    const RangeStats st = rangeStats(QVector<QStringList>(grid.begin() + 1, grid.end()), true);
    CHECK(st.numeric == 2 && st.sum == 30.5 && st.min == 10 && st.max == 20.5 && st.average == 15.25, "Thống kê số: tổng 30,5, TB 15,25");
    CHECK(st.errors == 1 && st.empty == 1, "Đếm ô lỗi và ô trống");
    CHECK(st.text == 8, "Mã 001… và chữ được tính là chữ (không phải số): " + QString::number(st.text));
    CHECK(!st.warnings.isEmpty(), "Có cảnh báo khi vùng có ô lỗi");
    const auto errs = findFormulaErrors(grid);
    CHECK(errs.size() == 1 && errs[0].row == 4 && errs[0].col == 2 && errs[0].meaning.contains(QStringLiteral("0")), "Tìm ô lỗi #DIV/0! đúng vị trí dòng 4 cột 2");
    CHECK(isErrorText(QStringLiteral("Err:508")) && isErrorText(QStringLiteral("#N/A")) && !isErrorText(QStringLiteral("N/A tên")), "Nhận diện chuỗi lỗi");
    const auto cd = groupForChart(parseTsv(QStringLiteral("Tỉnh\tTiền\nHN\t5\nHCM\t7\nHN\t3\nĐN\tx\n")), 0, 1, true, true);
    CHECK(cd.points.size() == 2 && cd.points[0].label == QLatin1String("HN") && cd.points[0].value == 8 && !cd.warnings.isEmpty(), "Gom nhóm cộng theo nhãn, bỏ ô không phải số và báo");
    const auto cc = groupForChart(parseTsv(QStringLiteral("T\nA\nB\nA\n")), 0, -1, true);
    CHECK(cc.points.size() == 2 && cc.points[0].value == 2, "Đếm theo nhãn");
    QString many = QStringLiteral("T\n"); for (int i = 0; i < 20; ++i) many += QStringLiteral("L%1\n").arg(i);
    const auto cm = groupForChart(parseTsv(many), 0, -1, true, true, 5);
    CHECK(cm.points.size() == 5 && cm.points.last().label == QStringLiteral("Khác") && cm.points.last().value == 16, "Nhóm thừa gộp vào Khác (16)");

    // ---------------- Xuất theo mẫu ----------------
    const QString tpl = mkTemplate(out + "/mau-nhap-hang.xlsx", true);
    const QByteArray tplHash = sha(tpl);
    const TemplateInfo ti = loadTemplate(tpl);
    CHECK(ti.ok, "Nạp mẫu: " + ti.error);
    CHECK(ti.headerRow == 2 && ti.headers.size() == 5 && ti.headers[0] == QStringLiteral("Mã hàng"), "Tìm dòng tiêu đề = 2 (bỏ dòng ghi chú), 5 cột");
    CHECK(ti.sheetNames == QStringList{QStringLiteral("Mẫu nhập")}, "Tên sheet tiếng Việt");
    CHECK(ti.existingDataRows == 1 && !ti.warnings.isEmpty(), "Cảnh báo dòng mẫu sẵn sẽ bị thay");
    CHECK(!loadTemplate(out + "/khong-co.xlsx").ok, "Mẫu không tồn tại => lỗi rõ ràng");
    CHECK(!loadTemplate(out + "/x.csv").ok, "Mẫu không phải xlsx => lỗi rõ ràng");

    const QStringList srcH = {QStringLiteral("ten hang"), QStringLiteral("MA HANG"), QStringLiteral("So luong"), QStringLiteral("Don gia (VND)"), QStringLiteral("Cột thừa")};
    const QVector<QStringList> src = {
        {QStringLiteral("Bút bi <xanh> & đỏ"), QStringLiteral("00123"), QStringLiteral("10"), QStringLiteral("1.500"), QStringLiteral("x")},
        {QStringLiteral("Vở"), QStringLiteral("A-2"), QStringLiteral("2,5"), QStringLiteral("abc"), QString()},
        {QString(), QString(), QString(), QString(), QStringLiteral("dòng chỉ có cột thừa")},
        {QStringLiteral("Thước"), QStringLiteral("T-9"), QStringLiteral("1.234"), QStringLiteral("2.000"), QString()}};
    ExportOptions o;
    o.mapping = autoMap(ti.headers, srcH);
    CHECK(o.mapping == QVector<int>({1, 0, 2, 3, -1}), "Ghép cột tự động theo tên (bỏ dấu/hoa thường): ");
    o.colTypes = {ColType::Text, ColType::Text, ColType::Number, ColType::Number, ColType::Text};
    const ValidateResult v = validate(ti, srcH, src, o);
    CHECK(v.errors.isEmpty(), "Kiểm tra: không có lỗi chặn");
    auto hasW = [&](const QString &s) { for (const QString &w : v.warnings) if (w.contains(s)) return true; return false; };
    CHECK(hasW(QStringLiteral("Ghi chú")), "Cảnh báo cột mẫu “Ghi chú” chưa có dữ liệu");
    CHECK(hasW(QStringLiteral("Cột thừa")), "Cảnh báo cột nguồn “Cột thừa” không được dùng");
    CHECK(hasW(QStringLiteral("không đọc được thành số")), "Cảnh báo ô “abc” ở cột số");
    CHECK(hasW(QStringLiteral("mơ hồ")), "Cảnh báo số 1.234 mơ hồ");
    ExportOptions bad = o; bad.mapping = {0, 0, -1, -1, -1};
    CHECK(!validate(ti, srcH, src, bad).errors.isEmpty(), "Một cột nguồn ghép hai lần => lỗi chặn");
    ExportOptions none = o; none.mapping = QVector<int>(5, -1);
    CHECK(!validate(ti, srcH, src, none).errors.isEmpty(), "Chưa ghép cột nào => lỗi chặn");

    const QString outX = out + "/ket-qua-xuat.xlsx";
    const ExportResult er = exportToTemplate(tpl, outX, ti, srcH, src, o);
    CHECK(er.ok, "Xuất: " + er.error);
    CHECK(er.rowsWritten == 3, "Ghi 3 dòng (bỏ dòng chỉ có dữ liệu ở cột không ghép): " + QString::number(er.rowsWritten));
    CHECK(er.unmappedTemplateColumns == QStringList{QStringLiteral("Ghi chú")}, "Báo cột mẫu chưa ghép");
    CHECK(sha(tpl) == tplHash, "Tệp mẫu gốc KHÔNG đổi (SHA-256)");
    CHECK(!exportToTemplate(tpl, outX, ti, srcH, src, o).ok && sha(outX).size(), "Không ghi đè tệp đích đã có");
    CHECK(!exportToTemplate(tpl, tpl, ti, srcH, src, o).ok && sha(tpl) == tplHash, "Không ghi đè lên chính mẫu");
    {
        QZipReader z(outX);
        const QString sheet = QString::fromUtf8(z.fileData("xl/worksheets/sheet1.xml"));
        CHECK(sheet.contains(QStringLiteral("<c r=\"A1\" t=\"s\"><v>0</v></c>")) && sheet.contains(QStringLiteral("mergeCells")) && QString::fromUtf8(z.fileData("xl/sharedStrings.xml")).contains(QStringLiteral("MẪU NHẬP HÀNG")), "Giữ dòng ghi chú và ô gộp của mẫu");
        CHECK(!sheet.contains(QStringLiteral("VD-001")), "Dòng mẫu sẵn đã bị thay");
        CHECK(sheet.contains(QStringLiteral("Bút bi &lt;xanh&gt; &amp; đỏ")), "Ký tự đặc biệt XML được thoát");
        CHECK(sheet.contains(QStringLiteral(">00123<")) && sheet.contains(QStringLiteral("t=\"inlineStr\"")), "Mã 00123 giữ nguyên số 0 đầu (ghi dạng chữ)");
        CHECK(sheet.contains(QStringLiteral("<c r=\"C3\"><v>10</v></c>")), "Số lượng 10 ghi dạng số thật");
        CHECK(sheet.contains(QStringLiteral("<c r=\"C4\"><v>2.5</v></c>")), "2,5 (kiểu Việt) ghi thành 2.5");
        CHECK(sheet.contains(QStringLiteral("<c r=\"D3\"><v>1500</v></c>")), "1.500 kiểu Việt = 1500");
        CHECK(sheet.contains(QStringLiteral("<c r=\"D4\" t=\"inlineStr\"><is><t xml:space=\"preserve\">abc</t>")), "“abc” ở cột số giữ nguyên dạng chữ, không đổi thành 0");
        CHECK(sheet.contains(QStringLiteral("<dimension ref=\"A1:E5\"/>")), "Cập nhật vùng dữ liệu A1:E5");
        CHECK(z.fileData("xl/calcChain.xml").isEmpty(), "Bỏ calcChain cũ");
        CHECK(!QString::fromUtf8(z.fileData("[Content_Types].xml")).contains(QStringLiteral("calcChain")) && !QString::fromUtf8(z.fileData("xl/_rels/workbook.xml.rels")).contains(QStringLiteral("calcChain")), "Gỡ tham chiếu calcChain khỏi gói");
        CHECK(!z.fileData("xl/sharedStrings.xml").isEmpty(), "Giữ các phần khác của gói");
    }
    // đọc lại bản xuất bằng chính bộ đọc (tiêu đề còn nguyên)
    const TemplateInfo back = loadTemplate(outX);
    CHECK(back.ok && back.headerRow == 2 && back.headers == ti.headers, "Đọc lại bản xuất: tiêu đề còn đúng");
    CHECK(back.existingDataRows == 3, "Đọc lại bản xuất: 3 dòng dữ liệu");

    std::cout << "\nTổng: " << g_pass << " đạt, " << g_fail << " lỗi\n" << (g_fail ? "KẾT QUẢ DATAASSIST: CÓ LỖI\n" : "KẾT QUẢ DATAASSIST: ĐẠT\n");
    return g_fail ? 1 : 0;
}
