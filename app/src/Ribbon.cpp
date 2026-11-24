// SPDX-License-Identifier: MPL-2.0
#include "Ribbon.h"

#include "Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

using dung::DocKind;

// Vẽ biểu tượng từ glyph của font icon (Segoe Fluent/MDL2) hoặc ký tự thay thế.
static QIcon glyphIcon(const QString& glyph, bool iconFont, qreal dpr) {
    const int px = 22;
    QPixmap pm(QSize(px, px) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f = iconFont ? QFont(theme::iconFontFamily()) : QFont(QStringLiteral("Segoe UI"));
    f.setPixelSize(iconFont ? 18 : (glyph.size() > 2 ? 10 : 15));
    f.setBold(!iconFont);
    p.setFont(f);
    p.setPen(theme::kBlueDark);
    p.drawText(QRect(0, 0, px, px), Qt::AlignCenter, glyph);
    p.end();
    QIcon icon(pm);
    // Trạng thái vô hiệu: màu xám
    QPixmap dis(pm.size());
    dis.setDevicePixelRatio(dpr);
    dis.fill(Qt::transparent);
    QPainter d(&dis);
    d.setFont(f);
    d.setPen(QColor(0xA0, 0xAE, 0xC0));
    d.drawText(QRect(0, 0, px, px), Qt::AlignCenter, glyph);
    d.end();
    icon.addPixmap(dis, QIcon::Disabled);
    return icon;
}

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
    m_pages->setFixedHeight(94);
    v->addWidget(m_tabs);
    v->addWidget(m_pages);
    connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
}

