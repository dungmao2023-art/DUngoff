// SPDX-License-Identifier: MPL-2.0
#include "HomePage.h"

#include "Theme.h"
#include "shell/ShellIcons.h"

#include <functional>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

// Thẻ bấm được: toàn bộ khung là nút (chuột + Enter/Space khi có focus).
class ClickCard : public QFrame {
public:
    explicit ClickCard(const QString& objectName, QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName(objectName);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover, true);
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

// Hình minh hoạ trang tài liệu / bảng tính trong băng chào (vẽ bằng nét, không dùng ảnh ngoài).
class HeroArt : public QWidget {
public:
    explicit HeroArt(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setMinimumSize(260, 150);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal w = width(), h = height();
        auto sheet = [&](QRectF r, qreal rot, const QColor& line) {
            p.save();
            p.translate(r.center());
            p.rotate(rot);
            QRectF rr(-r.width() / 2, -r.height() / 2, r.width(), r.height());
            p.setPen(QPen(QColor(0xD5, 0xE3, 0xFA), 1));
            p.setBrush(QColor(255, 255, 255, 235));
            p.drawRoundedRect(rr, 14, 14);
            p.setPen(Qt::NoPen);
            p.setBrush(line);
            for (int i = 0; i < 4; ++i)
                p.drawRoundedRect(QRectF(rr.left() + 22, rr.top() + 56 + i * 20, rr.width() - 44 - (i == 3 ? 40 : 0), 8), 4, 4);
            p.restore();
        };
        sheet(QRectF(w * 0.78, h * 0.64, w * 0.30, h * 0.96), 6, QColor(0xE9, 0xD5, 0xD0));
        sheet(QRectF(w * 0.62, h * 0.58, w * 0.34, h * 1.0), 4, QColor(0xCF, 0xE3, 0xD8));
        sheet(QRectF(w * 0.40, h * 0.62, w * 0.34, h * 1.0), -5, QColor(0xCF, 0xDD, 0xF3));
        // thẻ nhỏ W, X
        QPixmap bw = shell::badge(QStringLiteral("W"), QColor(0xE3, 0xED, 0xFF), theme::kWord, 36, devicePixelRatioF());
        QPixmap bx = shell::badge(QStringLiteral("X"), QColor(0xDC, 0xF2, 0xE6), theme::kExcel, 36, devicePixelRatioF());
        p.drawPixmap(QPointF(w * 0.40 - 6, h * 0.10), bw);
        p.drawPixmap(QPointF(w * 0.62 + 4, h * 0.02), bx);
    }
};

QFrame* makeFeatureCard(const QString& letter, const QColor& accent, const QColor& bg, const QString& title,
                        const QString& desc, const QString& link, std::function<void()> fn, QWidget* parent) {
    auto* c = new ClickCard(QStringLiteral("FeatureCard"), parent);
    c->onClick = std::move(fn);
    c->setMinimumHeight(196);
    auto* v = new QVBoxLayout(c);
    v->setContentsMargins(22, 20, 22, 18);
    v->setSpacing(6);
    auto* top = new QHBoxLayout;
    auto* ic = new QLabel(c);
    ic->setPixmap(shell::badge(letter, bg, accent, 46, c->devicePixelRatioF()));
    ic->setFixedSize(46, 46);
    top->addWidget(ic);
    top->addStretch(1);
    auto* ar = new QLabel(c);
    ar->setPixmap(shell::symbol(QStringLiteral("arrow"), QColor(0x8A, 0x97, 0xB4), QColor(0, 0, 0, 0), 22, c->devicePixelRatioF()));
    ar->setFixedSize(22, 22);
    top->addWidget(ar, 0, Qt::AlignTop);
    v->addLayout(top);
    v->addSpacing(6);
    auto* t = new QLabel(title, c);
    t->setObjectName(QStringLiteral("CardTitle"));
    v->addWidget(t);
    auto* d = new QLabel(desc, c);
    d->setObjectName(QStringLiteral("CardDesc"));
    d->setWordWrap(true);
    d->setMinimumHeight(36);
    v->addWidget(d);
    v->addStretch(1);
    auto* l = new QLabel(link + QStringLiteral("  →"), c);
    l->setObjectName(QStringLiteral("CardLink"));
    l->setStyleSheet(QStringLiteral("color:%1;").arg(accent.name()));
    v->addWidget(l);
    return c;
}

QFrame* makeQuickCard(const QString& sym, const QString& title, const QString& desc, std::function<void()> fn, QWidget* parent) {
    auto* c = new ClickCard(QStringLiteral("QuickCard"), parent);
    c->onClick = std::move(fn);
    c->setMinimumHeight(92);
    auto* v = new QVBoxLayout(c);
    v->setContentsMargins(16, 14, 16, 12);
    v->setSpacing(2);
    auto* ic = new QLabel(c);
    ic->setPixmap(shell::symbol(sym, theme::kBlue, QColor(0xE8, 0xF0, 0xFF), 32, c->devicePixelRatioF()));
    ic->setFixedSize(32, 32);
    v->addWidget(ic);
    auto* t = new QLabel(title, c);
    t->setObjectName(QStringLiteral("QuickTitle"));
    v->addWidget(t);
    auto* d = new QLabel(desc, c);
    d->setObjectName(QStringLiteral("QuickDesc"));
    d->setWordWrap(true);
    v->addWidget(d);
    v->addStretch(1);
    return c;
}

} // namespace

