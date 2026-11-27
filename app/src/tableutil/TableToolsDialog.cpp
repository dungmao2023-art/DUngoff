// SPDX-License-Identifier: MPL-2.0
#include "TableToolsDialog.h"
#include "TableOps.h"
#include "XlsxSource.h"

#include <QCheckBox>
#include <QFile>
#include <QDialogButtonBox>
#include <QSignalBlocker>
#include <functional>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace tbl;

namespace {

// Một nguồn CSV: đường dẫn + bảng mã + dấu phân cách + xem trước + xác nhận.
// Chỉ cho phép đọc khi bảng mã chắc chắn HOẶC người dùng đã xem trước và tích xác nhận.
class CsvPicker : public QWidget {
public:
    std::function<void()> onLoaded; // gọi khi đọc xong tiêu đề (để cập nhật danh sách cột)

    explicit CsvPicker(const QString& label, QWidget* parent = nullptr) : QWidget(parent) {
        auto* v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(label));
        m_path = new QLineEdit;
        m_path->setReadOnly(true);
        row->addWidget(m_path, 1);
        auto* pick = new QPushButton(tr("Chọn…"));
        row->addWidget(pick);
        v->addLayout(row);

        auto* row2 = new QHBoxLayout;
        row2->addWidget(new QLabel(tr("Bảng mã:")));
        m_enc = new QComboBox;
        for (Encoding e : {Encoding::Utf8, Encoding::Utf16LE, Encoding::Utf16BE, Encoding::Windows1258})
            m_enc->addItem(encodingName(e), int(e));
        row2->addWidget(m_enc);
        row2->addWidget(new QLabel(tr("Dấu phân cách:")));
        m_sep = new QComboBox;
        m_sep->addItem(tr("Dấu phẩy ( , )"), int(',')); m_sep->addItem(tr("Chấm phẩy ( ; )"), int(';')); m_sep->addItem(tr("Tab"), int('\t'));
        row2->addWidget(m_sep);
        row2->addStretch(1);
        v->addLayout(row2);
        m_sheet=new QComboBox;
        m_sheet->setToolTip(tr("Chọn worksheet từ tệp XLSX đã lưu."));
        m_sheet->hide();v->addWidget(m_sheet);

        m_reason = new QLabel;
        m_reason->setWordWrap(true);
        v->addWidget(m_reason);
        m_confirm = new QCheckBox(tr("Tôi đã xem trước bên dưới và xác nhận đây là bảng mã đúng"));
        v->addWidget(m_confirm);
        m_preview = new QPlainTextEdit;
        m_preview->setReadOnly(true);
        m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_preview->setMinimumHeight(110);
        v->addWidget(m_preview, 1);

        connect(pick, &QPushButton::clicked, this, [this] {
            const QString p = QFileDialog::getOpenFileName(this, tr("Chọn nguồn dữ liệu"), QString(), tr("Bảng dữ liệu (*.csv *.txt *.tsv *.xlsx);;CSV (*.csv *.txt *.tsv);;Excel (*.xlsx)"));
            if (!p.isEmpty()) setPath(p);
        });
        connect(m_sheet,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{loadSheet();});
        connect(m_enc, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { refreshPreview(); });
        connect(m_sep, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (onLoaded) onLoaded(); });
        connect(m_confirm, &QCheckBox::toggled, this, [this] { if (onLoaded) onLoaded(); });
    }

    QString path() const { return m_path->text(); }

    void setPath(const QString& p) {
        m_path->setText(p);
        m_loadedBytes = false;
        m_raw.clear();
        m_xlsxTable={};m_sheetList.clear();m_sheetError.clear();
        m_isXlsx=p.endsWith(QLatin1String(".xlsx"),Qt::CaseInsensitive);
        m_enc->setEnabled(!m_isXlsx);m_sep->setEnabled(!m_isXlsx);
        m_sheet->setVisible(m_isXlsx);
        {QSignalBlocker block(m_sheet);m_sheet->clear();}
        if(m_isXlsx){
            m_confirm->hide();
            if(!xlsxSheets(p,m_sheetList,m_sheetError)){
                m_reason->setText(m_sheetError);m_preview->clear();if(onLoaded)onLoaded();return;
            }
            {QSignalBlocker block(m_sheet);for(const auto& sheet:m_sheetList)m_sheet->addItem(sheet.name);m_sheet->setCurrentIndex(0);}
            loadSheet();return;
        }
        QString readError;
        if(!readCsvFileBytes(p,m_raw,readError)){
            m_reason->setText(readError);m_preview->clear();
            if(onLoaded)onLoaded();return;
        }
        m_loadedBytes = true;
        m_guess = detectCsvEncoding(m_raw);
        QSignalBlocker b(m_enc);
        const int idx = m_enc->findData(int(m_guess.enc == Encoding::Utf8Bom ? Encoding::Utf8 : m_guess.enc));
        if (idx >= 0) m_enc->setCurrentIndex(idx);
        m_confirm->setChecked(!m_guess.needsUserChoice);
        m_confirm->setVisible(m_guess.needsUserChoice);
        refreshPreview();
    }

    // Đọc bảng. false + lý do nếu chưa chọn tệp, chưa xác nhận, hoặc giải mã lỗi.
    bool load(Table& out, QString& err) const {
        if (path().isEmpty()) { err = tr("Chưa chọn tệp CSV."); return false; }
        if(m_isXlsx){
            if(!m_loadedBytes){err=m_sheetError.isEmpty()?tr("Sheet chưa đọc được."):m_sheetError;return false;}
            out=m_xlsxTable;err.clear();return true;
        }
        if (!m_loadedBytes) { err = tr("Tệp chưa đọc được."); return false; }
        if (needsConfirmation() && !m_confirm->isChecked()) { err = tr("Bảng mã chưa chắc chắn: hãy xem trước và tích xác nhận."); return false; }
        return readCsvBytes(m_raw, encoding(), out, err, QChar(m_sep->currentData().toInt()));
    }

