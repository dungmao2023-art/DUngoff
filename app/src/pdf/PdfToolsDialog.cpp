// SPDX-License-Identifier: MPL-2.0
#include "PdfToolsDialog.h"
#include "PdfOcr.h"
#include "PdfTextEdit.h"
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QColor>
#include <optional>
#include <memory>
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
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QIcon>
#include <QSignalBlocker>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <QTemporaryDir>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSplitter>
#include <QScrollArea>
#include "../print/PrintRender.h"

using namespace dungpdf;

namespace {
// Refresh metadata on each check; QFileInfo caches its observations.
struct PdfSourceSnapshot {
    QString path, canonical;
    qint64 size = -1;
    QDateTime modified, birth;
    explicit PdfSourceSnapshot(const QString& source) : path(source) {
        const QFileInfo f(path);
        if (!f.isFile() || !f.isReadable()) return;
        canonical = f.canonicalFilePath();
        size = f.size();
        modified = f.lastModified();
        birth = f.birthTime();
    }
    bool unchanged() const {
        const QFileInfo f(path);
        return !canonical.isEmpty() && size >= 0 && modified.isValid() &&
            f.isFile() && f.isReadable() && f.canonicalFilePath() == canonical &&
            f.size() == size && f.lastModified() == modified && f.birthTime() == birth;
    }
};
// A single rendered page with true PDF text geometry. No document edits.
class PdfTextCanvas final : public QWidget {
public:
    explicit PdfTextCanvas(QWidget* parent=nullptr):QWidget(parent){
        setFocusPolicy(Qt::StrongFocus);setMinimumSize(220,240);setCursor(Qt::IBeamCursor);
    }
    std::function<void(QPointF,QPointF)> select;
    std::function<void()> copySelection;
    QString selectedText;
    void setPage(printrender::PagePreview p){page=std::move(p);selectedText.clear();selection.clear();drag=false;update();}
    void setSelection(const printrender::PageSelection& s){selection=s.bounds;selectedText=s.text;update();}
protected:
    QRectF imageRect()const{
        if(page.image.isNull())return {};
        const auto size=QSizeF(page.image.size()).scaled(QSizeF(this->size()),Qt::KeepAspectRatio);
        return QRectF(QPointF((width()-size.width())/2,(height()-size.height())/2),size);
    }
    QPointF point(QPointF p)const{
        const auto r=imageRect();
        return QPointF(qBound(0.0,(p.x()-r.left())/r.width(),1.0),qBound(0.0,(p.y()-r.top())/r.height(),1.0));
    }
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.fillRect(rect(),QColor(235,238,243));
        const auto r=imageRect();if(r.isEmpty())return;
        p.drawImage(r,page.image);
        p.save();p.translate(r.topLeft());p.scale(r.width(),r.height());p.setPen(Qt::NoPen);
        p.setBrush(QColor(255,195,0,100));for(const auto& polygon:page.matches)p.drawPolygon(polygon);
        p.setBrush(QColor(35,115,235,110));for(const auto& polygon:selection)p.drawPolygon(polygon);
        p.restore();
        if(drag){p.setBrush(QColor(35,115,235,35));p.setPen(QColor(35,115,235));p.drawRect(QRectF(start,current).normalized());}
    }
    void mousePressEvent(QMouseEvent* e)override{
        if(e->button()!=Qt::LeftButton||!imageRect().contains(e->position()))return;
        setFocus();start=current=e->position();drag=true;selectedText.clear();selection.clear();update();
    }
    void mouseMoveEvent(QMouseEvent* e)override{if(drag){current=e->position();update();}}
    void mouseReleaseEvent(QMouseEvent* e)override{
        if(!drag||e->button()!=Qt::LeftButton)return;
        drag=false;update();if(select&&!imageRect().isEmpty())select(point(start),point(e->position()));
    }
    void keyPressEvent(QKeyEvent* e)override{
        if(e->matches(QKeySequence::Copy)){if(copySelection)copySelection();e->accept();return;}
        QWidget::keyPressEvent(e);
    }
private:
    printrender::PagePreview page;
    QList<QPolygonF> selection;
    QPointF start,current;bool drag=false;
};
QString human(qint64 b) {
    if (b < 1024) return QStringLiteral("%1 B").arg(b);
    if (b < 1024 * 1024) return QStringLiteral("%1 KB").arg(b / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(b / (1024.0 * 1024.0), 0, 'f', 1);
}
}  // namespace

void PdfToolsDialog::run(QWidget* parent, Tab tab, const QString& initialPath) {
    if (tab != Search && tab != Ocr && findQpdf().isEmpty()) {
        QMessageBox::warning(parent, tr("Công cụ PDF"),
                             tr("Không tìm thấy thành phần qpdf đi kèm (thư mục \"qpdf\" cạnh chương trình). "
                                "Hãy cài đặt lại Dũng Office."));
        return;
    }
    PdfToolsDialog dlg(parent);
    dlg.m_tabs->setCurrentIndex(tab);
    if (!initialPath.isEmpty()) {
        for (QLineEdit* e : {dlg.m_splitFile, dlg.m_rotFile, dlg.m_cmpFile, dlg.m_infoFile, dlg.m_numFile, dlg.m_stampFile, dlg.m_blankFile, dlg.m_orderFile, dlg.m_ocrFile, dlg.m_searchFile, dlg.m_noteFormFile})
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
    m_tabs->addTab(buildReorder(), tr("Sắp xếp / xóa trang"));
    m_tabs->addTab(buildOcr(), tr("OCR · TXT / PDF"));
    m_tabs->addTab(buildSearch(),tr("Tìm chữ PDF"));
    m_tabs->addTab(buildNoteForm(),tr("Chú thích / Form"));
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
                                             tr("Nhập mật khẩu để mở \"%1\". Mật khẩu chỉ giữ trong phiên hộp thoại; "
                                                "phiên bản qpdf hiện tại nhận qua đối số tiến trình, có thể bị tiến trình cùng người dùng nhìn thấy.").arg(QFileInfo(path).fileName()),
                                             QLineEdit::Password, QString(), &ok);
    if (!ok) return false;
    m_passwords.insert(path, pw);
    return true;
}

