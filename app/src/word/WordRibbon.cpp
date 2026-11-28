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
        {QStringLiteral(".uno:SpacePara2"), tr("Dòng đôi"), 0xE9E9, QStringLiteral("2.0"), false},
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
            {tr("Chỉnh sửa bảng"), {
                {QStringLiteral(".uno:InsertRowsAfter"), tr("Thêm hàng bên dưới"), 0xE710, QStringLiteral("+≡"), false},
                {QStringLiteral(".uno:InsertColumnsAfter"), tr("Thêm cột bên phải"), 0xE710, QStringLiteral("+‖"), false},
                {QStringLiteral("word:tableDeleteRow"), tr("Xóa hàng…"), 0xE74D, QStringLiteral("−≡"), false},
                {QStringLiteral("word:tableDeleteColumn"), tr("Xóa cột…"), 0xE74D, QStringLiteral("−‖"), false},
                {QStringLiteral("word:tableMergeRow"), tr("Gộp cả hàng…"), 0xE8B9, QStringLiteral("⊞"), false},
                {QStringLiteral("word:tableSplitCell"), tr("Tách ô…"), 0xE8B9, QStringLiteral("▥"), false},
            }},
            {tr("Đầu và chân trang"), {
                {QStringLiteral("word:headerEnable"), tr("Bật đầu trang"), 0xE8A5, QStringLiteral("H"), false},
                {QStringLiteral("word:footerEnable"), tr("Bật chân trang"), 0xE8A5, QStringLiteral("F"), false},
            }},
            {tr("Ký hiệu"), {
                {QStringLiteral("word:symbols"), tr("Thêm ký hiệu"), 0xE8D2, QStringLiteral("Ω"), false},
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
                {QStringLiteral("word:marginCustom"), tr("Lề tùy chỉnh"), 0xE7C3, QStringLiteral("▭"), false},
            }},
        }},
        {tr("Tham chiếu"), {
            {tr("Mục lục có sẵn"), {
                {QStringLiteral(".uno:UpdateAllIndexes"), tr("Cập nhật mục lục"), 0xE72C, QStringLiteral("↻"), false},
            }},
            {tr("Chú thích"), {
                {QStringLiteral("word:footnote"), tr("Chú thích cuối trang"), 0xE8D2, QStringLiteral("¹"), false},
                {QStringLiteral("word:endnote"), tr("Chú thích cuối văn bản"), 0xE8D2, QStringLiteral("²"), false},
            }},
        }},
        {tr("Gửi thư"), {
            {tr("Trộn thư"), {
                {QStringLiteral("word:mailMerge"), tr("Điền hàng loạt"), 0xE8D2, QStringLiteral("⇄"), false},
            }},
        }},
        {tr("Xem lại"), {
            {tr("Theo dõi thay đổi"), {
                {QStringLiteral(".uno:TrackChanges"), tr("Ghi thay đổi"), 0xE8D2, QStringLiteral("✎"), true},
                {QStringLiteral(".uno:ShowTrackedChanges"), tr("Hiện thay đổi"), 0xE7B3, QStringLiteral("◉"), true},
                {QStringLiteral(".uno:PreviousTrackedChange"), tr("Trước"), 0xE72B, QStringLiteral("←"), false},
                {QStringLiteral(".uno:NextTrackedChange"), tr("Sau"), 0xE72A, QStringLiteral("→"), false},
                {QStringLiteral(".uno:AcceptTrackedChange"), tr("Chấp nhận"), 0xE73E, QStringLiteral("✓"), false},
                {QStringLiteral(".uno:RejectTrackedChange"), tr("Từ chối"), 0xE711, QStringLiteral("×"), false},
                {QStringLiteral("word:acceptAllReview"), tr("Chấp nhận tất cả…"), 0xE73E, QStringLiteral("✓"), false},
                {QStringLiteral("word:rejectAllReview"), tr("Từ chối tất cả…"), 0xE711, QStringLiteral("×"), false},
            }},
            {tr("Bình luận"), {
                {QStringLiteral("word:addComment"), tr("Bình luận mới"), 0xE90A, QStringLiteral("+"), false},
            }},
            {tr("Kiểm tra"), {
                {QStringLiteral("word:reviewSyllable"), tr("Kiểm tra âm tiết"), 0xE8D2, QStringLiteral("Aa"), false},
                {QStringLiteral("word:reviewCount"), tr("Đếm từ"), 0xE8EF, QStringLiteral("#"), false},
                {QStringLiteral("word:inspect"), tr("Kiểm tra tài liệu"), 0xE946, QStringLiteral("✓"), false},
            }},
        }},
        {tr("In"), {{tr("In & xuất"), {
            {QStringLiteral("word:print"), tr("In / Xem trước"), 0xE749, QStringLiteral("🖨"), false},
            {QStringLiteral("word:exportPdf"), tr("Xuất PDF"), 0xEA90, QStringLiteral("PDF"), false},
        }}}},
        {tr("Văn bản Việt Nam"), {{tr("Thể thức hành chính"), {
            {QStringLiteral("word:adminHead"), tr("Quốc hiệu / đầu văn bản"), 0xE8A5, QStringLiteral("VN"), false},
            {QStringLiteral("word:adminTail"), tr("Nơi nhận / ký tên"), 0xE8A5, QStringLiteral("T"), false},
            {QStringLiteral("word:adminFull"), tr("Khung văn bản"), 0xE8A5, QStringLiteral("▤"), false},
            {QStringLiteral("word:adminMargin"), tr("Lề hành chính"), 0xE7C3, QStringLiteral("VN"), false},
        }}}},
        {tr("Tiện ích Việt"), {
            {tr("Văn bản tiếng Việt"), {
                {QStringLiteral("word:vnMoney"), tr("Số tiền → chữ"), 0xE8EF, QStringLiteral("đ"), false},
                {QStringLiteral("word:vnDate"), tr("Chèn ngày Việt"), 0xE787, QStringLiteral("📅"), false},
                {QStringLiteral("word:vnTcvn3"), tr("TCVN3 → Unicode"), 0xE8D2, QStringLiteral("Ư"), false},
                {QStringLiteral("word:vnSyllable"), tr("Kiểm tra âm tiết"), 0xE8D2, QStringLiteral("Aa"), false},
            }},
        }},
        {tr("Xem"), {view, {tr("Điều hướng"), {
            {QStringLiteral("word:outline"), tr("Dàn ý tài liệu"), 0xE8A5, QStringLiteral("☷"), false},
            {QStringLiteral(".uno:GoToStartOfDoc"), tr("Đầu tài liệu"), 0xE74A, QStringLiteral("↑"), false},
            {QStringLiteral(".uno:GoToEndOfDoc"), tr("Cuối tài liệu"), 0xE74B, QStringLiteral("↓"), false},
        }}}},
    };
}