HomePage::HomePage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("Home"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("HomeScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);
    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("HomeBody"));
    scroll->setWidget(body);
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(32, 26, 32, 24);
    v->setSpacing(14);

    // ---- Băng chào ----
    auto* hero = new QFrame(body);
    hero->setObjectName(QStringLiteral("HeroBanner"));
    hero->setMinimumHeight(236);
    auto* hh = new QHBoxLayout(hero);
    hh->setContentsMargins(34, 26, 24, 26);
    auto* txt = new QVBoxLayout;
    txt->setSpacing(6);
    auto* kick = new QLabel(tr("OFFICE ĐA NĂNG · MIỄN PHÍ"), hero);
    kick->setObjectName(QStringLiteral("HeroKicker"));
    auto* title = new QLabel(tr("Tất cả tài liệu, <span style='color:#2864BA;font-weight:400'>trong một nơi.</span>"), hero);
    title->setObjectName(QStringLiteral("HeroTitle"));
    title->setTextFormat(Qt::RichText);
    title->setWordWrap(true);
    auto* sub = new QLabel(tr("Word, Excel, PowerPoint, PDF và các tiện ích dành cho người Việt. Mở nhanh, thao tác gọn, giữ nguyên tài liệu gốc."), hero);
    sub->setObjectName(QStringLiteral("HeroText"));
    sub->setWordWrap(true);
    auto* go = new QPushButton(tr("Bắt đầu soạn thảo  ↗"), hero);
    go->setObjectName(QStringLiteral("HeroButton"));
    go->setCursor(Qt::PointingHandCursor);
    txt->addWidget(kick);
    txt->addWidget(title);
    txt->addWidget(sub);
    txt->addSpacing(8);
    txt->addWidget(go, 0, Qt::AlignLeft);
    txt->addStretch(1);
    hh->addLayout(txt, 3);
    hh->addWidget(new HeroArt(hero), 2);
    v->addWidget(hero);
    connect(go, &QPushButton::clicked, this, &HomePage::newDocument);

    // ---- 4 thẻ ứng dụng ----
    auto* h1 = new QHBoxLayout;
    auto* t1 = new QLabel(tr("Chọn công cụ để bắt đầu"), body);
    t1->setObjectName(QStringLiteral("SectionTitle"));
    auto* n1 = new QLabel(tr("Tạo mới hoặc mở tệp có sẵn"), body);
    n1->setObjectName(QStringLiteral("SectionNote"));
    h1->addWidget(t1);
    h1->addStretch(1);
    h1->addWidget(n1);
    v->addLayout(h1);
    m_featGrid = new QGridLayout;
    m_featGrid->setSpacing(16);
    m_feat << makeFeatureCard(QStringLiteral("W"), theme::kWord, QColor(0xE3, 0xED, 0xFF), tr("Word"),
                tr("Viết và định dạng văn bản, mở và lưu DOCX, xuất PDF."), tr("Soạn thảo ngay"),
                [this] { emit newDocument(); }, body)
           << makeFeatureCard(QStringLiteral("X"), theme::kExcel, QColor(0xDC, 0xF2, 0xE6), tr("Excel"),
                tr("Nhiều sheet, công thức, bảng tính lớn mở mượt."), tr("Mở bảng tính"),
                [this] { emit newWorkbook(); }, body)
           << makeFeatureCard(QStringLiteral("P"), theme::kPowerPoint, QColor(0xFD, 0xEA, 0xE2), tr("PowerPoint"),
                tr("Thiết kế slide, chọn bố cục và trình chiếu trực tiếp."), tr("Tạo bài trình chiếu"),
                [this] { emit newPresentation(); }, body)
           << makeFeatureCard(QStringLiteral("PDF"), theme::kPdf, QColor(0xFB, 0xE3, 0xE5), tr("PDF"),
                tr("Xem PDF, ghép, tách, xoay và đánh số trang."), tr("Mở tệp PDF"),
                [this] { emit openPdf(); }, body);
    v->addLayout(m_featGrid);

    // ---- 6 thẻ nhanh ----
    auto* h2 = new QHBoxLayout;
    auto* t2 = new QLabel(tr("Làm nhanh, khỏi tìm công cụ ngoài"), body);
    t2->setObjectName(QStringLiteral("SectionTitle"));
    auto* n2 = new QLabel(tr("Chọn việc cần làm"), body);
    n2->setObjectName(QStringLiteral("SectionNote"));
    h2->addWidget(t2);
    h2->addStretch(1);
    h2->addWidget(n2);
    v->addLayout(h2);
    m_quickGrid = new QGridLayout;
    m_quickGrid->setSpacing(14);
    m_quick << makeQuickCard(QStringLiteral("open"), tr("Mở tệp"), tr("Excel, Word, PowerPoint, PDF"), [this] { emit openFile(); }, body)
            << makeQuickCard(QStringLiteral("docs"), tr("Tài liệu mới"), tr("Trang Word trống"), [this] { emit newDocument(); }, body)
            << makeQuickCard(QStringLiteral("work"), tr("Sổ làm việc mới"), tr("Bảng tính Excel trống"), [this] { emit newWorkbook(); }, body)
            << makeQuickCard(QStringLiteral("projects"), tr("Trình chiếu mới"), tr("Bản trình chiếu trống"), [this] { emit newPresentation(); }, body)
            << makeQuickCard(QStringLiteral("templates"), tr("Kho mẫu"), tr("Hợp đồng, báo giá, biên bản"), [this] { emit openTemplates(); }, body)
            << makeQuickCard(QStringLiteral("tools"), tr("Công cụ"), tr("Trộn thư, tìm tệp, chụp màn hình"), [this] { emit openToolbox(); }, body);
    v->addLayout(m_quickGrid);

    // ---- Băng thông tin ----
    auto* info = new QFrame(body);
    info->setObjectName(QStringLiteral("InfoBanner"));
    auto* iv = new QVBoxLayout(info);
    iv->setContentsMargins(20, 14, 20, 14);
    iv->setSpacing(3);
    auto* ik = new QLabel(tr("OFFLINE FIRST"), info);
    ik->setObjectName(QStringLiteral("InfoKicker"));
    auto* it = new QLabel(tr("Mọi tệp được xử lý ngay trên máy bạn, không cần tài khoản và không gửi lên mạng. Tệp gốc luôn được giữ nguyên: Lưu thành bản sao theo mặc định."), info);
    it->setObjectName(QStringLiteral("InfoText"));
    it->setWordWrap(true);
    iv->addWidget(ik);
    iv->addWidget(it);
    v->addWidget(info);

    // ---- Gần đây ----
    auto* row = new QHBoxLayout;
    auto* recentTitle = new QLabel(tr("Gần đây"), body);
    recentTitle->setObjectName(QStringLiteral("SectionTitle"));
    auto* open = new QPushButton(tr("Mở tệp…"), body);
    open->setObjectName(QStringLiteral("Primary"));
    open->setCursor(Qt::PointingHandCursor);
    row->addWidget(recentTitle);
    row->addStretch(1);
    row->addWidget(open);
    v->addLayout(row);
    connect(open, &QPushButton::clicked, this, &HomePage::openFile);

    m_recent = new QListWidget(body);
    m_recent->setObjectName(QStringLiteral("Recent"));
    m_recent->setMinimumHeight(150);
    v->addWidget(m_recent);
    connect(m_recent, &QListWidget::itemActivated, this, [this](QListWidgetItem* i) {
        emit openPath(i->data(Qt::UserRole).toString());
    });

    m_status = new QLabel(body);
    m_status->setObjectName(QStringLiteral("HomeSubtitle"));
    v->addWidget(m_status);
    v->addStretch(1);
    relayout();
}

