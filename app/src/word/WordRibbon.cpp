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
    const Group find{tr("Tìm kiếm"), {
        {QStringLiteral("word:find"), tr("Tìm"), 0xE721, QStringLiteral("🔍"), false},
        {QStringLiteral("word:replace"), tr("Thay thế"), 0xE8EE, QStringLiteral("⇄"), false},
        {QStringLiteral("word:count"), tr("Đếm từ"), 0xE8EF, QStringLiteral("#"), false},
    }};
    return {
        {tr("Trang đầu"), {file, undo, clip, font, para, styles, find}},
        {tr("Chèn"), {
            {tr("Phần tử"), {
                {QStringLiteral("word:table"), tr("Bảng"), 0xE80A, QStringLiteral("▦"), false},
                {QStringLiteral("word:image"), tr("Ảnh"), 0xEB9F, QStringLiteral("🖼"), false},
                {QStringLiteral("word:link"), tr("Liên kết"), 0xE71B, QStringLiteral("🔗"), false},
                {QStringLiteral(".uno:InsertPagebreak"), tr("Ngắt trang"), 0xE7C3, QStringLiteral("⤓"), false},
            }},
        }},
        {tr("Bố cục"), {
            {tr("Trang"), {
                {QStringLiteral("word:a4"), tr("Khổ A4 dọc"), 0xE7C3, QStringLiteral("A4"), false},
                {QStringLiteral("word:landscape"), tr("A4 ngang"), 0xE7C3, QStringLiteral("A4↔"), false},
            }},
        }},
        {tr("Xem"), {view}},
    };
}
