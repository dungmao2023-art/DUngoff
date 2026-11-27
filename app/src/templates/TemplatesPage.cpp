// SPDX-License-Identifier: MPL-2.0
#include "TemplatesPage.h"

#include "TemplateLibrary.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString prettyName(QString s) {
    s.remove(QRegularExpression(QStringLiteral("^\\d+[_ ]*")));      // bỏ số thứ tự đầu
    s.remove(QRegularExpression(QStringLiteral("\\.(docx|xlsx|pptx)$"), QRegularExpression::CaseInsensitiveOption));
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    return s.trimmed();
}
}  // namespace

TemplatesPage::TemplatesPage(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 16);
    auto* title = new QLabel(tr("Thư viện mẫu"), this);
    title->setStyleSheet(QStringLiteral("font-size:22px;font-weight:700;color:#1C2940;"));
    v->addWidget(title);
    auto* warn = new QLabel(tr("<b>Lưu ý:</b> đây là mẫu soạn thảo tham khảo, không phải bản gốc do cơ quan nhà nước công bố. "
                               "Với thủ tục hành chính, hồ sơ sinh viên, biểu mẫu kế toán hãy lấy bản đang được cơ quan/trường công bố "
                               "(bấm \"Nguồn chính thức\")."), this);
    warn->setWordWrap(true);
    warn->setStyleSheet(QStringLiteral("background:#FFF7E6;color:#7A5B00;border:1px solid #F0DDAD;border-radius:8px;padding:8px 12px;"));
    v->addWidget(warn);
    m_info = new QLabel(this);
    m_info->setStyleSheet(QStringLiteral("color:#697992;"));
    v->addWidget(m_info);

    auto* row = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Tìm mẫu: hợp đồng, đơn xin nghỉ, biên bản…"));
    auto* use = new QPushButton(tr("Tạo bản làm việc từ mẫu"), this);
    use->setStyleSheet(QStringLiteral("QPushButton{background:#275DD6;color:white;border:0;border-radius:8px;padding:8px 14px;font-weight:700;}"));
    auto* src = new QPushButton(tr("Nguồn chính thức"), this);
    auto* inst = new QPushButton(tr("Cài / cập nhật gói mẫu…"), this);
    row->addWidget(m_search, 1);
    row->addWidget(use);
    row->addWidget(src);
    row->addWidget(inst);
    v->addLayout(row);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({tr("Mẫu"), tr("Định dạng"), tr("Tình trạng")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->setUniformRowHeights(true);
    v->addWidget(m_tree, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this] { applyFilter(); });
    connect(use, &QPushButton::clicked, this, [this] { useSelected(); });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this] { useSelected(); });
    connect(src, &QPushButton::clicked, this, [this] { showSources(); });
    connect(inst, &QPushButton::clicked, this, [this] { installPackage(); });
    reload();
}

void TemplatesPage::reload() {
    m_tree->clear();
    const tpl::LibInfo lib = tpl::installed();
    if (!lib.valid) {
        m_root.clear();
        m_info->setText(tr("Chưa có gói mẫu. Bấm \"Cài / cập nhật gói mẫu…\" và chọn tệp .zip hoặc thư mục gói mẫu."));
        return;
    }
    m_root = lib.root;
    const QList<tpl::Entry> list = tpl::entries(lib);
    m_info->setText(tr("Gói: %1  •  phiên bản %2  •  %3 mẫu").arg(lib.name, lib.version).arg(list.size()));
    QHash<QString, QTreeWidgetItem*> dirs;
    for (const tpl::Entry& e : list) {
        const QStringList parts = e.relPath.split(QLatin1Char('/'));
        QTreeWidgetItem* parent = nullptr;
        QString key;
        for (int i = 0; i < parts.size() - 1; ++i) {
            key += parts[i] + QLatin1Char('/');
            QTreeWidgetItem*& d = dirs[key];
            if (!d) {
                d = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
                d->setText(0, prettyName(parts[i]));
                QFont f = d->font(0);
                f.setBold(true);
                d->setFont(0, f);
            }
            parent = d;
        }
        auto* it = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        it->setText(0, prettyName(parts.last()));
        it->setText(1, e.format);
        it->setText(2, e.status);
        it->setData(0, Qt::UserRole, e.relPath);
    }
    // Thư mục kỹ thuật 00_ (hướng dẫn) không cần hiện nếu rỗng; mở sẵn cấp đầu.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) m_tree->topLevelItem(i)->setExpanded(true);
}

static bool filterItem(QTreeWidgetItem* it, const QString& q) {
    bool any = false;
    for (int i = 0; i < it->childCount(); ++i) any = filterItem(it->child(i), q) || any;
    const bool leaf = it->childCount() == 0;
    const bool match = q.isEmpty() || it->text(0).toLower().contains(q);
    it->setHidden(leaf ? !match : !(any || match));
    if (!q.isEmpty() && (any)) it->setExpanded(true);
    return leaf ? match : (any || match);
}

void TemplatesPage::applyFilter() {
    const QString q = m_search->text().trimmed().toLower();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) filterItem(m_tree->topLevelItem(i), q);
}

void TemplatesPage::useSelected() {
    auto* it = m_tree->currentItem();
    if (!it || it->data(0, Qt::UserRole).toString().isEmpty()) {
        QMessageBox::information(this, tr("Thư viện mẫu"), tr("Hãy chọn một mẫu (không phải tên thư mục)."));
        return;
    }
    const QString rel = it->data(0, Qt::UserRole).toString();
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
    const auto current=tpl::installed();
    bool listed=false;for(const auto& entry:tpl::entries(current))if(entry.relPath==rel)listed=true;
    if(!current.valid||current.root!=m_root||!listed){
        QMessageBox::warning(this,tr("Thư viện mẫu"),tr("Gói mẫu đã thay đổi hoặc tệp không còn hợp lệ. Hãy chọn lại mẫu."));reload();return;
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