static int colsFor(int w, int minCard, int maxCols) {
    return qBound(1, (w + 16) / (minCard + 16), maxCols);
}

void HomePage::relayout() {
    const int w = width() - 64;
    const int fc = colsFor(w, 230, 4), qc = colsFor(w, 180, 6);
    auto place = [](QGridLayout* g, const QList<QWidget*>& items, int cols) {
        for (QWidget* x : items) g->removeWidget(x);
        for (int i = 0; i < items.size(); ++i) g->addWidget(items[i], i / cols, i % cols);
        for (int c = 0; c < 6; ++c) g->setColumnStretch(c, c < cols ? 1 : 0);
    };
    if (fc != m_featCols) { m_featCols = fc; place(m_featGrid, m_feat, fc); }
    if (qc != m_quickCols) { m_quickCols = qc; place(m_quickGrid, m_quick, qc); }
}

void HomePage::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout();
}

void HomePage::setRecent(const QStringList& files) {
    QListWidget* list = m_recent;
    list->clear();
    for (const QString& f : files) {
        QFileInfo fi(f);
        auto* it = new QListWidgetItem(fi.fileName() + QStringLiteral("    —    ") + QDir::toNativeSeparators(fi.absolutePath()));
        it->setData(Qt::UserRole, f);
        it->setToolTip(QDir::toNativeSeparators(f));
        if (!fi.exists()) it->setForeground(theme::kMuted);
        list->addItem(it);
    }
    if (files.isEmpty()) {
        auto* it = new QListWidgetItem(tr("Chưa có tệp nào. Kéo thả tệp .xlsx, .docx, .pptx hoặc .pdf vào cửa sổ để mở."));
        it->setFlags(Qt::NoItemFlags);
        list->addItem(it);
    }
}

void HomePage::setEngineStatus(const QString& text) {
    m_status->setText(text);
}
