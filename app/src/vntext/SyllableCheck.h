// SPDX-License-Identifier: MPL-2.0
// Kiểm tra chính tả tiếng Việt theo ÂM TIẾT (viết mới hoàn toàn, không dùng từ điển nào của bên thứ ba).
//
// Tiếng Việt viết mỗi âm tiết cách nhau bằng dấu cách. Mỗi âm tiết = [phụ âm đầu] + vần + thanh điệu.
// Bộ kiểm tra dùng luật âm vị và chính tả (bảng vần được SINH từ nguyên âm × âm cuối, xem SyllableCheck.cpp):
//   - phụ âm đầu hợp lệ; quy tắc c/k/q, g/gh, ng/ngh, gi, qu, y/i, iê/yê
//   - vần hợp lệ (nguyên âm + âm đệm + âm cuối)
//   - vần kết thúc bằng c, ch, p, t chỉ mang thanh sắc hoặc nặng
//   - dấu thanh đặt đúng nguyên âm (chấp nhận cả hai kiểu hoà/hòa, thuỷ/thủy)
//
// GIỚI HẠN (quan trọng):
//   - Chỉ bắt âm tiết KHÔNG TỒN TẠI trong tiếng Việt. KHÔNG bắt sai từ: "xử dụng" (đúng: sử dụng),
//     "chấp hành" / "trấp hành"... đều gồm âm tiết hợp lệ nên được coi là đúng.
//   - Không có từ điển nên không biết từ ghép, tên riêng, nghĩa; không sửa theo ngữ cảnh.
//   - Từ mượn viết nguyên dạng ("email", "show"), chữ f/j/w/z, số, từ không có nguyên âm (viết tắt) -> KHÔNG kiểm.
//   - Chữ thuần ASCII không dấu (có thể là tiếng Anh, tên nước ngoài) chỉ đạt độ chắc chắn "Trung bình" khi sai;
//     chữ có dấu/mũ/móc/đ đặc trưng tiếng Việt mới đạt "Cao". Mặc định checkText() chỉ báo mức "Cao".
//   - Bảng vần dựa trên hiểu biết âm vị học; vần rất hiếm có thể bị coi là "Hiếm" (Unusual) hoặc, nếu thiếu, bị báo nhầm.
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace vntext::spell {

enum class Verdict {
    Valid,       // âm tiết hợp lệ
    Unusual,     // cấu trúc cho phép nhưng hiếm / thường gặp ở từ mượn, tên riêng
    Invalid,     // không tồn tại trong tiếng Việt
    NotChecked   // không phải chữ Việt để kiểm (từ mượn, viết tắt, ký tự lạ)
};

enum class Certainty { None = 0, Low = 1, Medium = 2, High = 3 };

struct SyllableResult {
    Verdict verdict = Verdict::NotChecked;
    Certainty certainty = Certainty::None; // độ chắc chắn của KẾT LUẬN (Valid: Cao; Invalid: xem ghi chú ở đầu tệp)
    QString reason;                        // giải thích (tiếng Việt) khi Invalid/Unusual/NotChecked
    QStringList suggestions;               // gợi ý sửa (tối đa 5), cùng chữ hoa/thường kiểu gốc ở chữ đầu
    QString onset;                         // phụ âm đầu đã tách (chữ thường)
    QString rime;                          // vần đã tách (chữ thường, không dấu thanh)
    int tone = 0;                          // 0 ngang, 1 huyền, 2 hỏi, 3 ngã, 4 sắc, 5 nặng
};

// Kiểm một âm tiết (không chứa dấu cách).
SyllableResult checkSyllable(const QString &syllable);
bool isValidSyllable(const QString &syllable);   // Valid hoặc Unusual

struct TextIssue {
    int start = 0;       // vị trí trong chuỗi gốc (đơn vị QChar)
    int length = 0;
    QString text;
    SyllableResult result;
};

// Quét văn bản, trả các âm tiết Invalid có độ chắc chắn >= minCertainty. Không sửa gì, không đổi văn bản.
QVector<TextIssue> checkText(const QString &text, Certainty minCertainty = Certainty::High);

// Một dòng mô tả cho danh sách kết quả: «…ngữ cảnh…» kèm lý do và gợi ý. Chỉ đọc, không đổi văn bản.
QString describeIssue(const QString &text, const TextIssue &issue, int radius = 18);

// Sinh toàn bộ âm tiết hợp lệ KHÔNG dấu thanh... (có dấu mũ/móc) bằng luật; dùng cho kiểm thử và thống kê.
QStringList generateToneless();
// Sinh mọi âm tiết có thanh (áp dụng luật thanh cho vần p,t,c,ch), dạng dựng sẵn NFC.
QStringList generateAllSyllables();

} // namespace vntext::spell
