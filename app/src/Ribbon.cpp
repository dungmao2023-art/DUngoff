// SPDX-License-Identifier: MPL-2.0
#include "Ribbon.h"

#include "Theme.h"
#include "ui/Icons.h"

#include <QFrame>
#include <QComboBox>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QGridLayout>
#include <QSignalBlocker>
#include "word/WordCommands.h"
#include "excel/ExcelCommands.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

using dung::DocKind;

Ribbon::Ribbon(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("Ribbon"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(8, 0, 8, 4);
    v->setSpacing(0);
    m_tabs = new QTabBar(this);
    m_tabs->setObjectName(QStringLiteral("RibbonTabs"));
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    m_pages = new QStackedWidget(this);
    m_pages->setFixedHeight(100);
    v->addWidget(m_tabs);
    v->addWidget(m_pages);
    connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
}

QList<Ribbon::Tab> Ribbon::tabsFor(DocKind kind, bool ro) const {
    // Biểu tượng: lấy từ dung::ui::iconNameForCommand(id) (app/src/ui/CommandIcons.cpp); 2 trường glyph cũ không còn dùng.
    const Group file{tr("Tệp"), {
        {QStringLiteral("save"), tr("Lưu"), 0xE74E, QStringLiteral("💾"), false},
        {QStringLiteral("saveAs"), tr("Lưu thành"), 0xE792, QStringLiteral("⇩"), false},
    }};
    const Tab fileTab{tr("Tệp"), {
        {tr("Lưu"), {
            {QStringLiteral("save"), tr("Lưu"), 0xE74E, QStringLiteral("💾"), false},
            {QStringLiteral("saveAs"), tr("Lưu thành"), 0xE792, QStringLiteral("⇩"), false},
            {QStringLiteral("exportPdf"), tr("Xuất PDF"), 0xEA90, QStringLiteral("PDF"), false},
        }},
        {tr("In"), {
            {QStringLiteral("print"), tr("In / Xem trước"), 0xE749, QStringLiteral("🖨"), false},
        }},
        {tr("Tệp"), {
            {QStringLiteral("open"), tr("Mở"), 0xE8E5, QStringLiteral("📂"), false},
            {QStringLiteral("close"), tr("Đóng"), 0xE711, QStringLiteral("✕"), false},
        }},
    }};
    const Group clip{tr("Bộ nhớ tạm"), {
        {QStringLiteral("paste"), tr("Dán"), 0xE77F, QStringLiteral("📋"), false},
        {QStringLiteral("cut"), tr("Cắt"), 0xE8C6, QStringLiteral("✂"), false},
        {QStringLiteral("copy"), tr("Sao chép"), 0xE8C8, QStringLiteral("⧉"), false},
        {QStringLiteral(".uno:SelectAll"), tr("Chọn tất cả"), 0xE8B3, QStringLiteral("☐"), false},
        {QStringLiteral(".uno:FormatPaintbrush"), tr("Chép định dạng"), 0xE790, QStringLiteral("🖌"), false},
    }};
    const Group undo{tr("Hoàn tác"), {
        {QStringLiteral(".uno:Undo"), tr("Hoàn tác"), 0xE7A7, QStringLiteral("↶"), false},
        {QStringLiteral(".uno:Redo"), tr("Làm lại"), 0xE7A6, QStringLiteral("↷"), false},
    }};
    const Group font{tr("Phông chữ"), {
        {QStringLiteral(".uno:Bold"), tr("Đậm"), 0xE8DD, QStringLiteral("B"), true},
        {QStringLiteral(".uno:Italic"), tr("Nghiêng"), 0xE8DB, QStringLiteral("I"), true},
        {QStringLiteral(".uno:Underline"), tr("Gạch chân"), 0xE8DC, QStringLiteral("U"), true},
    }};
    const Group view{tr("Thu phóng"), {
        {QStringLiteral("zoomOut"), tr("Thu nhỏ"), 0xE71F, QStringLiteral("−"), false},
        {QStringLiteral("zoom100"), tr("100%"), 0xE71E, QStringLiteral("1:1"), false},
        {QStringLiteral("zoomIn"), tr("Phóng to"), 0xE8A3, QStringLiteral("+"), false},
    }};

    if (kind == DocKind::Drawing && !ro) {
        return {fileTab,
            {tr("Chỉnh sửa bản sao"), {file, clip, undo, font, view,
                {tr("Nội dung"), {
                    {QStringLiteral(".uno:Text"), tr("Hộp chữ"), 0xE8D2, QStringLiteral("T"), false},
                    {QStringLiteral(".uno:BasicShapes.rectangle"), tr("Hình chữ nhật"), 0xE7C3, QStringLiteral("□"), false},
                }},
                {tr("Trang"), {
                    {QStringLiteral("pdfPrev"), tr("Trang trước"), 0xE70E, QStringLiteral("▲"), false},
                    {QStringLiteral("pdfNext"), tr("Trang sau"), 0xE70D, QStringLiteral("▼"), false},
                }},
            }},
            {tr("In / xuất"), {{tr("Bản PDF"), {
                {QStringLiteral("print"), tr("Xem trước / in"), 0xE749, QStringLiteral("🖨"), false},
                {QStringLiteral("exportPdf"), tr("Xuất PDF mới"), 0xEA90, QStringLiteral("PDF"), false},
            }}}},
        };
    }
    if (kind == DocKind::Drawing) {
        // PDF: xem + công cụ PDF (mã ở app/src/pdf/). ID "pdfTools:*" xử lý trong MainWindow.
        return {fileTab,
            {tr("Trang đầu"), {file, view}},
            {tr("Công cụ PDF"), {
                {tr("Chỉnh sửa"), {
                    {QStringLiteral("pdfEditCopy"), tr("Tạo bản chỉnh sửa"), 0xE8D2, QStringLiteral("T"), false},
                }},
                {tr("Trang"), {
                    {QStringLiteral("pdfPrev"), tr("Trang trước"), 0xE70E, QStringLiteral("▲"), false},
                    {QStringLiteral("pdfNext"), tr("Trang sau"), 0xE70D, QStringLiteral("▼"), false},
                }},
                {tr("Thao tác"), {
                    {QStringLiteral("pdfTools:merge"), tr("Gộp"), 0xE8B9, QStringLiteral("⊞"), false},
                    {QStringLiteral("pdfTools:split"), tr("Tách"), 0xE8C6, QStringLiteral("✂"), false},
                    {QStringLiteral("pdfTools:reorder"), tr("Sắp xếp trang"), 0xE8C8, QStringLiteral("↕"), false},
                    {QStringLiteral("pdfTools:rotate"), tr("Xoay"), 0xE7AD, QStringLiteral("⟳"), false},
                    {QStringLiteral("pdfTools:compress"), tr("Nén"), 0xE8B7, QStringLiteral("⇲"), false},
                }},
                {tr("Trang"), {
                    {QStringLiteral("pdfTools:number"), tr("Đánh số trang"), 0xE8EF, QStringLiteral("#"), false},
                    {QStringLiteral("pdfTools:stamp"), tr("Chèn chữ/ảnh · đóng dấu"), 0xE7C3, QStringLiteral("◉"), false},
                    {QStringLiteral("pdfTools:ocr"), tr("OCR → văn bản"), 0xE8D2, QStringLiteral("Aa"), false},
                    {QStringLiteral("pdfTools:images"), tr("Ảnh → PDF"), 0xE8B9, QStringLiteral("▣"), false},
                    {QStringLiteral("pdfTools:blank"), tr("Trang trắng"), 0xE74D, QStringLiteral("□"), false},
                }},
                {tr("Tệp"), {
                    {QStringLiteral("pdfTools:info"), tr("Thông tin"), 0xE946, QStringLiteral("ⓘ"), false},
                    {QStringLiteral("pdfTools:search"), tr("Tìm chữ"), 0xE721, QStringLiteral("⌕"), false},
                }},
            }},
        };
    }

    if (ro) {
        // Chế độ chỉ đọc: chỉ còn các thao tác không sửa nội dung.
        return {fileTab, {tr("Trang đầu"), {file, {tr("Bộ nhớ tạm"), {{QStringLiteral("copy"), tr("Sao chép"), 0xE8C8, QStringLiteral("⧉"), false}}}, view}}};
    }

    switch (kind) {
    case DocKind::Spreadsheet: {
        const auto B = [](const char* id, const QString& label, char16_t g, const char* fb, bool chk) {
            return Btn{QString::fromUtf8(id), label, g, QString::fromUtf8(fb), chk};
        };
        return {fileTab,
            {tr("Trang đầu"), {file, undo, clip, font,
                {tr("Phông & màu"), {
                    B("xl:fontName", tr("Phông"), 0xE8D2, "Aa", false),
                    B("xl:fontSize", tr("Cỡ chữ"), 0xE8E8, "pt", false),
                    B(".uno:Strikeout", tr("Gạch ngang"), 0xE8DD, "S̶", true),
                    B("xl:textColor", tr("Màu chữ"), 0xE8D3, "A", false),
                    B("xl:fillColor", tr("Màu nền"), 0xE790, "▧", false),
                }},
                {tr("Căn lề"), {
                    B(".uno:AlignLeft", tr("Trái"), 0xE8E4, "⇤", true),
                    B(".uno:AlignHorizontalCenter", tr("Giữa"), 0xE8E3, "↔", true),
                    B(".uno:AlignRight", tr("Phải"), 0xE8E2, "⇥", true),
                    B(".uno:AlignTop", tr("Trên"), 0xE74A, "⤒", true),
                    B(".uno:AlignVCenter", tr("Giữa dọc"), 0xE8E3, "↕", true),
                    B(".uno:AlignBottom", tr("Dưới"), 0xE74B, "⤓", true),
                    B(".uno:ToggleMergeCells", tr("Gộp ô"), 0xE8B9, "⊞", true),
                    B(".uno:WrapText", tr("Xuống dòng"), 0xE751, "↩", true),
                }},
                {tr("Số"), {
                    B(".uno:NumberFormatStandard", tr("Chung"), 0xE8EF, "123", false),
                    B(".uno:NumberFormatThousands", tr("Dấu phân cách"), 0xE8C7, "1,0", true),
                    B(".uno:NumberFormatPercent", tr("Phần trăm"), 0xE94C, "%", true),
                    B(".uno:NumberFormatDate", tr("Ngày"), 0xE787, "📅", true),
                    B(".uno:NumberFormatIncDecimals", tr("Thêm số lẻ"), 0xE948, ".0+", false),
                    B(".uno:NumberFormatDecDecimals", tr("Bớt số lẻ"), 0xE949, ".0−", false),
                }},
            }},
            {tr("Chèn"), {
                {tr("Hàng & cột"), {
                    B(".uno:InsertRowsBefore", tr("Chèn hàng"), 0xE710, "+≡", false),
                    B(".uno:InsertColumnsBefore", tr("Chèn cột"), 0xE710, "+‖", false),
                    B("xl:deleteRow", tr("Xoá hàng…"), 0xE74D, "−≡", false),
                    B("xl:deleteColumn", tr("Xoá cột…"), 0xE74D, "−‖", false),
                    B("xl:colWidth", tr("Rộng cột"), 0xE746, "↔|", false),
                    B("xl:rowHeight", tr("Cao hàng"), 0xE745, "↕|", false),
                    B(".uno:SetOptimalColumnWidth", tr("Vừa cột"), 0xE746, "⇔", false),
                }},
                {tr("Sheet"), {
                    B("xl:sheetAdd", tr("Thêm sheet"), 0xE710, "+▭", false),
                    B("xl:sheetRename", tr("Đổi tên"), 0xE8AC, "Ab", false),
                    B("xl:sheetDup", tr("Nhân đôi"), 0xE8C8, "⧉", false),
                    B("xl:sheetDelete", tr("Xoá sheet"), 0xE74D, "−▭", false),
                }},
            }},
            {tr("Biểu đồ"), {
                {tr("Chèn vào sheet"), {
                    B("xl:insertChart", tr("Biểu đồ cột"), 0xE9D2, "▥", false),
                }},
            }},
            {tr("Công thức"), {
                {tr("Trợ lý"), {
                    B("xl:formulaAssistant", tr("Tôi muốn"), 0xE8EF, "ƒx", false),
                    B("xl:rangeTools", tr("Công cụ vùng"), 0xE8B9, "▦", false),
                    B("xl:vnMoney", tr("Số → chữ"), 0xE8EF, "đ", false),
                    B(".uno:AutoSum", tr("Tổng tự động"), 0xE8EF, "Σ", false),
                }},
            }},
            {tr("Dữ liệu"), {
                {tr("Điền"), {
                    B("xl:fillDown", tr("Điền xuống"), 0xE74B, "↓", false),
                    B("xl:fillRight", tr("Điền sang phải"), 0xE72A, "→", false),
                    B("xl:fillUp", tr("Điền lên"), 0xE74A, "↑", false),
                    B("xl:fillLeft", tr("Điền sang trái"), 0xE72B, "←", false),
                    B("xl:fillSeries", tr("Điền dãy số"), 0xE8EF, "1…", false),
                }},
                {tr("Sắp xếp & lọc"), {
                    B("xl:sortAscending", tr("Tăng dần"), 0xE8CB, "A↓", false),
                    B("xl:sortDescending", tr("Giảm dần"), 0xE8CB, "Z↓", false),
                    B(".uno:DataFilterAutoFilter", tr("Bộ lọc"), 0xE71C, "⏷", true),
                }},
                {tr("PivotTable"), {
                    B("xl:refreshPivot", tr("Cập nhật Pivot"), 0xE72C, "↻", false),
                }},
                {tr("Tiện ích bảng"), {
                    B("tableTools:dedup", tr("Dòng trùng"), 0xE8B9, "⧉", false),
                    B("tableTools:clean", tr("Làm sạch ngày/tiền"), 0xE8EF, "✧", false),
                    B("tableTools:compare", tr("So sánh CSV"), 0xE8AB, "⇄", false),
                }},
                {tr("Tìm kiếm"), {
                    B("xl:find", tr("Tìm"), 0xE721, "🔍", false),
                    B("xl:replace", tr("Thay thế"), 0xE8AC, "⇄", false),
                }},
            }},
            {tr("Xem"), {view, {tr("Cố định ô"), {
                B(".uno:FreezePanes", tr("Cố định ô"), 0xE840, "❄", true),
                B(".uno:FreezePanesRow", tr("Hàng đầu"), 0xE840, "❄≡", false),
                B(".uno:FreezePanesColumn", tr("Cột đầu"), 0xE840, "❄‖", false),
            }}}},
        };
    }
    case DocKind::Text:
    {
        QList<Tab> r{fileTab};
        r += wordTabs(file, undo, clip, view);
        return r;
    }
    case DocKind::Presentation:
        return {fileTab,
            {tr("Trang đầu"), {file, undo, clip,
                {tr("Trang chiếu"), {
                    {QStringLiteral(".uno:InsertPage"), tr("Trang mới"), 0xE710, QStringLiteral("+"), false},
                    {QStringLiteral(".uno:DuplicatePage"), tr("Nhân bản"), 0xE8C8, QStringLiteral("⧉"), false},
                    {QStringLiteral(".uno:DeletePage"), tr("Xoá trang"), 0xE74D, QStringLiteral("−"), false},
                    {QStringLiteral(".uno:MovePageUp"), tr("Dời lên"), 0xE74A, QStringLiteral("↑"), false},
                    {QStringLiteral(".uno:MovePageDown"), tr("Dời xuống"), 0xE74B, QStringLiteral("↓"), false},
                    {QStringLiteral(".uno:HideSlide"), tr("Ẩn trang"), 0xED1A, QStringLiteral("⊘"), false},
                    {QStringLiteral(".uno:ShowSlide"), tr("Hiện trang"), 0xE7B3, QStringLiteral("◉"), false},
                }},
                {tr("Bố cục"), {
                    {QStringLiteral(".uno:AssignLayout#{\"WhatLayout\":{\"type\":\"long\",\"value\":\"0\"}}"), tr("Tiêu đề"), 0xE8A1, QStringLiteral("T"), false},
                    {QStringLiteral(".uno:AssignLayout#{\"WhatLayout\":{\"type\":\"long\",\"value\":\"20\"}}"), tr("Trống"), 0xE7C3, QStringLiteral("□"), false},
                }},
                font,
                {tr("Cỡ chữ"), {
                    {QStringLiteral(".uno:FontHeight#{\"FontHeight.Height\":{\"type\":\"float\",\"value\":\"18\"}}"), tr("Cỡ 18"), 0xE8E9, QStringLiteral("18"), false},
                    {QStringLiteral(".uno:FontHeight#{\"FontHeight.Height\":{\"type\":\"float\",\"value\":\"24\"}}"), tr("Cỡ 24"), 0xE8E9, QStringLiteral("24"), false},
                    {QStringLiteral(".uno:FontHeight#{\"FontHeight.Height\":{\"type\":\"float\",\"value\":\"32\"}}"), tr("Cỡ 32"), 0xE8E9, QStringLiteral("32"), false},
                    {QStringLiteral(".uno:FontHeight#{\"FontHeight.Height\":{\"type\":\"float\",\"value\":\"44\"}}"), tr("Cỡ 44"), 0xE8E9, QStringLiteral("44"), false},
                }},
                {tr("Màu chữ"), {
                    {QStringLiteral(".uno:Color#{\"Color\":{\"type\":\"long\",\"value\":\"0\"}}"), tr("Đen"), 0xE790, QStringLiteral("A"), false},
                    {QStringLiteral(".uno:Color#{\"Color\":{\"type\":\"long\",\"value\":\"16711680\"}}"), tr("Đỏ"), 0xE790, QStringLiteral("A"), false},
                    {QStringLiteral(".uno:Color#{\"Color\":{\"type\":\"long\",\"value\":\"2051007\"}}"), tr("Xanh"), 0xE790, QStringLiteral("A"), false},
                }},
                {tr("Đoạn văn"), {
                    {QStringLiteral(".uno:LeftPara"), tr("Trái"), 0xE8E4, QStringLiteral("⇤"), true},
                    {QStringLiteral(".uno:CenterPara"), tr("Giữa"), 0xE8E3, QStringLiteral("≡"), true},
                    {QStringLiteral(".uno:RightPara"), tr("Phải"), 0xE8E2, QStringLiteral("⇥"), true},
                    {QStringLiteral(".uno:DefaultBullet"), tr("Gạch đầu dòng"), 0xE8FD, QStringLiteral("•"), true},
                    {QStringLiteral(".uno:DefaultNumbering"), tr("Đánh số"), 0xE9D5, QStringLiteral("1."), true},
                }},
            }},
            {tr("Chèn"), {{tr("Đối tượng"), {
                    {QStringLiteral("ppt.textbox"), tr("Hộp văn bản"), 0xE8D2, QStringLiteral("T"), false},
                    {QStringLiteral("ppt.rect"), tr("Hình chữ nhật"), 0xE739, QStringLiteral("▭"), false},
                    {QStringLiteral("ppt.image"), tr("Ảnh"), 0xEB9F, QStringLiteral("🖼"), false},
                    {QStringLiteral(".uno:InsertTable#{\"Columns\":{\"type\":\"long\",\"value\":\"3\"},\"Rows\":{\"type\":\"long\",\"value\":\"2\"}}"), tr("Bảng 3×2"), 0xE80A, QStringLiteral("▦"), false},
                }}}},
            {tr("Trình chiếu"), {{tr("Trình chiếu"), {
                    {QStringLiteral("ppt.show"), tr("Từ đầu"), 0xE768, QStringLiteral("▶"), false},
                    {QStringLiteral("ppt.showHere"), tr("Từ trang này"), 0xE768, QStringLiteral("▷"), false},
                    {QStringLiteral("ppt.preview"), tr("Xem trước slide"), 0xE768, QStringLiteral("▷"), false},

                }}}},
            {tr("Hoạt ảnh"), {{tr("Xuất PPTX"), {
                {QStringLiteral("ppt.animationCopy"), tr("Thêm hoạt ảnh"), 0xE7B8, QStringLiteral("▷"), false},
                {QStringLiteral("ppt.clearAnimationCopy"), tr("Xóa hoạt ảnh"), 0xE74D, QStringLiteral("×"), false},
            }}}},
            {tr("Chuyển cảnh"), {{tr("Xuất PPTX"), {
                {QStringLiteral("ppt.transitionNote"), tr("Đặt chuyển cảnh"), 0xE7B8, QStringLiteral("▷"), false},
            }}}},
            {tr("Thiết kế"), {{tr("Bố cục"), {
                {QStringLiteral(".uno:AssignLayout#{\"WhatLayout\":{\"type\":\"long\",\"value\":\"0\"}}"), tr("Tiêu đề"), 0xE8A1, QStringLiteral("T"), false},
                {QStringLiteral(".uno:AssignLayout#{\"WhatLayout\":{\"type\":\"long\",\"value\":\"20\"}}"), tr("Trống"), 0xE7C3, QStringLiteral("□"), false},
            }}, {tr("Slide Master"), {
                {QStringLiteral("ppt.masterBackground"), tr("Đổi nền master"), 0xE8A1, QStringLiteral("▧"), false},
            }}, {tr("Thuộc tính chữ"), {
                {QStringLiteral("ppt.fontSize"), tr("Cỡ tùy chỉnh"), 0xE8E9, QStringLiteral("Aa"), false},
                {QStringLiteral("ppt.textColor"), tr("Màu tùy chỉnh"), 0xE790, QStringLiteral("A"), false},
            }}}},
            {tr("Xem"), {view, {tr("Chế độ xem"), {
                {QStringLiteral("ppt.normal"), tr("Trang chiếu"), 0xE7C3, QStringLiteral("□"), false},
                {QStringLiteral("ppt.notes"), tr("Ghi chú diễn giả"), 0xE8D2, QStringLiteral("T"), false},
                {QStringLiteral("ppt.goto"), tr("Đến trang"), 0xE7C3, QStringLiteral("#"), false},
            }}}},
            {tr("In"), {{tr("In & xuất"), {
                {QStringLiteral("print"), tr("In / Xem trước"), 0xE749, QStringLiteral("🖨"), false},
                {QStringLiteral("exportPdf"), tr("Xuất PDF"), 0xEA90, QStringLiteral("PDF"), false},
            }}}},
        };
    default: // PDF / bản vẽ
        return {fileTab, {tr("Trang đầu"), {file, view}}};
    }
}

QWidget* Ribbon::buildPage(const Tab& t) {
    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;} QScrollArea > QWidget > QWidget{background:transparent;}"));
    scroll->horizontalScrollBar()->setFixedHeight(6);
    auto* page = new QWidget;
    auto* h = new QHBoxLayout(page);
    h->setContentsMargins(0, 4, 0, 0);
    h->setSpacing(6);
    auto groups=t.groups;
    // Put font controls first on Home so they remain visible on smaller screens.
    if(t.title==tr("Trang đầu")&&!m_lastReadOnly) {
        for(int i=0;i<groups.size();++i)
            if(groups[i].title==tr("Phông chữ")) { groups.move(i,0);break; }
    }
    for (int gi = 0; gi < groups.size(); ++gi) {
        const Group& g = groups[gi];
        auto* gw = new QWidget(page);
        auto* gv = new QVBoxLayout(gw);
        gv->setContentsMargins(0, 0, 0, 0);
        gv->setSpacing(0);
        const bool fontGroup=g.title==tr("Phông chữ")&&!m_lastReadOnly &&
            (m_lastKind==int(DocKind::Text)||m_lastKind==int(DocKind::Spreadsheet)||m_lastKind==int(DocKind::Presentation));
        if(fontGroup) {
            auto* controls=new QHBoxLayout;
            controls->setSpacing(3);
            auto* family=new QComboBox(gw);family->setObjectName(QStringLiteral("RibbonFontFamily"));
            // Some platform backends (notably Windows offscreen) cannot
            // enumerate fonts. Keep the application default selectable without
            // inventing a list of installed fonts.
            QStringList families=QFontDatabase::families();
            if(families.isEmpty()) {
                const QString defaultFamily=QGuiApplication::font().family().trimmed();
                families.append(defaultFamily.isEmpty()?QStringLiteral("sans-serif"):defaultFamily);
            }
            family->addItems(families);
            family->setPlaceholderText(tr("Chọn phông"));
            family->setCurrentIndex(-1);family->setFixedWidth(158);
            family->setToolTip(tr("Phông chữ"));family->setAccessibleName(tr("Phông chữ"));
            auto* points=new QComboBox(gw);points->setObjectName(QStringLiteral("RibbonFontSize"));
            points->setPlaceholderText(tr("Cỡ"));points->setFixedWidth(65);
            for(double pt:dung::word::commonFontSizes())points->addItem(QString::number(pt));
            points->setCurrentIndex(-1);points->setToolTip(tr("Cỡ chữ (pt)"));points->setAccessibleName(tr("Cỡ chữ"));
            m_fontBox=family;m_sizeBox=points;
            const bool excel=m_lastKind==int(DocKind::Spreadsheet);
            connect(family,qOverload<int>(&QComboBox::activated),this,[this,family,excel](int index){
                if(index<0||family->itemText(index).isEmpty())return;
                const auto name=family->itemText(index).toUtf8().toStdString();
                const auto json=excel?dung::excel::fontName(name):dung::word::fontName(name).second;
                emit unoCommand(QStringLiteral(".uno:CharFontName#")+QString::fromStdString(json));
            });
            connect(points,qOverload<int>(&QComboBox::activated),this,[this,points,excel](int index){
                bool ok=false;const double pt=points->itemText(index).toDouble(&ok);
                if(!ok||pt<1||pt>400)return;
                const auto json=excel?dung::excel::fontHeight(qRound(pt)):dung::word::fontSize(pt).second;
                emit unoCommand(QStringLiteral(".uno:FontHeight#")+QString::fromStdString(json));
            });
            controls->addWidget(family);controls->addWidget(points);controls->addStretch(1);
            gv->addLayout(controls);
        }
        auto* row = new QGridLayout;
        row->setContentsMargins(0,0,0,0);row->setSpacing(2);
        int buttonIndex=0;
        const int rows=fontGroup?1:(g.buttons.size()>3?2:1);
        for (const Btn& b : g.buttons) {
            if(fontGroup && b.id==QLatin1String("word:font"))continue;
            if(m_lastKind==int(DocKind::Spreadsheet)&&b.id==QLatin1String("xl:fontName"))continue;
            auto* tb = new QToolButton(gw);
            tb->setObjectName(QStringLiteral("RibbonButton"));
            tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
            tb->setCheckable(b.checkable);
            tb->setAutoRaise(true);
            tb->setFocusPolicy(Qt::NoFocus); // không lấy focus khỏi tài liệu
            tb->setIcon(dung::ui::icon(dung::ui::iconNameForCommand(b.id), theme::kBlueDark));
            tb->setIconSize(QSize(22, 22));
            tb->setText(b.label);
            tb->setToolTip(b.label);
            tb->setAccessibleName(b.label);
            tb->setFixedSize(32,29);
            const QString id = b.id;
            connect(tb, &QToolButton::clicked, this, [this, id, tb] {
                if (id.startsWith(QLatin1String(".uno:"))) {
                    if (tb->isCheckable()) tb->setChecked(!tb->isChecked()); // trạng thái thật do engine báo lại
                    emit unoCommand(id);
                } else {
                    emit appAction(id);
                }
            });
            m_buttons.insert(id, tb);
            row->addWidget(tb,buttonIndex%rows,buttonIndex/rows);
            ++buttonIndex;
        }
        gv->addLayout(row);gv->addStretch(1);
        auto* lbl = new QLabel(g.title, gw);
        lbl->setObjectName(QStringLiteral("GroupLabel"));
        lbl->setAlignment(Qt::AlignHCenter);
        gv->addWidget(lbl);
        h->addWidget(gw);
        if (gi + 1 < groups.size()) {
            auto* sep = new QFrame(page);
            sep->setObjectName(QStringLiteral("GroupSep"));
            sep->setFrameShape(QFrame::VLine);
            h->addWidget(sep);
        }
    }
    h->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

void Ribbon::setKind(DocKind kind, bool readOnly) {
    if (m_lastKind == (int)kind && m_lastReadOnly == readOnly) return;
    m_lastKind = (int)kind;
    m_lastReadOnly = readOnly;
    {   // màu nhấn theo ứng dụng: QSS chọn theo thuộc tính "kind"
        const char* k = kind == DocKind::Spreadsheet ? "excel" : kind == DocKind::Presentation ? "ppt"
                      : kind == DocKind::Drawing ? "pdf" : "word";
        setProperty("kind", QString::fromLatin1(k));
        style()->unpolish(this);
        style()->polish(this);
    }
    while (m_tabs->count()) m_tabs->removeTab(0);
    while (m_pages->count()) {
        QWidget* w = m_pages->widget(0);
        m_pages->removeWidget(w);
        w->deleteLater();
    }
    m_buttons.clear();m_fontBox=nullptr;m_sizeBox=nullptr;
    for (const Tab& t : tabsFor(kind, readOnly)) {
        m_tabs->addTab(t.title);
        m_pages->addWidget(buildPage(t));
    }
    // Lần đầu mở tab "Trang đầu" (chỉ số 1, sau tab "Tệp")
    m_tabs->setCurrentIndex(qMin(1, m_tabs->count()-1));
}

void Ribbon::setCommandState(const QString& cmd, const QString& value) {
    QComboBox* selector=cmd==QLatin1String(".uno:CharFontName")?m_fontBox.data():
        cmd==QLatin1String(".uno:FontHeight")?m_sizeBox.data():nullptr;
    if(selector) {
        QSignalBlocker blocker(selector);
        if(value==QLatin1String("disabled")) selector->setEnabled(false);
        else {
            selector->setEnabled(true);
            if(value!=QLatin1String("enabled")) {
                QString text=value.trimmed();
                bool addMissing=false;
                if(selector==m_sizeBox.data()) {
                    bool ok=false;const double pt=text.toDouble(&ok);
                    if(ok&&pt>=1&&pt<=400){text=QString::number(pt);addMissing=true;}
                } else addMissing=!text.isEmpty();
                // Most caret movements repeat the current font/size. Skip the
                // linear model search only if the actual selected item matches.
                // Never cache engine acknowledgements instead of widget state.
                const bool same=selector->currentIndex()>=0&&selector->currentText()==text;
                if(!same) {
                    int index=selector->findText(text,Qt::MatchFixedString);
                    if(index<0&&addMissing) {
                        selector->addItem(text);
                        index=selector->count()-1;
                    }
                    if(selector->currentIndex()!=index)selector->setCurrentIndex(index);
                }
            }
        }
    }
    auto it = m_buttons.find(cmd);
    if (it == m_buttons.end() || !it.value()) return;
    QToolButton* b = it.value();
    if (value == QLatin1String("disabled")) b->setEnabled(false);
    else if (value == QLatin1String("enabled")) b->setEnabled(true);
    else if (b->isCheckable()) { b->setEnabled(true); b->setChecked(value == QLatin1String("true")); }
}

void Ribbon::resetStates(const QHash<QString, QString>& states) {
    for(auto* box:{m_fontBox.data(),m_sizeBox.data()})if(box){QSignalBlocker block(box);box->setCurrentIndex(-1);box->setEnabled(true);}
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
        if (it.value() && it.value()->isCheckable()) it.value()->setChecked(false);
    for (auto it = states.begin(); it != states.end(); ++it) setCommandState(it.key(), it.value());
}

QStringList Ribbon::commandIds() const {
    QStringList ids;
    for (DocKind k : {DocKind::Text, DocKind::Spreadsheet, DocKind::Presentation, DocKind::Drawing})
        for (bool ro : {false, true})
            for (const Tab& t : tabsFor(k, ro))
                for (const Group& g : t.groups)
                    for (const Btn& b : g.buttons)
                        if (!ids.contains(b.id)) ids << b.id;
    return ids;
}
