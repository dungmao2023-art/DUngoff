// SPDX-License-Identifier: MPL-2.0
// Kiểm thử tiện ích văn bản tiếng Việt bằng ví dụ có đáp án đúng.
#include "vntext/VnText.h"

#include <QSet>
#include <QString>
#include <cstdio>

using namespace vntext;

static int g_fail = 0;
static void check(bool ok, const QString &what, const QString &detail = QString()) {
    std::printf("[%s]  %s\n", ok ? "ĐẠT" : "LỖI", what.toUtf8().constData());
    if (!ok) { ++g_fail; if (!detail.isEmpty()) std::printf("       %s\n", detail.toUtf8().constData()); }
}
static void check(bool ok, const char *what, const QString &detail = QString()) { check(ok, QString::fromUtf8(what), detail); }
static void eqNum(quint64 n, const char *expect, bool le = false, bool tu = true) {
    const QString got = numberToWords(n, le, tu), want = QString::fromUtf8(expect);
    check(got == want, QStringLiteral("số %1 -> %2").arg(n).arg(want), QStringLiteral("nhận được: ") + got);
}
static void eqMoney(const char *in, const char *expect, const MoneyOptions &o = {}) {
    const auto r = moneyToWords(QString::fromUtf8(in), o);
    const QString want = QString::fromUtf8(expect);
    check(r.ok && r.text == want, QStringLiteral("tiền \"%1\" -> %2").arg(QString::fromUtf8(in), want),
          QStringLiteral("nhận được: ok=%1 text=%2 lỗi=%3").arg(r.ok).arg(r.text, r.error));
}
static void rejectMoney(const char *in, const char *why) {
    const auto r = moneyToWords(QString::fromUtf8(in));
    check(!r.ok && r.text.isEmpty() && !r.error.isEmpty(), QStringLiteral("tiền \"%1\" bị từ chối (%2)").arg(QString::fromUtf8(in), QString::fromUtf8(why)),
          QStringLiteral("nhận được: ") + r.text);
}

