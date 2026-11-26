// SPDX-License-Identifier: MPL-2.0
#include "PdfToolsDialog.h"
#include "PdfOcr.h"
#include <QPlainTextEdit>
#include <QApplication>
#include <QClipboard>
#include <QSaveFile>

#include <QCheckBox>
#include <QDesktopServices>
#include <QUrl>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QProgressDialog>
#include <QStringList>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <QTemporaryDir>
#include <QTimer>
#include <QPixmap>
#include <QScrollArea>
#include "../print/PrintRender.h"

using namespace dungpdf;

namespace {
QString human(qint64 b) {
    if (b < 1024) return QStringLiteral("%1 B").arg(b);
    if (b < 1024 * 1024) return QStringLiteral("%1 KB").arg(b / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(b / (1024.0 * 1024.0), 0, 'f', 1);
}
}  // namespace

void PdfToolsDialog::run(QWidget* parent, Tab tab, const QString& initialPath) {
    if (findQpdf().isEmpty()) {
        QMessageBox::warning(parent, tr("Công cụ PDF"),
                             tr("Không tìm thấy thành phần qpdf đi kèm (thư mục \"qpdf\" cạnh chương trình). "
                                "Hãy cài đặt lại Dũng Office."));
        return;
    }
    PdfToolsDialog dlg(parent);
    dlg.m_tabs->setCurrentIndex(tab);
    if (!initialPath.isEmpty()) {
        for (QLineEdit* e : {dlg.m_splitFile, dlg.m_rotFile, dlg.m_cmpFile, dlg.m_infoFile, dlg.m_numFile, dlg.m_stampFile, dlg.m_blankFile, dlg.m_orderFile, dlg.m_ocrFile})
            if (e) e->setText(initialPath);
        dlg.refreshPageCount(initialPath, dlg.m_splitInfo);
        dlg.refreshPageCount(initialPath, dlg.m_rotInfo);
        dlg.refreshPageCount(initialPath, dlg.m_numInfo);
        dlg.refreshPageCount(initialPath, dlg.m_stampInfo);
        dlg.refreshPageCount(initialPath, dlg.m_blankInfo);
        dlg.refreshPageCount(initialPath, dlg.m_orderInfo);
        dlg.m_mergeList->addItem(initialPath);
    }
    dlg.exec();
}

PdfToolsDialog::PdfToolsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Công cụ PDF"));
    resize(1060, 680);
    auto* lay = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildMerge(), tr("Gộp"));
    m_tabs->addTab(buildSplit(), tr("Tách"));
    m_tabs->addTab(buildRotate(), tr("Xoay"));
    m_tabs->addTab(buildCompress(), tr("Nén"));
    m_tabs->addTab(buildInfo(), tr("Thông tin"));
    m_tabs->addTab(buildNumber(), tr("Đánh số trang"));
    m_tabs->addTab(buildStamp(), tr("Đóng dấu"));
    m_tabs->addTab(buildImages(), tr("Ảnh → PDF"));
    m_tabs->addTab(buildBlank(), tr("Trang trắng"));
    m_tabs->addTab(buildReorder(), tr("Sắp xếp"));
    m_tabs->addTab(buildOcr(), tr("OCR · TXT / PDF"));
    lay->addWidget(m_tabs);
    auto* note = new QLabel(tr("Tệp gốc không bao giờ bị sửa: kết quả luôn lưu thành tệp mới."), this);
    note->setStyleSheet("color:#555");
    lay->addWidget(note);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

QString PdfToolsDialog::pickPdf() {
    return QFileDialog::getOpenFileName(this, tr("Chọn tệp PDF"), QString(), tr("Tệp PDF (*.pdf)"));
}

QWidget* PdfToolsDialog::fileRow(QLineEdit** edit, const std::function<void()>& onChosen) {
    auto* w = new QWidget(this);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    *edit = new QLineEdit(w);
    (*edit)->setReadOnly(true);
    auto* b = new QPushButton(tr("Chọn..."), w);
    QLineEdit* e = *edit;
    connect(b, &QPushButton::clicked, this, [this, e, onChosen] {
        const QString p = pickPdf();
        if (p.isEmpty()) return;
        e->setText(p);
        if (onChosen) onChosen();
    });
    h->addWidget(e, 1);
    h->addWidget(b);
    return w;
}

Input PdfToolsDialog::inputFor(const QString& path) const {
    return Input{path, m_passwords.value(path)};
}

bool PdfToolsDialog::askPassword(const QString& path) {
    bool ok = false;
    const QString pw = QInputDialog::getText(this, tr("Tệp có mật khẩu"),
                                             tr("Nhập mật khẩu để mở \"%1\":").arg(QFileInfo(path).fileName()),
                                             QLineEdit::Password, QString(), &ok);
    if (!ok) return false;
    m_passwords.insert(path, pw);
    return true;
}

Result PdfToolsDialog::runWithPasswords(const QStringList& paths, const std::function<Result()>& op) {
    Result r;
    for (int attempt = 0; attempt < 4; ++attempt) {
        // Chạy ở luồng nền, giữ giao diện phản hồi bằng vòng lặp sự kiện cục bộ.
        QEventLoop loop;
        QThread* th = QThread::create([&] { r = op(); });
        connect(th, &QThread::finished, &loop, &QEventLoop::quit);
        setEnabled(false);
        th->start();
        loop.exec();
        th->wait();
        delete th;
        setEnabled(true);
        if (!r.needPassword) return r;
        // Hỏi mật khẩu cho tệp đầu tiên chưa có mật khẩu đúng.
        bool asked = false;
        for (const QString& p : paths) {
            const dungpdf::Info i = info(p, m_passwords.value(p));
            if (i.needPassword) {
                if (!askPassword(p)) { r.error = tr("Đã hủy: chưa nhập mật khẩu."); r.needPassword = false; return r; }
                asked = true;
                break;
            }
        }
        if (!asked) return r;
    }
    return r;
}

