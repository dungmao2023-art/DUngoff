// SPDX-License-Identifier: MPL-2.0
#include "TemplatesPage.h"

#include "TemplateLibrary.h"
#include "ui/Icons.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QListWidget>
#include <QScrollArea>
#include <QStyle>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString prettyName(QString s) {
    s.remove(QRegularExpression(QStringLiteral("^\\d+[_ ]*")));      // bỏ số thứ tự đầu
    s.remove(QRegularExpression(QStringLiteral("\\.(docx|pptx)$"), QRegularExpression::CaseInsensitiveOption));
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    return s.trimmed();
}
}  // namespace

namespace {
constexpr int kCardW = 210, kCardH = 128, kGap = 14;
const char* kAll = "__all__";

QString catOf(const QString& rel) {
    const QStringList parts = rel.split(QLatin1Char('/'));
    return parts.size() > 1 ? prettyName(parts.first()) : QObject::tr("Khác");
}
}  // namespace

TemplatesPage::TemplatesPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("TplPage"));
    setStyleSheet(QStringLiteral(
        "QWidget#TplPage{background:#F4F7FC;}"
        "QLabel#TplTitle{font-size:24px;font-weight:700;color:#17294D;}"
        "QLabel#TplDesc{font-size:13px;color:#697992;}"
        "QLabel#TplWarn{background:#FFF7E6;color:#7A5B00;border:1px solid #F0DDAD;border-radius:12px;padding:8px 12px;font-size:12px;}"
        "QFrame#TplSide{background:#FFFFFF;border:1px solid #DEE5F0;border-radius:16px;}"
        "QLabel#TplSideTitle{font-size:11px;font-weight:700;color:#8A97B4;letter-spacing:1px;}"
        "QListWidget#TplCats{background:transparent;border:none;outline:0;font-size:13px;color:#4A5A7C;}"
        "QListWidget#TplCats::item{padding:7px 10px;border-radius:9px;margin:1px 0;}"
        "QListWidget#TplCats::item:hover{background:#EEF4FF;}"
        "QListWidget#TplCats::item:selected{background:#E8F0FF;color:#17294D;font-weight:700;}"
        "QFrame#TplCard{background:#FFFFFF;border:1px solid #DEE5F0;border-radius:16px;}"
        "QFrame#TplCard:hover{border-color:#9DB8EE;}"
        "QFrame#TplCard[sel=\"true\"]{border:2px solid #2864BA;background:#F5F9FF;}"
        "QLabel#TplCardName{font-size:13px;font-weight:700;color:#17294D;}"
        "QLabel#TplCardMeta{font-size:11px;color:#697992;}"
        "QLabel#TplBadge{border-radius:8px;color:white;font-size:10px;font-weight:700;padding:2px 8px;}"
        "QLineEdit#TplSearch{background:#FFFFFF;border:1px solid #DEE5F0;border-radius:12px;padding:8px 12px;font-size:13px;}"
        "QPushButton#TplPrimary{background:#2864BA;color:white;border:0;border-radius:12px;padding:9px 16px;font-weight:700;}"
        "QPushButton#TplPrimary:disabled{background:#B9C7E2;}"
        "QPushButton#TplGhost{background:#FFFFFF;color:#2864BA;border:1px solid #DEE5F0;border-radius:12px;padding:9px 14px;font-weight:600;}"
        "QPushButton#TplGhost:hover{background:#EEF4FF;}"));
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(28, 22, 28, 18);
    v->setSpacing(10);

    auto* title = new QLabel(tr("Kho mẫu"), this);
    title->setObjectName(QStringLiteral("TplTitle"));
    v->addWidget(title);
    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("TplDesc"));
    m_info->setWordWrap(true);
    v->addWidget(m_info);
    auto* warn = new QLabel(tr("<b>Lưu ý:</b> đây là mẫu soạn thảo tham khảo, không phải bản gốc do cơ quan nhà nước công bố. "
                               "Với thủ tục hành chính, hồ sơ sinh viên, biểu mẫu kế toán hãy lấy bản đang được cơ quan/trường công bố "
                               "(bấm \"Nguồn chính thức\")."), this);
    warn->setObjectName(QStringLiteral("TplWarn"));
    warn->setWordWrap(true);
    v->addWidget(warn);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("TplSearch"));
    m_search->setPlaceholderText(tr("Tìm mẫu: hợp đồng, đơn xin nghỉ, biên bản…"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(dung::ui::icon(QStringLiteral("search"), QColor(0x8A, 0x97, 0xB4)), QLineEdit::LeadingPosition);
    m_use = new QPushButton(tr("Tạo bản làm việc từ mẫu"), this);
    m_use->setObjectName(QStringLiteral("TplPrimary"));
    m_use->setEnabled(false);
    m_use->setCursor(Qt::PointingHandCursor);
    auto* src = new QPushButton(tr("Nguồn chính thức"), this);
    src->setObjectName(QStringLiteral("TplGhost"));
    src->setCursor(Qt::PointingHandCursor);
    auto* inst = new QPushButton(tr("Cài / cập nhật gói mẫu…"), this);
    inst->setObjectName(QStringLiteral("TplGhost"));
    inst->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_search, 1);
    row->addWidget(m_use);
    row->addWidget(src);
    row->addWidget(inst);
    v->addLayout(row);

    auto* body = new QHBoxLayout;
    body->setSpacing(14);
    auto* side = new QFrame(this);
    side->setObjectName(QStringLiteral("TplSide"));
    side->setFixedWidth(220);
    auto* sv = new QVBoxLayout(side);
    sv->setContentsMargins(12, 12, 12, 12);
    auto* st = new QLabel(tr("DANH MỤC"), side);
    st->setObjectName(QStringLiteral("TplSideTitle"));
    sv->addWidget(st);
    m_cats = new QListWidget(side);
    m_cats->setObjectName(QStringLiteral("TplCats"));
    m_cats->setFrameShape(QFrame::NoFrame);
    m_cats->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cats->setTextElideMode(Qt::ElideNone);
    m_cats->setWordWrap(true);
    sv->addWidget(m_cats, 1);
    body->addWidget(side);

    auto* right = new QVBoxLayout;
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("TplDesc"));
    right->addWidget(m_count);
    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;}"));
    m_gridHost = new QWidget;
    m_gridHost->setObjectName(QStringLiteral("TplGridHost"));
    m_gridHost->setStyleSheet(QStringLiteral("QWidget#TplGridHost{background:transparent;}"));
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(2, 2, 2, 2);
    m_grid->setSpacing(kGap);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scroll->setWidget(m_gridHost);
    right->addWidget(m_scroll, 1);
    m_empty = new QLabel(this);
    m_empty->setObjectName(QStringLiteral("TplDesc"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->hide();
    right->addWidget(m_empty);
    body->addLayout(right, 1);
    v->addLayout(body, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildCards(); });
    connect(m_cats, &QListWidget::currentRowChanged, this, [this] { rebuildCards(); });
    connect(m_use, &QPushButton::clicked, this, [this] { useSelected(); });
    connect(src, &QPushButton::clicked, this, [this] { showSources(); });
    connect(inst, &QPushButton::clicked, this, [this] { installPackage(); });
    reload();
}