private:
    void loadSheet(){
        if(!m_isXlsx)return;
        m_loadedBytes=false;m_xlsxTable={};m_sheetError.clear();m_preview->clear();
        const int index=m_sheet->currentIndex();
        if(index>=0&&index<m_sheetList.size())
            m_loadedBytes=readXlsxTable(path(),m_sheetList[index],m_xlsxTable,m_sheetError);
        if(m_loadedBytes){
            QString preview=m_xlsxTable.header.join(QStringLiteral(" | "));
            for(int i=0;i<m_xlsxTable.rows.size()&&i<20;++i)preview+=QStringLiteral("\n")+m_xlsxTable.rows[i].join(QStringLiteral(" | "));
            m_preview->setPlainText(preview);
            m_reason->setText(tr("XLSX: đọc giá trị lưu trong ô, không phải định dạng hiển thị. Ngày/số định dạng có thể hiện dạng số gốc; công thức bị từ chối. Chỉ xuất bản sao CSV."));
        }else m_reason->setText(m_sheetError);
        if(onLoaded)onLoaded();
    }
    Encoding encoding() const { return Encoding(m_enc->currentData().toInt()); }

    bool needsConfirmation() const {
        return m_guess.needsUserChoice || (encoding() != m_guess.enc &&
            !(m_guess.enc == Encoding::Utf8Bom && encoding() == Encoding::Utf8));
    }

    void refreshPreview() {
        if(m_isXlsx)return;
        if (!m_loadedBytes) { if (onLoaded) onLoaded(); return; }
        {
            QSignalBlocker block(m_confirm);
            m_confirm->setVisible(needsConfirmation());
            m_confirm->setChecked(!needsConfirmation());
        }
        QString text, err;
        if (!decodeText(m_raw, encoding(), text, err)) {
            m_preview->setPlainText(err);
            m_reason->setText(tr("<b>Gợi ý:</b> %1").arg(m_guess.reason.toHtmlEscaped()));
            if (onLoaded) onLoaded();
            return;
        }
        m_preview->setPlainText(previewLines(text, 20).join(QLatin1Char('\n')));
        { QSignalBlocker b(m_sep); const int si = m_sep->findData(int(guessSeparator(text).unicode())); if (si >= 0) m_sep->setCurrentIndex(si); }
        QString r = tr("<b>Gợi ý:</b> %1 (độ tin cậy %2%).").arg(m_guess.reason.toHtmlEscaped()).arg(int(m_guess.confidence * 100));
        if (encoding() != m_guess.enc && !(m_guess.enc == Encoding::Utf8Bom && encoding() == Encoding::Utf8))
            r += tr(" Bạn đang chọn bảng mã khác gợi ý.");
        m_reason->setText(r);
        if (onLoaded) onLoaded();
    }

    QLineEdit* m_path; QComboBox* m_enc; QComboBox* m_sep; QLabel* m_reason; QCheckBox* m_confirm; QPlainTextEdit* m_preview;
    QByteArray m_raw; EncodingGuess m_guess; bool m_loadedBytes = false;
    QComboBox* m_sheet=nullptr;bool m_isXlsx=false;QList<XlsxSheet> m_sheetList;Table m_xlsxTable;QString m_sheetError;
};

QString defaultOut(const QString& src, const QString& suffix) {
    const QFileInfo fi(src);
    return fi.dir().filePath(fi.completeBaseName() + QStringLiteral(" - ") + suffix + QStringLiteral(".csv"));
}

}  // namespace

struct TableToolsDialog::Impl {
    QTabWidget* tabs = nullptr;
    CsvPicker* a = nullptr;
    CsvPicker* b = nullptr;
    QListWidget* dupCols = nullptr; QCheckBox* fold = nullptr;
    QList<QComboBox*> kindBoxes; QWidget* kindHost = nullptr; QComboBox* money = nullptr;
    QComboBox* keyCol = nullptr;
    QComboBox* filterCol = nullptr; QComboBox* sortCol = nullptr;
    QComboBox* filterMode = nullptr; QLineEdit* filterValue = nullptr;
    QCheckBox* ignoreCase = nullptr; QCheckBox* descending = nullptr;
    QList<int> outputColumns; QStringList outputNames;
    int cleanCol=-1,dedupCol=-1;
    ColumnKind cleanKind=ColumnKind::Keep; MoneyLocale queryLocale=MoneyLocale::VN;
    bool dedupFold=false;
    int groupKey=-1,groupValue=-1;
    bool groupExtrema=false,groupAverage=false;
    QueryJoinSpec queryJoin;
    QStringList queryHeader;
    QPlainTextEdit* report = nullptr;
    Table tableA;
    bool haveA = false;
};

