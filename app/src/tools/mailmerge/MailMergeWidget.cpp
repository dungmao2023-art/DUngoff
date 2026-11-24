// SPDX-License-Identifier: MPL-2.0
#include "MailMergeWidget.h"
#include "MailMergeCore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QUrl>
#include <QVBoxLayout>

using namespace mailmerge;

struct MailMergeWidget::Impl {
    MailMergeWidget *w = nullptr;
    std::function<QString()> docProvider, selProvider;
    PdfExporter pdf;

    // trạng thái
    QString tplPath;
    ScanResult scan;
    Table table;
    QVector<QComboBox *> mapCombos;
    QString lastOutDir;

    // giao diện
    QLineEdit *tplEdit = nullptr;
    QPushButton *btnUseOpen = nullptr, *btnGetSel = nullptr;
    QLabel *tplStatus = nullptr;
    QListWidget *fieldList = nullptr;
    QLineEdit *selText = nullptr, *fieldName = nullptr;
    QTabWidget *dataTabs = nullptr;
    QPlainTextEdit *pasteEdit = nullptr;
    QCheckBox *pasteHeader = nullptr, *csvHeader = nullptr, *xlsxHeader = nullptr;
    QComboBox *pasteDelim = nullptr, *sheetCombo = nullptr;
    QLineEdit *csvEdit = nullptr, *xlsxEdit = nullptr;
    QTableWidget *preview = nullptr;
    QFormLayout *mapForm = nullptr;
    QWidget *mapHost = nullptr;
    QPlainTextEdit *report = nullptr, *previewText = nullptr;
    QSpinBox *recNo = nullptr;
    QLineEdit *outDir = nullptr, *pattern = nullptr;
    QRadioButton *rbDocx = nullptr, *rbPdf = nullptr, *rbMerged = nullptr;
    QLabel *runStatus = nullptr;
    QPushButton *btnOpenFolder = nullptr;

    void rescan();
    void loadData();
    void rebuildMapping();
    Mapping currentMapping() const;
    bool doCheck(bool quiet);
    void doPreview();
    void doMakeField();
    void doRun();
    void showReport(const QStringList &errors, const QStringList &warnings, const QString &ok);
};

namespace {
const QString kFilter = QStringLiteral("Word, PowerPoint, Excel (*.docx *.pptx *.xlsx);;Word (*.docx);;PowerPoint (*.pptx);;Excel (*.xlsx)");
bool isDocx(const QString &p) { return QFileInfo(p).suffix().compare(QLatin1String("docx"), Qt::CaseInsensitive) == 0; }
QString dotExt(const QString &p) { return QLatin1Char('.') + QFileInfo(p).suffix().toLower(); }
QPushButton *mkBtn(const QString &t, QWidget *parent) { auto *b = new QPushButton(t, parent); b->setAutoDefault(false); return b; }
}

void MailMergeWidget::Impl::showReport(const QStringList &errors, const QStringList &warnings, const QString &ok)
{
    QString s;
    for (const QString &e : errors) s += QStringLiteral("❌ ") + e + QLatin1Char('\n');
    for (const QString &x : warnings) s += QStringLiteral("⚠ ") + x + QLatin1Char('\n');
    if (errors.isEmpty() && !ok.isEmpty()) s += QStringLiteral("✔ ") + ok + QLatin1Char('\n');
    report->setPlainText(s);
}

