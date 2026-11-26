// SPDX-License-Identifier: MPL-2.0
// Kiểm thử tiện ích bảng tính: dòng trùng + hoàn tác, so sánh hai bảng, làm sạch ngày/tiền.
#include "TableOps.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

using namespace tbl;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; std::cout << "[ĐẠT]  " << QString(msg).toStdString() << "\n"; } \
    else { ++g_fail; std::cout << "[LỖI]  " << QString(msg).toStdString() << "  (" #cond ")\n"; } } while (0)

static Table T(const QStringList &h, const QVector<QStringList> &r) { Table t; t.header = h; t.rows = r; return t; }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // ---- Dòng trùng ----
    Table t = T({"Ma", "Ten", "SDT"}, {
        {"A01", "Nguyễn Văn An", "0912345678"},
        {"a01 ", "Nguyễn Văn An (trùng hoa/thường)", "0900000000"},
        {"B02", "Trần Thị Bình", "0987654321"},
        {"A01", "Nguyễn Văn An", "0912345678"},
        {"", "Dòng không mã 1", ""},
        {"", "Dòng không mã 2", ""},
    });
    const Table orig = t;
    auto d1 = removeDuplicates(t, {0}, false);
    CHECK(d1.ok && d1.cleaned.rows.size() == 5 && d1.removed.size() == 1 && d1.removed[0].originalIndex == 3, "trùng đúng khoá, phân biệt hoa/thường khi không chuẩn hoá");
    CHECK(d1.removed[0].keptIndex == 0, "diff ghi dòng được giữ");
    auto d2 = removeDuplicates(t, {0}, true);
    CHECK(d2.removed.size() == 2 && d2.cleaned.rows.size() == 4, "chuẩn hoá hoa/thường/khoảng trắng bắt thêm 'a01 '");
    CHECK(d2.cleaned.rows[3][1] == "Dòng không mã 1" || d2.cleaned.rows.contains(QStringList{"", "Dòng không mã 2", ""}), "dòng khoá rỗng không bị coi là trùng");
    CHECK(!d2.warnings.isEmpty(), "có cảnh báo về khoá rỗng");
    CHECK(undoDedup(d2) == orig, "hoàn tác dòng trùng == bảng gốc, từng ô");
    CHECK(undoDedup(d1) == orig, "hoàn tác (không chuẩn hoá) == bảng gốc");
    CHECK(t == orig, "bảng gốc không bị sửa");
    auto d3 = removeDuplicates(t, {0, 2}, false);
    CHECK(d3.removed.size() == 1, "khoá nhiều cột");
    CHECK(!removeDuplicates(t, {}, false).ok && !removeDuplicates(t, {9}, false).ok, "khoá sai bị từ chối");
    CHECK(t.rows[0][2] == "0912345678" && d1.cleaned.rows[0][2] == "0912345678", "số điện thoại giữ số 0 đầu");

    // ---- Lọc và sắp xếp dạng chữ, không sửa nguồn ----
    {
        const Table src = T({"ID", "City", "Phone"}, {
            {"010", "An Khê", "0912345678"}, {"002", "an khê", "0123"},
            {"002", "An Khê", "0456"}, {"001", "Huế", "9999999999999999"},
            {"003", "", "0000"}});
        FilterSortSpec spec; spec.filterCol = 1; spec.value = QStringLiteral("an khê");
        spec.ignoreCase = true; spec.sortCol = 0;
        auto r = filterSort(src, spec);
        CHECK(r.ok && r.table.rows.size() == 3, "lọc Unicode không phân biệt hoa thường");
        CHECK(r.table.rows.value(0).value(2) == "0123" && r.table.rows.value(1).value(2) == "0456", "sort ổn định, giữ số 0 đầu");
        CHECK(r.table.rows.value(2).value(0) == "010" && src.rows.value(0).value(0) == "010", "không sửa nguồn");
        spec.descending = true; r = filterSort(src, spec);
        CHECK(r.table.rows.value(0).value(0) == "010", "sort giảm dần");
        spec.mode = FilterMode::Blank; r = filterSort(src, spec);
        CHECK(r.ok && r.table.rows.size() == 1 && r.table.rows.value(0).value(2) == "0000", "lọc ô trống giữ mã");
        spec.mode = FilterMode::Contains; spec.value.clear();
        CHECK(!filterSort(src, spec).ok, "không chấp nhận contains rỗng");
        spec.filterCol = 99;
        CHECK(!filterSort(src, spec).ok, "từ chối chỉ số cột lỗi");
        spec.filterCol = -1; spec.sortCol = -1;
        CHECK(filterSort(src, spec).table == src, "không chọn bước thì giữ nguyên bảng");
        spec.filterCol = 1; spec.sortCol = 0; spec.mode = FilterMode::Equal;
        spec.value = QStringLiteral("An Khê"); spec.descending = true;
        const auto json = encodeFilterSortRecipe(src.header, spec);
        FilterSortSpec loaded; QString error;
        CHECK(decodeFilterSortRecipe(json, src.header, loaded, error), "nạp lại cấu hình JSON");
        CHECK(filterSort(src, loaded).table == filterSort(src, spec).table, "chạy lại cho kết quả giống hệt");
        CHECK(!json.contains("0912345678"), "cấu hình không chứa dòng dữ liệu");
        const QStringList reordered{"City", "ID", "Phone"};
        CHECK(!decodeFilterSortRecipe(json, reordered, loaded, error), "từ chối cột đổi thứ tự");
        CHECK(!decodeFilterSortRecipe(QByteArray("{bad"), src.header, loaded, error), "từ chối JSON hỏng");
        CHECK(loaded.filterCol == spec.filterCol && loaded.value == spec.value, "cấu hình lỗi không ghi đè lựa chọn hiện tại");
        auto invalid = json; invalid.replace("\"version\": 1", "\"version\": 2");
        CHECK(!decodeFilterSortRecipe(invalid, src.header, loaded, error), "từ chối cấu hình v2 thiếu cột");
        spec.outputColumns={2,0}; spec.outputNames={"Điện thoại","Mã"};
        const auto projected=filterSort(src,spec);
        CHECK(projected.ok && projected.table.header==spec.outputNames && projected.table.rows.size()==3,"project selected columns after filter");
        CHECK(projected.table.rows.value(0).value(0)=="0912345678" && projected.table.rows.value(0).value(1)=="010","projection follows sort and preserves text");
        const auto v2=encodeFilterSortRecipe(src.header,spec);
        CHECK(decodeFilterSortRecipe(v2,src.header,loaded,error) && filterSort(src,loaded).table==projected.table,"v2 recipe roundtrip");
        CHECK(decodeFilterSortRecipe(json,src.header,loaded,error) && loaded.outputColumns.isEmpty(),"v1 recipe remains compatible");
        auto bad=spec; bad.outputNames={"Mã","mã"};
        CHECK(!filterSort(src,bad).ok,"reject duplicate output names");
        bad=spec; bad.outputColumns={2,2};
        CHECK(!filterSort(src,bad).ok,"reject duplicate source columns");
        bad=spec; bad.outputColumns={2,99};
        CHECK(!filterSort(src,bad).ok,"reject invalid source column");
        bad=spec; bad.outputNames={"","Mã"};
        CHECK(!filterSort(src,bad).ok,"reject empty output name");
        auto ragged=src; ragged.rows.append(QStringList{"001"});
        CHECK(!filterSort(ragged,spec).ok,"reject projection of ragged rows");
    }

    // ---- So sánh ----
    Table a = T({"Ma", "Ten", "Tien"}, {{"1", "An", "1000"}, {"2", "Bình", "2000"}, {"3", "Chi", "3000"}, {"3", "Chi trùng", "3"}});
    Table b = T({"Ma", "Ten", "Tien", "GhiChu"}, {{"1", "An", "1000", ""}, {"2", "Bình", "2500", ""}, {"4", "Dũng", "4000", ""}});
    auto c = compareTables(a, b, "Ma");
    CHECK(c.ok && c.same == 1, "một dòng giống hệt");
    CHECK(c.changed.size() == 1 && c.changed[0].key == "2" && c.changed[0].column == "Tien" && c.changed[0].valueA == "2000" && c.changed[0].valueB == "2500", "phát hiện ô khác giá trị");
    CHECK(c.onlyInA.size() == 1 && c.onlyInA[0][0] == "3" && c.onlyInB.size() == 1 && c.onlyInB[0][0] == "4", "dòng chỉ có ở A và ở B");
    QString allw = c.warnings.join("\n");
    CHECK(allw.contains("Mã trùng trong bảng A") && allw.contains("GhiChu"), "cảnh báo mã trùng và cột chỉ có ở một bên");
    Table dt = diffAsTable(c);
    CHECK(dt.rows.size() == 3 && dt.header.size() == 5, "bảng khác biệt xuất ra đủ 3 dòng");
    CHECK(!compareTables(a, b, "KhongCo").ok, "cột mã không tồn tại bị từ chối");
    Table z1 = T({"Ma", "V"}, {{"1", "007"}}), z2 = T({"Ma", "V"}, {{"1", "7"}});
    CHECK(compareTables(z1, z2, "Ma").changed.size() == 1, "'007' khác '7' (không ép kiểu số)");
    Table e1 = T({"Ma", "V"}, {{"1", "12345678901234567890"}}), e2 = T({"Ma", "V"}, {{"1", "12345678901234567891"}});
    CHECK(compareTables(e1, e2, "Ma").changed.size() == 1, "số dài 20 chữ số so chính xác, không mất độ chính xác");

    // ---- Ngày ----
    CHECK(cleanDate("5/3/2026").value == "05/03/2026" && cleanDate("5/3/2026").converted, "ngày d/m/yyyy -> dd/mm/yyyy");
    CHECK(cleanDate("2026-03-05").value == "05/03/2026", "ngày ISO -> dd/mm/yyyy");
    CHECK(cleanDate("05.03.2026").value == "05/03/2026" && cleanDate("05-03-2026").value == "05/03/2026", "ngày dấu chấm/gạch");
    CHECK(!cleanDate("31/02/2026").converted && !cleanDate("31/02/2026").warning.isEmpty(), "ngày không tồn tại: giữ nguyên + cảnh báo");
    CHECK(!cleanDate("5/3/26").converted && !cleanDate("5/3/26").warning.isEmpty(), "năm 2 chữ số: không đoán");
    CHECK(!cleanDate("45000").converted && !cleanDate("0912345678").converted, "số seri/số điện thoại không bị đổi thành ngày");
    CHECK(cleanDate("05/03/2026").converted == false && cleanDate("05/03/2026").value == "05/03/2026", "đã chuẩn thì không đổi");
    CHECK(cleanDate("").value.isEmpty() && cleanDate("xin chào").value == "xin chào", "ô rỗng/chữ giữ nguyên");

    // ---- Tiền ----
    CHECK(cleanMoney("1.500.000").value == "1500000", "VN: 1.500.000 -> 1500000");
    CHECK(cleanMoney("1.500.000đ").value == "1500000" && cleanMoney("1.500.000 VNĐ").value == "1500000" && cleanMoney("1.500.000 ₫").value == "1500000", "bỏ ký hiệu tiền ở cuối");
    CHECK(cleanMoney("1.500.000,50").value == "1500000.5", "VN: dấu phẩy thập phân, bỏ 0 thừa, không làm tròn");
    CHECK(cleanMoney("1,500,000.25", MoneyLocale::EN).value == "1500000.25", "EN: 1,500,000.25");
    CHECK(cleanMoney("-2.000").value == "-2000" && cleanMoney("(2.000)").value == "-2000", "số âm");
    CHECK(!cleanMoney("1.50.000").converted && !cleanMoney("1.50.000").warning.isEmpty(), "dấu nghìn sai vị trí: giữ nguyên + cảnh báo");
    CHECK(!cleanMoney("1,5,0").converted, "phân cách lạ không đoán");
    CHECK(!cleanMoney("0912345678").converted && !cleanMoney("0912345678").warning.isEmpty(), "số điện thoại không bị coi là tiền");
    CHECK(!cleanMoney("1234567890123456789").converted, "số >15 chữ số giữ nguyên (không qua double)");
    CHECK(cleanMoney("12abc").value == "12abc" && !cleanMoney("12abc").warning.isEmpty(), "chữ lẫn số giữ nguyên");
    CHECK(cleanMoney("0,5").value == "0.5", "0,5 hợp lệ");
    CHECK(isProtectedText("007") && isProtectedText("12345678901234567") && !isProtectedText("0") && !isProtectedText("1500"), "nhận diện chuỗi cần bảo vệ");

    // ---- Làm sạch cả bảng + CSV ----
    Table raw = T({"Ma", "Ngay", "Tien"}, {
        {"001", "5/3/2026", "1.500.000đ"}, {"002", "31/02/2026", "abc"}, {"0912345678", "2026-12-01", "2.000"}});
    const Table rawCopy = raw;
    auto cr = cleanTable(raw, {{"Ngay", ColumnKind::Date}, {"Tien", ColumnKind::Money}, {"Ma", ColumnKind::Keep}, {"Khong", ColumnKind::Date}});
    CHECK(raw == rawCopy, "bảng gốc không bị sửa khi làm sạch");
    CHECK(cr.changes.size() == 4, "đúng số ô được đổi");
    CHECK(cr.cleaned.rows[0][0] == "001" && cr.cleaned.rows[2][0] == "0912345678", "cột mã giữ nguyên số 0 đầu");
    CHECK(cr.cleaned.rows[1][1] == "31/02/2026" && cr.cleaned.rows[1][2] == "abc", "ô lỗi giữ nguyên");
    CHECK(cr.warnings.size() == 3, "mỗi ô không chuyển được và cột thiếu đều có cảnh báo");

    QTemporaryDir dir;
    const QString p1 = dir.filePath("vào.csv"), p2 = dir.filePath("ra.csv");
    {
        QFile f(p1); f.open(QIODevice::WriteOnly);
        f.write("\xEF\xBB\xBFMa,Ten,Ghi chu\n001,\"Nguyễn, Văn \"\"An\"\"\",\"dòng1\ndòng2\"\n0912345678,Trần,\n");
    }
    Table rd; QString err;
    CHECK(readCsv(p1, rd, err) && rd.rows.size() == 2 && rd.rows[0][0] == "001" && rd.rows[0][1] == "Nguyễn, Văn \"An\"" && rd.rows[0][2] == "dòng1\ndòng2", "đọc CSV: BOM, dấu phẩy/nháy/xuống dòng trong ô, số 0 đầu");
    CHECK(writeCsvCopy(p2, rd, err), "ghi bản sao CSV");
    Table rd2; CHECK(readCsv(p2, rd2, err) && rd2 == rd, "ghi rồi đọc lại == nguyên vẹn");
    CHECK(!writeCsvCopy(p2, rd, err) && !err.isEmpty(), "không ghi đè tệp đã có");
    Table existing; CHECK(readCsv(p2, existing, err) && existing == rd, "tệp có sẵn không đổi sau từ chối ghi");
    {
        Table parsed;
        CHECK(readCsvBytes("ID,Name\r001,An\r002,Binh\r", Encoding::Utf8, parsed, err)
            && parsed.rows.size() == 2 && parsed.rows.value(1).value(0) == "002", "CSV xuống dòng CR đơn");
        const Table before = parsed;
        CHECK(!readCsvBytes("ID,Name\n001,a\"b\"\n", Encoding::Utf8, parsed, err) && parsed == before,
            "từ chối dấu nháy giữa ô, giữ kết quả cũ");
        CHECK(!readCsvBytes("ID,Name\n001,\"An\"x\n", Encoding::Utf8, parsed, err) && parsed == before,
            "từ chối chữ sau dấu nháy đóng");
        CHECK(!readCsvBytes("ID,Name\n001,\"An\n", Encoding::Utf8, parsed, err) && parsed == before,
            "từ chối dấu nháy chưa đóng");
        CHECK(readCsvBytes("ID,Name\r\n001,\"a\r\nb\"\r\n", Encoding::Utf8, parsed, err)
            && parsed.rows.value(0).value(1) == "a\r\nb", "giữ nguyên CRLF bên trong ô");
        const auto p3 = dir.filePath("crlf.csv");
        CHECK(writeCsvCopy(p3, parsed, err) && err.isEmpty(), "ghi bản sao xóa lỗi cũ");
        Table round; CHECK(readCsv(p3, round, err) && round == parsed, "roundtrip CRLF và số 0 đầu");
        CHECK(!writeCsvCopy(dir.filePath("empty.csv"), Table{}, err), "không xuất bảng thiếu tiêu đề");
        CHECK(!readCsvBytes("ID|Name", Encoding::Utf8, parsed, err, QLatin1Char('|')), "từ chối phân cách không hỗ trợ");
    }
    QFile bad(dir.filePath("hong.csv")); bad.open(QIODevice::WriteOnly); bad.write("Ma\n\xFF\xFE\xFD\n"); bad.close();
    Table rb; CHECK(!readCsv(dir.filePath("hong.csv"), rb, err), "CSV không phải UTF-8: từ chối, không đoán bảng mã");
    CHECK(!readCsv(dir.filePath("khong-co.csv"), rb, err), "tệp không tồn tại báo lỗi");


    // ---- Nhận diện bảng mã CSV ----
    {
        const QString vi = QStringLiteral("Họ tên,Địa chỉ\n001,Nguyễn Văn An\n");
        const QByteArray u8 = vi.toUtf8();
        const QByteArray cp = QByteArray::fromHex("486ff22074ea6e2cd069f26120636869d20a3030312c4e677579eade6e2056e36e20416e0a");
        QString txt, e2;
        auto g = detectCsvEncoding(QByteArray("\xEF\xBB\xBF") + u8);
        CHECK(g.enc == Encoding::Utf8Bom && g.confidence == 1.0 && !g.needsUserChoice, "BOM UTF-8: chắc chắn, không cần hỏi");
        g = detectCsvEncoding(u8);
        CHECK(g.enc == Encoding::Utf8 && g.confidence >= 0.95 && !g.needsUserChoice, "UTF-8 không BOM có tiếng Việt: nhận ra");
        g = detectCsvEncoding("Ma,Ten\n1,An\n");
        CHECK(g.enc == Encoding::Utf8 && g.confidence == 1.0, "ASCII thuần: UTF-8");
        QByteArray le("\xFF\xFE"), be("\xFE\xFF");
        for (QChar c : vi) { le.append(char(c.unicode() & 0xFF)); le.append(char(c.unicode() >> 8)); be.append(char(c.unicode() >> 8)); be.append(char(c.unicode() & 0xFF)); }
        g = detectCsvEncoding(le); CHECK(g.enc == Encoding::Utf16LE && !g.needsUserChoice, "BOM UTF-16 LE");
        g = detectCsvEncoding(be); CHECK(g.enc == Encoding::Utf16BE && !g.needsUserChoice, "BOM UTF-16 BE");
        CHECK(decodeText(le, Encoding::Utf16LE, txt, e2) && txt == vi, "giải mã UTF-16 LE đúng nguyên văn");
        CHECK(decodeText(be, Encoding::Utf16BE, txt, e2) && txt == vi, "giải mã UTF-16 BE đúng nguyên văn");
        g = detectCsvEncoding(cp);
        CHECK(g.enc == Encoding::Windows1258 && g.needsUserChoice && g.confidence < 0.95 && !g.reason.isEmpty(), "Windows-1258: chỉ gợi ý, BẮT người dùng xác nhận");
        CHECK(decodeText(cp, Encoding::Windows1258, txt, e2) && txt == vi, "giải mã Windows-1258 ra đúng chữ Việt (NFC)");
        g = detectCsvEncoding(QByteArray("\xA9\xB5\xAE\xB1 abc\n"));
        CHECK(g.needsUserChoice && g.confidence < 0.5, "byte không giống tiếng Việt: độ tin cậy thấp, cần người dùng chọn");
        g = detectCsvEncoding(QByteArray("Ma\n\x81\x8A\n"));
        CHECK(g.enc == Encoding::Unknown && g.needsUserChoice, "byte không định nghĩa: không đoán");
        CHECK(!decodeText(QByteArray("Ma\n\x81\n"), Encoding::Windows1258, txt, e2) && !e2.isEmpty(), "Windows-1258 gặp byte lạ: từ chối, không thay ký tự");
        CHECK(!decodeText(cp, Encoding::Utf8, txt, e2), "chọn nhầm UTF-8 cho tệp Windows-1258: từ chối");
        CHECK(!decodeText(QByteArray("abc"), Encoding::Utf16LE, txt, e2), "UTF-16 độ dài lẻ: từ chối");
        QByteArray nobom; for (QChar c : QStringLiteral("Ma,Ten\n001,Nam\n002,Hoa\n")) { nobom.append(char(c.unicode())); nobom.append('\0'); }
        g = detectCsvEncoding(nobom);
        CHECK(g.enc == Encoding::Utf16LE && g.needsUserChoice && g.confidence < 0.95, "UTF-16 không BOM: gợi ý và bắt xác nhận");

        QString big; for (int i = 0; i < 25; ++i) big += QStringLiteral("d%1;x\r\n").arg(i);
        const QStringList pv = previewLines(big, 20);
        CHECK(pv.size() == 20 && pv[0] == "d0;x" && !pv[0].contains(QLatin1Char('\r')), "xem trước đúng 20 dòng, bỏ \\r");
        CHECK(guessSeparator(big) == QLatin1Char(';') && guessSeparator("a,b\n") == QLatin1Char(',') && guessSeparator("a\tb\n") == QLatin1Char('\t'), "đoán dấu phân cách");

        QTemporaryDir d2;
        const QString pc = d2.filePath("cu.csv");
        { QFile f(pc); f.open(QIODevice::WriteOnly); f.write(cp); }
        Table t1; QString er;
        CHECK(!readCsv(pc, t1, er), "readCsv mặc định (UTF-8) vẫn từ chối tệp Windows-1258");
        CHECK(readCsvWith(pc, Encoding::Windows1258, t1, er) && t1.header.value(1) == QStringLiteral("Địa chỉ") && t1.rows.value(0).value(1) == QStringLiteral("Nguyễn Văn An") && t1.rows[0][0] == "001", "đọc CSV Windows-1258 sau khi chọn bảng mã");
        QFile chk(pc); chk.open(QIODevice::ReadOnly);
        CHECK(chk.readAll() == cp, "tệp gốc không bị đổi byte nào");
        const QString outp = d2.filePath("moi.csv");
        CHECK(writeCsvCopy(outp, t1, er), "ghi bản sao UTF-8 từ tệp Windows-1258");
        Table t2; CHECK(readCsv(outp, t2, er) && t2 == t1, "bản sao đọc lại nguyên vẹn");
    }

    std::cout << "\nKẾT QUẢ: " << (g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT") << "  (đạt " << g_pass << ", lỗi " << g_fail << ")\n";
    return g_fail ? 1 : 0;
}