int main() {
    // ---- 1. Số -> chữ ----
    eqNum(0, "không"); eqNum(1, "một"); eqNum(4, "bốn"); eqNum(5, "năm"); eqNum(10, "mười");
    eqNum(11, "mười một"); eqNum(14, "mười bốn"); eqNum(15, "mười lăm");
    eqNum(20, "hai mươi"); eqNum(21, "hai mươi mốt"); eqNum(24, "hai mươi tư"); eqNum(24, "hai mươi bốn", false, false);
    eqNum(25, "hai mươi lăm"); eqNum(99, "chín mươi chín");
    eqNum(100, "một trăm"); eqNum(101, "một trăm linh một"); eqNum(101, "một trăm lẻ một", true);
    eqNum(105, "một trăm linh năm"); eqNum(110, "một trăm mười"); eqNum(115, "một trăm mười lăm");
    eqNum(1000, "một nghìn"); eqNum(1005, "một nghìn không trăm linh năm");
    eqNum(1015, "một nghìn không trăm mười lăm"); eqNum(2025, "hai nghìn không trăm hai mươi lăm");
    eqNum(100000, "một trăm nghìn"); eqNum(1000000, "một triệu");
    eqNum(1500000, "một triệu năm trăm nghìn"); eqNum(2000005, "hai triệu không trăm linh năm");
    eqNum(1010000, "một triệu không trăm mười nghìn");
    eqNum(1000000000ULL, "một tỷ");
    eqNum(1234567890ULL, "một tỷ hai trăm ba mươi tư triệu năm trăm sáu mươi bảy nghìn tám trăm chín mươi");
    eqNum(1000000000000ULL, "một nghìn tỷ");
    eqNum(999999999999999999ULL,
          "chín trăm chín mươi chín triệu tỷ chín trăm chín mươi chín nghìn tỷ chín trăm chín mươi chín tỷ "
          "chín trăm chín mươi chín triệu chín trăm chín mươi chín nghìn chín trăm chín mươi chín");

    eqMoney("1500000", "Một triệu năm trăm nghìn đồng chẵn");
    eqMoney("1.500.000", "Một triệu năm trăm nghìn đồng chẵn");
    eqMoney("1,500,000", "Một triệu năm trăm nghìn đồng chẵn");
    eqMoney("1.500.000 đ", "Một triệu năm trăm nghìn đồng chẵn");
    eqMoney("1.500.000,00", "Một triệu năm trăm nghìn đồng chẵn");
    eqMoney("-1500", "Âm một nghìn năm trăm đồng chẵn");
    eqMoney("0", "Không đồng chẵn");
    eqMoney("007", "Bảy đồng chẵn");
    { MoneyOptions o; o.currency = false; o.capitalize = false; eqMoney("2025", "hai nghìn không trăm hai mươi lăm", o); }
    rejectMoney("1.5", "phần thập phân khác 0, không làm tròn");
    rejectMoney("1500000,50", "phần thập phân khác 0");
    rejectMoney("12a3", "ký tự lạ");
    rejectMoney("1.50.000", "dấu nghìn sai chỗ");
    rejectMoney("", "rỗng");
    rejectMoney("1234567890123456789", "quá 18 chữ số");
    { const auto r = moneyToWords(QStringLiteral("1.500.000,00")); check(r.ok && !r.warnings.isEmpty(), "phần thập phân toàn 0 có cảnh báo"); }

    // ---- 2. Ngày ----
    const QDate d1(2026, 3, 5), d2(2026, 2, 5), d3(2026, 11, 24);
    check(formatDateVn(d1) == QString::fromUtf8("ngày 05 tháng 3 năm 2026"), "ngày theo NĐ 30: ngày<10 thêm 0, tháng 3 không thêm", formatDateVn(d1));
    check(formatDateVn(d2) == QString::fromUtf8("ngày 05 tháng 02 năm 2026"), "ngày theo NĐ 30: tháng 2 thêm 0", formatDateVn(d2));
    check(formatDateVn(d3) == QString::fromUtf8("ngày 24 tháng 11 năm 2026"), "ngày hai chữ số giữ nguyên");
    check(formatDateVn(d1, DateStyle::Plain) == QString::fromUtf8("ngày 5 tháng 3 năm 2026"), "kiểu không thêm số 0");
    check(formatDateVn(d1, DateStyle::Padded) == QString::fromUtf8("ngày 05 tháng 03 năm 2026"), "kiểu luôn 2 chữ số");
    check(formatDateNumeric(d1) == QStringLiteral("05/03/2026"), "dd/mm/yyyy");
    check(dateToSpelledWords(d1) == QString::fromUtf8("ngày năm tháng ba năm hai nghìn không trăm hai mươi sáu"), "ngày bằng chữ", dateToSpelledWords(d1));
    check(dateToSpelledWords(d3) == QString::fromUtf8("ngày hai mươi tư tháng mười một năm hai nghìn không trăm hai mươi sáu"), "ngày bằng chữ (24/11)", dateToSpelledWords(d3));
    for (const char *s : {"5/3/2026", "05/03/2026", "05-03-2026", "5.3.2026", "ngày 5 tháng 3 năm 2026", "Ngày 05 tháng 03 năm 2026", " 5/3/2026 ", "ngày 05 tháng 3 năm 2026"}) {
        const auto r = parseDateVn(QString::fromUtf8(s));
        check(r.ok && r.date == d1, QStringLiteral("đọc ngày \"%1\"").arg(QString::fromUtf8(s)), r.error);
    }
    check(parseDateVn(QStringLiteral("29/02/2024")).ok, "29/02/2024 (năm nhuận) hợp lệ");
    for (const char *s : {"29/02/2026", "31/04/2026", "00/01/2026", "5/13/2026", "5/3/26", "5/3/20266", "hôm nay", "", "ngày 5 tháng 3"}) {
        const auto r = parseDateVn(QString::fromUtf8(s));
        check(!r.ok && !r.error.isEmpty(), QStringLiteral("từ chối ngày \"%1\"").arg(QString::fromUtf8(s)));
    }
    { const auto r = parseDateVn(QString::fromUtf8("ngày 24 tháng 11 năm 2026")); check(r.ok && formatDateVn(r.date) == QString::fromUtf8("ngày 24 tháng 11 năm 2026"), "vòng tròn đọc -> viết ngày"); }

    // ---- 3. Bảng mã cũ (TCVN3) ----
    struct Case { const char16_t *legacy; const char *expect; };
    const Case good[] = {
        {u"Céng hßa x· héi chñ nghÜa ViÖt Nam", "Cộng hòa xã hội chủ nghĩa Việt Nam"},
        {u"§éc lËp - Tù do - H¹nh phóc", "Độc lập - Tự do - Hạnh phúc"},
        {u"Ch­¬ng tr×nh Th«ng tin", "Chương trình Thông tin"},
        {u"Së Gi¸o dôc vµ §µo t¹o", "Sở Giáo dục và Đào tạo"},
    };
    for (const auto &c : good) {
        const auto r = legacyToUnicode(QString::fromUtf16(c.legacy), LegacyEncoding::TCVN3);
        const QString want = QString::fromUtf8(c.expect);
        check(r.verdict == Verdict::Converted && r.text == want, QStringLiteral("TCVN3 -> %1").arg(want),
              QStringLiteral("verdict=%1 tin cậy=%2 nhận được: %3").arg(int(r.verdict)).arg(r.confidence).arg(r.text));
    }
    // Không ép khi không chắc: tiếng Pháp/Latin-1 trùng mã (été café), ký hiệu µm, một từ lẻ.
    const char16_t *unsure[] = {u"été café", u"5 µm", u"Hµ", u"naïve café à la carte"};
    for (const char16_t *s : unsure) {
        const QString in = QString::fromUtf16(s);
        const auto r = legacyToUnicode(in, LegacyEncoding::TCVN3);
        check(r.verdict == Verdict::NeedsReview && r.text == in && !r.warnings.isEmpty(),
              QStringLiteral("không ép khi không chắc: %1").arg(in), QStringLiteral("verdict=%1 tin cậy=%2 text=%3").arg(int(r.verdict)).arg(r.confidence).arg(r.text));
    }
    { // force: cho phép người dùng quyết định, nhưng có cảnh báo
        const auto r = legacyToUnicode(QString(u"Hµ"), LegacyEncoding::TCVN3, true);
        check(r.verdict == Verdict::Converted && r.text == QString::fromUtf8("Hà") && r.warnings.size() >= 1, "force: chuyển nhưng có cảnh báo");
    }
    { // đã là Unicode: không đụng
        const QString in = QString::fromUtf8("Cộng hòa xã hội chủ nghĩa Việt Nam");
        const auto r = legacyToUnicode(in, LegacyEncoding::TCVN3);
        check(r.verdict == Verdict::AlreadyUnicode && r.text == in, "văn bản đã Unicode giữ nguyên");
    }
    { const QString in = QStringLiteral("Hello world 123"); const auto r = legacyToUnicode(in, LegacyEncoding::TCVN3);
      check(r.verdict == Verdict::NothingToConvert && r.text == in, "văn bản thuần ASCII: không có gì để chuyển"); }
    { const QString in = QString(u"ViÖt Nam"); const auto r = legacyToUnicode(in, LegacyEncoding::VniWindows, true);
      check(r.verdict == Verdict::Unsupported && r.text == in && !r.warnings.isEmpty(), "VNI-Windows: chưa hỗ trợ, không đoán dù force"); }
    // Cấu trúc âm tiết
    for (const char *s : {"việt", "nam", "nghĩa", "trình", "thông", "chương", "tạo", "đào", "học", "ngoài", "quốc", "tiếng", "người", "yêu"})
        check(isPlausibleSyllable(QString::fromUtf8(s)), QStringLiteral("âm tiết hợp lệ: %1").arg(QString::fromUtf8(s)));
    for (const char *s : {"ộtộ", "caộ", "bàt", "việtt", "xyz", "kkà"})
        check(!isPlausibleSyllable(QString::fromUtf8(s)), QStringLiteral("âm tiết không hợp lệ: %1").arg(QString::fromUtf8(s)));

    { // toàn vẹn bảng mã: đúng 74 mã (67 chữ thường + 7 chữ hoa cơ sở), kết quả là ký tự dựng sẵn, không trùng nhau
        int mapped = 0; bool precomposed = true; QSet<QChar> seen; bool distinct = true;
        for (ushort c = 0xA1; c <= 0xFE; ++c) {
            const QString in(QChar(c));
            const auto r = legacyToUnicode(in, LegacyEncoding::TCVN3, true);
            if (r.text != in) {
                ++mapped;
                if (r.text.size() != 1 || r.text != r.text.normalized(QString::NormalizationForm_C)) precomposed = false;
                if (seen.contains(r.text.at(0))) distinct = false;
                seen.insert(r.text.at(0));
            }
        }
        check(mapped == 74, "bảng TCVN3 có đúng 74 mã", QStringLiteral("số mã: %1").arg(mapped));
        check(precomposed, "mọi mã chuyển ra một ký tự Unicode dựng sẵn");
        check(distinct, "không có hai mã trùng ký tự đích");
    }

    std::printf("\nKẾT QUẢ: %s (%d lỗi)\n", g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT", g_fail);
    return g_fail ? 1 : 0;
}