void MailMergeWidget::Impl::rescan()
{
    fieldList->clear();
    scan = ScanResult();
    if (tplPath.isEmpty()) { tplStatus->setText(QObject::tr("Chưa chọn mẫu.")); rebuildMapping(); return; }
    scan = scanTemplate(tplPath);
    if (!scan.ok) {
        tplStatus->setText(QStringLiteral("❌ ") + scan.error);
    } else if (scan.fields.isEmpty()) {
        tplStatus->setText(QObject::tr("Mẫu chưa có trường nào. Gõ {{Tên}} vào mẫu, hoặc bôi chọn chữ cần thay rồi bấm \"Biến đoạn này thành trường\"."));
    } else {
        tplStatus->setText(QObject::tr("Tìm thấy %1 trường.").arg(scan.fields.size()) + (scan.warnings.isEmpty() ? QString() : QStringLiteral("  ⚠ ") + scan.warnings.join(QLatin1Char(' '))));
        for (const auto &f : scan.fields)
            fieldList->addItem(QObject::tr("«%1» — %2 chỗ (%3)").arg(f.name).arg(f.count).arg(f.where.join(QStringLiteral(", "))));
    }
    // Gộp một tệp chỉ có với Word; PowerPoint/Excel luôn xuất mỗi người một tệp.
    const bool canMerge = isDocx(tplPath);
    rbMerged->setEnabled(canMerge);
    rbMerged->setToolTip(canMerge ? QString() : QObject::tr("Gộp một tệp chỉ hỗ trợ mẫu Word (.docx)."));
    if (!canMerge && rbMerged->isChecked()) rbDocx->setChecked(true);
    rbDocx->setText(QObject::tr("Mỗi người một tệp (cùng loại với mẫu)"));
    if (pattern->text().isEmpty() && !scan.fields.isEmpty()) pattern->setText(QStringLiteral("{") + scan.fields.first().name + QStringLiteral("}"));
    rebuildMapping();
}

void MailMergeWidget::Impl::rebuildMapping()
{
    // xoá form cũ
    while (mapForm->rowCount() > 0) mapForm->removeRow(0);
    mapCombos.clear();
    if (!scan.ok || scan.fields.isEmpty() || !table.ok() || table.header.isEmpty()) return;
    const Mapping auto_ = autoMap(scan.fields, table);
    for (const auto &f : scan.fields) {
        auto *cb = new QComboBox(mapHost);
        cb->addItem(QObject::tr("— chọn cột —"), -1);
        for (int i = 0; i < table.header.size(); ++i) cb->addItem(table.header[i], i);
        const int idx = auto_.value(f.name, -1);
        cb->setCurrentIndex(idx + 1);
        mapForm->addRow(QObject::tr("Trường «%1» lấy từ cột:").arg(f.name), cb);
        mapCombos << cb;
    }
}

Mapping MailMergeWidget::Impl::currentMapping() const
{
    Mapping m;
    for (int i = 0; i < scan.fields.size() && i < mapCombos.size(); ++i) m.insert(scan.fields[i].name, mapCombos[i]->currentData().toInt());
    return m;
}

void MailMergeWidget::Impl::loadData()
{
    const int tab = dataTabs->currentIndex();
    if (tab == 0) {
        static const Delim ds[] = {Delim::Auto, Delim::Tab, Delim::Semicolon, Delim::Comma, Delim::None};
        table = parsePasted(pasteEdit->toPlainText(), ds[qBound(0, pasteDelim->currentIndex(), 4)], pasteHeader->isChecked());
    } else if (tab == 1) {
        table = readCsvFile(csvEdit->text(), csvHeader->isChecked());
    } else {
        table = readXlsxFile(xlsxEdit->text(), qMax(0, sheetCombo->currentIndex()), xlsxHeader->isChecked());
    }
    preview->clear();
    if (!table.ok()) {
        preview->setRowCount(0); preview->setColumnCount(0);
        showReport({table.error}, {}, QString());
    } else {
        const int rows = qMin(30, int(table.rows.size()));
        preview->setColumnCount(table.header.size());
        preview->setRowCount(rows);
        preview->setHorizontalHeaderLabels(table.header);
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < table.header.size(); ++c) preview->setItem(r, c, new QTableWidgetItem(table.rows[r].value(c)));
        showReport({}, table.warnings, QObject::tr("Đã đọc %1 dòng, %2 cột.").arg(table.rows.size()).arg(table.header.size()));
    }
    rebuildMapping();
}

bool MailMergeWidget::Impl::doCheck(bool quiet)
{
    if (tplPath.isEmpty() || !scan.ok) { showReport({QObject::tr("Chưa có mẫu hợp lệ.")}, {}, QString()); return false; }
    const Check c = validate(scan.fields, table, currentMapping());
    QStringList warn = c.warnings;
    warn << scan.warnings;
    showReport(c.errors, warn, QObject::tr("Sẵn sàng: %1 bản ghi sẽ được tạo.").arg(c.usableRecords));
    Q_UNUSED(quiet)
    return c.errors.isEmpty();
}

