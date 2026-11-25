// SPDX-License-Identifier: MPL-2.0
#include "../Theme.h"
#include "../ui/Icons.h"
#include "ToolboxPage.h"

#include "ExternalTool.h"
#include "ExternalToolWidget.h"
#include "ToolRegistry.h"

#include <QDesktopServices>
#include <QFrame>
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
#include <QKeyEvent>
#include <QMouseEvent>
#include <functional>

namespace {
// Thẻ công cụ: cả khung là nút (chuột, Enter, Space).
class ToolCard : public QFrame {
public:
    explicit ToolCard(QWidget* parent) : QFrame(parent) {
        setObjectName(QStringLiteral("ToolCard"));
        setCursor(Qt::PointingHandCursor);
        setMinimumSize(210, 84);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAttribute(Qt::WA_Hover, true);
        setFocusPolicy(Qt::StrongFocus);
        setStyleSheet(QStringLiteral("QFrame#ToolCard{background:white;border:1px solid #DCE5F2;border-radius:14px;}"
                                     "QFrame#ToolCard:hover,QFrame#ToolCard:focus{border-color:#2864BA;background:#FAFCFF;}"
                                     "QFrame#ToolCard QLabel{background:transparent;border:none;}"));
    }
    std::function<void()> onClick;
protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->pos()) && onClick) onClick();
    }
    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) && onClick) onClick();
        else QFrame::keyPressEvent(e);
    }
};
}  // namespace

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
        ti.iconText = QStringLiteral("settings");
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
    auto addCard = [&](QGridLayout* grid, int i, const toolbox::ToolInfo& t) {
        auto* wrap = new QWidget(body);
        auto* wl = new QGridLayout(wrap);
        wl->setContentsMargins(0, 0, 0, 0);
        const QString id = t.id;
        auto* card = new ToolCard(wrap);  // khung bấm được: tên + mô tả tự xuống dòng, không bị cắt chữ
        card->onClick = [this, id] { openTool(id); };
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(14, 10, 34, 10);
        cl->setSpacing(2);
        const QIcon tic = dung::ui::iconOrEmpty(t.iconText, theme::kBlue);
        auto* nm = new QLabel(card);
        if (!tic.isNull()) {  // icon vector chung đặt cạnh tên
            auto* row = new QHBoxLayout;
            auto* il = new QLabel(card);
            il->setPixmap(tic.pixmap(QSize(22, 22)));
            il->setFixedSize(24, 24);
            row->addWidget(il);
            nm->setText(t.name);
            row->addWidget(nm, 1);
            cl->addLayout(row);
        } else {
            nm->setText(t.iconText + QStringLiteral("  ") + t.name);
            cl->addWidget(nm);
        }
        nm->setStyleSheet(QStringLiteral("font-weight:700;font-size:13px;color:#17294D;"));
        nm->setWordWrap(true);
        if (!t.description.isEmpty()) {
            auto* ds = new QLabel(t.description, card);
            ds->setStyleSheet(QStringLiteral("font-size:11px;color:#6B7A99;"));
            ds->setWordWrap(true);
            cl->addWidget(ds);
        }
        cl->addStretch(1);
        wl->addWidget(card, 0, 0);
        auto* pin = new QToolButton(wrap);
        const bool pinned = toolbox::isPinned(id);
        pin->setText(pinned ? QStringLiteral("★") : QStringLiteral("☆"));
        pin->setToolTip(pinned ? tr("Bỏ ghim") : tr("Ghim lên đầu trang Công cụ"));
        pin->setCursor(Qt::PointingHandCursor);
        pin->setAutoRaise(true);
        pin->setStyleSheet(QStringLiteral("QToolButton{border:none;font-size:16px;color:#275DD6;background:transparent;}"));
        connect(pin, &QToolButton::clicked, this, [this, id, pinned] { toolbox::setPinned(id, !pinned); rebuild(); });
        wl->addWidget(pin, 0, 0, Qt::AlignTop | Qt::AlignRight);
        grid->addWidget(wrap, i / 3, i % 3);
    };
    QList<toolbox::ToolInfo> pinnedList;
    for (const QString& pid : toolbox::pinnedIds())
        for (const auto& t : list)
            if (t.id == pid) pinnedList << t;
    if (!pinnedList.isEmpty()) {
        auto* h = new QLabel(tr("★ Đã ghim"), body);
        h->setStyleSheet(QStringLiteral("font-size:14px;font-weight:700;color:#275DD6;margin-top:10px;"));
        bv->addWidget(h);
        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(10);
        int i = 0;
        for (const auto& t : pinnedList) addCard(grid, i++, t);
        bv->addLayout(grid);
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
            addCard(grid, i++, t);
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
