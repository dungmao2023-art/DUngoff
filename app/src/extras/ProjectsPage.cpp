// SPDX-License-Identifier: MPL-2.0
#include "ProjectsPage.h"
#include "../update/UpdateDialog.h"

#include "WorkspaceStore.h"
#include "../ui/Icons.h"
#include <QGraphicsDropShadowEffect>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const char* kRepo = "https://github.com/dungmao2023-art/DUngoff";

QString escapeHtml(const QString& s) { return s.toHtmlEscaped(); }
int c_row(const QListWidgetItem* it) { return it ? it->data(Qt::UserRole).toInt() : -1; }

// Danh sách đạt / chưa đạt của Dũng Office. Chỉ ghi "đạt" cho mục đã có kiểm thử tự động hoặc đã chạy thật.
struct Item { const char* text; bool done; };
const Item kOfficeItems[] = {
    {"Mở, sửa, lưu DOCX / XLSX / PPTX trên engine thật (kiểm thử tự động)", true},
    {"Lưu ra bản mới, không ghi đè tệp gốc; sao lưu trước khi thay", true},
    {"Công cụ PDF: gộp, tách, xoay, nén, mở tệp có mật khẩu", true},
    {"Bộ cài Windows ngoại tuyến, không cần quyền quản trị", true},
    {"Sửa lỗi gõ chữ không hiện (đã kiểm ở mức engine, chờ thử trên máy thật)", true},
    {"Ghi chú, Lịch, Liên hệ cục bộ", true},
    {"Căn lề chuẩn văn bản hành chính Việt Nam", false},
    {"Kho mẫu theo quy định pháp luật Việt Nam", false},
    {"OCR tiếng Việt ngoại tuyến", false},
    {"Tự lưu và khôi phục sau sự cố", false},
    {"Chuyển font TCVN3/VNI sang Unicode", false},
    {"Đọc số tiền thành chữ", false},
    {"Ký số bộ cài (hiện Windows SmartScreen còn cảnh báo)", false},
    {"Xác nhận định dạng Excel nâng cao (data bar) bằng Excel thật", false},
};
}  // namespace

ProjectsPage::ProjectsPage(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("PrjBody"));
    body->setStyleSheet(QStringLiteral(
        "QWidget#PrjBody{background:#F4F7FC;}"
        "QLabel#PrjTitle{font-size:24px;font-weight:700;color:#17294D;}"
        "QLabel#PrjDesc{font-size:13px;color:#697992;}"
        "QLabel#PrjSub{font-size:16px;font-weight:700;color:#17294D;}"
        "QFrame#PHead,QFrame#PCol{background:#FFFFFF;border:1px solid #DEE5F0;border-radius:16px;}"
        "QFrame#PHead QLabel,QFrame#PCol QLabel{border:0;background:transparent;}"
        "QListWidget{background:#F8FAFE;border:1px solid #E6ECF6;border-radius:12px;padding:4px;outline:0;}"
        "QListWidget::item{padding:5px 6px;border-radius:8px;}"
        "QListWidget::item:selected{background:#E8F0FF;color:#17294D;}"
        "QPushButton#PrjPrimary{background:#2864BA;color:white;border:0;border-radius:12px;padding:9px 18px;font-weight:700;}"
        "QPushButton#PrjPrimary:hover{background:#1B4A90;}"
        "QPushButton#PrjGhost{background:#FFFFFF;color:#2864BA;border:1px solid #DEE5F0;border-radius:12px;padding:8px 14px;font-weight:600;}"
        "QPushButton#PrjGhost:hover{background:#EEF4FF;}"));
    scroll->setWidget(body);
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(16);
    auto* title = new QLabel(tr("Dự án"), body);
    title->setObjectName(QStringLiteral("PrjTitle"));
    v->addWidget(title);
    auto* desc = new QLabel(tr("Tiến độ Dũng Office và các dự án khác của tác giả — ghi lại cục bộ trên máy bạn."), body);
    desc->setObjectName(QStringLiteral("PrjDesc"));
    desc->setWordWrap(true);
    v->addWidget(desc);
    v->addWidget(buildHeader());
    auto* sub = new QLabel(tr("Các dự án khác của tác giả"), body);
    sub->setObjectName(QStringLiteral("PrjSub"));
    v->addWidget(sub);
    load();
    auto* cols = new QHBoxLayout;
    cols->setSpacing(14);
    for (int i = 0; i < 3; ++i) cols->addWidget(buildColumn(i), 1);
    v->addLayout(cols);
    v->addStretch(1);
    for (int i = 0; i < 3; ++i) refreshColumn(i);
}

