// SPDX-License-Identifier: MPL-2.0
// Các tab ribbon của Word (DocKind::Text). Nút "word:*" mở hộp thoại (WordActions), nút ".uno:*" gửi thẳng engine.
#include "../Ribbon.h"

QList<Ribbon::Tab> Ribbon::wordTabs(const Group& file, const Group& undo, const Group& clip, const Group& view) const {
    const Group font{tr("Phông chữ"), {
        {QStringLiteral("word:font"), tr("Phông"), 0xE8D2, QStringLiteral("Aa"), false},
        {QStringLiteral("word:size"), tr("Cỡ chữ"), 0xE8E9, QStringLiteral("A↕"), false},
        {QStringLiteral(".uno:Bold"), tr("Đậm"), 0xE8DD, QStringLiteral("B"), true},
        {QStringLiteral(".uno:Italic"), tr("Nghiêng"), 0xE8DB, QStringLiteral("I"), true},
        {QStringLiteral(".uno:Underline"), tr("Gạch chân"), 0xE8DC, QStringLiteral("U"), true},
        {QStringLiteral(".uno:Strikeout"), tr("Gạch ngang"), 0xEDE0, QStringLiteral("S̶"), true},
        {QStringLiteral(".uno:SuperScript"), tr("Chỉ số trên"), 0xE8BC, QStringLiteral("x²"), true},
        {QStringLiteral(".uno:SubScript"), tr("Chỉ số dưới"), 0xE8BC, QStringLiteral("x₂"), true},
        {QStringLiteral(".uno:ResetAttributes"), tr("Xoá định dạng"), 0xE894, QStringLiteral("Aₓ"), false},
        {QStringLiteral("word:color"), tr("Màu chữ"), 0xE8D3, QStringLiteral("A"), false},
        {QStringLiteral("word:back"), tr("Tô nền"), 0xE7E6, QStringLiteral("▮"), false},
    }};
    const Group para{tr("Đoạn văn"), {
        {QStringLiteral(".uno:LeftPara"), tr("Trái"), 0xE8E4, QStringLiteral("⇤"), true},
        {QStringLiteral(".uno:CenterPara"), tr("Giữa"), 0xE8E3, QStringLiteral("↔"), true},
        {QStringLiteral(".uno:RightPara"), tr("Phải"), 0xE8E2, QStringLiteral("⇥"), true},
        {QStringLiteral(".uno:JustifyPara"), tr("Đều"), 0xE8E1, QStringLiteral("≡"), true},
        {QStringLiteral(".uno:SpacePara1"), tr("Dòng đơn"), 0xE9E9, QStringLiteral("1.0"), false},
        {QStringLiteral(".uno:SpacePara15"), tr("Dòng 1,5"), 0xE9E9, QStringLiteral("1.5"), false},
        {QStringLiteral(".uno:DefaultBullet"), tr("Gạch đầu dòng"), 0xE8FD, QStringLiteral("•"), true},
        {QStringLiteral(".uno:DefaultNumbering"), tr("Đánh số"), 0xE9D5, QStringLiteral("1."), true},
        {QStringLiteral(".uno:DecrementIndent"), tr("Giảm lề"), 0xE290, QStringLiteral("◀≡"), false},
        {QStringLiteral(".uno:IncrementIndent"), tr("Tăng lề"), 0xE291, QStringLiteral("≡▶"), false},
    }};
    const Group styles{tr("Kiểu"), {
        {QStringLiteral("word:style0"), tr("Thường"), 0xE8A5, QStringLiteral("T"), false},
        {QStringLiteral("word:style1"), tr("Tiêu đề 1"), 0xE8A5, QStringLiteral("H1"), false},
        {QStringLiteral("word:style2"), tr("Tiêu đề 2"), 0xE8A5, QStringLiteral("H2"), false},
        {QStringLiteral("word:style3"), tr("Tiêu đề 3"), 0xE8A5, QStringLiteral("H3"), false},
    }};
    const Group changeCase{tr("Hoa/thường"), {
        {QStringLiteral(".uno:ChangeCaseToUpper"), tr("HOA"), 0xE8D2, QStringLiteral("AA"), false},
        {QStringLiteral(".uno:ChangeCaseToLower"), tr("thường"), 0xE8D2, QStringLiteral("aa"), false},
        {QStringLiteral(".uno:ChangeCaseToSentenceCase"), tr("Đầu câu"), 0xE8D2, QStringLiteral("Aa"), false},
        {QStringLiteral(".uno:ChangeCaseToTitleCase"), tr("Mỗi Từ"), 0xE8D2, QStringLiteral("Ab"), false},
        {QStringLiteral(".uno:ChangeCaseToToggleCase"), tr("Đảo"), 0xE8D2, QStringLiteral("aA"), false},
    }};
    const Group find{tr("Tìm kiếm"), {
        {QStringLiteral("word:find"), tr("Tìm"), 0xE721, QStringLiteral("🔍"), false},
        {QStringLiteral("word:replace"), tr("Thay thế"), 0xE8EE, QStringLiteral("⇄"), false},
        {QStringLiteral("word:count"), tr("Đếm từ"), 0xE8EF, QStringLiteral("#"), false},
    }};
    return {
        {tr("Trang đầu"), {file, undo, clip, font, changeCase, para, styles, find}},
        {tr("Chèn"), {
            {tr("Phần tử"), {
                {QStringLiteral("word:table"), tr("Bảng"), 0xE80A, QStringLiteral("▦"), false},
                {QStringLiteral("word:image"), tr("Ảnh"), 0xEB9F, QStringLiteral("🖼"), false},
                {QStringLiteral("word:link"), tr("Liên kết"), 0xE71B, QStringLiteral("🔗"), false},
                {QStringLiteral(".uno:InsertPagebreak"), tr("Ngắt trang"), 0xE7C3, QStringLiteral("⤓"), false},
                {QStringLiteral(".uno:InsertPageNumberField"), tr("Số trang"), 0xE8A5, QStringLiteral("#"), false},
            }},
            {tr("Ký hiệu"), {
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"©\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Bản quyền"), 0xE8D2, QStringLiteral("©"), false},
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"®\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Đã đăng ký"), 0xE8D2, QStringLiteral("®"), false},
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"™\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Nhãn hiệu"), 0xE8D2, QStringLiteral("™"), false},
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"€\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Euro"), 0xE8D2, QStringLiteral("€"), false},
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"°\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Độ"), 0xE8D2, QStringLiteral("°"), false},
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"±\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Cộng trừ"), 0xE8D2, QStringLiteral("±"), false},
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"≠\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Khác"), 0xE8D2, QStringLiteral("≠"), false},
                {QStringLiteral(".uno:InsertSymbol#{\"Symbols\":{\"type\":\"string\",\"value\":\"→\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}"), tr("Mũi tên"), 0xE8D2, QStringLiteral("→"), false},
            }},
        }},
        {tr("Bố cục"), {
            {tr("Trang"), {
                {QStringLiteral("word:a4"), tr("Khổ A4 dọc"), 0xE7C3, QStringLiteral("A4"), false},
                {QStringLiteral("word:landscape"), tr("A4 ngang"), 0xE7C3, QStringLiteral("A4↔"), false},
            }},
            {tr("Lề trang"), {
                {QStringLiteral("word:marginVN"), tr("Lề chuẩn VN"), 0xE7C3, QStringLiteral("VN"), false},
                {QStringLiteral("word:marginNormal"), tr("Lề thường"), 0xE7C3, QStringLiteral("▭"), false},
                {QStringLiteral("word:marginNarrow"), tr("Lề hẹp"), 0xE7C3, QStringLiteral("▯"), false},
            }},
        }},
        {tr("Tiện ích Việt"), {
            {tr("Văn bản tiếng Việt"), {
                {QStringLiteral("word:vnMoney"), tr("Số tiền → chữ"), 0xE8EF, QStringLiteral("đ"), false},
                {QStringLiteral("word:vnDate"), tr("Chèn ngày Việt"), 0xE787, QStringLiteral("📅"), false},
                {QStringLiteral("word:vnTcvn3"), tr("TCVN3 → Unicode"), 0xE8D2, QStringLiteral("Ư"), false},
                {QStringLiteral("word:vnSyllable"), tr("Kiểm tra âm tiết"), 0xE8D2, QStringLiteral("Aa"), false},
            }},
        }},
        {tr("Xem"), {view}},
    };
}