void TemplatesPage::reload() {
    m_cards.clear();
    m_selected.clear();
    m_use->setEnabled(false);
    m_cats->blockSignals(true);
    m_cats->clear();
    const tpl::LibInfo lib = tpl::installed();
    if (!lib.valid) {
        m_root.clear();
        m_info->setText(tr("Chưa có gói mẫu. Bấm \"Cài / cập nhật gói mẫu…\" và chọn tệp .zip hoặc thư mục gói mẫu."));
        m_cats->blockSignals(false);
        rebuildCards();
        return;
    }
    m_root = lib.root;
    const QList<tpl::Entry> list = tpl::entries(lib);
    m_info->setText(tr("Gói: %1  •  phiên bản %2  •  %3 mẫu").arg(lib.name, lib.version).arg(list.size()));
    QStringList cats;
    for (const tpl::Entry& e : list) {
        Card c;
        c.rel = e.relPath;
        c.category = catOf(e.relPath);
        c.name = prettyName(e.relPath.split(QLatin1Char('/')).last());
        c.format = e.format;
        c.status = e.status;
        m_cards.append(c);
        if (!cats.contains(c.category)) cats << c.category;
    }
    auto* all = new QListWidgetItem(tr("Tất cả mẫu (%1)").arg(m_cards.size()), m_cats);
    all->setData(Qt::UserRole, QString::fromLatin1(kAll));
    for (const QString& c : cats) {
        int n = 0;
        for (const Card& k : m_cards) n += k.category == c;
        auto* it = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(c).arg(n), m_cats);
        it->setData(Qt::UserRole, c);
    }
    m_cats->blockSignals(false);
    m_cats->setCurrentRow(0);   // phát tín hiệu → rebuildCards()
    rebuildCards();
}