void MailMergeWidget::Impl::doPreview()
{
    if (!doCheck(true)) return;
    const auto recs = buildRecords(scan.fields, table, currentMapping());
    if (recs.isEmpty()) return;
    const int i = qBound(1, recNo->value(), int(recs.size())) - 1;
    QString err;
    const QString t = mailmerge::previewText(tplPath, recs[i], 6000, &err);
    previewText->setPlainText(err.isEmpty() ? t : QStringLiteral("❌ ") + err);
}

void MailMergeWidget::Impl::doMakeField()
{
    if (tplPath.isEmpty()) { QMessageBox::information(w, QObject::tr("Điền tên hàng loạt"), QObject::tr("Hãy chọn mẫu (.docx, .pptx hoặc .xlsx) trước.")); return; }
    const QFileInfo fi(tplPath);
    const QString suggestion = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(" - mẫu trường") + dotExt(tplPath));
    QString dst = QFileDialog::getSaveFileName(w, QObject::tr("Lưu bản mẫu mới (mẫu gốc giữ nguyên)"), suggestion,
                                               QObject::tr("Cùng loại với mẫu (*%1)").arg(dotExt(tplPath)), nullptr, QFileDialog::DontConfirmOverwrite);
    if (dst.isEmpty()) return;
    if (!dst.endsWith(dotExt(tplPath), Qt::CaseInsensitive)) dst += dotExt(tplPath);
    const OpResult r = makeFieldTemplate(tplPath, dst, selText->text(), fieldName->text());
    if (!r.ok) { QMessageBox::warning(w, QObject::tr("Biến đoạn thành trường"), r.error); return; }
    tplPath = dst;
    tplEdit->setText(dst);
    rescan();
    QString msg = QObject::tr("Đã tạo bản mẫu mới có trường «%1» (%2 chỗ). Mẫu gốc không bị sửa.").arg(fieldName->text().trimmed()).arg(r.replaced);
    if (!r.warnings.isEmpty()) msg += QStringLiteral("\n\n") + r.warnings.join(QLatin1Char('\n'));
    QMessageBox::information(w, QObject::tr("Biến đoạn thành trường"), msg);
}