QList<Ribbon::Tab> Ribbon::tabsFor(DocKind kind, bool ro) const {
    // Biểu tượng: mã Segoe MDL2/Fluent Icons + ký tự thay thế khi không có font biểu tượng.
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
        {tr("Tệp"), {
            {QStringLiteral("open"), tr("Mở"), 0xE8E5, QStringLiteral("📂"), false},
            {QStringLiteral("close"), tr("Đóng"), 0xE711, QStringLiteral("✕"), false},
        }},
    }};
    const Group clip{tr("Bộ nhớ tạm"), {
        {QStringLiteral("paste"), tr("Dán"), 0xE77F, QStringLiteral("📋"), false},
        {QStringLiteral("cut"), tr("Cắt"), 0xE8C6, QStringLiteral("✂"), false},
        {QStringLiteral("copy"), tr("Sao chép"), 0xE8C8, QStringLiteral("⧉"), false},
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

    if (ro) {
        // Chế độ chỉ đọc: chỉ còn các thao tác không sửa nội dung.
        return {fileTab, {tr("Trang đầu"), {file, {tr("Bộ nhớ tạm"), {{QStringLiteral("copy"), tr("Sao chép"), 0xE8C8, QStringLiteral("⧉"), false}}}, view}}};
    }

    switch (kind) {
    case DocKind::Spreadsheet:
        return {fileTab,
            {tr("Trang đầu"), {file, undo, clip, font,
                {tr("Căn lề"), {
                    {QStringLiteral(".uno:AlignLeft"), tr("Trái"), 0xE8E4, QStringLiteral("⇤"), true},
                    {QStringLiteral(".uno:AlignHorizontalCenter"), tr("Giữa"), 0xE8E3, QStringLiteral("↔"), true},
                    {QStringLiteral(".uno:AlignRight"), tr("Phải"), 0xE8E2, QStringLiteral("⇥"), true},
                    {QStringLiteral(".uno:ToggleMergeCells"), tr("Gộp ô"), 0xE8B9, QStringLiteral("⊞"), true},
                    {QStringLiteral(".uno:WrapText"), tr("Xuống dòng"), 0xE751, QStringLiteral("↩"), true},
                }},
                {tr("Số"), {
                    {QStringLiteral(".uno:NumberFormatCurrency"), tr("Tiền tệ"), 0xE8C7, QStringLiteral("₫"), true},
                    {QStringLiteral(".uno:NumberFormatPercent"), tr("Phần trăm"), 0xE94C, QStringLiteral("%"), true},
                    {QStringLiteral(".uno:NumberFormatIncDecimals"), tr("Thêm số lẻ"), 0xE948, QStringLiteral(".0+"), false},
                    {QStringLiteral(".uno:NumberFormatDecDecimals"), tr("Bớt số lẻ"), 0xE949, QStringLiteral(".0−"), false},
                }},
            }},
            {tr("Chèn"), {
                {tr("Hàng & cột"), {
                    {QStringLiteral(".uno:InsertRowsBefore"), tr("Chèn hàng"), 0xE710, QStringLiteral("+≡"), false},
                    {QStringLiteral(".uno:InsertColumnsBefore"), tr("Chèn cột"), 0xE710, QStringLiteral("+‖"), false},
                    {QStringLiteral(".uno:DeleteRows"), tr("Xoá hàng"), 0xE74D, QStringLiteral("−≡"), false},
                    {QStringLiteral(".uno:DeleteColumns"), tr("Xoá cột"), 0xE74D, QStringLiteral("−‖"), false},
                }},
            }},
            {tr("Dữ liệu"), {
                {tr("Tính toán"), {
                    {QStringLiteral(".uno:AutoSum"), tr("Tổng"), 0xE8EF, QStringLiteral("Σ"), false},
                }},
                {tr("Sắp xếp & lọc"), {
                    {QStringLiteral(".uno:SortAscending"), tr("Tăng dần"), 0xE8CB, QStringLiteral("A↓"), false},
                    {QStringLiteral(".uno:SortDescending"), tr("Giảm dần"), 0xE8CB, QStringLiteral("Z↓"), false},
                    {QStringLiteral(".uno:DataFilterAutoFilter"), tr("Bộ lọc"), 0xE71C, QStringLiteral("⏷"), true},
                }},
            }},
            {tr("Xem"), {view, {tr("Cửa sổ"), {
                {QStringLiteral(".uno:FreezePanes"), tr("Cố định ô"), 0xE840, QStringLiteral("❄"), true},
            }}}},
        };
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
                    {QStringLiteral("ppt.transitionNote"), tr("Hiệu ứng"), 0xE7B8, QStringLiteral("ⓘ"), false},
                }}}},
            {tr("Xem"), {view}},
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
    const QString iconFont = theme::iconFontFamily();
    for (int gi = 0; gi < t.groups.size(); ++gi) {
        const Group& g = t.groups[gi];
        auto* gw = new QWidget(page);
        auto* gv = new QVBoxLayout(gw);
        gv->setContentsMargins(0, 0, 0, 0);
        gv->setSpacing(0);
        auto* row = new QHBoxLayout;
        row->setSpacing(2);
        for (const Btn& b : g.buttons) {
            auto* tb = new QToolButton(gw);
            tb->setObjectName(QStringLiteral("RibbonButton"));
            tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            tb->setCheckable(b.checkable);
            tb->setAutoRaise(true);
            tb->setFocusPolicy(Qt::NoFocus); // không lấy focus khỏi tài liệu
            const QString g1 = iconFont.isEmpty() ? b.fallback : QString(QChar(b.mdl2));
            tb->setIcon(glyphIcon(g1, !iconFont.isEmpty(), devicePixelRatioF()));
            tb->setIconSize(QSize(22, 22));
            tb->setText(b.label);
            tb->setToolTip(b.label);
            tb->setMinimumHeight(56);
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
            row->addWidget(tb);
        }
        gv->addLayout(row);
        auto* lbl = new QLabel(g.title, gw);
        lbl->setObjectName(QStringLiteral("GroupLabel"));
        lbl->setAlignment(Qt::AlignHCenter);
        gv->addWidget(lbl);
        h->addWidget(gw);
        if (gi + 1 < t.groups.size()) {
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
    const int current = m_tabs->currentIndex();
    while (m_tabs->count()) m_tabs->removeTab(0);
    while (m_pages->count()) {
        QWidget* w = m_pages->widget(0);
        m_pages->removeWidget(w);
        w->deleteLater();
    }
    m_buttons.clear();
    for (const Tab& t : tabsFor(kind, readOnly)) {
        m_tabs->addTab(t.title);
        m_pages->addWidget(buildPage(t));
    }
    // Lần đầu mở tab "Trang đầu" (chỉ số 1, sau tab "Tệp")
    m_tabs->setCurrentIndex(std::clamp(current < 0 ? 1 : current, 0, m_tabs->count() - 1));
}

void Ribbon::setCommandState(const QString& cmd, const QString& value) {
    auto it = m_buttons.find(cmd);
    if (it == m_buttons.end() || !it.value()) return;
    QToolButton* b = it.value();
    if (value == QLatin1String("disabled")) b->setEnabled(false);
    else if (value == QLatin1String("enabled")) b->setEnabled(true);
    else if (b->isCheckable()) { b->setEnabled(true); b->setChecked(value == QLatin1String("true")); }
}

void Ribbon::resetStates(const QHash<QString, QString>& states) {
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
        if (it.value() && it.value()->isCheckable()) it.value()->setChecked(false);
    for (auto it = states.begin(); it != states.end(); ++it) setCommandState(it.key(), it.value());
}