void TemplatesPage::rebuildCards() {
    for (Card& c : m_cards) {
        if (c.frame) { m_grid->removeWidget(c.frame); c.frame->deleteLater(); c.frame = nullptr; }
    }
    const QString q = m_search->text().trimmed().toLower();
    const auto* ci = m_cats->currentItem();
    const QString cat = ci ? ci->data(Qt::UserRole).toString() : QString::fromLatin1(kAll);
    int shown = 0;
    for (Card& c : m_cards) {
        if (cat != QLatin1String(kAll) && c.category != cat) continue;
        if (!q.isEmpty() && !c.name.toLower().contains(q) && !c.category.toLower().contains(q)) continue;
        auto* f = new QFrame(m_gridHost);
        f->setObjectName(QStringLiteral("TplCard"));
        f->setFixedSize(kCardW, kCardH);
        f->setCursor(Qt::PointingHandCursor);
        f->setProperty("rel", c.rel);
        f->setProperty("sel", c.rel == m_selected);
        f->setToolTip(c.name);
        f->installEventFilter(this);
        auto* fl = new QVBoxLayout(f);
        fl->setContentsMargins(14, 12, 14, 12);
        fl->setSpacing(4);
        auto* top = new QHBoxLayout;
        auto* badge = new QLabel(c.format.isEmpty() ? tr("MẪU") : c.format, f);
        badge->setObjectName(QStringLiteral("TplBadge"));
        const bool ppt = c.format.compare(QLatin1String("PPTX"), Qt::CaseInsensitive) == 0;
        badge->setStyleSheet(QStringLiteral("background:%1;").arg(ppt ? QStringLiteral("#C45530") : QStringLiteral("#2864BA")));
        badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        top->addWidget(badge);
        top->addStretch(1);
        fl->addLayout(top);
        auto* nm = new QLabel(c.name, f);
        nm->setObjectName(QStringLiteral("TplCardName"));
        nm->setWordWrap(true);
        nm->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        nm->setAttribute(Qt::WA_TransparentForMouseEvents);
        fl->addWidget(nm, 1);
        auto* meta = new QLabel(c.status.isEmpty() ? c.category : c.status, f);
        meta->setObjectName(QStringLiteral("TplCardMeta"));
        meta->setWordWrap(true);
        meta->setAttribute(Qt::WA_TransparentForMouseEvents);
        fl->addWidget(meta);
        c.frame = f;
        ++shown;
    }
    m_count->setText(tr("%1 mẫu — bấm để chọn, bấm đúp để tạo bản làm việc").arg(shown));
    m_empty->setVisible(shown == 0);
    if (shown == 0)
        m_empty->setText(m_root.isEmpty() ? tr("Chưa có mẫu nào.") : tr("Không có mẫu khớp. Thử từ khoá khác hoặc chọn \"Tất cả mẫu\"."));
    m_scroll->setVisible(shown > 0);
    relayout();
}

void TemplatesPage::relayout() {
    const int avail = m_scroll->viewport()->width() - 4;
    const int cols = qMax(1, (avail + kGap) / (kCardW + kGap));
    int i = 0;
    for (Card& c : m_cards) {
        if (!c.frame) continue;
        m_grid->addWidget(c.frame, i / cols, i % cols);
        ++i;
    }
}

void TemplatesPage::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout();
}

bool TemplatesPage::eventFilter(QObject* o, QEvent* e) {
    auto* f = qobject_cast<QFrame*>(o);
    if (f && f->objectName() == QLatin1String("TplCard")) {
        if (e->type() == QEvent::MouseButtonPress) { select(f->property("rel").toString()); return true; }
        if (e->type() == QEvent::MouseButtonDblClick) { select(f->property("rel").toString()); useSelected(); return true; }
    }
    return QWidget::eventFilter(o, e);
}

void TemplatesPage::select(const QString& rel) {
    m_selected = rel;
    for (const Card& c : m_cards) {
        if (!c.frame) continue;
        c.frame->setProperty("sel", c.rel == rel);
        c.frame->style()->unpolish(c.frame);
        c.frame->style()->polish(c.frame);
    }
    m_use->setEnabled(!rel.isEmpty());
}

