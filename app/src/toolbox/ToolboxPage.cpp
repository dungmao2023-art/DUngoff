// SPDX-License-Identifier: MPL-2.0
#include "ToolboxPage.h"

#include "ExternalTool.h"
#include "ExternalToolWidget.h"
#include "ToolRegistry.h"

#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
// Tool ngoài đã quét, giữ để tạo giao diện khi người dùng mở (id -> spec)
QMap<QString, toolbox::ToolSpec>& externals() {
    static QMap<QString, toolbox::ToolSpec> m;
    return m;
}
} // namespace

ToolboxPage::ToolboxPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 16);
    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack);

    // ---- trang danh sách ----
    m_listPage = new QWidget(m_stack);
    auto* lv = new QVBoxLayout(m_listPage);
    lv->setContentsMargins(0, 0, 0, 0);
    auto* title = new QLabel(tr("Công cụ"), m_listPage);
    title->setStyleSheet(QStringLiteral("font-size:22px;font-weight:700;color:#1C2940;"));
    lv->addWidget(title);
    auto* row = new QHBoxLayout();
    m_search = new QLineEdit(m_listPage);
    m_search->setPlaceholderText(tr("Tìm công cụ… (gõ không dấu cũng được)"));
    m_search->setClearButtonEnabled(true);
    auto* rescan = new QPushButton(tr("Quét lại"), m_listPage);
    auto* openDir = new QPushButton(tr("Mở thư mục tool ngoài"), m_listPage);
    row->addWidget(m_search, 1);
    row->addWidget(rescan);
    row->addWidget(openDir);
    lv->addLayout(row);
    m_errors = new QLabel(m_listPage);
    m_errors->setWordWrap(true);
    m_errors->setStyleSheet(QStringLiteral("background:#FFF7E6;color:#7A5B00;border:1px solid #F0DDAD;border-radius:8px;padding:6px 10px;"));
    m_errors->hide();
    lv->addWidget(m_errors);
    m_scroll = new QScrollArea(m_listPage);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    lv->addWidget(m_scroll, 1);
    m_stack->addWidget(m_listPage);

    // ---- trang một công cụ ----
    m_toolPage = new QWidget(m_stack);
    m_toolLayout = new QVBoxLayout(m_toolPage);
    m_toolLayout->setContentsMargins(0, 0, 0, 0);
    auto* top = new QHBoxLayout();
    auto* back = new QPushButton(tr("← Công cụ"), m_toolPage);
    m_toolTitle = new QLabel(m_toolPage);
    m_toolTitle->setStyleSheet(QStringLiteral("font-size:20px;font-weight:700;color:#1C2940;"));
    top->addWidget(back);
    top->addWidget(m_toolTitle, 1);
    m_toolLayout->addLayout(top);
    m_stack->addWidget(m_toolPage);

    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuild(); });
    connect(rescan, &QPushButton::clicked, this, &ToolboxPage::reload);
    connect(openDir, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(toolbox::toolsRoot())); });
    connect(back, &QPushButton::clicked, this, &ToolboxPage::closeTool);
    reload();
}

void ToolboxPage::reload() {
    // Tool ngoài nằm trong bộ nhớ riêng của trang (không đăng ký vào sổ chung để quét lại không để lại bản cũ)
    externals().clear();
    const toolbox::ScanResult sr = toolbox::scan(toolbox::toolsRoot());
    for (const auto& s : sr.tools) externals().insert(s.id, s);
    if (sr.errors.isEmpty()) {
        m_errors->hide();
    } else {
        m_errors->setText(tr("Có %1 tool ngoài bị bỏ qua:\n• %2").arg(sr.errors.size()).arg(sr.errors.join(QStringLiteral("\n• "))));
        m_errors->show();
    }
    rebuild();
}

void ToolboxPage::rebuild() {
    // gộp công cụ nội bộ (đã lọc theo ô tìm kiếm) với tool ngoài
    QList<toolbox::ToolInfo> list = toolbox::search(m_search->text());
    const QString q = toolbox::foldVn(m_search->text());
    for (const auto& s : externals()) {
        const QString hay = toolbox::foldVn(s.name + QLatin1Char(' ') + s.description + QLatin1Char(' ') + s.category);
        bool all = true;
        for (const QString& w : q.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            if (!hay.contains(w)) { all = false; break; }
        if (!all) continue;
        toolbox::ToolInfo ti;
        ti.id = QStringLiteral("ngoai:") + s.id;
        ti.name = s.name;
        ti.description = s.description;
        ti.category = s.category;
        ti.iconText = QStringLiteral("⚙");
        ti.external = true;
        list << ti;
    }
    QStringList cats;
    for (const auto& t : list)
        if (!cats.contains(t.category)) cats << t.category;
    cats.sort(Qt::CaseInsensitive);

    auto* body = new QWidget();
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(0, 8, 0, 8);
    if (list.isEmpty()) {
        auto* none = new QLabel(m_search->text().isEmpty() ? tr("Chưa có công cụ nào.") : tr("Không có công cụ khớp \"%1\".").arg(m_search->text()), body);
        none->setStyleSheet(QStringLiteral("color:#697992;"));
        bv->addWidget(none);
    }
    for (const QString& c : cats) {
        auto* h = new QLabel(c, body);
        h->setStyleSheet(QStringLiteral("font-size:14px;font-weight:700;color:#275DD6;margin-top:10px;"));
        bv->addWidget(h);
        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(10);
        int i = 0;
        for (const auto& t : list) {
            if (t.category != c) continue;
            auto* card = new QToolButton(body);
            card->setToolButtonStyle(Qt::ToolButtonTextOnly);
            card->setText(t.iconText + QStringLiteral("  ") + t.name + (t.description.isEmpty() ? QString() : QStringLiteral("\n") + t.description));
            card->setCursor(Qt::PointingHandCursor);
            card->setMinimumSize(210, 74);
            card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            card->setStyleSheet(QStringLiteral("QToolButton{background:white;border:1px solid #D9E1EC;border-radius:10px;padding:10px;text-align:left;color:#1E293B;}"
                                               "QToolButton:hover{border-color:#275DD6;background:#F3F7FF;}"));
            const QString id = t.id;
            connect(card, &QToolButton::clicked, this, [this, id] { openTool(id); });
            grid->addWidget(card, i / 3, i % 3);
            ++i;
        }
        bv->addLayout(grid);
    }
    bv->addStretch(1);
    m_scroll->setWidget(body); // QScrollArea giải phóng widget cũ
    m_listBody = body;
}

void ToolboxPage::openTool(const QString& id) {
    closeTool();
    QWidget* w = nullptr;
    QString name;
    if (id.startsWith(QStringLiteral("ngoai:"))) {
        const auto it = externals().constFind(id.mid(6));
        if (it == externals().constEnd()) return;
        w = new toolbox::ExternalToolWidget(it.value(), m_toolPage);
        name = it.value().name;
    } else {
        toolbox::ToolInfo ti;
        if (!toolbox::find(id, &ti) || !ti.create) return;
        w = ti.create(m_toolPage);
        name = ti.name;
    }
    if (!w) return;
    m_toolWidget = w;
    m_toolTitle->setText(name);
    m_toolLayout->addWidget(w, 1);
    m_stack->setCurrentWidget(m_toolPage);
}

void ToolboxPage::closeTool() {
    if (m_toolWidget) {
        m_toolLayout->removeWidget(m_toolWidget);
        delete m_toolWidget; // huỷ widget: ToolRunner của tool ngoài tự dừng tiến trình đang chạy
        m_toolWidget = nullptr;
    }
    m_stack->setCurrentWidget(m_listPage);
}
