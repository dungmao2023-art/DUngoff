// SPDX-License-Identifier: MPL-2.0
#include "PdfToolsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFileDialog>
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
        for (QLineEdit* e : {dlg.m_splitFile, dlg.m_rotFile, dlg.m_cmpFile, dlg.m_infoFile, dlg.m_numFile, dlg.m_stampFile, dlg.m_blankFile})
            if (e) e->setText(initialPath);
        dlg.refreshPageCount(initialPath, dlg.m_splitInfo);
        dlg.refreshPageCount(initialPath, dlg.m_rotInfo);
        dlg.refreshPageCount(initialPath, dlg.m_numInfo);
        dlg.refreshPageCount(initialPath, dlg.m_stampInfo);
        dlg.refreshPageCount(initialPath, dlg.m_blankInfo);
        dlg.m_mergeList->addItem(initialPath);
    }
    dlg.exec();
}

PdfToolsDialog::PdfToolsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Công cụ PDF"));
    resize(680, 460);
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
    QMessageBox::information(this, okTitle, msg);
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
const char* kAsciiHint = "Chữ có dấu tiếng Việt chưa dùng được ở mục này (font PDF chuẩn không có chữ có dấu). "
                         "Hãy dùng chữ không dấu, hoặc đóng dấu bằng ảnh đã có chữ tiếng Việt.";
}  // namespace

QWidget* PdfToolsDialog::buildNumber() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
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
    auto* go = new QPushButton(tr("Đánh số..."), w);
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
    auto* f = new QFormLayout(w);
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
    f->addRow(tr("Vị trí:"), m_stampPos);
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
    auto* go = new QPushButton(tr("Đóng dấu..."), w);
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