void PdfToolsDialog::showResult(const Result& r, const QString& okTitle) {
    if (!r.ok) {
        QMessageBox::warning(this, tr("Không thực hiện được"), r.error.isEmpty() ? tr("Có lỗi không rõ.") : r.error);
        return;
    }
    QString msg = tr("Đã lưu:\n%1").arg(r.files.size() > 8 ? tr("%1 tệp trong thư mục đã chọn").arg(r.files.size())
                                                            : r.files.join('\n'));
    if (!r.backup.isEmpty()) msg += tr("\n\nBản cũ đã được sao lưu tại:\n%1").arg(r.backup);
    if (!r.warnings.isEmpty()) msg += tr("\n\nLưu ý:\n%1").arg(r.warnings.join('\n'));
    QMessageBox box(this);
    box.setWindowTitle(okTitle); box.setText(msg);
    auto* open = box.addButton(r.files.size() == 1 ? tr("Xem bản PDF kết quả") : tr("Mở thư mục kết quả"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    open->setEnabled(!r.files.isEmpty());
    box.exec();
    if (box.clickedButton() == open && !r.files.isEmpty()) {
        const QString path = r.files.size() == 1 ? r.files.first() : QFileInfo(r.files.first()).absolutePath();
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) QMessageBox::warning(this, tr("Xem kết quả"), tr("Không mở được tệp hoặc thư mục."));
    }
}

void PdfToolsDialog::refreshPageCount(const QString& path, QLabel* label) {
    if (!label) return;
    dungpdf::Info i = info(path, m_passwords.value(path));
    if (i.needPassword && askPassword(path)) i = info(path, m_passwords.value(path));
    label->setText(i.ok ? tr("%1 trang, %2").arg(i.pages).arg(human(i.bytes)) : i.error);
}

// Chọn đích: hộp thoại lưu của hệ thống tự hỏi khi trùng tên; chỉ khi người dùng xác nhận mới thay (có sao lưu).
static QString pickDest(QWidget* p, const QString& src, const QString& label) {
    return QFileDialog::getSaveFileName(p, QObject::tr("Lưu kết quả thành"), suggestOutputPath(src, label),
                                        QObject::tr("Tệp PDF (*.pdf)"));
}

QWidget* PdfToolsDialog::buildMerge() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    v->addWidget(new QLabel(tr("Thêm các tệp PDF theo thứ tự cần gộp:"), w));
    m_mergeList = new QListWidget(w);
    v->addWidget(m_mergeList, 1);
    auto* h = new QHBoxLayout;
    auto* add = new QPushButton(tr("Thêm..."), w);
    auto* rem = new QPushButton(tr("Bớt"), w);
    auto* up = new QPushButton(tr("Lên"), w);
    auto* dn = new QPushButton(tr("Xuống"), w);
    auto* go = new QPushButton(tr("Gộp..."), w);
    for (auto* b : {add, rem, up, dn}) h->addWidget(b);
    h->addStretch(1);
    h->addWidget(go);
    v->addLayout(h);
    connect(add, &QPushButton::clicked, this, [this] {
        const QStringList fs = QFileDialog::getOpenFileNames(this, tr("Chọn các tệp PDF"), QString(), tr("Tệp PDF (*.pdf)"));
        for (const QString& f : fs) m_mergeList->addItem(f);
    });
    connect(rem, &QPushButton::clicked, this, [this] { delete m_mergeList->takeItem(m_mergeList->currentRow()); });
    auto move = [this](int d) {
        const int r = m_mergeList->currentRow(), n = r + d;
        if (r < 0 || n < 0 || n >= m_mergeList->count()) return;
        m_mergeList->insertItem(n, m_mergeList->takeItem(r));
        m_mergeList->setCurrentRow(n);
    };
    connect(up, &QPushButton::clicked, this, [move] { move(-1); });
    connect(dn, &QPushButton::clicked, this, [move] { move(1); });
    connect(go, &QPushButton::clicked, this, [this] {
        QStringList paths;
        for (int i = 0; i < m_mergeList->count(); ++i) paths << m_mergeList->item(i)->text();
        if (paths.size() < 2) { QMessageBox::information(this, tr("Gộp"), tr("Cần ít nhất hai tệp.")); return; }
        const QString dest = pickDest(this, paths.first(), tr("gộp"));
        if (dest.isEmpty()) return;
        // Đích trùng một tệp nguồn sẽ bị PdfOps từ chối; ở đây chỉ báo trước cho rõ.
        const bool replace = QFileInfo::exists(dest);
        const Result r = runWithPasswords(paths, [this, paths, dest, replace] {
            QList<Input> ins;
            for (const QString& p : paths) ins << inputFor(p);
            return merge(ins, dest, replace);
        });
        showResult(r, tr("Gộp xong"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildSplit() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_splitFile, [this] { refreshPageCount(m_splitFile->text(), m_splitInfo); }));
    m_splitInfo = new QLabel(w);
    f->addRow(QString(), m_splitInfo);
    m_splitKeep = new QRadioButton(tr("Lấy một số trang ra một tệp mới"), w);
    auto* each = new QRadioButton(tr("Tách mỗi trang thành một tệp riêng"), w);
    m_splitKeep->setChecked(true);
    f->addRow(m_splitKeep);
    m_splitPages = new QLineEdit(w);
    m_splitPages->setPlaceholderText(tr("Ví dụ: 1-3,5,8-"));
    f->addRow(tr("Các trang:"), m_splitPages);
    f->addRow(each);
    auto* go = new QPushButton(tr("Thực hiện..."), w);
    f->addRow(go);
    connect(go, &QPushButton::clicked, this, [this, each] {
        const QString src = m_splitFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Tách"), tr("Hãy chọn tệp PDF.")); return; }
        if (each->isChecked()) {
            const QString dir = QFileDialog::getExistingDirectory(this, tr("Chọn thư mục lưu các trang"),
                                                                  QFileInfo(src).absolutePath());
            if (dir.isEmpty()) return;
            showResult(runWithPasswords({src}, [this, src, dir] { return splitEach(inputFor(src), dir); }),
                       tr("Tách xong"));
        } else {
            const QString spec = m_splitPages->text().trimmed();
            if (spec.isEmpty()) { QMessageBox::information(this, tr("Tách"), tr("Hãy nhập các trang cần lấy.")); return; }
            const QString dest = pickDest(this, src, tr("trích"));
            if (dest.isEmpty()) return;
            const bool replace = QFileInfo::exists(dest);
            showResult(runWithPasswords({src}, [this, src, spec, dest, replace] {
                           return extract(inputFor(src), spec, dest, replace);
                       }),
                       tr("Tách xong"));
        }
    });
    return w;
}

QWidget* PdfToolsDialog::buildReorder() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_orderFile, [this] {
        m_orderSpec->clear();
        refreshPageCount(m_orderFile->text(), m_orderInfo);
    }));
    m_orderInfo = new QLabel(w);
    f->addRow(QString(), m_orderInfo);
    m_orderSpec = new QLineEdit(w);
    m_orderSpec->setPlaceholderText(tr("VD: 3,1-2,4-"));
    f->addRow(tr("Thứ tự trang mới:"), m_orderSpec);
    auto* note = new QLabel(tr("Nhập các trang theo thứ tự mong muốn. Mỗi trang phải xuất hiện đúng một lần. Không sửa tệp gốc; muốn xóa trang, dùng Tách."), w);
    note->setWordWrap(true);
    f->addRow(note);
    auto* go = new QPushButton(tr("Lưu bản đã sắp xếp..."), w);
    f->addRow(go);
    connect(go, &QPushButton::clicked, this, [this] {
        const QString src = m_orderFile->text(), spec = m_orderSpec->text().trimmed();
        if (src.isEmpty() || spec.isEmpty()) { QMessageBox::information(this, tr("Sắp xếp"), tr("Hãy chọn PDF và nhập thứ tự trang.")); return; }
        const QString dest = pickDest(this, src, tr("sắp xếp"));
        if (dest.isEmpty()) return;
        const bool replace = QFileInfo::exists(dest);
        showResult(runWithPasswords({src}, [this, src, spec, dest, replace] {
            return reorder(inputFor(src), spec, dest, replace);
        }), tr("Sắp xếp xong"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildRotate() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_rotFile, [this] { refreshPageCount(m_rotFile->text(), m_rotInfo); }));
    m_rotInfo = new QLabel(w);
    f->addRow(QString(), m_rotInfo);
    m_rotPages = new QLineEdit(w);
    m_rotPages->setPlaceholderText(tr("Để trống = tất cả; ví dụ: 1-3,5"));
    f->addRow(tr("Các trang:"), m_rotPages);
    m_rotAngle = new QComboBox(w);
    m_rotAngle->addItem(tr("Quay phải 90°"), 90);
    m_rotAngle->addItem(tr("Quay 180°"), 180);
    m_rotAngle->addItem(tr("Quay trái 90°"), -90);
    f->addRow(tr("Góc xoay:"), m_rotAngle);
    auto* go = new QPushButton(tr("Xoay..."), w);
    f->addRow(go);
    connect(go, &QPushButton::clicked, this, [this] {
        const QString src = m_rotFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Xoay"), tr("Hãy chọn tệp PDF.")); return; }
        const QString dest = pickDest(this, src, tr("xoay"));
        if (dest.isEmpty()) return;
        const bool replace = QFileInfo::exists(dest);
        const QString spec = m_rotPages->text().trimmed();
        const int deg = m_rotAngle->currentData().toInt();
        showResult(runWithPasswords({src}, [this, src, spec, deg, dest, replace] {
                       return rotate(inputFor(src), spec, deg, dest, replace);
                   }),
                   tr("Xoay xong"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildCompress() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_cmpFile, nullptr));
    auto* hint = new QLabel(tr("Nén không làm mất nội dung (nén lại dữ liệu, không giảm chất lượng ảnh). "
                               "Tệp đã nén sẵn có thể giảm rất ít."), w);
    hint->setWordWrap(true);
    f->addRow(hint);
    auto* go = new QPushButton(tr("Nén..."), w);
    f->addRow(go);
    connect(go, &QPushButton::clicked, this, [this] {
        const QString src = m_cmpFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Nén"), tr("Hãy chọn tệp PDF.")); return; }
        const QString dest = pickDest(this, src, tr("nén"));
        if (dest.isEmpty()) return;
        const bool replace = QFileInfo::exists(dest);
        Result r = runWithPasswords({src}, [this, src, dest, replace] { return compress(inputFor(src), dest, replace); });
        if (r.ok && !r.files.isEmpty()) {
            const qint64 a = QFileInfo(src).size(), b = QFileInfo(r.files.first()).size();
            r.warnings << tr("Dung lượng: %1 → %2").arg(human(a), human(b));
        }
        showResult(r, tr("Nén xong"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildInfo() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_infoFile, [this] {
        const QString p = m_infoFile->text();
        dungpdf::Info i = info(p, m_passwords.value(p));
        if (i.needPassword && askPassword(p)) i = info(p, m_passwords.value(p));
        m_infoText->setText(i.ok ? tr("Số trang: %1\nDung lượng: %2\nĐặt mật khẩu: %3")
                                       .arg(i.pages).arg(human(i.bytes)).arg(i.encrypted ? tr("có") : tr("không"))
                                 : i.error);
    }));
    m_infoText = new QLabel(w);
    m_infoText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    f->addRow(m_infoText);
    return w;
}

// ---- Đánh số trang, đóng dấu, ảnh -> PDF, trang trắng ----

namespace {
void fillPositions(QComboBox* c, bool withDiagonal) {
    c->addItem(QObject::tr("Dưới giữa"), int(Position::BottomCenter));
    c->addItem(QObject::tr("Dưới phải"), int(Position::BottomRight));
    c->addItem(QObject::tr("Dưới trái"), int(Position::BottomLeft));
    c->addItem(QObject::tr("Trên giữa"), int(Position::TopCenter));
    c->addItem(QObject::tr("Trên phải"), int(Position::TopRight));
    c->addItem(QObject::tr("Trên trái"), int(Position::TopLeft));
    c->addItem(QObject::tr("Giữa trang"), int(Position::Center));
    if (withDiagonal) c->insertItem(0, QObject::tr("Chéo giữa trang"), int(Position::Diagonal));
}
// Preview from the SAME PDF operation as saving, to a private temporary directory.
// Only the current page is rendered; auto regeneration is opt-in and capped for large sources.
class OperationPreview : public QWidget {
public:
    using Generate = std::function<Result(const QString&)>;
    explicit OperationPreview(QWidget* parent, Generate generate, std::function<QString()> source)
        : QWidget(parent), generate_(std::move(generate)), source_(std::move(source)) {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 0, 0, 0);
        auto* title = new QLabel(tr("Xem trước PDF đầu ra"), this);
        title->setStyleSheet(QStringLiteral("font-weight:700;"));
        root->addWidget(title);
        auto* bar = new QHBoxLayout;
        auto* go = new QPushButton(tr("Cập nhật"), this);
        automatic_ = new QCheckBox(tr("Tự cập nhật"), this);
        automatic_->setToolTip(tr("Tạo bản PDF tạm sau khi ngừng chỉnh khoảng 0,7 giây. Chỉ tự cập nhật tệp dưới 10 MB, tối đa 30 trang."));
        bar->addWidget(go); bar->addWidget(automatic_); bar->addStretch();
        root->addLayout(bar);
        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true); scroll->setAlignment(Qt::AlignCenter);
        image_ = new QLabel(scroll); image_->setAlignment(Qt::AlignCenter); image_->setWordWrap(true);
        image_->setMinimumSize(220, 180);
        scroll->setWidget(image_); root->addWidget(scroll, 1);
        auto* nav = new QHBoxLayout;
        nav->addWidget(new QLabel(tr("Trang:"), this));
        page_ = new QSpinBox(this); page_->setRange(1, 1); page_->setEnabled(false);
        nav->addWidget(page_); nav->addStretch(1); root->addLayout(nav);
        message_ = new QLabel(this); message_->setWordWrap(true); root->addWidget(message_);
        timer_ = new QTimer(this); timer_->setSingleShot(true); timer_->setInterval(700);
        connect(go, &QPushButton::clicked, this, [this] { timer_->stop(); refresh(false); });
        connect(automatic_, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked) changed(); else timer_->stop();
        });
        connect(timer_, &QTimer::timeout, this, [this] { if (isVisible() && automatic_->isChecked()) refresh(true); });
        connect(page_, &QSpinBox::valueChanged, this, [this](int) { render(); });
        if (!printrender::available() || !temp_.isValid()) {
            go->setEnabled(false); automatic_->setEnabled(false);
            message_->setText(tr("Không có bộ vẽ PDF hoặc thư mục tạm. Vẫn có thể lưu bằng công cụ PDF."));
        } else {
            message_->setText(tr("Chọn tệp và thông số, sau đó cập nhật để xem bản PDF thật. Tệp nguồn không bị sửa."));
        }
    }
    void changed() {
        if (busy_) return;
        fresh_ = false;
        page_->setEnabled(false);
        image_->clear();
        message_->setText(tr("Thông số đã thay đổi. Cần cập nhật xem trước."));
        if (automatic_->isChecked() && isVisible()) timer_->start();
    }
private:
    void refresh(bool autoMode) {
        if (busy_ || !temp_.isValid() || !printrender::available()) return;
        const QString src = source_();
        fresh_ = false; page_->setEnabled(false); image_->clear();
        if (src.isEmpty() || !QFileInfo(src).isFile()) { message_->setText(tr("Hãy chọn PDF nguồn.")); return; }
        if (autoMode) {
            if (QFileInfo(src).size() > 10 * 1024 * 1024) {
                message_->setText(tr("Tệp lớn: bấm Cập nhật để tạo xem trước thủ công.")); return;
            }
            const auto input = dungpdf::info(src);
            if (!input.ok || input.pages > 30 || input.encrypted) {
                message_->setText(tr("Tệp dài, có mật khẩu hoặc chưa đọc được: bấm Cập nhật thủ công.")); return;
            }
        }
        busy_ = true;
        message_->setText(tr("Đang tạo PDF xem trước…"));
        const QString candidate = temp_.filePath(QStringLiteral("preview-%1.pdf").arg(++generation_));
        const Result result = generate_(candidate);
        busy_ = false;
        if (!result.ok) {
            QFile::remove(candidate);
            message_->setText(result.error.isEmpty() ? tr("Không tạo được xem trước.") : result.error);
            return;
        }
        const auto info = printrender::pdfInfo(candidate);
        if (!info.ok || info.pages < 1) {
            QFile::remove(candidate);
            message_->setText(tr("Không đọc được PDF xem trước: %1").arg(info.error));
            return;
        }
        if (!current_.isEmpty()) QFile::remove(current_);
        current_ = candidate; fresh_ = true;
        page_->setRange(1, info.pages); page_->setEnabled(true);
        render();
        message_->setText(tr("Bản PDF tạm đã cập nhật (%1 trang). Bấm Lưu để xuất bản chính thức.").arg(info.pages));
    }
    void render() {
        if (!fresh_) return;
        QString error;
        const QImage img = printrender::renderPage(current_, page_->value(), QSize(460, 540), &error);
        if (img.isNull()) image_->setText(tr("Không vẽ được trang: %1").arg(error));
        else image_->setPixmap(QPixmap::fromImage(img));
    }
    QTemporaryDir temp_;
    Generate generate_;
    std::function<QString()> source_;
    QCheckBox* automatic_{};
    QSpinBox* page_{};
    QLabel *image_{}, *message_{};
    QTimer* timer_{};
    QString current_;
    int generation_ = 0;
    bool busy_ = false, fresh_ = false;
};

const char* kAsciiHint = "Chữ có dấu tiếng Việt chưa dùng được ở mục này (font PDF chuẩn không có chữ có dấu). "
                         "Hãy dùng chữ không dấu, hoặc đóng dấu bằng ảnh đã có chữ tiếng Việt.";
}  // namespace

QWidget* PdfToolsDialog::buildNumber() {
    auto* w = new QWidget(this);
    auto* root = new QHBoxLayout(w);
    auto* form = new QWidget(w);
    auto* f = new QFormLayout(form);
    root->addWidget(form, 1);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_numFile, [this] { refreshPageCount(m_numFile->text(), m_numInfo); }));
    m_numInfo = new QLabel(w);
    f->addRow(QString(), m_numInfo);
    m_numFormat = new QLineEdit(QStringLiteral("{n}"), w);
    m_numFormat->setPlaceholderText(tr("Ví dụ: Trang {n}/{total}"));
    f->addRow(tr("Mẫu chữ:"), m_numFormat);
    m_numPos = new QComboBox(w);
    fillPositions(m_numPos, false);
    f->addRow(tr("Vị trí:"), m_numPos);
    m_numStart = new QSpinBox(w);
    m_numStart->setRange(0, 100000);
    m_numStart->setValue(1);
    f->addRow(tr("Số bắt đầu:"), m_numStart);
    m_numSize = new QDoubleSpinBox(w);
    m_numSize->setRange(4, 72);
    m_numSize->setValue(11);
    f->addRow(tr("Cỡ chữ:"), m_numSize);
    m_numPages = new QLineEdit(QStringLiteral("1-"), w);
    m_numPages->setPlaceholderText(tr("Ví dụ: 1- hoặc 2-10"));
    f->addRow(tr("Các trang được đánh số:"), m_numPages);
    auto* hint = new QLabel(tr("{n} = số trang, {total} = số cuối cùng. Chỉ nhận chữ không dấu."), w);
    hint->setStyleSheet("color:#555");
    f->addRow(hint);
    auto options = [this] {
        NumberOptions o;
        o.format = m_numFormat->text();
        o.pos = Position(m_numPos->currentData().toInt());
        o.start = m_numStart->value();
        o.fontSize = m_numSize->value();
        o.pages = m_numPages->text().trimmed().isEmpty() ? QStringLiteral("1-") : m_numPages->text().trimmed();
        return o;
    };
    auto* preview = new OperationPreview(w, [this, options](const QString& dest) {
        const QString src = m_numFile->text(); const auto o = options();
        return runWithPasswords({src}, [this, src, o, dest] { return addPageNumbers(inputFor(src), o, dest); });
    }, [this] { return m_numFile->text(); });
    root->addWidget(preview, 1);
    for (auto* e : {m_numFile, m_numFormat, m_numPages}) connect(e, &QLineEdit::textChanged, preview, [preview] { preview->changed(); });
    connect(m_numPos, &QComboBox::currentIndexChanged, preview, [preview] { preview->changed(); });
    connect(m_numStart, &QSpinBox::valueChanged, preview, [preview] { preview->changed(); });
    connect(m_numSize, &QDoubleSpinBox::valueChanged, preview, [preview] { preview->changed(); });
    auto* go = new QPushButton(tr("Lưu PDF đánh số..."), w);
    f->addRow(go);
    connect(go, &QPushButton::clicked, this, [this] {
        const QString src = m_numFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Đánh số trang"), tr("Hãy chọn tệp PDF.")); return; }
        const QString fmt = m_numFormat->text();
        if (!isAsciiText(fmt)) { QMessageBox::warning(this, tr("Đánh số trang"), tr(kAsciiHint)); return; }
        NumberOptions o;
        o.format = fmt;
        o.pos = Position(m_numPos->currentData().toInt());
        o.start = m_numStart->value();
        o.fontSize = m_numSize->value();
        o.pages = m_numPages->text().trimmed().isEmpty() ? QStringLiteral("1-") : m_numPages->text().trimmed();
        const QString dest = pickDest(this, src, tr("đánh số"));
        if (dest.isEmpty()) return;
        const bool replace = QFileInfo::exists(dest);
        showResult(runWithPasswords({src}, [this, src, o, dest, replace] { return addPageNumbers(inputFor(src), o, dest, replace); }),
                   tr("Đánh số xong"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildStamp() {
    auto* w = new QWidget(this);
    auto* root = new QHBoxLayout(w);
    auto* form = new QWidget(w);
    auto* f = new QFormLayout(form);
    root->addWidget(form, 1);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_stampFile, [this] { refreshPageCount(m_stampFile->text(), m_stampInfo); }));
    m_stampInfo = new QLabel(w);
    f->addRow(QString(), m_stampInfo);
    m_stampText = new QLineEdit(w);
    m_stampText->setPlaceholderText(tr("Ví dụ: BAN SAO / DRAFT (chữ không dấu)"));
    f->addRow(tr("Chữ đóng dấu:"), m_stampText);
    // Ảnh đóng dấu (JPEG/PNG)
    auto* imgRow = new QWidget(w);
    auto* ih = new QHBoxLayout(imgRow);
    ih->setContentsMargins(0, 0, 0, 0);
    m_stampImage = new QLineEdit(imgRow);
    m_stampImage->setReadOnly(true);
    auto* pick = new QPushButton(tr("Chọn ảnh..."), imgRow);
    auto* clr = new QPushButton(tr("Bỏ"), imgRow);
    ih->addWidget(m_stampImage, 1);
    ih->addWidget(pick);
    ih->addWidget(clr);
    f->addRow(tr("Hoặc ảnh (JPEG/PNG):"), imgRow);
    connect(pick, &QPushButton::clicked, this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, tr("Chọn ảnh đóng dấu"), QString(), tr("Ảnh (*.png *.jpg *.jpeg)"));
        if (!p.isEmpty()) m_stampImage->setText(p);
    });
    connect(clr, &QPushButton::clicked, this, [this] { m_stampImage->clear(); });
    m_stampPos = new QComboBox(w);
    fillPositions(m_stampPos, true);
    m_stampPos->addItem(tr("Tùy chỉnh vị trí"), int(Position::Custom));
    f->addRow(tr("Vị trí:"), m_stampPos);
    m_stampX = new QDoubleSpinBox(w); m_stampY = new QDoubleSpinBox(w);
    for (auto* v : {m_stampX, m_stampY}) { v->setRange(0, 100); v->setValue(50); v->setSuffix(QStringLiteral(" %")); v->setEnabled(false); }
    f->addRow(tr("Từ trái (0–100%):"), m_stampX);
    f->addRow(tr("Từ trên (0–100%):"), m_stampY);
    connect(m_stampPos, &QComboBox::currentIndexChanged, w, [this] {
        const bool custom = m_stampPos->currentData().toInt() == int(Position::Custom);
        m_stampX->setEnabled(custom); m_stampY->setEnabled(custom);
    });
    auto* overlayNote = new QLabel(tr("Chèn chữ/ảnh lên trang (lớp phủ). Không xóa hay sửa chữ gốc; không dùng để che dữ liệu mật."), w);
    overlayNote->setWordWrap(true); f->addRow(overlayNote);
    m_stampSize = new QDoubleSpinBox(w);
    m_stampSize->setRange(6, 200);
    m_stampSize->setValue(48);
    f->addRow(tr("Cỡ chữ:"), m_stampSize);
    m_stampOpacity = new QDoubleSpinBox(w);
    m_stampOpacity->setRange(0.05, 1.0);
    m_stampOpacity->setSingleStep(0.05);
    m_stampOpacity->setValue(0.3);
    f->addRow(tr("Độ đậm (0.05–1):"), m_stampOpacity);
    m_stampWidth = new QDoubleSpinBox(w);
    m_stampWidth->setRange(5, 100);
    m_stampWidth->setValue(30);
    f->addRow(tr("Bề rộng ảnh (% trang):"), m_stampWidth);
    m_stampPages = new QLineEdit(QStringLiteral("1-"), w);
    f->addRow(tr("Các trang:"), m_stampPages);
    auto options = [this] {
        StampOptions o;
        o.text = m_stampText->text(); o.imagePath = m_stampImage->text();
        o.pos = Position(m_stampPos->currentData().toInt()); o.fontSize = m_stampSize->value();
        o.opacity = m_stampOpacity->value(); o.widthPercent = m_stampWidth->value();
        o.xPercent = m_stampX->value(); o.yPercent = m_stampY->value();
        o.pages = m_stampPages->text().trimmed().isEmpty() ? QStringLiteral("1-") : m_stampPages->text().trimmed();
        return o;
    };
    auto* preview = new OperationPreview(w, [this, options](const QString& dest) {
        const QString src = m_stampFile->text(); const auto o = options();
        return runWithPasswords({src}, [this, src, o, dest] { return stamp(inputFor(src), o, dest); });
    }, [this] { return m_stampFile->text(); });
    root->addWidget(preview, 1);
    for (auto* e : {m_stampFile, m_stampText, m_stampImage, m_stampPages}) connect(e, &QLineEdit::textChanged, preview, [preview] { preview->changed(); });
    connect(m_stampPos, &QComboBox::currentIndexChanged, preview, [preview] { preview->changed(); });
    for (auto* s : {m_stampSize, m_stampOpacity, m_stampWidth, m_stampX, m_stampY}) connect(s, &QDoubleSpinBox::valueChanged, preview, [preview] { preview->changed(); });
    auto* go = new QPushButton(tr("Lưu PDF đóng dấu..."), w);
    f->addRow(go);
    connect(go, &QPushButton::clicked, this, [this] {
        const QString src = m_stampFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Đóng dấu"), tr("Hãy chọn tệp PDF.")); return; }
        const QString text = m_stampText->text(), img = m_stampImage->text();
        if (text.isEmpty() && img.isEmpty()) {
            QMessageBox::information(this, tr("Đóng dấu"), tr("Hãy nhập chữ đóng dấu hoặc chọn một ảnh."));
            return;
        }
        if (!text.isEmpty() && !isAsciiText(text)) { QMessageBox::warning(this, tr("Đóng dấu"), tr(kAsciiHint)); return; }
        StampOptions o;
        o.text = text;
        o.imagePath = img;
        o.pos = Position(m_stampPos->currentData().toInt());
        o.fontSize = m_stampSize->value();
        o.opacity = m_stampOpacity->value();
        o.widthPercent = m_stampWidth->value();
        o.xPercent = m_stampX->value(); o.yPercent = m_stampY->value();
        o.pages = m_stampPages->text().trimmed().isEmpty() ? QStringLiteral("1-") : m_stampPages->text().trimmed();
        const QString dest = pickDest(this, src, tr("đóng dấu"));
        if (dest.isEmpty()) return;
        const bool replace = QFileInfo::exists(dest);
        showResult(runWithPasswords({src}, [this, src, o, dest, replace] { return stamp(inputFor(src), o, dest, replace); }),
                   tr("Đóng dấu xong"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildImages() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    v->addWidget(new QLabel(tr("Thêm ảnh JPG/PNG theo thứ tự trang (mỗi ảnh một trang):"), w));
    m_imgList = new QListWidget(w);
    v->addWidget(m_imgList, 1);
    m_imgA4 = new QCheckBox(tr("Đặt mỗi ảnh vào trang A4 (giữ tỉ lệ). Bỏ chọn: trang đúng cỡ ảnh."), w);
    v->addWidget(m_imgA4);
    auto* h = new QHBoxLayout;
    auto* add = new QPushButton(tr("Thêm ảnh..."), w);
    auto* rem = new QPushButton(tr("Bớt"), w);
    auto* up = new QPushButton(tr("Lên"), w);
    auto* dn = new QPushButton(tr("Xuống"), w);
    auto* go = new QPushButton(tr("Tạo PDF..."), w);
    for (auto* b : {add, rem, up, dn}) h->addWidget(b);
    h->addStretch(1);
    h->addWidget(go);
    v->addLayout(h);
    connect(add, &QPushButton::clicked, this, [this] {
        const QStringList fs = QFileDialog::getOpenFileNames(this, tr("Chọn ảnh"), QString(), tr("Ảnh (*.png *.jpg *.jpeg)"));
        for (const QString& f : fs) m_imgList->addItem(f);
    });
    connect(rem, &QPushButton::clicked, this, [this] { delete m_imgList->takeItem(m_imgList->currentRow()); });
    auto move = [this](int d) {
        const int r = m_imgList->currentRow(), n = r + d;
        if (r < 0 || n < 0 || n >= m_imgList->count()) return;
        m_imgList->insertItem(n, m_imgList->takeItem(r));
        m_imgList->setCurrentRow(n);
    };
    connect(up, &QPushButton::clicked, this, [move] { move(-1); });
    connect(dn, &QPushButton::clicked, this, [move] { move(1); });
    connect(go, &QPushButton::clicked, this, [this] {
        QStringList imgs;
        for (int i = 0; i < m_imgList->count(); ++i) imgs << m_imgList->item(i)->text();
        if (imgs.isEmpty()) { QMessageBox::information(this, tr("Ảnh → PDF"), tr("Hãy thêm ít nhất một ảnh.")); return; }
        // Gợi ý tên dựa trên ảnh đầu tiên, đuôi .pdf; không bao giờ ghi đè khi chưa xác nhận.
        const QString dest = QFileDialog::getSaveFileName(this, tr("Lưu PDF thành"),
                                                          suggestOutputPath(QFileInfo(imgs.first()).absolutePath() + QStringLiteral("/") +
                                                                                QFileInfo(imgs.first()).completeBaseName() + QStringLiteral(".pdf"),
                                                                            tr("từ ảnh")),
                                                          tr("Tệp PDF (*.pdf)"));
        if (dest.isEmpty()) return;
        ImageOptions o;
        o.fitA4 = m_imgA4->isChecked();
        const bool replace = QFileInfo::exists(dest);
        showResult(runWithPasswords({}, [imgs, o, dest, replace] { return imagesToPdf(imgs, o, dest, replace); }),
                   tr("Tạo PDF xong"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildBlank() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    f->addRow(tr("Tệp PDF:"), fileRow(&m_blankFile, [this] { refreshPageCount(m_blankFile->text(), m_blankInfo); }));
    m_blankInfo = new QLabel(w);
    f->addRow(QString(), m_blankInfo);
    auto* hint = new QLabel(tr("Chỉ nhận ra trang hoàn toàn không có nội dung. Trang trông trắng nhưng là ảnh quét "
                               "hoặc có ô tô nền trắng sẽ KHÔNG được nhận ra. Bấm \"Kiểm tra\" để xem danh sách trước khi bỏ."), w);
    hint->setWordWrap(true);
    f->addRow(hint);
    auto* check = new QPushButton(tr("Kiểm tra"), w);
    auto* rem = new QPushButton(tr("Bỏ các trang trắng..."), w);
    f->addRow(check);
    f->addRow(rem);
    auto pagesText = [](const QList<int>& b) {
        QStringList l;
        for (int i : b) l << QString::number(i + 1);
        return l.join(QStringLiteral(", "));
    };
    connect(check, &QPushButton::clicked, this, [this, pagesText] {
        const QString src = m_blankFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Trang trắng"), tr("Hãy chọn tệp PDF.")); return; }
        BlankReport r = findBlankPages(inputFor(src));
        if (r.needPassword && askPassword(src)) r = findBlankPages(inputFor(src));
        if (!r.ok) { QMessageBox::warning(this, tr("Trang trắng"), r.error.isEmpty() ? tr("Có lỗi không rõ.") : r.error); return; }
        QString msg = r.blank.isEmpty() ? tr("Không có trang trắng (trong %1 trang).").arg(r.pages)
                                        : tr("Trang trắng (%1/%2): %3").arg(r.blank.size()).arg(r.pages).arg(pagesText(r.blank));
        if (!r.notes.isEmpty()) msg += tr("\n\nLưu ý:\n%1").arg(r.notes.join('\n'));
        QMessageBox::information(this, tr("Trang trắng"), msg);
    });
    connect(rem, &QPushButton::clicked, this, [this] {
        const QString src = m_blankFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Trang trắng"), tr("Hãy chọn tệp PDF.")); return; }
        const QString dest = pickDest(this, src, tr("bỏ trang trắng"));
        if (dest.isEmpty()) return;
        const bool replace = QFileInfo::exists(dest);
        showResult(runWithPasswords({src}, [this, src, dest, replace] { return removeBlankPages(inputFor(src), dest, replace); }),
                   tr("Đã bỏ trang trắng"));
    });
    return w;
}

QWidget* PdfToolsDialog::buildOcr() {
    auto* w = new QWidget(this);
    auto* root = new QVBoxLayout(w);
    auto* form = new QFormLayout;
    auto* page = new QSpinBox(w); page->setRange(1, 1);
    auto* last = new QSpinBox(w); last->setRange(1, 1);
    auto* lang = new QComboBox(w);
    lang->addItem(tr("Tiếng Việt + English"), QStringLiteral("vie+eng"));
    lang->addItem(tr("Tiếng Việt"), QStringLiteral("vie"));
    lang->addItem(tr("English"), QStringLiteral("eng"));
    form->addRow(tr("PDF nguồn:"), fileRow(&m_ocrFile, {}));
    form->addRow(tr("Từ trang:"), page);
    form->addRow(tr("Đến trang:"), last);
    form->addRow(tr("Ngôn ngữ:"), lang);
    root->addLayout(form);
    auto* note = new QLabel(tr("OCR cục bộ theo khoảng trang (tối đa 20 trang mỗi lượt): nhận dạng từ ảnh trang PDF, không gửi ra mạng, không sửa tệp nguồn. Cần cài Tesseract và dữ liệu ngôn ngữ tương ứng. Kết quả có thể sai, hãy đối chiếu bản gốc."), w);
    note->setWordWrap(true); root->addWidget(note);
    auto* status = new QLabel(w); status->setWordWrap(true); root->addWidget(status);
    auto* row = new QHBoxLayout;
    auto* go = new QPushButton(tr("Nhận dạng khoảng trang"), w);
    auto* pdf = new QPushButton(tr("Tạo PDF tìm kiếm được…"), w);
    auto* copy = new QPushButton(tr("Sao chép"), w);
    auto* save = new QPushButton(tr("Lưu TXT…"), w);
    row->addWidget(go); row->addWidget(pdf); row->addWidget(copy); row->addWidget(save); row->addStretch(); root->addLayout(row);
    auto* output = new QPlainTextEdit(w); output->setPlaceholderText(tr("Văn bản OCR sẽ hiện tại đây và có thể sửa.")); root->addWidget(output, 1);
    const QString exe = dungpdf::findTesseract();
    const bool ready = !exe.isEmpty() && printrender::available();
    status->setText(ready ? tr("Tesseract: %1").arg(exe) : tr("Thiếu Tesseract trong PATH hoặc bộ vẽ QtPdf. Cài rồi mở lại công cụ."));
    go->setEnabled(ready);
    pdf->setEnabled(ready);
    pdf->setToolTip(tr("Tạo bản sao ảnh trang với lớp chữ OCR ẩn; không giữ cấu trúc vector hoặc liên kết."));
    connect(m_ocrFile, &QLineEdit::textChanged, w, [=] {
        output->clear();
        const auto inf = printrender::pdfInfo(m_ocrFile->text());
        if (inf.ok) { page->setMaximum(inf.pages); last->setMaximum(inf.pages); page->setValue(1); last->setValue(1); status->setText(tr("PDF có %1 trang.").arg(inf.pages)); }
        else { page->setMaximum(1); last->setMaximum(1); status->setText(inf.error); }
    });
    connect(page, &QSpinBox::valueChanged, w, [=](int value) { if (last->value() < value) last->setValue(value); output->clear(); });
    connect(last, &QSpinBox::valueChanged, w, [=] { output->clear(); });
    connect(lang, &QComboBox::currentIndexChanged, w, [=] { output->clear(); });
    auto runOcr = [=](bool pdfMode) {
        if (!pdfMode) output->clear();
        if (m_ocrFile->text().isEmpty()) { status->setText(tr("Hãy chọn tệp PDF.")); return; }
        const auto source = m_ocrFile->text();
        const auto inf = printrender::pdfInfo(source);
        if (!inf.ok) { status->setText(inf.error); return; }
        const int first = page->value(), end = last->value();
        const auto rangeError = dungpdf::validateOcrRange(first, end, inf.pages);
        if (!rangeError.isEmpty()) { status->setText(rangeError); return; }
        const QString language = lang->currentData().toString();
        QString destination;
        if (pdfMode) {
            if (QMessageBox::question(this,tr("PDF có thể tìm kiếm"),
                tr("Tạo PDF mới chỉ gồm khoảng trang đã chọn, dưới dạng ảnh kèm lớp chữ OCR ẩn. "
                   "Chữ có thể nhận sai; kích thước trang gần đúng, mất vector, liên kết, biểu mẫu, chữ ký và nội dung tương tác. "
                   "Tệp nguồn không đổi. Tiếp tục?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
            destination = QFileDialog::getSaveFileName(this,tr("Lưu PDF OCR mới"),suggestOutputPath(source,tr("OCR")),tr("PDF (*.pdf)"));
            if (destination.isEmpty()) return;
            if (!destination.endsWith(QLatin1String(".pdf"),Qt::CaseInsensitive)) destination += QStringLiteral(".pdf");
            if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) {
                status->setText(tr("Chọn tệp PDF mới chưa tồn tại.")); return;
            }
        }
        QTemporaryDir temp;
        if (!temp.isValid()) { status->setText(tr("Không tạo được thư mục tạm.")); return; }
        const QString image = temp.filePath(QStringLiteral("ocr.png"));
        QStringList recognized;
        QList<Input> pdfPages;
        QString failure;
        const int count = end - first + 1;
        // Separate modal progress window: keep the source dialog disabled throughout.
        QProgressDialog progress(tr("Đang chuẩn bị OCR…"), tr("Dừng"), 0, count);
        progress.setWindowTitle(tr("Nhận dạng PDF"));
        progress.setWindowModality(Qt::ApplicationModal);
        progress.setAutoClose(false);
        progress.setAutoReset(false);
        progress.setMinimumDuration(0);
        setEnabled(false);
        progress.show();
        progress.setValue(0);
        for (int p = first; p <= end; ++p) {
            if (progress.wasCanceled()) break;
            progress.setLabelText(tr("Trang %1 · %2/%3").arg(p).arg(p-first+1).arg(count));
            QString err;
            const QImage img = printrender::renderPage(source, p, QSize(3000,3000), &err);
            if (img.isNull() || !img.save(image, "PNG")) {
                failure = tr("Trang %1: %2").arg(p).arg(err.isEmpty() ? tr("Không lưu được ảnh tạm.") : err);
                break;
            }
            dungpdf::OcrResult result;
            QEventLoop loop;
            const QString pagePdf = temp.filePath(QStringLiteral("page-%1.pdf").arg(p));
            const int dpi = qBound(70,qRound(img.dotsPerMeterX()*0.0254),2400);
            QThread* worker = QThread::create([&] {
                if (!pdfMode) result = dungpdf::recognizeImage(image, language);
                else {
                    result = dungpdf::searchableImagePdf(image, language, dpi, pagePdf);
                    if (result.ok) {
                        const auto verified = dungpdf::info(pagePdf);
                        if (!verified.ok || verified.pages != 1) result = {false,{},tr("PDF OCR một trang không qua kiểm tra.")};
                    }
                }
            });
            connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
            connect(&progress, &QProgressDialog::canceled, worker, &QThread::requestInterruption);
            worker->start();
            loop.exec();
            worker->wait();
            delete worker;
            if (progress.wasCanceled()) break;
            if (!result.ok) { failure = tr("Trang %1: %2").arg(p).arg(result.error); break; }
            if (pdfMode) pdfPages.append(Input{pagePdf,{}});
            else recognized.append(count == 1 ? result.text : tr("===== Trang %1 =====\n%2").arg(p).arg(result.text));
            progress.setValue(pdfMode ? pdfPages.size() : recognized.size());
        }
        const bool cancelled = progress.wasCanceled();
        progress.hide();
        setEnabled(true);
        if (pdfMode) {
            if (cancelled || !failure.isEmpty() || pdfPages.size()!=count) {
                status->setText(cancelled ? tr("Đã hủy. Không tạo PDF đầu ra.") : tr("Không tạo PDF: %1").arg(failure));
                return;
            }
            QStringList paths;
            for (const auto& in : pdfPages) paths << in.path;
            const auto result = runWithPasswords(paths,[&] {
                if (pdfPages.size()==1) return dungpdf::extract(pdfPages.first(),QStringLiteral("1"),destination,false);
                return dungpdf::merge(pdfPages,destination,false);
            });
            status->setText(result.ok ? tr("Đã lưu PDF OCR %1 trang: %2. Kiểm tra lại chữ nhận dạng.").arg(count).arg(destination) : result.error);
            return;
        }
        output->setPlainText(recognized.join(QStringLiteral("\n\n")));
        if (cancelled) status->setText(tr("Đã dừng. Giữ lại %1/%2 trang đã nhận dạng.").arg(recognized.size()).arg(count));
        else if (!failure.isEmpty()) status->setText(tr("%1 · Giữ lại %2/%3 trang đã nhận dạng.").arg(failure).arg(recognized.size()).arg(count));
        else status->setText(tr("Đã nhận dạng %1/%2 trang. Kiểm tra lại chữ và số trước khi sử dụng.").arg(recognized.size()).arg(count));
    };
    connect(go,&QPushButton::clicked,w,[=]{runOcr(false);});
    connect(pdf,&QPushButton::clicked,w,[=]{runOcr(true);});
    connect(copy, &QPushButton::clicked, w, [=] { QApplication::clipboard()->setText(output->toPlainText()); });
    connect(save, &QPushButton::clicked, w, [=] {
        if (output->toPlainText().isEmpty()) { status->setText(tr("Chưa có văn bản để lưu.")); return; }
        const QString dest = QFileDialog::getSaveFileName(this, tr("Lưu văn bản OCR"), QStringLiteral("OCR.txt"), tr("Văn bản UTF-8 (*.txt)"));
        if (dest.isEmpty()) return;
        if (!dest.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive) || QFileInfo::exists(dest)) { status->setText(tr("Chọn tên TXT mới, không ghi đè tệp có sẵn.")); return; }
        QSaveFile file(dest);
        const QByteArray data = output->toPlainText().toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) { status->setText(tr("Không lưu được văn bản.")); return; }
        status->setText(tr("Đã lưu %1").arg(dest));
    });
    return w;
}
