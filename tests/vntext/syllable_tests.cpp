// SPDX-License-Identifier: MPL-2.0
// Kiểm thử bộ kiểm tra âm tiết tiếng Việt: câu mẫu do dự án tự viết (không lấy từ nguồn có bản quyền).
#include "vntext/SyllableCheck.h"

#include <QSet>
#include <QString>
#include <cstdio>

using namespace vntext::spell;

static int g_fail = 0;
static int g_pass = 0;
static void check(bool ok, const QString &what, const QString &detail = QString()) {
    if (ok) { ++g_pass; return; }          // chỉ in dòng LỖI để log gọn; số đạt in ở cuối
    ++g_fail;
    std::printf("[LỖI]  %s\n", what.toUtf8().constData());
    if (!detail.isEmpty()) std::printf("       %s\n", detail.toUtf8().constData());
}
static QString U(const char *s) { return QString::fromUtf8(s); }

static const char *kValidText = R"(
Việt Nam là một đất nước nằm ở phía đông bán đảo Đông Dương. Thủ đô của nước ta là Hà Nội, thành phố lớn nhất là Thành phố Hồ Chí Minh.
Xin chào các bạn, hôm nay chúng ta cùng học cách soạn thảo văn bản, lập bảng tính và trình chiếu bằng phần mềm văn phòng.
Mẹ tôi thường dậy sớm để nấu cơm, quét nhà rồi ra chợ mua rau, cá và thịt. Buổi trưa cả nhà quây quần bên mâm cơm giản dị.
Người nông dân cần cù lao động trên cánh đồng lúa bát ngát, mồ hôi thấm đẫm những luống cày thẳng tắp.
Chúng tôi nghiên cứu nghiêm túc, nghĩ kỹ rồi mới quyết định; mọi nghi ngờ đều được giải đáp rõ ràng.
Giếng nước trong veo, giọt sương long lanh, gió lùa qua khe cửa, tiếng ve kêu rền rĩ suốt mùa hè.
Quý khách vui lòng xuất trình giấy tờ, quyết toán chi phí, thanh toán đúng hạn và giữ gìn tài sản chung.
Thuyền nhẹ lướt trên sông, khuya khoắt mà trăng vẫn tỏ, huyền diệu như một giấc mơ thuở nhỏ.
Anh ấy khoẻ mạnh, chị ấy xinh xắn, em bé ngoan ngoãn; cả ba người cùng đi dạo quanh hồ buổi chiều.
Hoà bình, hạnh phúc, thịnh vượng là mong ước của mọi người; thuỷ điện, nhiệt điện và điện gió cung cấp năng lượng.
Bác sĩ khuyên nên uống nhiều nước, ăn rau xanh, tập thể dục đều đặn và ngủ đủ giấc để cơ thể luôn khoẻ.
Hãy lưu tệp trước khi đóng, chọn khổ giấy A4, căn lề trái ba mươi mi-li-mét, lề phải mười lăm mi-li-mét.
Báo cáo tổng kết năm học gồm các nội dung: kết quả học tập, rèn luyện, hoạt động ngoại khoá và đề xuất cho năm sau.
Ngoằn ngoèo con đường lên núi, khoảng trời xanh biếc, những đoá hoa dại nở rực rỡ bên sườn đồi.
Tuổi thơ tôi gắn với dòng sông quê, chiếc cầu tre, cánh diều giấy và tiếng cười trong trẻo của lũ trẻ.
Cuộc họp bắt đầu lúc tám giờ sáng; đại biểu thảo luận sôi nổi về luật, nghị quyết và kế hoạch phát triển kinh tế.
Buồn vui, được mất, hợp tan đều là chuyện thường tình; điều quan trọng là biết trân trọng từng khoảnh khắc.
Ếch ngồi đáy giếng, ớt cay, ổi chín, ốc nhồi, ốm yếu, ánh sáng, ước muốn, ưu tiên, ướt át, ừ, ơi, ạ, ê, ôi, ủa, ấy.
Nguyễn Văn Dũng, Trần Thị Mai, Lê Quốc Huy, Phạm Ngọc Quỳnh, Hoàng Minh Tuấn, Huỳnh Thanh Tâm, Đặng Thu Hằng, Bùi Anh Khoa.
Võ Nguyên Giáp, Đỗ Thuỳ Dương, Ngô Bảo Châu, Dương Khuê, Lý Thường Kiệt, Vũ Hữu Nghĩa, Đinh Tiên Hoàng, Quang Trung, Nguyễn Huệ.
Cần Thơ, Đà Nẵng, Huế, Quảng Ngãi, Quảng Nam, Thanh Hoá, Nghệ An, Hà Tĩnh, Lâm Đồng, Khánh Hoà, Bình Thuận, Tiền Giang.
Cà phê sữa đá, bánh mì thịt nguội, phở bò tái, bún chả, bún riêu, bánh xèo, gỏi cuốn, chè đậu xanh, nước mía, sinh tố bơ.
Tôi muốn đặt mua một chiếc áo khoác mỏng, quần dài màu đen và đôi giày thể thao cỡ bốn mươi hai.
Trường học nằm cạnh bưu điện; học sinh xếp hàng ngay ngắn, chào cờ, hát quốc ca rồi vào lớp.
Giáo viên giảng bài, hướng dẫn thực hành, kiểm tra miệng, chấm điểm và nhận xét tinh thần học tập.
Truyện cổ tích kể rằng ngày xưa có chàng trai nghèo chăn trâu, biết thổi sáo, cứu giúp người gặp nạn.
Yêu thương, yên tâm, yếu đuối, yết kiến, ý kiến, y tế, kỳ lạ, kỷ niệm, kỹ sư, lý do, mỹ thuật, tỷ lệ, quý hiếm, quỳnh, quýt.
Khuyên nhủ, khuyến khích, khuya, khuỷu tay, huênh hoang, huỵch, quỵch, nguệch ngoạc, thuở, huơ tay, khuấy đều, tuần lễ, tuất.
Chuyển khoản, chuyện trò, chuẩn bị, xuyên suốt, truyền thống, nguyên nhân, quyền lợi, tuyển dụng, duyên dáng, thuyết trình.
Loạn lạc, xoắn xuýt, hoặc là, khoanh tay, doanh nghiệp, toán học, đoàn kết, hoàn thành, loét, khoèo chân, ngoéo tay.
Thiết kế, tiếp nhận, diễn đạt, biểu diễn, nhiệm vụ, hiệu quả, điều kiện, miền Trung, kiểm tra, riêng biệt, viết, việc, nghiệp.
Một hai ba bốn năm sáu bảy tám chín mười, trăm nghìn triệu tỷ; hai mươi mốt, hai mươi tư, một trăm linh năm.
Gì, gìn giữ, giữ gìn, gia đình, già, giá cả, giặt giũ, giường, giúp đỡ, giấy, giữa, gieo hạt, giẻ lau, giếng, giết, giềng, gỉ.
Ghi chép, ghế, ghét, ghê gớm, ghiền, kia, kìa, kể, kết, kiên trì, kinh nghiệm, kẻ, ke, kê, kích thước, kịch.
Cùng, của, cua, cuối, cuộc, cưa, cứ, cơ, cô, cũ, cá, cắt, cấp, cặp, cọc, cục, cúc, cực.
Nghe, nghề, nghỉ, nghĩa, nghìn, nghiêng, nghiệm, nghệch, ngày, ngoài, ngủ, người, nguyên, ngang, ngành, ngắn, ngọt, ngực, ngón.
Ba, bà, bả, bã, bá, bạ, các, cách, chắc, chặt, thức, thấp, thập, tích, tịch, tiếp, tiết, tiệc, việc, mốc, lộc, tộc, rộp, lớp, ớt, ứt.
Đang, đảng, đẹp, đúng, được, đường, đưa, đứng, đấu, đầu, đâu, đây, đẩy, đẫy, đậy.
Pin, pa, Pháp, Séc, Đắk Lắk, Búk, Ơ Đu, A Lưới, Điện Biên, Đồng Nai, Sơn La, Sa Pa, Phú Quốc, Vũng Tàu, Cà Mau, Bạc Liêu.
Ông bà, cha mẹ, anh chị, cô dì, chú bác, cậu mợ, thím, dượng; mừng rỡ, ngạc nhiên, hồi hộp, lo lắng, tự hào, xúc động.
Tết Nguyên Đán, Trung Thu, Quốc Khánh, Giỗ Tổ, rằm tháng Giêng, tháng Chạp, mùng một, ba mươi Tết, bánh chưng, bánh tét.
)";