TableToolsDialog::TableToolsDialog(QWidget* parent) : QDialog(parent), d(new Impl) {
    setWindowTitle(tr("Tiện ích bảng (CSV)"));
    resize(760, 720);
    auto* root = new QVBoxLayout(this);
    auto* note = new QLabel(tr("Mọi thao tác chỉ đọc tệp gốc và ghi ra <b>bản sao mới</b> cùng báo cáo. Ô không chuyển được sẽ giữ nguyên và có cảnh báo."));
    note->setWordWrap(true);
    root->addWidget(note);
    d->a = new CsvPicker(tr("Tệp CSV:"));
    root->addWidget(d->a, 2);

    d->tabs = new QTabWidget;
    root->addWidget(d->tabs, 1);

    // --- Dòng trùng ---
    auto* w1 = new QWidget; auto* l1 = new QVBoxLayout(w1);
    l1->addWidget(new QLabel(tr("Chọn cột làm khoá (dòng trùng khoá sẽ bị bỏ, giữ dòng đầu tiên):")));
    d->dupCols = new QListWidget; l1->addWidget(d->dupCols);
    d->fold = new QCheckBox(tr("So sánh không phân biệt hoa/thường và khoảng trắng hai đầu (không đổi dữ liệu)"));
    l1->addWidget(d->fold);
    auto* run1 = new QPushButton(tr("Xoá dòng trùng và lưu bản sao…")); l1->addWidget(run1);
    d->tabs->addTab(w1, tr("Dòng trùng"));

    // --- Làm sạch ---
    auto* w2 = new QWidget; auto* l2 = new QVBoxLayout(w2);
    l2->addWidget(new QLabel(tr("Chọn kiểu làm sạch cho từng cột (mã, số điện thoại, số dài và số 0 đầu luôn được bảo vệ):")));
    d->kindHost = new QWidget; new QFormLayout(d->kindHost);
    l2->addWidget(d->kindHost, 1);
    auto* mrow = new QHBoxLayout; mrow->addWidget(new QLabel(tr("Cách viết số tiền:")));
    d->money = new QComboBox; d->money->addItem(tr("Việt Nam: 1.500.000,5"), int(MoneyLocale::VN)); d->money->addItem(tr("Quốc tế: 1,500,000.5"), int(MoneyLocale::EN));
    mrow->addWidget(d->money); mrow->addStretch(1); l2->addLayout(mrow);
    auto* run2 = new QPushButton(tr("Làm sạch và lưu bản sao…")); l2->addWidget(run2);
    d->tabs->addTab(w2, tr("Làm sạch ngày/tiền"));

    // --- So sánh ---
    auto* w3 = new QWidget; auto* l3 = new QVBoxLayout(w3);
    d->b = new CsvPicker(tr("Bảng thứ hai:")); l3->addWidget(d->b, 1);
    auto* krow = new QHBoxLayout; krow->addWidget(new QLabel(tr("Cột mã để ghép dòng:")));
    d->keyCol = new QComboBox; krow->addWidget(d->keyCol, 1); l3->addLayout(krow);
    auto* run3 = new QPushButton(tr("So sánh và lưu bảng khác biệt…")); l3->addWidget(run3);
    auto* join = new QPushButton(tr("Ghép bảng theo khóa (Left / Inner)…")); l3->addWidget(join);
    d->tabs->addTab(w3, tr("So sánh hai bảng"));

    auto* w4 = new QWidget; auto* l4 = new QVBoxLayout(w4);
    auto* form4 = new QFormLayout;
    d->filterCol = new QComboBox; d->sortCol = new QComboBox;
    d->filterMode = new QComboBox;
    d->filterMode->addItem(tr("Bằng"), int(FilterMode::Equal));
    d->filterMode->addItem(tr("Chứa"), int(FilterMode::Contains));
    d->filterMode->addItem(tr("Ô trống"), int(FilterMode::Blank));
    d->filterMode->addItem(tr("Ô không trống"), int(FilterMode::NotBlank));
    d->filterValue = new QLineEdit;
    d->ignoreCase = new QCheckBox(tr("Không phân biệt hoa/thường"));
    d->descending = new QCheckBox(tr("Giảm dần"));
    form4->addRow(tr("Lọc theo cột:"), d->filterCol);
    form4->addRow(tr("Điều kiện:"), d->filterMode);
    form4->addRow(tr("Nội dung:"), d->filterValue);
    form4->addRow(tr("Sắp xếp theo cột (chữ):"), d->sortCol);
    form4->addRow(d->ignoreCase); form4->addRow(d->descending);
    l4->addLayout(form4);
    auto* joinStep=new QPushButton(tr("Ghép bảng thứ hai trước quy trình…"));l4->addWidget(joinStep);
    auto* extraSteps=new QPushButton(tr("Làm sạch và bỏ trùng trong quy trình…"));l4->addWidget(extraSteps);
    auto* chooseColumns = new QPushButton(tr("Chọn và đổi tên cột đầu ra…")); l4->addWidget(chooseColumns);
    auto* groupStep=new QPushButton(tr("Tổng hợp nhóm cuối quy trình…"));l4->addWidget(groupStep);
    auto* run4 = new QPushButton(tr("Xem trước kết quả quy trình…"));
    l4->addWidget(run4);
    auto* recipes = new QHBoxLayout;
    auto* saveRecipe = new QPushButton(tr("Lưu cấu hình…"));
    auto* loadRecipe = new QPushButton(tr("Nạp cấu hình…"));
    recipes->addWidget(saveRecipe); recipes->addWidget(loadRecipe);
    l4->addLayout(recipes);
    d->tabs->addTab(w4, tr("Lọc và sắp xếp"));
    auto* w5=new QWidget; auto* l5=new QVBoxLayout(w5);
    auto* groupNote=new QLabel(tr("Tổng hợp theo nhóm từ CSV/XLSX. Khóa so khớp chính xác, ô rỗng là một nhóm riêng. Đếm dòng, tính tổng và tùy chọn nhỏ nhất/lớn nhất của số chuẩn (dấu chấm thập phân, không dấu hàng nghìn, tối đa 4 chữ số lẻ). Không ép kiểu mã hoặc số điện thoại."),w5);
    groupNote->setWordWrap(true); l5->addWidget(groupNote);
    auto* groupRun=new QPushButton(tr("Chọn cột và tổng hợp…"),w5); l5->addWidget(groupRun);
    l5->addStretch(); d->tabs->addTab(w5,tr("Tổng hợp nhóm"));

    d->report = new QPlainTextEdit; d->report->setReadOnly(true); d->report->setPlaceholderText(tr("Báo cáo sẽ hiện ở đây."));
    d->report->setMinimumHeight(110);
    root->addWidget(d->report, 1);
    auto* close = new QPushButton(tr("Đóng")); connect(close, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(close, 0, Qt::AlignRight);

    // Khi tệp A đọc được: cập nhật danh sách cột cho 3 tab.
    d->a->onLoaded = [this] {
        QString err; Table t;
        d->haveA = d->a->load(t, err);
        d->dupCols->clear(); d->keyCol->clear();
        d->filterCol->clear(); d->sortCol->clear();
        d->outputColumns.clear(); d->outputNames.clear();
        d->cleanCol=-1;d->dedupCol=-1;d->cleanKind=ColumnKind::Keep;d->queryLocale=MoneyLocale::VN;d->dedupFold=false;
        d->groupKey=-1;d->groupValue=-1;d->groupExtrema=false;d->groupAverage=false;d->queryJoin={};d->queryHeader.clear();
        d->filterCol->addItem(tr("Không lọc"), -1);
        d->sortCol->addItem(tr("Giữ thứ tự gốc"), -1);
        auto* form = qobject_cast<QFormLayout*>(d->kindHost->layout());
        while (form->rowCount() > 0) form->removeRow(0);
        d->kindBoxes.clear();
        if (!d->haveA) { if (!err.isEmpty() && !d->a->path().isEmpty()) d->report->setPlainText(err); return; }
        d->tableA = t;d->queryHeader=t.header;
        for (int i = 0; i < t.header.size(); ++i) {
            const auto label = tr("%1 · %2").arg(i + 1).arg(t.header[i]);
            d->filterCol->addItem(label, i);
            d->sortCol->addItem(label, i);
        }
        d->report->setPlainText(tr("Đã đọc %1 cột, %2 dòng dữ liệu.").arg(t.header.size()).arg(t.rows.size()));
        for (const QString& h : t.header) {
            auto* it = new QListWidgetItem(h); it->setFlags(it->flags() | Qt::ItemIsUserCheckable); it->setCheckState(Qt::Unchecked); d->dupCols->addItem(it);
            d->keyCol->addItem(h);
            auto* c = new QComboBox; c->addItem(tr("Giữ nguyên"), int(ColumnKind::Keep)); c->addItem(tr("Ngày → dd/mm/yyyy"), int(ColumnKind::Date)); c->addItem(tr("Số tiền → số thuần"), int(ColumnKind::Money));
            form->addRow(h, c); d->kindBoxes << c;
        }
    };

    auto askSave = [this](const QString& suggestion) -> QString {
        const QString p = QFileDialog::getSaveFileName(this, tr("Lưu bản sao thành"), suggestion, tr("CSV (*.csv)"));
        if (p.isEmpty()) return {};
        if (QFileInfo(p).canonicalFilePath() == QFileInfo(d->a->path()).canonicalFilePath() || (d->b && !d->b->path().isEmpty() && QFileInfo(p).canonicalFilePath() == QFileInfo(d->b->path()).canonicalFilePath())) {
            QMessageBox::warning(this, tr("Tiện ích bảng"), tr("Không được ghi đè tệp gốc. Hãy chọn tên khác."));
            return {};
        }
        return p;
    };
    auto save = [this](const QString& path, const Table& t, QString& report) {
        QString err;
        if (!writeCsvCopy(path, t, err)) { report += tr("\nKHÔNG LƯU ĐƯỢC: %1").arg(err); QMessageBox::warning(this, tr("Tiện ích bảng"), err); return false; }
        report += tr("\nĐã lưu bản sao: %1 (UTF-8 có BOM, mở được bằng Excel).").arg(path);
        return true;
    };
    auto needA = [this]() -> bool {
        if (!d->haveA) { QMessageBox::information(this, tr("Tiện ích bảng"), tr("Hãy chọn bảng nguồn; xác nhận bảng mã CSV hoặc chọn worksheet XLSX.")); return false; }
        return true;
    };

    connect(run1, &QPushButton::clicked, this, [this, askSave, save, needA] {
        if (!needA()) return;
        QList<int> cols;
        for (int i = 0; i < d->dupCols->count(); ++i) if (d->dupCols->item(i)->checkState() == Qt::Checked) cols << i;
        if (cols.isEmpty()) { QMessageBox::information(this, tr("Tiện ích bảng"), tr("Hãy tích ít nhất một cột làm khoá.")); return; }
        const auto r = removeDuplicates(d->tableA, cols, d->fold->isChecked());
        QString rep = tr("Dòng trùng: bỏ %1 / %2 dòng.").arg(r.removed.size()).arg(d->tableA.rows.size());
        for (int i = 0; i < r.removed.size() && i < 100; ++i) rep += tr("\n  - dòng %1 trùng dòng %2: %3").arg(r.removed[i].originalIndex + 2).arg(r.removed[i].keptIndex + 2).arg(r.removed[i].cells.join(QStringLiteral(" | ")));
        if (r.removed.size() > 100) rep += tr("\n  … (còn %1 dòng nữa)").arg(r.removed.size() - 100);
        for (const QString& w : r.warnings) rep += tr("\nCẢNH BÁO: %1").arg(w);
        if (!r.ok) { d->report->setPlainText(rep + tr("\nKhông thực hiện được.")); return; }
        const QString out = askSave(defaultOut(d->a->path(), tr("bỏ trùng")));
        if (out.isEmpty()) { d->report->setPlainText(rep + tr("\n(Chưa lưu.)")); return; }
        save(out, r.cleaned, rep);
        d->report->setPlainText(rep);
    });

    connect(run2, &QPushButton::clicked, this, [this, askSave, save, needA] {
        if (!needA()) return;
        QList<QPair<QString, ColumnKind>> kinds;
        for (int i = 0; i < d->kindBoxes.size() && i < d->tableA.header.size(); ++i) {
            const auto k = ColumnKind(d->kindBoxes[i]->currentData().toInt());
            if (k != ColumnKind::Keep) kinds.append({d->tableA.header[i], k});
        }
        if (kinds.isEmpty()) { QMessageBox::information(this, tr("Tiện ích bảng"), tr("Hãy chọn kiểu làm sạch cho ít nhất một cột.")); return; }
        const auto r = cleanTable(d->tableA, kinds, MoneyLocale(d->money->currentData().toInt()));
        QString rep = tr("Làm sạch: đổi %1 ô, %2 cảnh báo (ô không chuyển được giữ nguyên).").arg(r.changes.size()).arg(r.warnings.size());
        for (int i = 0; i < r.changes.size() && i < 100; ++i) rep += tr("\n  - dòng %1, cột %2: \"%3\" → \"%4\"").arg(r.changes[i].row + 2).arg(d->tableA.header.value(r.changes[i].col)).arg(r.changes[i].before, r.changes[i].after);
        if (r.changes.size() > 100) rep += tr("\n  … (còn %1 thay đổi nữa)").arg(r.changes.size() - 100);
        for (int i = 0; i < r.warnings.size() && i < 100; ++i) rep += tr("\nCẢNH BÁO: %1").arg(r.warnings[i]);
        if (r.warnings.size() > 100) rep += tr("\n… (còn %1 cảnh báo nữa)").arg(r.warnings.size() - 100);
        const QString out = askSave(defaultOut(d->a->path(), tr("đã làm sạch")));
        if (out.isEmpty()) { d->report->setPlainText(rep + tr("\n(Chưa lưu.)")); return; }
        save(out, r.cleaned, rep);
        d->report->setPlainText(rep);
    });

    connect(groupRun,&QPushButton::clicked,this,[this,askSave,save,needA] {
        if(!needA() || d->tableA.header.isEmpty()) return;
        QDialog dlg(this);dlg.setWindowTitle(tr("Tổng hợp nhóm CSV"));
        auto* form=new QFormLayout(&dlg);
        auto* key=new QComboBox(&dlg);auto* value=new QComboBox(&dlg);
        value->addItem(tr("Chỉ đếm dòng"),-1);
        for(int i=0;i<d->tableA.header.size();++i) {
            const auto label=tr("%1 · %2").arg(i+1).arg(d->tableA.header[i]);
            key->addItem(label,i);value->addItem(label,i);
        }
        form->addRow(tr("Cột nhóm:"),key);form->addRow(tr("Cột số:"),value);
        auto* extrema=new QCheckBox(tr("Thêm giá trị nhỏ nhất và lớn nhất"),&dlg);
        auto* average=new QCheckBox(tr("Thêm trung bình (làm tròn 4 chữ số lẻ)"),&dlg);
        auto syncNumeric=[value,extrema,average]{
            const bool numeric=value->currentData().toInt()>=0;
            extrema->setEnabled(numeric);average->setEnabled(numeric);
            if(!numeric){extrema->setChecked(false);average->setChecked(false);}
        };
        connect(value,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,syncNumeric);
        syncNumeric();form->addRow(extrema);form->addRow(average);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dlg);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        if(dlg.exec()!=QDialog::Accepted) return;
        const auto r=groupCsv(d->tableA,key->currentData().toInt(),value->currentData().toInt(),extrema->isChecked(),average->isChecked());
        if(!r.ok){QMessageBox::warning(this,tr("Tổng hợp nhóm"),r.error);return;}
        QString rep=tr("%1 nhóm từ %2 dòng; ô số rỗng không tham gia thống kê số.").arg(r.table.rows.size()).arg(d->tableA.rows.size());
        rep+=QStringLiteral("\n")+r.table.header.join(QStringLiteral(" | "));
        for(int i=0;i<r.table.rows.size() && i<20;++i)rep+=QStringLiteral("\n")+r.table.rows[i].join(QStringLiteral(" | "));
        QDialog preview(this);preview.setWindowTitle(tr("Xem trước 20 nhóm"));preview.resize(720,420);
        auto* layout=new QVBoxLayout(&preview);
        auto* text=new QPlainTextEdit(rep,&preview);text->setReadOnly(true);layout->addWidget(text);
        auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&preview);layout->addWidget(actions);
        connect(actions,&QDialogButtonBox::accepted,&preview,&QDialog::accept);
        connect(actions,&QDialogButtonBox::rejected,&preview,&QDialog::reject);
        if(preview.exec()!=QDialog::Accepted)return;
        const auto out=askSave(defaultOut(d->a->path(),tr("tổng hợp nhóm")));
        if(!out.isEmpty())save(out,r.table,rep);
        d->report->setPlainText(rep);
    });
    auto recipeSpec = [this] {
        FilterSortSpec spec;
        spec.filterCol = d->filterCol->currentData().toInt();
        spec.sortCol = d->sortCol->currentData().toInt();
        spec.mode = FilterMode(d->filterMode->currentData().toInt());
        spec.value = d->filterValue->text();
        spec.ignoreCase = d->ignoreCase->isChecked();
        spec.descending = d->descending->isChecked();
        spec.outputColumns=d->outputColumns; spec.outputNames=d->outputNames;
        spec.cleanCol=d->cleanCol;spec.cleanKind=d->cleanKind;spec.moneyLocale=d->queryLocale;
        spec.dedupCol=d->dedupCol;spec.dedupFold=d->dedupFold;
        spec.groupKey=d->groupKey;spec.groupValue=d->groupValue;spec.groupExtrema=d->groupExtrema;spec.groupAverage=d->groupAverage;
        return spec;
    };
    auto resetQueryColumns=[this](const QStringList& header){
        d->queryHeader=header;
        d->filterCol->clear();d->sortCol->clear();
        d->filterCol->addItem(tr("Không lọc"),-1);d->sortCol->addItem(tr("Giữ thứ tự gốc"),-1);
        for(int i=0;i<header.size();++i){
            const auto label=tr("%1 · %2").arg(i+1).arg(header[i]);
            d->filterCol->addItem(label,i);d->sortCol->addItem(label,i);
        }
        d->filterMode->setCurrentIndex(0);d->filterValue->clear();d->ignoreCase->setChecked(false);d->descending->setChecked(false);
        d->outputColumns.clear();d->outputNames.clear();
        d->cleanCol=-1;d->dedupCol=-1;d->cleanKind=ColumnKind::Keep;d->queryLocale=MoneyLocale::VN;d->dedupFold=false;
        d->groupKey=-1;d->groupValue=-1;d->groupExtrema=false;d->groupAverage=false;
    };
    connect(joinStep,&QPushButton::clicked,this,[this,needA,resetQueryColumns]{
        if(!needA())return;
        QDialog dlg(this);dlg.setWindowTitle(tr("Ghép bảng trước quy trình"));auto* form=new QFormLayout(&dlg);
        auto* enabled=new QCheckBox(tr("Ghép bảng thứ hai trước khi xử lý"),&dlg);
        enabled->setChecked(d->queryJoin.enabled);
        auto* left=new QComboBox(&dlg);auto* right=new QComboBox(&dlg);auto* kind=new QComboBox(&dlg);
        Table tableB;QString error;const bool haveB=d->b->load(tableB,error);
        for(int i=0;i<d->tableA.header.size();++i)left->addItem(tr("%1 · %2").arg(i+1).arg(d->tableA.header[i]),i);
        if(haveB)for(int i=0;i<tableB.header.size();++i)right->addItem(tr("%1 · %2").arg(i+1).arg(tableB.header[i]),i);
        kind->addItem(tr("Left — giữ mọi dòng bảng trái"),int(JoinKind::Left));
        kind->addItem(tr("Inner — chỉ giữ dòng khớp"),int(JoinKind::Inner));
        left->setCurrentIndex(qMax(0,left->findData(d->queryJoin.leftKey)));
        right->setCurrentIndex(qMax(0,right->findData(d->queryJoin.rightKey)));
        kind->setCurrentIndex(kind->findData(int(d->queryJoin.kind)));
        auto* note=new QLabel(tr("Chọn bảng thứ hai tại tab So sánh hai bảng. Ghép trước các bước khác, có thể lọc và tổng hợp theo cột B. Thay cấu hình ghép sẽ đặt lại các bước sau để tránh sai cột."),&dlg);
        note->setWordWrap(true);form->addRow(note);form->addRow(enabled);form->addRow(tr("Khóa A:"),left);form->addRow(tr("Khóa B:"),right);form->addRow(tr("Kiểu ghép:"),kind);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dlg);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dlg,[&]{
            QueryJoinSpec candidate;
            candidate.enabled=enabled->isChecked();
            if(candidate.enabled){
                if(!haveB||right->currentIndex()<0||left->currentIndex()<0){QMessageBox::warning(&dlg,tr("Ghép bảng"),tr("Chưa có bảng thứ hai hợp lệ."));return;}
                candidate.leftKey=left->currentData().toInt();candidate.rightKey=right->currentData().toInt();
                candidate.kind=JoinKind(kind->currentData().toInt());candidate.rightHeader=tableB.header;
            }
            const auto schema=queryJoinInput(Table{d->tableA.header,{}},Table{tableB.header,{}},candidate);
            if(!schema.ok){QMessageBox::warning(&dlg,tr("Ghép bảng"),schema.error);return;}
            d->queryJoin=candidate;resetQueryColumns(schema.table.header);dlg.accept();
        });
        dlg.exec();
    });
    connect(extraSteps,&QPushButton::clicked,this,[this,needA] {
        if(!needA())return;
        QDialog dlg(this);dlg.setWindowTitle(tr("Quy trình dữ liệu"));
        auto* form=new QFormLayout(&dlg);
        auto* clean=new QComboBox(&dlg);auto* kind=new QComboBox(&dlg);
        auto* locale=new QComboBox(&dlg);auto* dedup=new QComboBox(&dlg);
        auto* fold=new QCheckBox(tr("Bỏ khoảng trắng đầu/cuối và không phân biệt hoa/thường khi bỏ trùng"),&dlg);
        clean->addItem(tr("Không làm sạch"),-1);dedup->addItem(tr("Không bỏ trùng"),-1);
        for(int i=0;i<d->queryHeader.size();++i){
            const auto label=tr("%1 · %2").arg(i+1).arg(d->queryHeader[i]);
            clean->addItem(label,i);dedup->addItem(label,i);
        }
        kind->addItem(tr("Ngày → dd/mm/yyyy"),int(ColumnKind::Date));
        kind->addItem(tr("Số tiền → số thuần"),int(ColumnKind::Money));
        locale->addItem(tr("Việt Nam: 1.234,56"),int(MoneyLocale::VN));
        locale->addItem(tr("Anh: 1,234.56"),int(MoneyLocale::EN));
        clean->setCurrentIndex(clean->findData(d->cleanCol));dedup->setCurrentIndex(dedup->findData(d->dedupCol));
        kind->setCurrentIndex(d->cleanKind==ColumnKind::Money?1:0);locale->setCurrentIndex(locale->findData(int(d->queryLocale)));
        fold->setChecked(d->dedupFold);
        form->addRow(tr("1. Cột làm sạch:"),clean);form->addRow(tr("Kiểu:"),kind);form->addRow(tr("Định dạng số:"),locale);
        auto* note=new QLabel(tr("2. Lọc theo thiết lập ở tab chính. 3. Bỏ trùng theo khóa dưới đây, giữ dòng đầu tiên sau lọc. Khóa rỗng luôn giữ. 4. Sắp xếp. 5. Chọn cột đầu ra. 6. Tổng hợp nhóm (nếu bật)."),&dlg);note->setWordWrap(true);form->addRow(note);
        form->addRow(tr("3. Cột bỏ trùng:"),dedup);form->addRow(fold);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dlg);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        if(dlg.exec()!=QDialog::Accepted)return;
        d->cleanCol=clean->currentData().toInt();d->cleanKind=d->cleanCol<0?ColumnKind::Keep:ColumnKind(kind->currentData().toInt());
        d->queryLocale=MoneyLocale(locale->currentData().toInt());d->dedupCol=dedup->currentData().toInt();d->dedupFold=fold->isChecked();
    });
    connect(groupStep,&QPushButton::clicked,this,[this,needA,recipeSpec] {
        if(!needA())return;
        QDialog dlg(this);dlg.setWindowTitle(tr("Tổng hợp cuối quy trình"));
        auto* form=new QFormLayout(&dlg);
        auto* key=new QComboBox(&dlg);auto* value=new QComboBox(&dlg);
        key->addItem(tr("Không tổng hợp"),-1);value->addItem(tr("Chỉ đếm dòng"),-1);
        for(int i=0;i<d->queryHeader.size();++i){
            const auto pos=d->outputColumns.indexOf(i);
            if(!d->outputColumns.isEmpty()&&pos<0)continue;
            const auto name=pos>=0?d->outputNames[pos]:d->queryHeader[i];
            const auto label=tr("%1 · %2").arg(i+1).arg(name);
            key->addItem(label,i);value->addItem(label,i);
        }
        key->setCurrentIndex(qMax(0,key->findData(d->groupKey)));
        value->setCurrentIndex(qMax(0,value->findData(d->groupValue)));
        auto* extrema=new QCheckBox(tr("Thêm giá trị nhỏ nhất và lớn nhất"),&dlg);
        extrema->setChecked(d->groupExtrema);
        auto* average=new QCheckBox(tr("Thêm trung bình (làm tròn 4 chữ số lẻ)"),&dlg);
        average->setChecked(d->groupAverage);
        auto syncExtrema=[&]{const bool numeric=key->currentData().toInt()>=0&&value->currentData().toInt()>=0;
            extrema->setEnabled(numeric);average->setEnabled(numeric);
            if(!numeric){extrema->setChecked(false);average->setChecked(false);}};
        connect(key,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,syncExtrema);
        connect(value,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,syncExtrema);
        syncExtrema();
        auto* note=new QLabel(tr("Tổng hợp sau khi làm sạch, lọc, bỏ trùng, sắp xếp và chọn cột. Cộng số thập phân chính xác tối đa 4 chữ số lẻ; khóa nhóm phân biệt số 0 đầu. Phải giữ cột nhóm và cột số trong bước chọn cột."),&dlg);
        note->setWordWrap(true);form->addRow(note);form->addRow(tr("Cột nhóm:"),key);form->addRow(tr("Cột số:"),value);form->addRow(extrema);form->addRow(average);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dlg);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dlg,[&]{
            auto spec=recipeSpec();spec.groupKey=key->currentData().toInt();
            spec.groupValue=spec.groupKey<0?-1:value->currentData().toInt();
            spec.groupExtrema=extrema->isChecked();spec.groupAverage=average->isChecked();
            const auto checked=filterSort(Table{d->queryHeader,{}},spec);
            if(!checked.ok){QMessageBox::warning(&dlg,tr("Tổng hợp"),checked.error);return;}
            d->groupKey=spec.groupKey;d->groupValue=spec.groupValue;d->groupExtrema=spec.groupExtrema;d->groupAverage=spec.groupAverage;dlg.accept();
        });
        dlg.exec();
    });
    connect(chooseColumns,&QPushButton::clicked,this,[this,needA,recipeSpec] {
        if(!needA()) return;
        QDialog dlg(this); dlg.setWindowTitle(tr("Cột đầu ra")); dlg.resize(540,430);
        auto* layout=new QVBoxLayout(&dlg);
        auto* note=new QLabel(tr("Tích cột cần giữ; nhấp đúp để đổi tên; dùng Lên/Xuống để đổi thứ tự. Lọc, sắp xếp và nhóm vẫn tham chiếu cột nguồn."),&dlg);
        note->setWordWrap(true); layout->addWidget(note);
        auto* list=new QListWidget(&dlg); layout->addWidget(list,1);
        QList<int> order=d->outputColumns;
        for(int i=0;i<d->queryHeader.size();++i)if(!order.contains(i))order.append(i);
        for(int i:order) {
            const auto pos=d->outputColumns.indexOf(i);
            auto* item=new QListWidgetItem(pos>=0?d->outputNames[pos]:d->queryHeader[i],list);
            item->setData(Qt::UserRole,i);
            item->setFlags(item->flags()|Qt::ItemIsUserCheckable|Qt::ItemIsEditable);
            item->setToolTip(tr("Cột nguồn %1: %2").arg(i+1).arg(d->queryHeader[i]));
            item->setCheckState(d->outputColumns.isEmpty()||pos>=0?Qt::Checked:Qt::Unchecked);
        }
        auto* ordering=new QHBoxLayout;
        auto* up=new QPushButton(tr("↑ Lên"),&dlg);auto* down=new QPushButton(tr("↓ Xuống"),&dlg);
        ordering->addWidget(up);ordering->addWidget(down);ordering->addStretch();layout->addLayout(ordering);
        auto move=[list](int delta){
            const int row=list->currentRow(),target=row+delta;
            if(row<0||target<0||target>=list->count())return;
            list->insertItem(target,list->takeItem(row));list->setCurrentRow(target);
        };
        connect(up,&QPushButton::clicked,&dlg,[move]{move(-1);});
        connect(down,&QPushButton::clicked,&dlg,[move]{move(1);});
        auto updateButtons=[list,up,down](int row){up->setEnabled(row>0);down->setEnabled(row>=0&&row+1<list->count());};
        connect(list,&QListWidget::currentRowChanged,&dlg,updateButtons);updateButtons(-1);
        auto* reset=new QPushButton(tr("Giữ tất cả cột và tên gốc"),&dlg); layout->addWidget(reset);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dlg); layout->addWidget(buttons);
        connect(reset,&QPushButton::clicked,&dlg,[&] { d->outputColumns.clear(); d->outputNames.clear(); dlg.accept(); });
        connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dlg,[&] {
            auto spec=recipeSpec();spec.outputColumns.clear();spec.outputNames.clear();
            for(int i=0;i<list->count();++i) if(list->item(i)->checkState()==Qt::Checked) {
                spec.outputColumns.append(list->item(i)->data(Qt::UserRole).toInt()); spec.outputNames.append(list->item(i)->text().trimmed());
            }
            if(spec.outputColumns.isEmpty()) { QMessageBox::warning(&dlg,tr("Cột đầu ra"),tr("Chọn ít nhất một cột.")); return; }
            const auto checked=filterSort(Table{d->queryHeader,{}},spec);
            if(!checked.ok) { QMessageBox::warning(&dlg,tr("Cột đầu ra"),checked.error); return; }
            d->outputColumns=spec.outputColumns; d->outputNames=spec.outputNames; dlg.accept();
        });
        dlg.exec();
    });
    connect(saveRecipe, &QPushButton::clicked, this, [this, needA, recipeSpec] {
        if (!needA()) return;
        const auto spec = recipeSpec();
        const auto validated = filterSort(Table{d->queryHeader, {}}, spec);
        if (!validated.ok) { QMessageBox::warning(this, tr("Cấu hình"), validated.error); return; }
        if(d->queryJoin.enabled){
            Table right;QString err;
            if(!d->b->load(right,err)||right.header!=d->queryJoin.rightHeader){QMessageBox::warning(this,tr("Cấu hình"),tr("Chọn đúng bảng thứ hai trước khi lưu cấu hình."));return;}
        }
        const auto bytes=encodeQueryRecipe(d->tableA.header,d->queryJoin,spec);
        if(bytes.isEmpty()||bytes.size()>1024*1024){QMessageBox::warning(this,tr("Cấu hình"),tr("Cấu hình không hợp lệ hoặc quá lớn."));return;}
        const auto path = QFileDialog::getSaveFileName(this, tr("Lưu cấu hình (có tên cột và điều kiện lọc)"), QString(), tr("Cấu hình JSON (*.json)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            QMessageBox::warning(this, tr("Cấu hình"), tr("Không tạo được tệp mới: %1").arg(f.errorString())); return;
        }
         if (f.write(bytes) != bytes.size() || !f.flush()) {
            QMessageBox::warning(this, tr("Cấu hình"), tr("Ghi không đầy đủ. Không sử dụng tệp cấu hình vừa tạo.")); return;
        }
        d->report->setPlainText(tr("Đã lưu cấu hình. Không lưu đường dẫn hoặc các dòng dữ liệu nguồn."));
    });
    connect(loadRecipe, &QPushButton::clicked, this, [this, needA,resetQueryColumns] {
        if (!needA()) return;
        const auto path = QFileDialog::getOpenFileName(this, tr("Nạp cấu hình lọc và sắp xếp"), QString(), tr("Cấu hình JSON (*.json)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024) {
            QMessageBox::warning(this, tr("Cấu hình"), tr("Không đọc được tệp hoặc tệp lớn hơn 1 MB.")); return;
        }
        FilterSortSpec spec;QueryJoinSpec candidate;QString error;Table right;QString rightError;
        d->b->load(right,rightError);
        if (!decodeQueryRecipe(f.read(1024 * 1024 + 1), d->tableA.header,right.header,candidate,spec,error)) {
            QMessageBox::warning(this, tr("Cấu hình"), error); return;
        }
        const auto schema=queryJoinInput(Table{d->tableA.header,{}},Table{right.header,{}},candidate);
        if(!schema.ok){QMessageBox::warning(this,tr("Cấu hình"),schema.error);return;}
        d->queryJoin=candidate;resetQueryColumns(schema.table.header);
        d->filterCol->setCurrentIndex(d->filterCol->findData(spec.filterCol));
        d->sortCol->setCurrentIndex(d->sortCol->findData(spec.sortCol));
        d->filterMode->setCurrentIndex(d->filterMode->findData(int(spec.mode)));
        d->filterValue->setText(spec.value);
        d->ignoreCase->setChecked(spec.ignoreCase);
        d->descending->setChecked(spec.descending);
        d->outputColumns=spec.outputColumns; d->outputNames=spec.outputNames;
        d->cleanCol=spec.cleanCol;d->cleanKind=spec.cleanKind;d->queryLocale=spec.moneyLocale;
        d->dedupCol=spec.dedupCol;d->dedupFold=spec.dedupFold;
        d->groupKey=spec.groupKey;d->groupValue=spec.groupValue;d->groupExtrema=spec.groupExtrema;d->groupAverage=spec.groupAverage;
        d->report->setPlainText(tr("Đã nạp cấu hình. Bấm Xem trước để chạy với dữ liệu hiện tại."));
    });
    connect(run4, &QPushButton::clicked, this, [this, askSave, save, needA, recipeSpec] {
        if (!needA()) return;
        Table right;QString error;
        if(d->queryJoin.enabled&&!d->b->load(right,error)){QMessageBox::warning(this,tr("Ghép bảng"),error);return;}
        const auto input=queryJoinInput(d->tableA,right,d->queryJoin);
        if(!input.ok){QMessageBox::warning(this,tr("Ghép bảng"),input.error);return;}
        if(input.table.header!=d->queryHeader){QMessageBox::warning(this,tr("Quy trình"),tr("Cấu trúc cột đã thay đổi; hãy thiết lập lại."));return;}
        const auto r = filterSort(input.table, recipeSpec());
        if (!r.ok) { QMessageBox::warning(this, tr("Lọc và sắp xếp"), r.error); return; }
        QString rep = tr("Kết quả: %1 / %2 dòng. Sắp xếp dạng chữ, không ép kiểu; nguồn không bị sửa.")
            .arg(r.table.rows.size()).arg(d->tableA.rows.size());
        rep += tr("\nLàm sạch %1 ô; %2 cảnh báo (giữ nguyên ô không chắc chắn); lọc bỏ %3 dòng; bỏ trùng %4 dòng.")
            .arg(r.cleanedCells).arg(r.cleanWarnings).arg(r.filteredRows).arg(r.removedDuplicates);
        if(d->queryJoin.enabled)rep+=tr("\nGhép: %1 khớp, %2 không khớp.").arg(input.matched).arg(input.unmatched);
        if(r.grouped)rep+=tr("\nTổng hợp %1 dòng thành %2 nhóm.").arg(r.groupedInputRows).arg(r.table.rows.size());
        rep += QStringLiteral("\n") + r.table.header.join(QStringLiteral(" | "));
        for (int i = 0; i < r.table.rows.size() && i < 20; ++i)
            rep += QStringLiteral("\n") + r.table.rows[i].join(QStringLiteral(" | "));
        QDialog preview(this); preview.setWindowTitle(tr("Xem trước · tối đa 20 dòng")); preview.resize(700, 420);
        auto* layout = new QVBoxLayout(&preview);
        auto* report = new QPlainTextEdit(rep, &preview); report->setReadOnly(true); layout->addWidget(report);
        auto* output = new QPushButton(tr("Lưu bản sao CSV…"), &preview);
        auto* cancel = new QPushButton(tr("Hủy"), &preview);
        layout->addWidget(output); layout->addWidget(cancel);
        connect(output, &QPushButton::clicked, &preview, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, &preview, &QDialog::reject);
        if (preview.exec() != QDialog::Accepted) return;
        const auto out = askSave(defaultOut(d->a->path(), tr("đã lọc")));
        if (!out.isEmpty()) save(out, r.table, rep);
        d->report->setPlainText(rep);
    });

    connect(join, &QPushButton::clicked, this, [this, askSave, save, needA] {
        if (!needA()) return;
        Table right; QString error;
        if (!d->b->load(right,error)) { QMessageBox::warning(this,tr("Ghép bảng"),error); return; }
        if(d->tableA.header.isEmpty() || right.header.isEmpty()) { QMessageBox::warning(this,tr("Ghép bảng"),tr("Hai bảng cần có tiêu đề.")); return; }
        QDialog options(this); options.setWindowTitle(tr("Ghép hai bảng"));
        auto* form=new QFormLayout(&options);
        auto* leftKey=new QComboBox(&options);
        auto* rightKey=new QComboBox(&options);
        auto* kind=new QComboBox(&options);
        for(int i=0;i<d->tableA.header.size();++i) leftKey->addItem(tr("%1 · %2").arg(i+1).arg(d->tableA.header[i]),i);
        for(int i=0;i<right.header.size();++i) rightKey->addItem(tr("%1 · %2").arg(i+1).arg(right.header[i]),i);
        kind->addItem(tr("Left — giữ mọi dòng bảng trái"),int(JoinKind::Left));
        kind->addItem(tr("Inner — chỉ giữ dòng có khóa khớp"),int(JoinKind::Inner));
        form->addRow(tr("Khóa bảng trái:"),leftKey); form->addRow(tr("Khóa bảng phải:"),rightKey); form->addRow(tr("Kiểu ghép:"),kind);
        auto* note=new QLabel(tr("So khớp chính xác dạng chữ, phân biệt hoa/thường và số 0 đầu. Khóa rỗng không ghép; khóa trùng ở bảng phải bị từ chối."),&options);
        note->setWordWrap(true); form->addRow(note);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&options); form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&options,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&options,&QDialog::reject);
        if(options.exec()!=QDialog::Accepted) return;
        const auto r=joinLookup(d->tableA,right,leftKey->currentData().toInt(),rightKey->currentData().toInt(),JoinKind(kind->currentData().toInt()));
        if(!r.ok) { QMessageBox::warning(this,tr("Ghép bảng"),r.error); return; }
        QString rep=tr("Khớp: %1 · Không khớp: %2 · Khóa rỗng trái: %3 · Khóa rỗng phải: %4. Kết quả: %5 dòng.")
            .arg(r.matched).arg(r.unmatched).arg(r.blankLeft).arg(r.blankRight).arg(r.table.rows.size());
        rep+=QStringLiteral("\n")+r.table.header.join(QStringLiteral(" | "));
        for(int i=0;i<r.table.rows.size() && i<20;++i) rep+=QStringLiteral("\n")+r.table.rows[i].join(QStringLiteral(" | "));
        QDialog preview(this); preview.setWindowTitle(tr("Xem trước ghép bảng · tối đa 20 dòng")); preview.resize(720,420);
        auto* layout=new QVBoxLayout(&preview);
        auto* text=new QPlainTextEdit(rep,&preview); text->setReadOnly(true); layout->addWidget(text);
        auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&preview); layout->addWidget(actions);
        connect(actions,&QDialogButtonBox::accepted,&preview,&QDialog::accept);
        connect(actions,&QDialogButtonBox::rejected,&preview,&QDialog::reject);
        if(preview.exec()!=QDialog::Accepted) return;
        const auto out=askSave(defaultOut(d->a->path(),tr("đã ghép")));
        if(!out.isEmpty()) save(out,r.table,rep);
        d->report->setPlainText(rep);
    });
    connect(run3, &QPushButton::clicked, this, [this, askSave, save, needA] {
        if (!needA()) return;
        Table tb; QString err;
        if (!d->b->load(tb, err)) { QMessageBox::information(this, tr("Tiện ích bảng"), err); return; }
        if (d->keyCol->currentText().isEmpty()) { QMessageBox::information(this, tr("Tiện ích bảng"), tr("Hãy chọn cột mã.")); return; }
        const auto r = compareTables(d->tableA, tb, d->keyCol->currentText());
        QString rep = tr("So sánh: giống %1, khác %2 ô, chỉ có ở tệp 1: %3 dòng, chỉ có ở tệp 2: %4 dòng.").arg(r.same).arg(r.changed.size()).arg(r.onlyInA.size()).arg(r.onlyInB.size());
        for (const QString& w : r.warnings) rep += tr("\nCẢNH BÁO: %1").arg(w);
        if (!r.ok) { d->report->setPlainText(rep + tr("\nKhông thực hiện được.")); return; }
        const QString out = askSave(defaultOut(d->a->path(), tr("khác biệt")));
        if (out.isEmpty()) { d->report->setPlainText(rep + tr("\n(Chưa lưu.)")); return; }
        save(out, diffAsTable(r), rep);
        d->report->setPlainText(rep);
    });
}

void TableToolsDialog::run(QWidget* parent, Tab tab, const QString& initialPath) {
    TableToolsDialog dlg(parent);
    dlg.d->tabs->setCurrentIndex(tab);
    if (!initialPath.isEmpty() && QFileInfo::exists(initialPath)) dlg.d->a->setPath(initialPath);
    dlg.exec();
    delete dlg.d;
}