void MailMergeWidget::Impl::doRun()
{
    if (!doCheck(false)) return;
    const auto recs = buildRecords(scan.fields, table, currentMapping());
    if (recs.isEmpty()) return;

    QString dir = outDir->text().trimmed();
    if (dir.isEmpty()) dir = QFileInfo(tplPath).dir().filePath(QStringLiteral("Ket qua - ") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    if (!QDir().mkpath(dir)) { QMessageBox::warning(w, QObject::tr("Xuất tệp"), QObject::tr("Không tạo được thư mục: %1").arg(dir)); return; }
    outDir->setText(dir);
    lastOutDir = dir;

    QStringList warn, failures;
    int ok = 0;
    if (rbMerged->isChecked()) {
        const QString base = QFileInfo(tplPath).completeBaseName() + QStringLiteral(" - gộp");
        const QString out = planFileNames({Values()}, base, dotExt(tplPath), dir, &warn).value(0);
        // Mẫu tên dùng {…} sẽ không hợp ở đây nên dùng tên cố định; sanitize đã xử lý ký tự lạ.
        const OpResult r = renderMerged(tplPath, out, recs);
        if (r.ok) { ++ok; warn << r.warnings; } else failures << r.error;
        runStatus->setText(r.ok ? QObject::tr("Đã tạo 1 tệp gộp %1 trang bản ghi: %2").arg(recs.size()).arg(QFileInfo(out).fileName())
                                : QStringLiteral("❌ ") + r.error);
    } else {
        const bool asPdf = rbPdf->isChecked();
        if (asPdf && !pdf) { QMessageBox::warning(w, QObject::tr("Xuất PDF"), QObject::tr("Chưa có engine để xuất PDF. Hãy chọn xuất tệp cùng loại với mẫu.")); return; }
        const QString srcExt = dotExt(tplPath);
        const QString ext = asPdf ? QStringLiteral(".pdf") : srcExt;
        const QStringList names = planFileNames(recs, pattern->text(), ext, dir, &warn);
        QProgressDialog prog(QObject::tr("Đang tạo tệp..."), QObject::tr("Dừng"), 0, recs.size(), w);
        prog.setWindowModality(Qt::WindowModal);
        prog.setMinimumDuration(0);
        QTemporaryDir tmp;
        for (int i = 0; i < recs.size(); ++i) {
            prog.setValue(i);
            QCoreApplication::processEvents();
            if (prog.wasCanceled()) { failures << QObject::tr("Đã dừng theo yêu cầu sau %1/%2 tệp (các tệp đã tạo giữ nguyên).").arg(i).arg(recs.size()); break; }
            if (asPdf) {
                const QString tmpDocx = tmp.filePath(QStringLiteral("b%1").arg(i) + srcExt);
                const OpResult r = renderOne(tplPath, tmpDocx, recs[i]);
                if (!r.ok) { failures << QStringLiteral("%1: %2").arg(QFileInfo(names[i]).fileName(), r.error); continue; }
                if (i == 0) warn << r.warnings;
                QString perr;
                if (!pdf(tmpDocx, names[i], &perr)) { failures << QStringLiteral("%1: %2").arg(QFileInfo(names[i]).fileName(), perr); QFile::remove(tmpDocx); continue; }
                QFile::remove(tmpDocx);
                ++ok;
            } else {
                const OpResult r = renderOne(tplPath, names[i], recs[i]);
                if (r.ok) { ++ok; if (i == 0) warn << r.warnings; }
                else failures << QStringLiteral("%1: %2").arg(QFileInfo(names[i]).fileName(), r.error);
            }
        }
        prog.setValue(recs.size());
        runStatus->setText(QObject::tr("Đã tạo %1/%2 tệp trong: %3").arg(ok).arg(recs.size()).arg(dir) + (failures.isEmpty() ? QString() : QObject::tr("  — %1 lỗi").arg(failures.size())));
    }
    btnOpenFolder->setEnabled(true);
    warn.removeDuplicates();
    QStringList errs = failures;
    showReport(errs, warn, QObject::tr("Hoàn tất: %1 tệp. Mẫu gốc không bị sửa.").arg(ok));
}

MailMergeWidget::MailMergeWidget(QWidget *parent) : QWidget(parent), d(new Impl)
{
    d->w = this;
    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto *content = new QWidget;
    scroll->setWidget(content);
    auto *lay = new QVBoxLayout(content);

    auto *intro = new QLabel(tr("<b>Điền tên hàng loạt</b> — giấy mời, giấy khen, thư, hợp đồng. Mẫu gốc luôn giữ nguyên; kết quả ra tệp mới."));
    intro->setWordWrap(true);
    lay->addWidget(intro);

    // ---- 1. Mẫu
    auto *g1 = new QGroupBox(tr("1. Chọn mẫu (.docx, .pptx hoặc .xlsx)"));
    auto *l1 = new QVBoxLayout(g1);
    auto *r1 = new QHBoxLayout;
    d->tplEdit = new QLineEdit; d->tplEdit->setPlaceholderText(tr("Đường dẫn tệp mẫu (.docx, .pptx, .xlsx)"));
    auto *bBrowse = mkBtn(tr("Chọn tệp…"), g1);
    d->btnUseOpen = mkBtn(tr("Dùng tài liệu đang mở"), g1); d->btnUseOpen->setVisible(false);
    r1->addWidget(d->tplEdit, 1); r1->addWidget(bBrowse); r1->addWidget(d->btnUseOpen);
    l1->addLayout(r1);
    d->tplStatus = new QLabel(tr("Chưa chọn mẫu.")); d->tplStatus->setWordWrap(true);
    l1->addWidget(d->tplStatus);
    d->fieldList = new QListWidget; d->fieldList->setMaximumHeight(90);
    l1->addWidget(d->fieldList);
    auto *hint = new QLabel(tr("Cách 1: gõ <b>{{Tên}}</b> hoặc <b>«Tên»</b> vào mẫu.  Cách 2: dán/lấy đoạn chữ cần thay (ví dụ một tên mẫu) rồi bấm nút bên dưới."));
    hint->setWordWrap(true); l1->addWidget(hint);
    auto *r1b = new QHBoxLayout;
    d->selText = new QLineEdit; d->selText->setPlaceholderText(tr("Đoạn chữ cần biến thành trường (nằm trong một đoạn văn)"));
    d->btnGetSel = mkBtn(tr("Lấy đoạn đang bôi chọn"), g1); d->btnGetSel->setVisible(false);
    r1b->addWidget(d->selText, 1); r1b->addWidget(d->btnGetSel);
    l1->addLayout(r1b);
    auto *r1c = new QHBoxLayout;
    d->fieldName = new QLineEdit; d->fieldName->setPlaceholderText(tr("Tên trường, ví dụ HoTen"));
    auto *bMake = mkBtn(tr("Biến đoạn này thành trường"), g1);
    r1c->addWidget(d->fieldName, 1); r1c->addWidget(bMake);
    l1->addLayout(r1c);
    lay->addWidget(g1);

    // ---- 2. Dữ liệu
    auto *g2 = new QGroupBox(tr("2. Danh sách cần điền"));
    auto *l2 = new QVBoxLayout(g2);
    d->dataTabs = new QTabWidget;
    {
        auto *t = new QWidget; auto *v = new QVBoxLayout(t);
        d->pasteEdit = new QPlainTextEdit; d->pasteEdit->setPlaceholderText(tr("Dán danh sách: mỗi dòng một người. Nhiều cột: ngăn bằng Tab (copy từ Excel) hoặc dấu ; …"));
        d->pasteEdit->setMinimumHeight(90);
        auto *h = new QHBoxLayout;
        d->pasteHeader = new QCheckBox(tr("Dòng đầu là tiêu đề cột"));
        d->pasteDelim = new QComboBox; d->pasteDelim->addItems({tr("Tự nhận dấu ngăn cột"), tr("Tab"), tr("Dấu ;"), tr("Dấu ,"), tr("Không tách cột (mỗi dòng 1 giá trị)")});
        h->addWidget(d->pasteHeader); h->addWidget(d->pasteDelim, 1);
        v->addWidget(d->pasteEdit); v->addLayout(h);
        d->dataTabs->addTab(t, tr("Dán danh sách"));
    }
    {
        auto *t = new QWidget; auto *v = new QVBoxLayout(t); auto *h = new QHBoxLayout;
        d->csvEdit = new QLineEdit; auto *b = mkBtn(tr("Chọn CSV…"), t);
        h->addWidget(d->csvEdit, 1); h->addWidget(b);
        d->csvHeader = new QCheckBox(tr("Dòng đầu là tiêu đề cột")); d->csvHeader->setChecked(true);
        v->addLayout(h); v->addWidget(d->csvHeader);
        d->dataTabs->addTab(t, tr("Tệp CSV"));
        connect(b, &QPushButton::clicked, this, [this]() {
            const QString p = QFileDialog::getOpenFileName(this, tr("Chọn tệp CSV"), QString(), tr("CSV (*.csv *.txt)"));
            if (!p.isEmpty()) { d->csvEdit->setText(p); d->loadData(); }
        });
    }
    {
        auto *t = new QWidget; auto *v = new QVBoxLayout(t); auto *h = new QHBoxLayout;
        d->xlsxEdit = new QLineEdit; auto *b = mkBtn(tr("Chọn Excel…"), t);
        h->addWidget(d->xlsxEdit, 1); h->addWidget(b);
        d->sheetCombo = new QComboBox;
        d->xlsxHeader = new QCheckBox(tr("Dòng đầu là tiêu đề cột")); d->xlsxHeader->setChecked(true);
        v->addLayout(h); v->addWidget(d->sheetCombo); v->addWidget(d->xlsxHeader);
        d->dataTabs->addTab(t, tr("Tệp Excel (.xlsx)"));
        connect(b, &QPushButton::clicked, this, [this]() {
            const QString p = QFileDialog::getOpenFileName(this, tr("Chọn tệp Excel"), QString(), tr("Excel (*.xlsx)"));
            if (p.isEmpty()) return;
            d->xlsxEdit->setText(p);
            QString err; const QStringList names = xlsxSheetNames(p, &err);
            d->sheetCombo->clear(); d->sheetCombo->addItems(names);
            if (!err.isEmpty()) d->report->setPlainText(QStringLiteral("❌ ") + err); else d->loadData();
        });
        connect(d->sheetCombo, qOverload<int>(&QComboBox::activated), this, [this](int) { d->loadData(); });
    }
    l2->addWidget(d->dataTabs);
    auto *bLoad = mkBtn(tr("Đọc dữ liệu"), g2);
    l2->addWidget(bLoad);
    d->preview = new QTableWidget; d->preview->setMaximumHeight(150);
    d->preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    l2->addWidget(d->preview);
    d->mapHost = new QWidget; d->mapForm = new QFormLayout(d->mapHost);
    l2->addWidget(d->mapHost);
    lay->addWidget(g2);

    // ---- 3. Kiểm tra & xem trước
    auto *g3 = new QGroupBox(tr("3. Kiểm tra và xem trước"));
    auto *l3 = new QVBoxLayout(g3);
    auto *r3 = new QHBoxLayout;
    auto *bCheck = mkBtn(tr("Kiểm tra"), g3);
    d->recNo = new QSpinBox; d->recNo->setRange(1, 1000000); d->recNo->setPrefix(tr("Bản ghi số "));
    auto *bPrev = mkBtn(tr("Xem trước"), g3);
    r3->addWidget(bCheck); r3->addStretch(1); r3->addWidget(d->recNo); r3->addWidget(bPrev);
    l3->addLayout(r3);
    d->report = new QPlainTextEdit; d->report->setReadOnly(true); d->report->setMinimumHeight(90);
    d->previewText = new QPlainTextEdit; d->previewText->setReadOnly(true); d->previewText->setMinimumHeight(110);
    d->previewText->setPlaceholderText(tr("Bản xem trước nội dung (văn bản thuần, chưa gồm định dạng trang). Kiểm tra hình thức bằng cách mở tệp xuất đầu tiên."));
    l3->addWidget(d->report); l3->addWidget(d->previewText);
    lay->addWidget(g3);

    // ---- 4. Xuất
    auto *g4 = new QGroupBox(tr("4. Xuất tệp (luôn là tệp mới, không ghi đè)"));
    auto *l4 = new QVBoxLayout(g4);
    d->rbDocx = new QRadioButton(tr("Mỗi người một tệp Word (.docx)")); d->rbDocx->setChecked(true);
    d->rbPdf = new QRadioButton(tr("Mỗi người một tệp PDF")); d->rbPdf->setEnabled(false);
    d->rbPdf->setToolTip(tr("Cần engine để xuất PDF; chức năng này chưa được nối ở nơi nhúng."));
    d->rbMerged = new QRadioButton(tr("Gộp tất cả vào một tệp Word nhiều trang (mỗi người một trang)"));
    l4->addWidget(d->rbDocx); l4->addWidget(d->rbPdf); l4->addWidget(d->rbMerged);
    auto *f4 = new QFormLayout;
    d->pattern = new QLineEdit; d->pattern->setPlaceholderText(tr("Ví dụ: {HoTen} hoặc {#}-{HoTen}"));
    d->outDir = new QLineEdit; d->outDir->setPlaceholderText(tr("Để trống: tự tạo thư mục mới cạnh tệp mẫu"));
    auto *rowDir = new QHBoxLayout; auto *bDir = mkBtn(tr("Chọn thư mục…"), g4);
    rowDir->addWidget(d->outDir, 1); rowDir->addWidget(bDir);
    f4->addRow(tr("Đặt tên tệp theo:"), d->pattern);
    f4->addRow(tr("Thư mục kết quả:"), rowDir);
    l4->addLayout(f4);
    auto *r4 = new QHBoxLayout;
    auto *bRun = mkBtn(tr("Tạo tệp"), g4); bRun->setDefault(false);
    d->btnOpenFolder = mkBtn(tr("Mở thư mục kết quả"), g4); d->btnOpenFolder->setEnabled(false);
    r4->addWidget(bRun); r4->addWidget(d->btnOpenFolder); r4->addStretch(1);
    l4->addLayout(r4);
    d->runStatus = new QLabel; d->runStatus->setWordWrap(true);
    l4->addWidget(d->runStatus);
    lay->addWidget(g4);
    lay->addStretch(1);

    // ---- nối tín hiệu
    connect(bBrowse, &QPushButton::clicked, this, [this]() {
        const QString p = QFileDialog::getOpenFileName(this, tr("Chọn mẫu"), QString(), kFilter);
        if (!p.isEmpty()) setTemplatePath(p);
    });
    connect(d->tplEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString p = d->tplEdit->text().trimmed();
        if (p != d->tplPath) setTemplatePath(p);
    });
    connect(d->btnUseOpen, &QPushButton::clicked, this, [this]() {
        const QString p = d->docProvider ? d->docProvider() : QString();
        if (p.isEmpty() || !(p.endsWith(QLatin1String(".docx"), Qt::CaseInsensitive) || p.endsWith(QLatin1String(".pptx"), Qt::CaseInsensitive) || p.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive))) {
            QMessageBox::information(this, tr("Điền tên hàng loạt"), tr("Tài liệu đang mở không phải tệp .docx/.pptx/.xlsx đã lưu. Hãy lưu thành một trong các loại này trước, hoặc chọn tệp."));
            return;
        }
        setTemplatePath(p);
    });
    connect(d->btnGetSel, &QPushButton::clicked, this, [this]() { if (d->selProvider) d->selText->setText(d->selProvider()); });
    connect(bMake, &QPushButton::clicked, this, [this]() { d->doMakeField(); });
    connect(bLoad, &QPushButton::clicked, this, [this]() { d->loadData(); });
    connect(d->pasteHeader, &QCheckBox::toggled, this, [this](bool) { if (d->dataTabs->currentIndex() == 0) d->loadData(); });
    connect(d->pasteDelim, qOverload<int>(&QComboBox::activated), this, [this](int) { if (d->dataTabs->currentIndex() == 0) d->loadData(); });
    connect(d->csvHeader, &QCheckBox::toggled, this, [this](bool) { if (d->dataTabs->currentIndex() == 1 && !d->csvEdit->text().isEmpty()) d->loadData(); });
    connect(d->xlsxHeader, &QCheckBox::toggled, this, [this](bool) { if (d->dataTabs->currentIndex() == 2 && !d->xlsxEdit->text().isEmpty()) d->loadData(); });
    connect(bCheck, &QPushButton::clicked, this, [this]() { d->doCheck(false); });
    connect(bPrev, &QPushButton::clicked, this, [this]() { d->doPreview(); });
    connect(bDir, &QPushButton::clicked, this, [this]() {
        const QString p = QFileDialog::getExistingDirectory(this, tr("Chọn thư mục kết quả"));
        if (!p.isEmpty()) d->outDir->setText(p);
    });
    connect(bRun, &QPushButton::clicked, this, [this]() { d->doRun(); });
    connect(d->btnOpenFolder, &QPushButton::clicked, this, [this]() { if (!d->lastOutDir.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(d->lastOutDir)); });
}

MailMergeWidget::~MailMergeWidget() { delete d; }

void MailMergeWidget::setCurrentDocumentProvider(std::function<QString()> f) { d->docProvider = std::move(f); d->btnUseOpen->setVisible(bool(d->docProvider)); }
void MailMergeWidget::setSelectionProvider(std::function<QString()> f) { d->selProvider = std::move(f); d->btnGetSel->setVisible(bool(d->selProvider)); }
void MailMergeWidget::setPdfExporter(PdfExporter f)
{
    d->pdf = std::move(f);
    d->rbPdf->setEnabled(bool(d->pdf));
    d->rbPdf->setToolTip(d->pdf ? QString() : tr("Cần engine để xuất PDF; chức năng này chưa được nối ở nơi nhúng."));
}
void MailMergeWidget::setTemplatePath(const QString &p)
{
    d->tplPath = p;
    d->tplEdit->setText(p);
    d->rescan();
}

QWidget *createMailMergeWidget(QWidget *parent) { return new MailMergeWidget(parent); }