void TemplatesPage::useSelected() {
    if (m_selected.isEmpty()) {
        QMessageBox::information(this, tr("Thư viện mẫu"), tr("Hãy chọn một mẫu trước."));
        return;
    }
    const QString rel = m_selected;
    const QString src = m_root + QLatin1Char('/') + rel;
    const QString ext = QFileInfo(src).suffix();
    const QString base = prettyName(QFileInfo(src).fileName());
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString dst = QFileDialog::getSaveFileName(this, tr("Lưu bản làm việc của mẫu"), dir + QLatin1Char('/') + base + QLatin1Char('.') + ext,
                                               tr("Tài liệu (*.%1)").arg(ext));
    if (dst.isEmpty()) return;
    if (!dst.endsWith(QLatin1Char('.') + ext, Qt::CaseInsensitive)) dst += QLatin1Char('.') + ext;
    QString err;
    // Hộp thoại lưu đã hỏi người dùng về việc ghi đè; mẫu gốc không bao giờ bị sửa.
    if (QFile::exists(dst)) {
        QMessageBox::warning(this, tr("Thư viện mẫu"), tr("Tệp đã tồn tại. Dũng Office không ghi đè tệp có sẵn — hãy chọn tên khác."));
        return;
    }
    if (!tpl::makeWorkingCopy(src, dst, &err)) { QMessageBox::warning(this, tr("Thư viện mẫu"), err); return; }
    emit openPath(dst);
}

void TemplatesPage::installPackage() {
    QMessageBox ask(this);
    ask.setWindowTitle(tr("Cài / cập nhật gói mẫu"));
    ask.setText(tr("Gói mới sẽ THAY HẲN gói mẫu cũ cùng loại (bản cũ bị xoá để không dùng nhầm mẫu đã hết hiệu lực).\n"
                   "Các tài liệu bạn đã tạo từ mẫu không bị ảnh hưởng.\n\nChọn nguồn gói mẫu:"));
    auto* zipBtn = ask.addButton(tr("Tệp .zip…"), QMessageBox::AcceptRole);
    auto* dirBtn = ask.addButton(tr("Thư mục…"), QMessageBox::AcceptRole);
    ask.addButton(tr("Huỷ"), QMessageBox::RejectRole);
    ask.exec();
    tpl::InstallResult r;
    auto run = [&](bool allowOlder) {
        if (ask.clickedButton() == zipBtn) return tpl::installFromZip(m_pending, allowOlder);
        return tpl::installFromDir(m_pending, allowOlder);
    };
    if (ask.clickedButton() == zipBtn) {
        m_pending = QFileDialog::getOpenFileName(this, tr("Chọn gói mẫu (.zip)"), QString(), tr("Gói mẫu (*.zip)"));
    } else if (ask.clickedButton() == dirBtn) {
        m_pending = QFileDialog::getExistingDirectory(this, tr("Chọn thư mục gói mẫu"));
    } else {
        return;
    }
    if (m_pending.isEmpty()) return;
    r = run(false);
    if (r.needConfirm) {
        if (QMessageBox::question(this, tr("Gói cũ hơn"), r.message + tr("\n\nVẫn thay bằng gói cũ hơn?")) != QMessageBox::Yes) return;
        r = run(true);
    }
    QMessageBox::information(this, tr("Thư viện mẫu"), r.message);
    if (r.ok) reload();
}

void TemplatesPage::showSources() {
    const tpl::LibInfo lib = tpl::installed();
    QString html = tr("<h3>Nguồn chính thức</h3><p>Luôn đối chiếu với bản đang được cơ quan công bố.</p><ul>");
    for (const tpl::Source& s : tpl::sources(lib))
        html += QStringLiteral("<li><b>%1</b> — %2<br><a href=\"%3\">%3</a><br><i>%4</i></li>")
                    .arg(s.group.toHtmlEscaped(), s.document.toHtmlEscaped(), s.url.toHtmlEscaped(), s.guide.toHtmlEscaped());
    html += QStringLiteral("</ul>");
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Nguồn chính thức"));
    dlg.resize(720, 520);
    auto* v = new QVBoxLayout(&dlg);
    auto* tb = new QTextBrowser(&dlg);
    tb->setOpenExternalLinks(true);
    tb->setHtml(html);
    v->addWidget(tb);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    v->addWidget(bb);
    dlg.exec();
}
