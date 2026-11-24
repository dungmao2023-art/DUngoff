// SPDX-License-Identifier: MPL-2.0
// Tiện ích văn bản tiếng Việt (chỉ cần QtCore, không phụ thuộc engine hay giao diện):
//   1) số tiền VNĐ -> chữ        2) ngày dd/mm/yyyy <-> "ngày .. tháng .. năm .."
//   3) bảng mã cũ (TCVN3) -> Unicode, KHÔNG ép khi không chắc.
// Nguyên tắc dự án: không đổi dữ liệu âm thầm. Mọi trường hợp mơ hồ trả về lỗi/cảnh báo kèm
// văn bản gốc nguyên vẹn; không làm tròn số tiền, không ép chuyển bảng mã khi điểm tin cậy thấp.
#pragma once
#include <QDate>
#include <QString>
#include <QStringList>

namespace vntext {

// ---------- 1. Số -> chữ ----------
// Giới hạn: tối đa 18 chữ số (dưới 10^18, đơn vị cao nhất "triệu tỷ"). Không có phần thập phân.
QString numberToWords(quint64 n, bool useLe = false /* "lẻ" thay vì "linh" */, bool useTu = true /* 24 = "hai mươi tư" */);

struct MoneyOptions {
    bool useLe = false;       // "một trăm lẻ năm" thay vì "một trăm linh năm"
    bool useTu = true;        // "hai mươi tư" thay vì "hai mươi bốn"
    bool currency = true;     // thêm "đồng"
    bool chan = true;         // thêm "chẵn" sau "đồng" (thông lệ chứng từ)
    bool capitalize = true;   // viết hoa chữ cái đầu
};

struct MoneyResult {
    bool ok = false;
    QString text;             // rỗng nếu !ok
    QString error;            // lý do từ chối (không đoán, không làm tròn)
    QStringList warnings;
};

// Nhận chuỗi số tiền: "1500000", "1.500.000", "1,500,000", "-250000", "1.500.000 đ", "1.500.000,00".
// Phần thập phân khác 0 -> TỪ CHỐI (không làm tròn âm thầm). Dấu '.'/',' mơ hồ được giải theo quy tắc nêu trong .cpp.
MoneyResult moneyToWords(const QString &amount, const MoneyOptions &opt = {});

// ---------- 2. Ngày ----------
enum class DateStyle {
    Admin,   // theo NĐ 30/2020/NĐ-CP: ngày < 10 và tháng 1, 2 thêm số 0 ("ngày 05 tháng 02 năm 2026", "ngày 5 tháng 3 năm 2026")
    Plain,   // không thêm số 0
    Padded   // luôn 2 chữ số
};

struct DateParse {
    bool ok = false;
    QDate date;
    QString error;
};

QString formatDateVn(const QDate &d, DateStyle style = DateStyle::Admin);       // "ngày .. tháng .. năm .."
QString formatDateNumeric(const QDate &d);                                       // "dd/mm/yyyy"
QString dateToSpelledWords(const QDate &d);                                      // "ngày năm tháng ba năm hai nghìn không trăm hai mươi sáu"
// Đọc "5/3/2026", "05-03-2026", "5.3.2026", "ngày 5 tháng 3 năm 2026". Năm bắt buộc 4 chữ số
// (năm 2 chữ số mơ hồ nên bị từ chối). Ngày không có thật (30/02) bị từ chối. Không đọc dạng chữ.
DateParse parseDateVn(const QString &text);

// ---------- 3. Bảng mã cũ -> Unicode ----------
enum class LegacyEncoding { TCVN3, VniWindows };

enum class Verdict {
    Converted,        // đủ tin cậy: đã chuyển
    NeedsReview,      // không chắc: văn bản giữ nguyên (trừ khi force)
    AlreadyUnicode,   // đã có chữ Việt Unicode: không đụng
    NothingToConvert, // không có ký tự thuộc bảng mã
    Unsupported       // bảng mã chưa được hỗ trợ/kiểm chứng
};

struct ConvertResult {
    QString text;           // luôn là văn bản gốc nếu verdict != Converted (hoặc force)
    Verdict verdict = Verdict::NothingToConvert;
    double confidence = 0.0; // tỷ lệ "từ" có ký tự bảng mã mà sau khi chuyển là âm tiết tiếng Việt hợp lệ
    int affectedWords = 0;
    QStringList warnings;
};

// Văn bản cũ đọc ra từ tệp .doc/.docx thường là các ký tự Latin-1 (vd "ViÖt" = "Việt" trong TCVN3).
// Chỉ hỗ trợ chữ thường có dấu và 7 chữ hoa cơ sở (Ă Â Ê Ô Ơ Ư Đ) của TCVN3 (ABC).
// Chữ HOA CÓ DẤU của font .VnTimeH dùng bảng khác, KHÔNG được hỗ trợ (có cảnh báo).
// VniWindows: chưa có bảng đã kiểm chứng -> trả Unsupported, không đoán.
ConvertResult legacyToUnicode(const QString &text, LegacyEncoding enc, bool force = false);

// Nội bộ nhưng công khai để kiểm thử: âm tiết (chữ thường, đã Unicode) có hợp lệ theo cấu trúc tiếng Việt?
bool isPlausibleSyllable(const QString &lowerSyllable);

} // namespace vntext