Result PdfToolsDialog::runWithPasswords(const QStringList& paths, const std::function<Result()>& op) {
    Result r;
    QStringList encrypted;
    // Preflight every input before any output is created. Do not retry a writing
    // operation automatically: split may have already published some pages.
    QStringList checked;
    for(const QString& path:paths){
        if(checked.contains(path))continue;
        checked<<path;
        dungpdf::Info i;
        for(int attempt=0;attempt<4;++attempt){
            i=info(path,m_passwords.value(path));
            if(!i.needPassword)break;
            if(attempt==3||!askPassword(path)){
                r.error=tr("Đã dừng: chưa xác minh được mật khẩu PDF nguồn.");return r;
            }
        }
        if(!i.ok){r.error=i.error;r.needPassword=i.needPassword;return r;}
        if(i.encrypted)encrypted<<QFileInfo(path).fileName();
    }
    if(!encrypted.isEmpty()){
        const auto shown=encrypted.mid(0,8).join(QLatin1Char('\n'));
        QString message=tr("%1 tệp nguồn đang được mã hóa:\n%2").arg(encrypted.size()).arg(shown);
        if(encrypted.size()>8)message+=tr("\n… và %1 tệp khác.").arg(encrypted.size()-8);
        message+=tr("\n\nBản xuất có thể không còn mật khẩu hoặc giới hạn quyền. Tệp nguồn giữ nguyên. "
                    "Chỉ tiếp tục nếu bạn có quyền tạo bản sao không được bảo vệ. Tiếp tục xuất?");
        if(QMessageBox::question(this,tr("Xác nhận xuất PDF có mật khẩu"),message,
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){
            r.error=tr("Đã hủy trước khi xuất PDF; chưa tạo tệp kết quả.");return r;
        }
    }
    QEventLoop loop;
    QThread* th=QThread::create([&]{r=op();});
    connect(th,&QThread::finished,&loop,&QEventLoop::quit);
    setEnabled(false);th->start();loop.exec();th->wait();delete th;setEnabled(true);
    return r;
}

void PdfToolsDialog::showResult(const Result& r, const QString& okTitle) {
    if (!r.ok) {
        QString message=r.error.isEmpty()?tr("Có lỗi không rõ."):r.error;
        if(!r.files.isEmpty())message+=tr("\n\nĐã xuất %1 tệp trước khi gặp lỗi:\n%2").arg(r.files.size())
            .arg(r.files.mid(0,8).join(QLatin1Char('\n')));
        if(!r.backup.isEmpty())message+=tr("\n\nBản sao lưu hoàn chỉnh:\n%1").arg(r.backup);
        QMessageBox::warning(this,tr("Không thực hiện được"),message);
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
    label->setText(i.ok ? tr("%1 trang, %2%3").arg(i.pages).arg(human(i.bytes))
        .arg(i.encrypted?tr(" · Có mật khẩu/mã hóa"):QString()) : i.error);
}

// Export tools always create new files. Do not infer overwrite permission from
// existence after the native save dialog: another process may have created the name.
static QString pickDest(QWidget* p, const QString& src, const QString& label) {
    const auto path=QFileDialog::getSaveFileName(p,QObject::tr("Lưu bản PDF mới (không ghi đè)"),suggestOutputPath(src,label),
        QObject::tr("Tệp PDF (*.pdf)"),nullptr,QFileDialog::DontConfirmOverwrite);
    if(path.isEmpty())return {};
    const QFileInfo target(path);
    if(target.exists()||target.isSymLink()){
        QMessageBox::warning(p,QObject::tr("Lưu bản PDF mới"),QObject::tr("Tên này đã tồn tại. Hãy chọn tên khác để giữ nguyên mọi tệp cũ."));
        return {};
    }
    return path;
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
        const bool replace = false;
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
            const bool replace = false;
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
    auto* visual=new QPushButton(tr("Quản lý trang trực quan…"),w);
    visual->setToolTip(tr("Kéo thả, xem hình thu nhỏ, bỏ trang và lưu một bản PDF mới."));
    f->addRow(visual);
    connect(visual,&QPushButton::clicked,this,[this]{
        const QString src=m_orderFile->text();
        if(src.isEmpty()){QMessageBox::information(this,tr("Quản lý trang"),tr("Hãy chọn PDF."));return;}
        const auto source=dungpdf::fingerprint(src);
        const auto inf=info(src,m_passwords.value(src));
        if(!inf.ok||source.size()!=32||inf.encrypted||inf.pages<1||inf.pages>200){
            QMessageBox::information(this,tr("Quản lý trang"),tr("Chỉ hỗ trợ trực quan PDF không mã hóa, từ 1 đến 200 trang. Với tài liệu khác, dùng cách nhập số trang bên dưới."));return;
        }
        const PdfSourceSnapshot snapshot(src);
        if(!snapshot.unchanged()||dungpdf::fingerprint(src)!=source){QMessageBox::warning(this,tr("Quản lý trang"),tr("Tệp nguồn đã thay đổi."));return;}
        QDialog dlg(this);dlg.setWindowTitle(tr("Quản lý trang PDF"));dlg.resize(870,650);
        auto* root=new QVBoxLayout(&dlg);
        auto* hint=new QLabel(tr("Kéo thả để đổi thứ tự. Chọn một hoặc nhiều trang để bỏ. Tệp gốc không thay đổi."),&dlg);
        hint->setWordWrap(true);root->addWidget(hint);
        auto* split=new QSplitter(Qt::Horizontal,&dlg);root->addWidget(split,1);
        auto* pages=new QListWidget(split);
        pages->setIconSize(QSize(92,120));pages->setSelectionMode(QAbstractItemView::ExtendedSelection);
        pages->setDragDropMode(QAbstractItemView::InternalMove);pages->setDefaultDropAction(Qt::MoveAction);
        pages->setDragDropOverwriteMode(false);pages->setAlternatingRowColors(true);
        pages->setAccessibleName(tr("Danh sách trang PDF theo thứ tự xuất"));
        auto* preview=new QLabel(split);preview->setAlignment(Qt::AlignCenter);preview->setMinimumSize(260,320);
        preview->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);preview->setWordWrap(true);
        split->addWidget(pages);split->addWidget(preview);split->setStretchFactor(0,1);split->setStretchFactor(1,2);
        QHash<int,QIcon> cached;
        auto restore=[&]{
            QSignalBlocker guard(pages);pages->clear();
            for(int k=1;k<=inf.pages;++k){
                auto* item=new QListWidgetItem(tr("Trang gốc %1").arg(k),pages);
                item->setData(Qt::UserRole,k);item->setIcon(cached.value(k));
                item->setSizeHint(QSize(160,126));
            }
        };
        restore();
        auto* tools=new QHBoxLayout;root->addLayout(tools);
        auto* up=new QPushButton(tr("↑ Lên"),&dlg);auto* down=new QPushButton(tr("↓ Xuống"),&dlg);
        auto* remove=new QPushButton(tr("Bỏ trang đã chọn"),&dlg);auto* reset=new QPushButton(tr("Khôi phục tất cả"),&dlg);
        for(auto* b:{up,down,remove,reset})tools->addWidget(b);
        tools->addStretch();
        auto* status=new QLabel(&dlg);root->addWidget(status);
        auto* bb=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dlg);
        bb->button(QDialogButtonBox::Save)->setText(tr("Lưu PDF mới…"));root->addWidget(bb);
        bool stale=false;
        auto refresh=[&]{
            const auto selected=pages->selectedItems();const int row=pages->currentRow();
            const bool one=selected.size()==1&&row>=0;
            up->setEnabled(!stale&&one&&row>0);down->setEnabled(!stale&&one&&row+1<pages->count());
            remove->setEnabled(!stale&&!selected.isEmpty()&&selected.size()<pages->count());
            reset->setEnabled(!stale);bb->button(QDialogButtonBox::Save)->setEnabled(!stale&&pages->count()>0);
            status->setText(stale?tr("Nguồn đã thay đổi. Đóng và mở lại công cụ."):
                tr("Giữ %1/%2 trang · đã chọn %3").arg(pages->count()).arg(inf.pages).arg(selected.size()));
        };
        auto markStale=[&]{stale=true;preview->clear();preview->setText(tr("Tệp nguồn đã thay đổi."));refresh();};
        auto showPage=[&]{
            if(stale||!snapshot.unchanged()){markStale();return;}
            const auto* item=pages->currentItem();if(!item){preview->clear();return;}
            const int page=item->data(Qt::UserRole).toInt();
            QString error;
            const auto img=printrender::renderPage(src,page,QSize(640,680),&error);
            if(!snapshot.unchanged()){markStale();return;}
            if(img.isNull()){preview->setText(tr("Trang %1 · %2").arg(page).arg(error));return;}
            preview->setPixmap(QPixmap::fromImage(img).scaled(preview->size().boundedTo(QSize(640,680)),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        };
        connect(pages,&QListWidget::currentRowChanged,&dlg,[&](int){showPage();refresh();});
        connect(pages,&QListWidget::itemSelectionChanged,&dlg,refresh);
        connect(pages->model(),&QAbstractItemModel::rowsMoved,&dlg,[&]{refresh();});
        auto move=[&](int delta){
            const int from=pages->currentRow(),to=from+delta;
            if(stale||pages->selectedItems().size()!=1||from<0||to<0||to>=pages->count())return;
            {QSignalBlocker guard(pages);auto* item=pages->takeItem(from);pages->insertItem(to,item);pages->setCurrentRow(to);}
            refresh();
        };
        connect(up,&QPushButton::clicked,&dlg,[&]{move(-1);});
        connect(down,&QPushButton::clicked,&dlg,[&]{move(1);});
        connect(remove,&QPushButton::clicked,&dlg,[&]{
            const auto selected=pages->selectedItems();if(stale||selected.isEmpty()||selected.size()>=pages->count())return;
            const int row=pages->currentRow();
            {QSignalBlocker guard(pages);for(auto* item:selected)delete pages->takeItem(pages->row(item));pages->setCurrentRow(qBound(0,row,pages->count()-1));}
            showPage();refresh();
        });
        connect(reset,&QPushButton::clicked,&dlg,[&]{if(stale)return;restore();pages->setCurrentRow(0);refresh();});
        // Load thumbnails in small event-loop steps. No unbounded image cache.
        QTimer thumbTimer;thumbTimer.setInterval(35);int next=1;
        connect(&thumbTimer,&QTimer::timeout,&dlg,[&]{
            if(stale||!snapshot.unchanged()){thumbTimer.stop();markStale();return;}
            if(next>inf.pages){thumbTimer.stop();return;}
            const int page=next++;
            const auto img=printrender::renderPage(src,page,QSize(92,120));
            if(!snapshot.unchanged()){thumbTimer.stop();markStale();return;}
            if(img.isNull())return;
            const QIcon icon(QPixmap::fromImage(img));cached.insert(page,icon);
            for(int k=0;k<pages->count();++k)if(pages->item(k)->data(Qt::UserRole).toInt()==page){pages->item(k)->setIcon(icon);break;}
        });
        connect(bb,&QDialogButtonBox::accepted,&dlg,[&]{if(!stale&&pages->count()>0)dlg.accept();});
        connect(bb,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        pages->setCurrentRow(0);refresh();if(printrender::available())thumbTimer.start();
        if(dlg.exec()!=QDialog::Accepted)return;
        thumbTimer.stop();
        QList<int> keep;for(int k=0;k<pages->count();++k)keep.append(pages->item(k)->data(Qt::UserRole).toInt());
        if(!snapshot.unchanged()||dungpdf::fingerprint(src)!=source){
            QMessageBox::warning(this,tr("Quản lý trang"),tr("Nguồn đã thay đổi. Không xuất bản sao."));return;
        }
        if(keep.size()<inf.pages&&QMessageBox::question(this,tr("Xác nhận bỏ trang"),
            tr("Giữ %1/%2 trang theo thứ tự mới? Tệp nguồn không thay đổi. Đây không phải xóa thông tin mật.").arg(keep.size()).arg(inf.pages),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        const QString dest=pickDest(this,src,tr("quản lý trang"));if(dest.isEmpty())return;
        showResult(runWithPasswords({src},[=]{return dungpdf::organizePages(inputFor(src),keep,inf.pages,source,dest);}),tr("Quản lý trang xong"));
    });
    m_orderSpec = new QLineEdit(w);
    m_orderSpec->setPlaceholderText(tr("VD: 3,1-2,4-"));
    auto* mode=new QComboBox(w);
    mode->addItem(tr("Sắp xếp lại (giữ mọi trang)"),false);
    mode->addItem(tr("Xóa các trang đã nhập"),true);
    f->addRow(tr("Thao tác:"),mode);
    f->addRow(tr("Danh sách trang:"),m_orderSpec);
    auto* note = new QLabel(tr("Nhập các trang theo thứ tự mong muốn. Mỗi trang phải xuất hiện đúng một lần. Không sửa tệp gốc."), w);
    note->setWordWrap(true);
    f->addRow(note);
    auto* go = new QPushButton(tr("Lưu bản đã sắp xếp..."), w);
    f->addRow(go);
    connect(mode,QOverload<int>::of(&QComboBox::currentIndexChanged),w,[=]{
        const bool remove=mode->currentData().toBool();
        m_orderSpec->clear();
        m_orderSpec->setPlaceholderText(remove?tr("VD: 2,5-7"):tr("VD: 3,1-2,4-"));
        note->setText(remove?tr("Nhập những trang cần xóa. Trang còn lại giữ thứ tự gốc. Không được xóa tất cả. Đây không phải công cụ xóa thông tin mật (redaction)."):
            tr("Nhập thứ tự mới, mỗi trang đúng một lần. Không sửa tệp gốc."));
        go->setText(remove?tr("Lưu bản đã xóa trang..."):tr("Lưu bản đã sắp xếp..."));
    });
    connect(go,&QPushButton::clicked,this,[this,mode]{
        const QString src=m_orderFile->text(),spec=m_orderSpec->text().trimmed();
        const bool remove=mode->currentData().toBool();
        if(src.isEmpty()||spec.isEmpty()){QMessageBox::information(this,tr("Trang PDF"),tr("Hãy chọn PDF và nhập danh sách trang."));return;}
        int expectedPages=0;
        if(remove){
            auto inf=info(src,m_passwords.value(src));
            if(inf.needPassword&&askPassword(src))inf=info(src,m_passwords.value(src));
            if(!inf.ok){QMessageBox::warning(this,tr("Xóa trang"),inf.error);return;}
            const auto plan=planRemovePages(spec,inf.pages);
            if(!plan.ok){QMessageBox::warning(this,tr("Xóa trang"),plan.error);return;}
            expectedPages=inf.pages;
            const auto shown=plan.keepSpec.size()>160?plan.keepSpec.left(160)+QStringLiteral("…"):plan.keepSpec;
            if(QMessageBox::question(this,tr("Xác nhận xóa trang"),
                tr("Xóa %1/%2 trang, giữ %3 trang (%4). Chỉ tạo bản PDF mới, tệp gốc giữ nguyên. Tiếp tục?")
                    .arg(plan.removed).arg(expectedPages).arg(plan.retained).arg(shown),
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        }
        const QString dest=pickDest(this,src,remove?tr("xóa trang"):tr("sắp xếp"));
        if(dest.isEmpty())return;
        showResult(runWithPasswords({src},[this,src,spec,dest,remove,expectedPages]{
            return remove?removePages(inputFor(src),spec,expectedPages,dest):reorder(inputFor(src),spec,dest,false);
        }),remove?tr("Xóa trang xong"):tr("Sắp xếp xong"));
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
        const bool replace = false;
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
        const bool replace = false;
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
    auto* hint = new QLabel(tr("{n} = số trang, {total} = số cuối cùng. Hỗ trợ chữ tiếng Việt một dòng nếu có phông phù hợp."), w);
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
        NumberOptions o;
        o.format = fmt;
        o.pos = Position(m_numPos->currentData().toInt());
        o.start = m_numStart->value();
        o.fontSize = m_numSize->value();
        o.pages = m_numPages->text().trimmed().isEmpty() ? QStringLiteral("1-") : m_numPages->text().trimmed();
        const QString dest = pickDest(this, src, tr("đánh số"));
        if (dest.isEmpty()) return;
        const bool replace = false;
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
    m_stampText->setPlaceholderText(tr("Ví dụ: BẢN SAO / DỰ THẢO"));
    m_stampText->setMaxLength(200);
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
        const bool replace = false;
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
        const QString dest=pickDest(this,QFileInfo(imgs.first()).absolutePath()+QStringLiteral("/")+
            QFileInfo(imgs.first()).completeBaseName()+QStringLiteral(".pdf"),tr("từ ảnh"));
        if (dest.isEmpty()) return;
        ImageOptions o;
        o.fitA4 = m_imgA4->isChecked();
        const bool replace = false;
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
    connect(rem, &QPushButton::clicked, this, [this, pagesText] {
        const QString src = m_blankFile->text();
        if (src.isEmpty()) { QMessageBox::information(this, tr("Trang trắng"), tr("Hãy chọn tệp PDF.")); return; }
        const PdfSourceSnapshot snapshot(src);
        BlankReport confirmed = findBlankPages(inputFor(src));
        if (confirmed.needPassword && askPassword(src)) confirmed = findBlankPages(inputFor(src));
        if (!confirmed.ok || !snapshot.unchanged()) {
            QMessageBox::warning(this,tr("Trang trắng"),confirmed.error.isEmpty()?tr("Tệp nguồn đã thay đổi hoặc không đọc được."):confirmed.error);
            return;
        }
        if (confirmed.blank.isEmpty() || confirmed.blank.size() >= confirmed.pages) {
            QMessageBox::information(this,tr("Trang trắng"),confirmed.blank.isEmpty()?tr("Không có trang trắng để bỏ."):tr("Không bỏ mọi trang của tài liệu."));
            return;
        }
        const QString prompt=tr("Bỏ %1/%2 trang: %3?\nCòn %4 trang. Bản gốc giữ nguyên.\n\n%5")
            .arg(confirmed.blank.size()).arg(confirmed.pages).arg(pagesText(confirmed.blank))
            .arg(confirmed.pages-confirmed.blank.size()).arg(confirmed.notes.join('\n'));
        if(QMessageBox::question(this,tr("Xác nhận bỏ trang trắng"),prompt,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        const QString dest = pickDest(this, src, tr("bỏ trang trắng"));
        if (dest.isEmpty()) return;
        if(!snapshot.unchanged()||m_blankFile->text()!=src){
            QMessageBox::warning(this,tr("Trang trắng"),tr("Tệp nguồn đã thay đổi. Hãy kiểm tra lại trước khi xuất."));
            return;
        }
        showResult(runWithPasswords({src}, [this, src, dest, snapshot, confirmed] {
            if(!snapshot.unchanged()){
                Result r;r.error=tr("Tệp nguồn đã thay đổi; chưa xuất bản sao.");return r;
            }
            return removeBlankPages(inputFor(src), dest, false, &confirmed);
        }),tr("Đã bỏ trang trắng"));
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
    auto* layout = new QComboBox(w);
    layout->addItem(tr("Tự động (nhiều cột/trang văn bản)"), 3);
    layout->addItem(tr("Một khối văn bản"), 6);
    layout->addItem(tr("Một dòng chữ"), 7);
    layout->addItem(tr("Chữ rải rác (biểu mẫu, nhãn)"), 11);
    layout->setToolTip(tr("Chọn theo bố cục ảnh. Không tự động nhận bảng hay khôi phục định dạng."));
    form->addRow(tr("Bố cục nhận dạng:"), layout);
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
    const bool pdfReady = ready && !findQpdf().isEmpty();
    pdf->setEnabled(pdfReady);
    pdf->setToolTip(pdfReady
        ? tr("Tạo bản sao ảnh trang với lớp chữ OCR ẩn; không giữ cấu trúc vector hoặc liên kết.")
        : tr("Tạo PDF cần QtPdf, Tesseract và qpdf. Nhận dạng TXT không cần qpdf."));
    connect(m_ocrFile, &QLineEdit::textChanged, w, [=] {
        output->clear();
        const auto inf = printrender::pdfInfo(m_ocrFile->text());
        if (inf.ok) { page->setMaximum(inf.pages); last->setMaximum(inf.pages); page->setValue(1); last->setValue(1); status->setText(tr("PDF có %1 trang.").arg(inf.pages)); }
        else { page->setMaximum(1); last->setMaximum(1); status->setText(inf.error); }
    });
    connect(page, &QSpinBox::valueChanged, w, [=](int value) { if (last->value() < value) last->setValue(value); output->clear(); });
    connect(last, &QSpinBox::valueChanged, w, [=] { output->clear(); });
    connect(lang, &QComboBox::currentIndexChanged, w, [=] { output->clear(); });
    connect(layout, &QComboBox::currentIndexChanged, w, [=] { output->clear(); });
    auto runOcr = [=](bool pdfMode) {
        if (pdfMode && findQpdf().isEmpty()) {
            status->setText(tr("Thiếu qpdf để tạo PDF. Bạn vẫn có thể nhận dạng và lưu TXT."));
            return;
        }
        if (!pdfMode) output->clear();
        if (m_ocrFile->text().isEmpty()) { status->setText(tr("Hãy chọn tệp PDF.")); return; }
        const auto source = m_ocrFile->text();
        const PdfSourceSnapshot snapshot(source);
        if (!snapshot.unchanged()) { status->setText(tr("Không xác minh được PDF nguồn.")); return; }
        const auto inf = printrender::pdfInfo(source);
        if (!inf.ok) { status->setText(inf.error); return; }
        const int first = page->value(), end = last->value();
        const auto rangeError = dungpdf::validateOcrRange(first, end, inf.pages);
        if (!rangeError.isEmpty()) { status->setText(rangeError); return; }
        const QString language = lang->currentData().toString();
        const int segmentation = layout->currentData().toInt();
        if (!dungpdf::validOcrLayout(segmentation)) { status->setText(tr("Bố cục OCR không hợp lệ.")); return; }
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
        if (!snapshot.unchanged()) { status->setText(tr("PDF nguồn đã thay đổi; hãy chọn lại tệp.")); return; }
        QTemporaryDir temp;
        if (!temp.isValid()) { status->setText(tr("Không tạo được thư mục tạm.")); return; }
        const QString image = temp.filePath(QStringLiteral("ocr.png"));
        QStringList recognized;
        QList<Input> pdfPages;
        QString failure;
        bool sourceChanged = false;
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
            if (!snapshot.unchanged()) { sourceChanged = true; break; }
            progress.setLabelText(tr("Trang %1 · %2/%3").arg(p).arg(p-first+1).arg(count));
            QString err;
            const QImage img = printrender::renderPage(source, p, QSize(3000,3000), &err);
            if (!snapshot.unchanged()) { sourceChanged = true; break; }
            if (img.isNull() || !img.save(image, "PNG")) {
                failure = tr("Trang %1: %2").arg(p).arg(err.isEmpty() ? tr("Không lưu được ảnh tạm.") : err);
                break;
            }
            dungpdf::OcrResult result;
            QEventLoop loop;
            const QString pagePdf = temp.filePath(QStringLiteral("page-%1.pdf").arg(p));
            const int dpi = qBound(70,qRound(img.dotsPerMeterX()*0.0254),2400);
            QThread* worker = QThread::create([&] {
                if (!pdfMode) result = dungpdf::recognizeImage(image, language, segmentation);
                else {
                    result = dungpdf::searchableImagePdf(image, language, dpi, pagePdf, segmentation);
                    if (result.ok && !QThread::currentThread()->isInterruptionRequested()) {
                        const auto verified = dungpdf::info(pagePdf);
                        if (!verified.ok || verified.pages != 1) result = {false,{},tr("PDF OCR một trang không qua kiểm tra.")};
                    }
                }
            });
            connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
            connect(&progress, &QProgressDialog::canceled, worker, &QThread::requestInterruption);
            // The progress dialog may have been cancelled before the worker started.
            if (progress.wasCanceled()) { delete worker; break; }
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
        if (sourceChanged || !snapshot.unchanged()) {
            status->setText(tr("PDF nguồn đã thay đổi trong khi OCR. Đã bỏ kết quả lượt này; hãy chọn lại tệp."));
            return;
        }
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
        if (!dest.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive) || QFileInfo::exists(dest) || QFileInfo(dest).isSymLink()) { status->setText(tr("Chọn tên TXT mới, không ghi đè tệp có sẵn.")); return; }
        QFile file(dest);
        const QByteArray data = output->toPlainText().toUtf8();
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { status->setText(tr("Không tạo được tệp TXT mới: %1").arg(file.errorString())); return; }
        if (file.write(data) != data.size() || !file.flush()) {
            file.close();
            const bool removed = file.remove();
            status->setText(removed ? tr("Không lưu được văn bản.") : tr("Ghi TXT không đầy đủ; không sử dụng tệp vừa tạo."));
            return;
        }
        file.close();
        status->setText(tr("Đã lưu %1").arg(dest));
    });
    return w;
}

QWidget* PdfToolsDialog::buildSearch() {
    auto* w=new QWidget(this);
    auto* root=new QVBoxLayout(w);
    auto* form=new QFormLayout;
    auto* first=new QSpinBox(w);first->setRange(1,1);
    auto* last=new QSpinBox(w);last->setRange(1,1);
    auto* query=new QLineEdit(w);query->setMaxLength(256);
    query->setPlaceholderText(tr("Nhập từ hoặc cụm từ"));
    auto* match=new QCheckBox(tr("Phân biệt hoa/thường"),w);
    form->addRow(tr("PDF nguồn:"),fileRow(&m_searchFile,{}));
    form->addRow(tr("Từ khóa:"),query);
    form->addRow(tr("Từ trang:"),first);form->addRow(tr("Đến trang:"),last);
    form->addRow(match);root->addLayout(form);
    auto* note=new QLabel(tr("Đọc và kéo chọn chữ trực tiếp trên trang, không cần tìm từ khóa. Tìm kiếm trong lớp chữ (tối đa 200 trang/lượt). PDF chỉ có ảnh cần OCR."),w);
    note->setWordWrap(true);root->addWidget(note);
    auto* actions=new QHBoxLayout;
    auto* browse=new QPushButton(tr("Đọc / sửa trực tiếp"),w);
    auto* go=new QPushButton(tr("Tìm trong PDF"),w);
    actions->addWidget(browse);actions->addWidget(go);actions->addStretch();
    root->addLayout(actions);
    browse->setEnabled(printrender::available());go->setEnabled(printrender::available());
    auto* status=new QLabel(w);status->setWordWrap(true);status->setTextFormat(Qt::PlainText);root->addWidget(status);
    auto* list=new QListWidget(w);list->setAccessibleName(tr("Trang PDF hoặc kết quả tìm"));
    list->setMaximumHeight(116);root->addWidget(list);
    auto* nav=new QHBoxLayout;
    auto* previous=new QPushButton(tr("← Trước"),w);auto* next=new QPushButton(tr("Sau →"),w);
    auto* jump=new QSpinBox(w);jump->setRange(1,1);jump->setPrefix(tr("Trang "));
    jump->setAccessibleName(tr("Số trang cần mở"));
    auto* openPage=new QPushButton(tr("Đến trang"),w);
    previous->setEnabled(false);next->setEnabled(false);jump->setEnabled(false);openPage->setEnabled(false);
    nav->addWidget(previous);nav->addWidget(next);nav->addStretch();nav->addWidget(jump);nav->addWidget(openPage);root->addLayout(nav);
    auto* text=new QPlainTextEdit(w);text->setReadOnly(true);
    text->setPlaceholderText(tr("Chọn trang để đọc, kéo chọn chữ trên ảnh PDF hoặc sao chép chữ trích xuất."));
    text->setToolTip(tr("Chữ trích xuất được chuẩn hóa Unicode NFC; tô sáng tối đa 1.000 vị trí trên trang, không sửa PDF."));
    auto* canvas=new PdfTextCanvas(w);
    auto* split=new QSplitter(Qt::Horizontal,w);split->addWidget(canvas);split->addWidget(text);split->setStretchFactor(0,2);split->setStretchFactor(1,1);root->addWidget(split,3);
    auto* bottom=new QHBoxLayout;root->addLayout(bottom);
    auto* copy=new QPushButton(tr("Sao chép chữ đã chọn"),w);copy->setEnabled(false);bottom->addWidget(copy);
    auto snapshot=std::make_shared<std::optional<PdfSourceSnapshot>>();
    auto* edit=new QPushButton(tr("Sửa chữ gốc đã chọn"),w);bottom->addWidget(edit);bottom->addStretch();edit->setEnabled(false);
    if(!pdfedit::available())edit->setToolTip(tr("Bản cài chưa có PDFium Editing SDK."));
    auto browsing=std::make_shared<bool>(false);
    auto refreshNav=[=]{
        const int row=list->currentRow();
        const int current=row>=0?list->item(row)->data(Qt::UserRole).toInt():0;
        previous->setEnabled(row>0||(*browsing&&current>1));
        next->setEnabled(row>=0&&(row+1<list->count()||(*browsing&&current<jump->maximum())));
        if(row>=0){QSignalBlocker guard(jump);jump->setValue(list->item(row)->data(Qt::UserRole).toInt());}
    };
    auto clear=[=]{snapshot->reset();*browsing=false;list->clear();text->setExtraSelections({});text->clear();canvas->setPage({});copy->setEnabled(false);edit->setEnabled(false);status->clear();refreshNav();};
    auto openRange=[=](int page){
        clear();
        const QString path=m_searchFile->text();
        const PdfSourceSnapshot source(path);
        const auto info=printrender::pdfInfo(path);
        if(!info.ok||!source.unchanged()){status->setText(info.error.isEmpty()?tr("PDF nguồn đã thay đổi."):info.error);return;}
        if(page<1||page>info.pages)return;
        // Populate a bounded page window. Jump can reach any page of the
        // document, including pages beyond the first 200.
        const int a=((page-1)/200)*200+1,b=qMin(a+199,info.pages);
        *snapshot=source;*browsing=true;
        {QSignalBlocker guard(list);
            for(int p=a;p<=b;++p){auto* item=new QListWidgetItem(tr("Trang %1").arg(p),list);item->setData(Qt::UserRole,p);}
        }
        status->setText(tr("Đọc trực tiếp · trang %1–%2 / %3. Kéo chọn chữ trên trang để sao chép hoặc sửa.").arg(a).arg(b).arg(info.pages));
        jump->setMaximum(info.pages);jump->setEnabled(true);openPage->setEnabled(true);
        list->setCurrentRow(page-a);refreshNav();
    };
    connect(browse,&QPushButton::clicked,w,[=]{openRange(qMax(1,jump->value()));});
    connect(openPage,&QPushButton::clicked,w,[=]{openRange(jump->value());});
    connect(previous,&QPushButton::clicked,w,[=]{
        const int row=list->currentRow();if(row>0)list->setCurrentRow(row-1);
        else if(*browsing&&row==0&&jump->value()>1)openRange(jump->value()-1);
    });
    connect(next,&QPushButton::clicked,w,[=]{
        const int row=list->currentRow();if(row>=0&&row+1<list->count())list->setCurrentRow(row+1);
        else if(*browsing&&row>=0&&jump->value()<jump->maximum())openRange(jump->value()+1);
    });
    auto rejectStale=[=]{clear();status->setText(tr("PDF nguồn đã thay đổi. Hãy tìm lại."));};
    auto copyCurrent=[=]{
        if(!snapshot->has_value()||!snapshot->value().unchanged()){rejectStale();return;}
        if(!canvas->selectedText.isEmpty())QApplication::clipboard()->setText(canvas->selectedText);
    };
    connect(copy,&QPushButton::clicked,w,copyCurrent);
    canvas->copySelection=copyCurrent;
    // Invalidate the preview as soon as a normal external file update becomes
    // observable. Recheck at point of use even if the timer has not run.
    auto* freshness=new QTimer(w);
    freshness->setInterval(750);
    connect(freshness,&QTimer::timeout,w,[=]{
        if(w->isVisible()&&snapshot->has_value()&&!snapshot->value().unchanged())rejectStale();
    });
    freshness->start();
    connect(edit,&QPushButton::clicked,w,[=]{
        if(!snapshot->has_value()||!snapshot->value().unchanged()){rejectStale();return;}
        if(!list->currentItem()||canvas->selectedText.isEmpty())return;
        const auto source=snapshot->value();const int page=list->currentItem()->data(Qt::UserRole).toInt();const QString old=canvas->selectedText;
        bool ok=false;const auto replacement=QInputDialog::getText(this,tr("Sửa chữ PDF"),tr("Thay nguyên đối tượng chữ đã chọn. Hỗ trợ tiếng Việt nếu font gốc có ký tự tương ứng. Lưu bản sao PDF mới."),QLineEdit::Normal,old,&ok);
        if(!ok)return;
        if(!source.unchanged()){rejectStale();return;}
        const auto dest=pickDest(this,source.path,tr("Sửa chữ"));if(dest.isEmpty())return;
        if(!source.unchanged()){rejectStale();return;}
        showResult(runWithPasswords({source.path},[=]{return pdfedit::replaceObject(source.path,page,old,replacement,dest);}),tr("Sửa chữ PDF"));
    });

    canvas->select=[=](QPointF from,QPointF to){
        const auto* item=list->currentItem();
        if(!item){canvas->setSelection({});copy->setEnabled(false);edit->setEnabled(false);return;}
        if(!snapshot->has_value()||!snapshot->value().unchanged()){rejectStale();return;}
        const auto selected=printrender::selectText(snapshot->value().path,item->data(Qt::UserRole).toInt(),from,to);
        if(!snapshot->value().unchanged()){rejectStale();return;}
        canvas->setSelection(selected);copy->setEnabled(!selected.text.isEmpty());edit->setEnabled(pdfedit::available()&&!selected.text.isEmpty());
        if(!selected.error.isEmpty())status->setText(selected.error);
    };
    connect(m_searchFile,&QLineEdit::textChanged,w,[=]{
        clear();
        const auto info=printrender::pdfInfo(m_searchFile->text());
        first->setMaximum(qMax(1,info.pages));last->setMaximum(qMax(1,info.pages));
        first->setValue(1);last->setValue(qBound(1,info.pages,200));
        jump->setMaximum(qMax(1,info.pages));jump->setValue(1);
        jump->setEnabled(info.ok);openPage->setEnabled(info.ok);
        status->setText(info.ok?tr("%1 trang.").arg(info.pages):info.error);
        if(info.ok)openRange(1);
    });
    connect(query,&QLineEdit::textChanged,w,clear);
    connect(first,&QSpinBox::valueChanged,w,clear);
    connect(last,&QSpinBox::valueChanged,w,clear);
    connect(match,&QCheckBox::toggled,w,clear);
    connect(go,&QPushButton::clicked,w,[=]{
        clear();
        const QString path=m_searchFile->text(),needle=query->text();
        const int a=first->value(),b=last->value();const bool cs=match->isChecked();
        const PdfSourceSnapshot source(path);
        printrender::TextSearch result;
        QProgressDialog progress(tr("Đang tìm chữ PDF…"),tr("Dừng"),0,0);
        progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);
        QEventLoop loop;
        QThread* worker=QThread::create([&]{result=printrender::searchText(path,needle,a,b,cs);});
        connect(worker,&QThread::finished,&loop,&QEventLoop::quit);
        connect(&progress,&QProgressDialog::canceled,worker,&QThread::requestInterruption);
        setEnabled(false);progress.show();worker->start();loop.exec();worker->wait();delete worker;
        const bool cancelled=progress.wasCanceled();progress.hide();setEnabled(true);
        if(cancelled||!result.ok){status->setText(cancelled?tr("Đã dừng tìm kiếm."):result.error);return;}
        if(!source.unchanged()||m_searchFile->text()!=path||query->text()!=needle||first->value()!=a||last->value()!=b||match->isChecked()!=cs){
            status->setText(tr("Nguồn hoặc điều kiện tìm đã thay đổi. Hãy tìm lại."));return;
        }
        *snapshot=source;
        for(const auto& hit:result.hits){
            auto* item=new QListWidgetItem(tr("Trang %1 · %2 lần · %3").arg(hit.page).arg(hit.occurrences).arg(hit.snippet),list);
            item->setData(Qt::UserRole,hit.page);
        }
        status->setText(tr("%1 trang có kết quả / %2 trang đã đọc. %3 trang có lớp chữ.").arg(result.hits.size()).arg(result.scanned).arg(result.textPages));
        if(!result.textPages)status->setText(status->text()+tr(" Hãy thử OCR."));
        if(list->count())list->setCurrentRow(0);
    });
    connect(query,&QLineEdit::returnPressed,go,&QPushButton::click);
    connect(list,&QListWidget::currentItemChanged,w,[=](QListWidgetItem* item,QListWidgetItem*){
        text->setExtraSelections({});text->clear();canvas->setPage({});copy->setEnabled(false);edit->setEnabled(false);refreshNav();if(!item)return;
        if(!snapshot->has_value()||!snapshot->value().unchanged()){
            rejectStale();return;
        }
        QString error;
        const QString extracted=printrender::pageText(snapshot->value().path,item->data(Qt::UserRole).toInt(),&error);
        if(!snapshot->value().unchanged()){
            rejectStale();return;
        }
        if(!error.isEmpty()){status->setText(error);return;}
        const QString needleForPage=*browsing?QString():query->text();
        auto preview=printrender::previewText(snapshot->value().path,item->data(Qt::UserRole).toInt(),QSize(1000,1400),needleForPage,match->isChecked());
        if(!snapshot->value().unchanged()){rejectStale();return;}
        if(!preview.error.isEmpty())status->setText(preview.error);
        else canvas->setPage(std::move(preview));
        text->setPlainText(extracted.normalized(QString::NormalizationForm_C));
        const auto needle=needleForPage.trimmed().normalized(QString::NormalizationForm_C);
        const auto offsets=printrender::highlightOffsets(text->toPlainText(),needle,match->isChecked());
        QList<QTextEdit::ExtraSelection> highlights;
        for(auto pos:offsets){
            if(pos+needle.size()>=text->document()->characterCount())continue;
            QTextEdit::ExtraSelection hit;
            hit.cursor=QTextCursor(text->document());
            hit.cursor.setPosition(int(pos));
            hit.cursor.setPosition(int(pos+needle.size()),QTextCursor::KeepAnchor);
            hit.format.setBackground(QColor(255,224,130));
            hit.format.setForeground(QColor(25,25,25));
            highlights.append(hit);
        }
        text->setExtraSelections(highlights);
        if(!highlights.isEmpty()){text->setTextCursor(highlights.first().cursor);text->ensureCursorVisible();}
        else text->moveCursor(QTextCursor::Start);
    });
    return w;
}

QWidget* PdfToolsDialog::buildNoteForm(){
    auto* w=new QWidget(this);auto* f=new QFormLayout(w);
    f->addRow(tr("PDF nguồn:"),fileRow(&m_noteFormFile,{}));
    auto* page=new QSpinBox(w);page->setRange(1,100000);
    auto* x=new QSpinBox(w);x->setRange(0,100);x->setValue(90);x->setSuffix("%");
    auto* y=new QSpinBox(w);y->setRange(0,100);y->setValue(10);y->setSuffix("%");
    auto* note=new QPlainTextEdit(w);note->setPlaceholderText(tr("Nội dung ghi chú tiếng Việt"));note->setMaximumHeight(100);
    f->addRow(tr("Trang"),page);f->addRow(tr("Vị trí ngang từ trái"),x);f->addRow(tr("Vị trí dọc từ trên"),y);f->addRow(tr("Ghi chú"),note);
    auto* add=new QPushButton(tr("Lưu chú thích PDF"),w);f->addRow(add);
    connect(add,&QPushButton::clicked,this,[=]{
        const QString src=m_noteFormFile->text();if(src.isEmpty())return;
        const auto dest=pickDest(this,src,tr("Chú thích"));if(dest.isEmpty())return;
        const int p=page->value(),xx=x->value(),yy=y->value();const auto content=note->toPlainText();
        showResult(runWithPasswords({src},[=]{return addTextNote(inputFor(src),p,xx,yy,content,dest);}),tr("Chú thích PDF"));
    });
    auto* fields=new QComboBox(w);auto* value=new QLineEdit(w);value->setMaxLength(500);
    auto* scan=new QPushButton(tr("Đọc trường form"),w);f->addRow(scan);f->addRow(tr("Trường"),fields);f->addRow(tr("Giá trị mới"),value);
    auto* fill=new QPushButton(tr("Lưu form PDF"),w);fill->setEnabled(false);f->addRow(fill);
    auto* status=new QLabel(tr("Chú thích thật; form chữ ASCII một dòng. Chưa hỗ trợ XFA, chữ ký, PDF mã hóa. Xuất bản sao mới."),w);status->setWordWrap(true);f->addRow(status);
    connect(m_noteFormFile,&QLineEdit::textChanged,w,[=]{fields->clear();value->clear();fill->setEnabled(false);});
    connect(fields,&QComboBox::currentIndexChanged,w,[=](int i){value->setText(i>=0?fields->itemData(i).toString():QString());});
    connect(scan,&QPushButton::clicked,this,[=]{
        fields->clear();fill->setEnabled(false);if(m_noteFormFile->text().isEmpty())return;
        const auto result=textFormFields(inputFor(m_noteFormFile->text()));
        if(!result.ok){QMessageBox::warning(this,tr("Form PDF"),result.error);return;}
        for(int i=0;i<result.names.size();++i)fields->addItem(result.names.at(i),result.values.at(i));
        fill->setEnabled(fields->count()>0);
    });
    connect(fill,&QPushButton::clicked,this,[=]{
        const auto src=m_noteFormFile->text(),name=fields->currentText(),v=value->text();
        if(src.isEmpty()||name.isEmpty())return;
        const auto dest=pickDest(this,src,tr("Điền form"));if(dest.isEmpty())return;
        showResult(runWithPasswords({src},[=]{return fillTextField(inputFor(src),name,v,dest);}),tr("Form PDF"));
    });
    return w;
}