void ProjectsPage::flush() { save(); }

QWidget* ProjectsPage::buildHeader() {
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("PHead"));
    {
        auto* sh = new QGraphicsDropShadowEffect(card);
        sh->setBlurRadius(24); sh->setOffset(0, 4); sh->setColor(QColor(30, 60, 120, 30));
        card->setGraphicsEffect(sh);
    }
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(18, 16, 18, 16);
    auto* row = new QHBoxLayout;
    auto* name = new QLabel(tr("<b style='font-size:17px'>Dũng Office</b><br><span style='color:#697992'>Phiên bản %1 · miễn phí trọn đời · tác giả Trần Văn Dũng</span>")
                                .arg(QStringLiteral(DUNG_VERSION)), card);
    name->setTextFormat(Qt::RichText);
    row->addWidget(name, 1);
    auto* upd = new QPushButton(tr("Cập nhật"), card);
    upd->setIcon(dung::ui::icon(QStringLiteral("update"), Qt::white));
    upd->setObjectName(QStringLiteral("PrjPrimary"));
    upd->setCursor(Qt::PointingHandCursor);
    upd->setToolTip(tr("Kiểm tra bản mới. Ứng dụng chỉ kết nối mạng khi bạn bấm; không tự cài."));
    connect(upd, &QPushButton::clicked, this, [this] { UpdateDialog dlg(this); dlg.exec(); });
    auto* repo = new QPushButton(tr("Mã nguồn"), card);
    repo->setObjectName(QStringLiteral("PrjGhost"));
    repo->setIcon(dung::ui::icon(QStringLiteral("link"), QColor(0x28, 0x64, 0xBA)));
    repo->setCursor(Qt::PointingHandCursor);
    connect(repo, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QString::fromLatin1(kRepo))); });
    row->addWidget(upd);
    row->addWidget(repo);
    v->addLayout(row);

    int done = 0, total = 0;
    QString okHtml, todoHtml;
    for (const Item& it : kOfficeItems) {
        ++total;
        const QString line = QStringLiteral("<div style='margin:3px 0'>%1 %2</div>").arg(it.done ? QStringLiteral("<span style='color:#21866E'>•</span>") : QStringLiteral("<span style='color:#AF791D'>–</span>"), escapeHtml(tr(it.text)));
        if (it.done) { ++done; okHtml += line; } else todoHtml += line;
    }
    auto* sum = new QLabel(tr("<b>Tiến độ:</b> %1/%2 mục đạt").arg(done).arg(total), card);
    v->addWidget(sum);
    auto* two = new QHBoxLayout;
    auto* okBox = new QLabel(QStringLiteral("<b style='color:#21866E'>ĐẠT</b>") + okHtml, card);
    auto* todoBox = new QLabel(QStringLiteral("<b style='color:#AF791D'>CHƯA ĐẠT</b>") + todoHtml, card);
    for (QLabel* l : {okBox, todoBox}) { l->setTextFormat(Qt::RichText); l->setWordWrap(true); l->setAlignment(Qt::AlignTop | Qt::AlignLeft); two->addWidget(l, 1); }
    v->addLayout(two);
    auto* note = new QLabel(tr("Mục \"đạt\" chỉ ghi khi đã có kiểm thử tự động hoặc đã chạy thật; các mục khác vẫn đang phát triển."), card);
    note->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    v->addWidget(note);
    return card;
}

