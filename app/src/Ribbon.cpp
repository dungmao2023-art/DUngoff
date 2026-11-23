// SPDX-License-Identifier: MPL-2.0
#include "Ribbon.h"

#include "Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
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
    m_pages->setFixedHeight(78);
    v->addWidget(m_tabs);
    v->addWidget(m_pages);
    connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
}

QList<Ribbon::Tab> Ribbon::tabsFor(DocKind kind, bool ro) const {
    // Biểu tượng: mã Segoe MDL2/Fluent Icons + ký tự thay thế khi không có font biểu tượng.
    const Group file{tr("Tệp"), {
        {QStringLiteral("save"), tr("Lưu"), 0xE74E, QStringLiteral("💾"), false},
        {QStringLiteral("saveAs"), tr("Lưu thành"), 0xE792, QStringLiteral("⇩"), false},
        {QStringLiteral("exportPdf"), tr("Xuất PDF"), 0xEA90, QStringLiteral("PDF"), false},
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
        return {{tr("Trang đầu"), {file, {tr("Bộ nhớ tạm"), {{QStringLiteral("copy"), tr("Sao chép"), 0xE8C8, QStringLiteral("⧉"), false}}}, view}}};
    }

    switch (kind) {
    case DocKind::Spreadsheet:
        return {
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
                {tr("Ô"), {
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
        return {
            {tr("Trang đầu"), {file, undo, clip, font,
                {tr("Đoạn văn"), {
                    {QStringLiteral(".uno:LeftPara"), tr("Trái"), 0xE8E4, QStringLiteral("⇤"), true},
                    {QStringLiteral(".uno:CenterPara"), tr("Giữa"), 0xE8E3, QStringLiteral("↔"), true},
                    {QStringLiteral(".uno:RightPara"), tr("Phải"), 0xE8E2, QStringLiteral("⇥"), true},
                    {QStringLiteral(".uno:JustifyPara"), tr("Đều"), 0xE8E1, QStringLiteral("≡"), true},
                    {QStringLiteral(".uno:DefaultBullet"), tr("Gạch đầu dòng"), 0xE8FD, QStringLiteral("•"), true},
                    {QStringLiteral(".uno:DefaultNumbering"), tr("Đánh số"), 0xE9D5, QStringLiteral("1."), true},
                }},
            }},
            {tr("Chèn"), {{tr("Trang"), {
                {QStringLiteral(".uno:InsertPagebreak"), tr("Ngắt trang"), 0xE7C3, QStringLiteral("⤓"), false},
            }}}},
            {tr("Xem"), {view}},
        };
    case DocKind::Presentation:
        return {
            {tr("Trang đầu"), {file, undo, clip,
                {tr("Trang chiếu"), {
                    {QStringLiteral(".uno:InsertPage"), tr("Trang mới"), 0xE710, QStringLiteral("+"), false},
                    {QStringLiteral(".uno:DuplicatePage"), tr("Nhân bản"), 0xE8C8, QStringLiteral("⧉"), false},
                    {QStringLiteral(".uno:DeletePage"), tr("Xoá trang"), 0xE74D, QStringLiteral("−"), false},
                }},
                font,
            }},
            {tr("Xem"), {view}},
        };
    default: // PDF / bản vẽ
        return {{tr("Trang đầu"), {file, view}}};
    }
}

QWidget* Ribbon::buildPage(const Tab& t) {
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
            tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
            tb->setCheckable(b.checkable);
            tb->setAutoRaise(true);
            tb->setFocusPolicy(Qt::NoFocus); // không lấy focus khỏi tài liệu
            const QString g1 = iconFont.isEmpty() ? b.fallback : QString(QChar(b.mdl2));
            tb->setText(g1 + QLatin1Char('\n') + b.label);
            if (!iconFont.isEmpty()) {
                // Dòng biểu tượng dùng font icon; dòng chữ dùng font giao diện (Qt chọn fallback theo ký tự).
                QFont f = tb->font();
                f.setFamilies({QStringLiteral("Segoe UI"), iconFont});
                tb->setFont(f);
            }
            tb->setToolTip(b.label);
            tb->setMinimumHeight(52);
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
    return page;
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
    m_tabs->setCurrentIndex(std::clamp(current, 0, m_tabs->count() - 1));
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
