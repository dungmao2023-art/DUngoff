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
    QFile bad(dir.filePath("hong.csv")); bad.open(QIODevice::WriteOnly); bad.write("Ma\n\xFF\xFE\xFD\n"); bad.close();
    Table rb; CHECK(!readCsv(dir.filePath("hong.csv"), rb, err), "CSV không phải UTF-8: từ chối, không đoán bảng mã");
    CHECK(!readCsv(dir.filePath("khong-co.csv"), rb, err), "tệp không tồn tại báo lỗi");

    std::cout << "\nKẾT QUẢ: " << (g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT") << "  (đạt " << g_pass << ", lỗi " << g_fail << ")\n";
    return g_fail ? 1 : 0;
}