QWidget* ProjectsPage::buildColumn(int i) {
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("PCol"));
    {
        auto* sh = new QGraphicsDropShadowEffect(card);
        sh->setBlurRadius(24); sh->setOffset(0, 4); sh->setColor(QColor(30, 60, 120, 30));
        card->setGraphicsEffect(sh);
    }
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 16, 16, 16);
    Col& c = m_cols[i];
    c.avatar = new QLabel(card);
    c.avatar->setFixedSize(120, 120);
    c.avatar->setAlignment(Qt::AlignCenter);
    c.avatar->setStyleSheet(QStringLiteral("background:#EAF0FF;border-radius:14px;color:#275DD6;font-size:36px;font-weight:700;"));
    v->addWidget(c.avatar, 0, Qt::AlignHCenter);
    auto* pick = new QPushButton(tr("Chọn ảnh đại diện"), card);
    pick->setObjectName(QStringLiteral("PrjGhost"));
    connect(pick, &QPushButton::clicked, this, [this, i] { chooseImage(i); });
    v->addWidget(pick);
    c.name = new QLabel(card);
    c.name->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;"));
    c.name->setAlignment(Qt::AlignHCenter);
    c.name->setWordWrap(true);
    v->addWidget(c.name);
    c.link = new QLabel(card);
    c.link->setAlignment(Qt::AlignHCenter);
    c.link->setWordWrap(true);
    c.link->setTextFormat(Qt::RichText);
    c.link->setOpenExternalLinks(false);
    connect(c.link, &QLabel::linkActivated, this, [](const QString& u) {
        if (u.startsWith(QStringLiteral("https://")) || u.startsWith(QStringLiteral("http://"))) QDesktopServices::openUrl(QUrl(u));
    });
    v->addWidget(c.link);
    auto* edit = new QPushButton(tr("Sửa tên và liên kết"), card);
    edit->setObjectName(QStringLiteral("PrjGhost"));
    connect(edit, &QPushButton::clicked, this, [this, i] {
        QJsonObject p = m_projects[i].toObject();
        bool ok = false;
        const QString n = QInputDialog::getText(this, tr("Tên dự án"), tr("Tên:"), QLineEdit::Normal, p.value(QStringLiteral("name")).toString(), &ok);
        if (!ok) return;
        const QString u = QInputDialog::getText(this, tr("Liên kết dự án"), tr("Địa chỉ (bắt đầu bằng https://):"), QLineEdit::Normal,
                                                p.value(QStringLiteral("url")).toString(), &ok);
        if (!ok) return;
        p.insert(QStringLiteral("name"), n.trimmed());
        p.insert(QStringLiteral("url"), u.trimmed());
        m_projects[i] = p;
        save();
        refreshColumn(i);
    });
    v->addWidget(edit);
    auto* hdr = new QLabel(tr("<b>Đạt / chưa đạt</b> (nhấp đúp để đổi)"), card);
    hdr->setTextFormat(Qt::RichText);
    v->addWidget(hdr);
    c.list = new QListWidget(card);
    c.list->setMinimumHeight(140);
    v->addWidget(c.list, 1);
    connect(c.list, &QListWidget::itemDoubleClicked, this, [this, i](QListWidgetItem* it) {
        QJsonObject p = m_projects[i].toObject();
        QJsonArray items = p.value(QStringLiteral("items")).toArray();
        const int k = c_row(it);
        if (k < 0 || k >= items.size()) return;
        QJsonObject o = items[k].toObject();
        o.insert(QStringLiteral("done"), !o.value(QStringLiteral("done")).toBool());
        items[k] = o;
        p.insert(QStringLiteral("items"), items);
        m_projects[i] = p;
        save();
        refreshColumn(i);
    });
    auto* btns = new QHBoxLayout;
    auto* add = new QPushButton(tr("Thêm mục"), card);
    auto* del = new QPushButton(tr("Xoá mục"), card);
    add->setObjectName(QStringLiteral("PrjGhost"));
    del->setObjectName(QStringLiteral("PrjGhost"));
    add->setIcon(dung::ui::icon(QStringLiteral("add"), QColor(0x28, 0x64, 0xBA)));
    del->setIcon(dung::ui::icon(QStringLiteral("delete"), QColor(0xB9, 0x44, 0x49)));
    btns->addWidget(add);
    btns->addWidget(del);
    v->addLayout(btns);
    connect(add, &QPushButton::clicked, this, [this, i] {
        bool ok = false;
        const QString t = QInputDialog::getText(this, tr("Thêm mục"), tr("Nội dung (mặc định là chưa đạt):"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || t.isEmpty()) return;
        QJsonObject p = m_projects[i].toObject();
        QJsonArray items = p.value(QStringLiteral("items")).toArray();
        QJsonObject o;
        o.insert(QStringLiteral("text"), t);
        o.insert(QStringLiteral("done"), false);
        items.append(o);
        p.insert(QStringLiteral("items"), items);
        m_projects[i] = p;
        save();
        refreshColumn(i);
    });
    connect(del, &QPushButton::clicked, this, [this, i] {
        auto* it = m_cols[i].list->currentItem();
        if (!it) return;
        QJsonObject p = m_projects[i].toObject();
        QJsonArray items = p.value(QStringLiteral("items")).toArray();
        const int k = c_row(it);
        if (k < 0 || k >= items.size()) return;
        items.removeAt(k);
        p.insert(QStringLiteral("items"), items);
        m_projects[i] = p;
        save();
        refreshColumn(i);
    });
    return card;
}