struct BadCase { const char *text; const char *expectSuggestion; Certainty expectCert; };

static const BadCase kBad[] = {
    {"tòan", "toàn", Certainty::High},
    {"hòan", "hoàn", Certainty::High},
    {"bàt", "bát", Certainty::High},
    {"tàp", "tạp", Certainty::High},
    {"ngĩa", "nghĩa", Certainty::High},
    {"ngề", "nghề", Certainty::High},
    {"nghả", "ngả", Certainty::High},
    {"cẻ", "kẻ", Certainty::High},
    {"cê", "kê", Certainty::High},
    {"gê", "ghê", Certainty::High},
    {"gẻ", "ghẻ", Certainty::High},
    {"kả", "cả", Certainty::High},
    {"kổ", "cổ", Certainty::High},
    {"qủa", "quả", Certainty::High},
    {"thuơng", "thương", Certainty::High},
    {"truờng", "trường", Certainty::High},
    {"viêc", "việc", Certainty::High},
    {"tiêp", "tiếp", Certainty::High},
    {"hóan", "hoán", Certainty::High},
    {"giỏi", nullptr, Certainty::None},   // đúng: không được báo
};

int main() {
    // ---- 1. Bảng vần sinh bằng luật
    const QStringList toneless = generateToneless();
    const QStringList all = generateAllSyllables();
    std::printf("Số âm tiết không thanh sinh ra: %d; có thanh: %d\n", int(toneless.size()), int(all.size()));
    check(toneless.size() > 1500 && toneless.size() < 6000, U("số âm tiết không thanh nằm trong khoảng hợp lý"), QString::number(toneless.size()));
    check(all.size() > 7000 && all.size() < 25000, U("số âm tiết có thanh nằm trong khoảng hợp lý"), QString::number(all.size()));
    int selfBad = 0; QString selfEx;
    for (const QString &s : all) {
        const auto r = checkSyllable(s);
        if (r.verdict != Verdict::Valid && r.verdict != Verdict::Unusual) { ++selfBad; if (selfEx.isEmpty()) selfEx = s; }
    }
    check(selfBad == 0, U("mọi âm tiết sinh ra đều được chấp nhận"), QStringLiteral("%1 âm tiết bị từ chối, ví dụ: %2").arg(selfBad).arg(selfEx));
    int stopBad = 0;
    for (const QString &s : all) {
        if (s.endsWith(QLatin1String("t")) || s.endsWith(QLatin1String("c")) || s.endsWith(QLatin1String("p")) || s.endsWith(QLatin1String("ch"))) {
            const QString d = s.normalized(QString::NormalizationForm_D);
            if (d.contains(QChar(0x0300)) || d.contains(QChar(0x0309)) || d.contains(QChar(0x0303))) ++stopBad;
        }
    }
    check(stopBad == 0, U("không sinh ra vần p/t/c/ch với thanh huyền, hỏi, ngã"));
    const QSet<QString> allSet(all.begin(), all.end());
    for (const char *s : {"việt", "nghĩa", "người", "quyền", "giường", "khuya", "hoà", "huỳnh", "yêu", "ghét", "kỳ"}) {
        const QString nfc = U(s).normalized(QString::NormalizationForm_C);
        check(allSet.contains(nfc), QStringLiteral("danh sách sinh có \"%1\"").arg(U(s)));
    }

    // ---- 2. Văn bản đúng không bị báo lỗi (mọi mức: không có âm tiết nào bị coi là Invalid)
    {
        const QString text = U(kValidText);
        const auto issuesAny = checkText(text, Certainty::Low);
        check(issuesAny.isEmpty(), U("ngữ liệu đúng: không có âm tiết nào bị báo lỗi (mọi mức)"));
        for (int k = 0; k < qMin(10, int(issuesAny.size())); ++k)
            std::printf("  bị báo nhầm: %s — %s\n", issuesAny[k].text.toUtf8().constData(), issuesAny[k].result.reason.toUtf8().constData());
        const auto nfd = checkText(text.normalized(QString::NormalizationForm_D), Certainty::Low);
        check(nfd.isEmpty(), U("ngữ liệu đúng ở dạng NFD cũng không bị báo lỗi"));
        const auto up = checkText(text.toUpper(), Certainty::Low);
        check(up.isEmpty(), U("ngữ liệu in hoa không bị báo lỗi"), up.isEmpty() ? QString() : up.first().text);
    }

    // ---- 3. Từ sai: bị báo, đúng độ chắc chắn, có gợi ý đúng
    for (const BadCase &b : kBad) {
        const QString w = U(b.text);
        const auto r = checkSyllable(w);
        if (b.expectCert == Certainty::None) { check(r.verdict == Verdict::Valid, QStringLiteral("không báo nhầm: %1").arg(w)); continue; }
        check(r.verdict == Verdict::Invalid, QStringLiteral("báo lỗi: %1").arg(w), QStringLiteral("verdict=%1 lý do=%2").arg(int(r.verdict)).arg(r.reason));
        check(r.certainty == b.expectCert, QStringLiteral("độ chắc chắn của %1").arg(w), QString::number(int(r.certainty)));
        check(!r.reason.isEmpty(), QStringLiteral("có lý do cho %1").arg(w));
        if (b.expectSuggestion)
            check(r.suggestions.contains(U(b.expectSuggestion)), QStringLiteral("%1 -> gợi ý %2").arg(w, U(b.expectSuggestion)), r.suggestions.join(QLatin1String(", ")));
        check(r.suggestions.size() <= 5, QStringLiteral("số gợi ý <= 5 cho %1").arg(w));
    }
    // hai dấu thanh trên một chữ (viết bằng ký tự tổ hợp)
    {
        const QString w = QString::fromUtf16(u"lụ́c");
        const auto r = checkSyllable(w);
        check(r.verdict == Verdict::Invalid && r.certainty == Certainty::High, U("hai dấu thanh trên một chữ bị báo"));
    }
    // kiểu chữ của gợi ý theo chữ gốc
    { const auto r = checkSyllable(U("Tòan")); check(r.suggestions.contains(U("Toàn")), U("giữ chữ hoa đầu: Tòan -> Toàn"), r.suggestions.join(QLatin1String(","))); }
    { const auto r = checkSyllable(U("TÒAN")); check(r.suggestions.contains(U("TOÀN")), U("giữ in hoa: TÒAN -> TOÀN"), r.suggestions.join(QLatin1String(","))); }
    // thiếu dấu hoàn toàn (ASCII): chỉ độ chắc chắn Trung bình, vẫn có gợi ý
    {
        const auto r = checkSyllable(U("nguoi"));
        check(r.verdict == Verdict::Invalid && r.certainty == Certainty::Medium, U("\"nguoi\" (không dấu): Invalid, độ chắc chắn Trung bình"));
        check(r.suggestions.contains(U("ngươi")), U("\"nguoi\" gợi ý có \"ngươi\""), r.suggestions.join(QLatin1String(",")));
    }
    // quá dài: thiếu dấu cách
    {
        const auto r = checkSyllable(U("chúngtôi"));
        check(r.verdict == Verdict::Invalid && r.certainty == Certainty::High && r.reason.contains(U("dấu cách")), U("\"chúngtôi\": thiếu dấu cách"));
    }

    // ---- 4. Không kiểm / không báo bừa
    for (const char *s : {"UBND", "TP", "kg", "cm", "WiFi", "Zalo", "jQuery", "señor", "naïve"}) {
        const auto r = checkSyllable(U(s));
        check(r.verdict == Verdict::NotChecked, QStringLiteral("không kiểm: %1").arg(U(s)), QString::number(int(r.verdict)));
    }
    {
        const QString en = U("Please open the email, check the show schedule and send the report to the manager before the meeting.");
        check(checkText(en).isEmpty(), U("câu tiếng Anh không bị báo ở mức Cao"));
        check(!checkText(en, Certainty::Medium).isEmpty(), U("nhưng vẫn thấy được ở mức Trung bình (người dùng chủ động bật)"));
    }
    check(checkText(U("Đắk Lắk Búk")).isEmpty(), U("tên địa danh Đắk Lắk không bị báo"));

    // ---- 5. Giới hạn đã nêu: KHÔNG bắt sai từ, chỉ bắt âm tiết không tồn tại
    check(checkText(U("xử dụng chấp hành trấp hành sắp xếp sắp sếp"), Certainty::Low).isEmpty(),
          U("giới hạn: sai từ (xử dụng/trấp hành) không bị bắt vì âm tiết vẫn hợp lệ"));
    // ---- 6. Vị trí báo lỗi
    {
        const QString t = U("Tôi tòan tâm.");
        const auto is = checkText(t);
        check(is.size() == 1 && is[0].start == 4 && is[0].length == 4 && is[0].text == U("tòan"), U("vị trí/độ dài của âm tiết báo lỗi"));
    }
    // ---- 7. Chấp nhận cả hai kiểu đặt dấu
    for (const char *s : {"hoà", "hòa", "thuỷ", "thủy", "khoẻ", "khỏe", "tuý", "túy", "loé", "lóe", "quỳnh"})
        check(checkSyllable(U(s)).verdict == Verdict::Valid, QStringLiteral("hai kiểu đặt dấu đều đúng: %1").arg(U(s)));
    for (const char *s : {"hóan", "ngườĩ", "qủa"})
        check(checkSyllable(U(s)).verdict != Verdict::Valid, QStringLiteral("dấu sai chữ: %1").arg(U(s)));

    std::printf("\nĐạt: %d; lỗi: %d\nKẾT QUẢ: %s\n", g_pass, g_fail, g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT");
    return g_fail ? 1 : 0;
}