void ProjectsPage::load() {
    const QJsonObject o = workspace::load(QStringLiteral("projects"));
    m_projects = o.value(QStringLiteral("projects")).toArray();
    while (m_projects.size() < 3) {
        QJsonObject p;
        p.insert(QStringLiteral("name"), tr("Dự án %1").arg(m_projects.size() + 1));
        p.insert(QStringLiteral("url"), QString());
        p.insert(QStringLiteral("image"), QString());
        p.insert(QStringLiteral("items"), QJsonArray());
        m_projects.append(p);
    }
}

void ProjectsPage::save() {
    QJsonObject o;
    o.insert(QStringLiteral("projects"), m_projects);
    o.insert(QStringLiteral("version"), 1);
    if (!workspace::save(QStringLiteral("projects"), o))
        QMessageBox::warning(this, tr("Dũng Office"), tr("Không lưu được danh sách dự án."));
}

void ProjectsPage::refreshColumn(int i) {
    Col& c = m_cols[i];
    const QJsonObject p = m_projects[i].toObject();
    const QString name = p.value(QStringLiteral("name")).toString();
    c.name->setText(name);
    const QString url = p.value(QStringLiteral("url")).toString();
    if (url.startsWith(QStringLiteral("https://")) || url.startsWith(QStringLiteral("http://")))
        c.link->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(url.toHtmlEscaped(), url.toHtmlEscaped()));
    else
        c.link->setText(tr("<i style='color:#697992'>Chưa có liên kết</i>"));
    QPixmap pm;
    const QString img = p.value(QStringLiteral("image")).toString();
    if (!img.isEmpty() && pm.load(img)) {
        c.avatar->setPixmap(pm.scaled(c.avatar->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        c.avatar->setPixmap(QPixmap());
        c.avatar->setText(name.isEmpty() ? QStringLiteral("?") : name.left(1).toUpper());
    }
    c.list->clear();
    const QJsonArray items = p.value(QStringLiteral("items")).toArray();
    for (int k = 0; k < items.size(); ++k) {
        const QJsonObject o = items[k].toObject();
        const bool done = o.value(QStringLiteral("done")).toBool();
        auto* it = new QListWidgetItem(dung::ui::icon(done ? QStringLiteral("check") : QStringLiteral("timer"), done ? QColor(0x21, 0x86, 0x6E) : QColor(0xAF, 0x79, 0x1D)),
                                       o.value(QStringLiteral("text")).toString());
        it->setData(Qt::UserRole, k);
        c.list->addItem(it);
    }
}

void ProjectsPage::chooseImage(int i) {
    const QString f = QFileDialog::getOpenFileName(this, tr("Chọn ảnh đại diện"), QString(), tr("Ảnh (*.png *.jpg *.jpeg *.bmp *.gif)"));
    if (f.isEmpty()) return;
    // Sao chép vào thư mục dữ liệu để ảnh vẫn còn nếu tệp gốc bị di chuyển; không động vào tệp gốc.
    QDir().mkpath(workspace::dataDir() + QStringLiteral("/avatars"));
    const QString dst = workspace::dataDir() + QStringLiteral("/avatars/project%1.%2").arg(i + 1).arg(QFileInfo(f).suffix().toLower());
    QFile::remove(dst);
    if (!QFile::copy(f, dst)) {
        QMessageBox::warning(this, tr("Dũng Office"), tr("Không sao chép được ảnh."));
        return;
    }
    QJsonObject p = m_projects[i].toObject();
    p.insert(QStringLiteral("image"), dst);
    m_projects[i] = p;
    save();
    refreshColumn(i);
}
