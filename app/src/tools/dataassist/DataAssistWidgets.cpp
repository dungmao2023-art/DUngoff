// SPDX-License-Identifier: MPL-2.0
#include "DataAssistWidgets.h"
#include "DataChecks.h"
#include "FormulaCatalog.h"
#include "TemplateExport.h"
#include "MailMergeCore.h"
#include "TableOps.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QSaveFile>
#include <QFile>
#include <QInputDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QRegularExpression>
#include <QTimer>
#include <QTreeWidget>
#include "../../templates/TemplateLibrary.h"

namespace dataassist {
Hooks &hooks() { static Hooks h; return h; }
}
using namespace dataassist;

// ============================================================ ƒx · Tôi muốn
namespace {
class FormulaAssistant : public QWidget {
public:
    explicit FormulaAssistant(QWidget *p) : QWidget(p)
    {
        auto *lay = new QVBoxLayout(this);
        auto* intro = new QLabel(QStringLiteral("<b>ƒx · Tôi muốn</b><br>1. Chọn hàm → 2. Điền dữ liệu → 3. Chọn ô kết quả"));
        intro->setWordWrap(true);
        lay->addWidget(intro);
        auto *top = new QVBoxLayout;
        search_ = new QLineEdit; search_->setPlaceholderText(QStringLiteral("Tôi muốn…"));
        cat_ = new QComboBox; cat_->addItem(QStringLiteral("Tất cả nhóm"), -1);
        for (Category c : allCategories()) cat_->addItem(categoryName(c), int(c));
        top->addWidget(search_); top->addWidget(cat_);
        lay->addLayout(top);
        list_ = new QComboBox; list_->setObjectName(QStringLiteral("FormulaFunction"));
        list_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        list_->setMinimumContentsLength(18); lay->addWidget(list_);
        desc_ = new QLabel; desc_->setWordWrap(true); desc_->hide(); lay->addWidget(desc_);
        pairs_ = new QSpinBox; pairs_->setRange(1, 8); pairs_->setValue(1);
        pairs_->setPrefix(QStringLiteral("Số cặp điều kiện: ")); pairs_->hide(); lay->addWidget(pairs_);
        autoPick_ = new QCheckBox(QStringLiteral("Tự lấy vùng khi chọn trên bảng tính"));
        autoPick_->setChecked(true); autoPick_->hide(); lay->addWidget(autoPick_);
        form_ = new QFormLayout; form_->setRowWrapPolicy(QFormLayout::WrapAllRows); lay->addLayout(form_);
        optional_ = new QCheckBox(QStringLiteral("Thêm tham số tùy chọn"));
        optional_->hide(); lay->addWidget(optional_);
        connect(optional_,&QCheckBox::toggled,this,[this](bool on){
            if(!on) for(int i=0;i<edits_.size();++i) if(args_[i].optional) {
                QSignalBlocker block(edits_[i]); edits_[i]->clear();
            }
            rebuild();
        });
        connect(pairs_, &QSpinBox::valueChanged, this, [this](int) {
            if (!cur_ || !hasConditionPairs(*cur_)) return;
            cancelPick();
            QStringList saved;
            for (auto* e : edits_) saved << e->text();
            while (form_->rowCount()) form_->removeRow(0);
            edits_.clear();
            populateArgs(saved);
        });
        output_ = new QWidget(this); output_->setObjectName(QStringLiteral("FormulaOutput")); auto* outLay = new QVBoxLayout(output_); outLay->setContentsMargins(0,0,0,0);
        auto* targetRow = new QHBoxLayout;
        target_ = new QLineEdit; target_->setObjectName(QStringLiteral("FormulaTarget")); target_->setPlaceholderText(QStringLiteral("VD: C6"));
        auto* useTarget = new QPushButton(QStringLiteral("Lấy ô"));
        auto* pickTarget = new QPushButton(QStringLiteral("Chọn ô"));
        pickTarget->setToolTip(QStringLiteral("Bấm rồi chọn đúng một ô kết quả trên bảng tính"));
        targetRow->addWidget(target_, 1); targetRow->addWidget(useTarget); targetRow->addWidget(pickTarget);
        connect(pickTarget,&QPushButton::clicked,this,[this]{armPick(target_);});
        outLay->addWidget(new QLabel(QStringLiteral("3. Chọn ô kết quả (sheet hiện tại)")));
        outLay->addLayout(targetRow);
        connect(useTarget, &QPushButton::clicked, this, [this] {
            const QString addr = hooks().selectedCellAddress ? hooks().selectedCellAddress() : QString();
            if (addr.isEmpty()) { status_->setText(QStringLiteral("Chưa nhận được địa chỉ ô từ engine.")); return; }
            cancelPick();
            target_->setText(addr);
        });
        result_ = new QLineEdit; result_->setObjectName(QStringLiteral("FormulaResult")); result_->setPlaceholderText(QStringLiteral("Công thức sẽ hiện ở đây — sửa tự do được"));
        outLay->addWidget(result_);
        quickToggle_ = new QCheckBox(QStringLiteral("Phép tính nhanh (+ − × ÷)"));
        lay->addWidget(quickToggle_);
        quickPanel_ = new QWidget(this);
        auto* quick = new QFormLayout(quickPanel_);
        quickPanel_->hide();
        auto* qa = quickA_ = new QLineEdit; qa->setPlaceholderText(QStringLiteral("A1 hoặc số"));
        auto* qb = quickB_ = new QLineEdit; qb->setPlaceholderText(QStringLiteral("B1 hoặc số"));
        auto quickField = [this](QLineEdit* e) {
            auto* field = new QWidget;
            auto* row = new QHBoxLayout(field); row->setContentsMargins(0,0,0,0);
            row->addWidget(e,1);
            auto* pick = new QPushButton(QStringLiteral("Chọn ô"));
            row->addWidget(pick);
            connect(pick,&QPushButton::clicked,this,[this,e]{armPick(e);});
            return field;
        };
        auto* op = new QComboBox;
        for (const auto& s : {QStringLiteral("+"), QStringLiteral("-"), QStringLiteral("*"), QStringLiteral("/"), QStringLiteral("^"), QStringLiteral("Tỷ lệ %")}) op->addItem(s);
        auto* make = new QPushButton(QStringLiteral("Tạo phép tính nhanh"));
        quick->addRow(QStringLiteral("Giá trị A"), quickField(qa)); quick->addRow(QStringLiteral("Phép tính"), op); quick->addRow(QStringLiteral("Giá trị B"), quickField(qb));
        quick->addRow(make); lay->addWidget(quickPanel_);
        connect(make, &QPushButton::clicked, this, [this, qa, qb, op] {
            const auto a = qa->text().trimmed(), b = qb->text().trimmed();
            if (a.isEmpty() || b.isEmpty()) { status_->setText(QStringLiteral("Nhập đủ hai giá trị hoặc địa chỉ ô.")); return; }
            const QString symbol = op->currentText();
            const QString formula = symbol == QStringLiteral("Tỷ lệ %")
                ? QStringLiteral("=(%1)/(%2)*100").arg(a,b)
                : QStringLiteral("=(%1)%2(%3)").arg(a,symbol,b);
            const auto check = quickSyntaxCheck(formula);
            if (!check.ok) { status_->setText(check.message); return; }
            result_->setText(formula);
            status_->setText(QStringLiteral("Công thức sẵn sàng. Chọn ô kết quả rồi chèn để engine tính. Tỷ lệ % trả về số phần trăm, ví dụ 25."));
        });
        auto *btns = new QVBoxLayout;
        insert_ = new QPushButton(QStringLiteral("Chèn vào ô kết quả"));
        copy_ = new QPushButton(QStringLiteral("Sao chép công thức"));
        btns->addWidget(insert_); btns->addWidget(copy_); btns->addStretch();
        outLay->addLayout(btns); lay->addWidget(output_); output_->hide();
        status_ = new QLabel; status_->setWordWrap(true); lay->addWidget(status_);
        cancel_ = new QPushButton(QStringLiteral("Hủy chọn ô/vùng")); cancel_->hide();
        lay->addWidget(cancel_);
        connect(cancel_,&QPushButton::clicked,this,[this]{cancelPick();autoPick_->setChecked(false);status_->setText(QStringLiteral("Đã hủy chọn tham chiếu. Bấm Chọn ô/vùng để tiếp tục."));});
        connect(search_, &QLineEdit::textChanged, this, [this] { refill(); });
        connect(cat_, &QComboBox::currentIndexChanged, this, [this] { refill(); });
        connect(list_, &QComboBox::currentIndexChanged, this, [this](int r) { select(r); });
        connect(quickToggle_,&QCheckBox::toggled,this,[this](bool on){
            cancelPick(); clearForm(); target_->clear();
            quickPanel_->setVisible(on); search_->setEnabled(!on);cat_->setEnabled(!on);list_->setEnabled(!on);
            { QSignalBlocker block(list_); list_->setCurrentIndex(-1); }
            autoPick_->setVisible(on);
            status_->setText(on?QStringLiteral("Nhập hoặc chọn hai giá trị rồi tạo phép tính."):QStringLiteral("Chọn một hàm để bắt đầu."));
        });
        connect(result_,&QLineEdit::textChanged,this,[this](const QString& v){
            const bool valid=!v.isEmpty() && quickSyntaxCheck(v).ok;
            if(!v.isEmpty())output_->show();
            insert_->setEnabled(valid); copy_->setEnabled(valid);
        });
        connect(copy_, &QPushButton::clicked, this, [this] { qApp->clipboard()->setText(result_->text()); status_->setText(QStringLiteral("Đã sao chép. Dán vào ô Excel (dấu ngăn đối số theo cài đặt của engine).")); });
        connect(insert_, &QPushButton::clicked, this, [this] {
            const SyntaxCheck sc = quickSyntaxCheck(result_->text());
            if (!sc.ok) { status_->setText(QStringLiteral("Chưa chèn: ") + sc.message); return; }
            if (!hooks().insertFormulaAt) { status_->setText(QStringLiteral("Chưa nối với tab Excel đang mở — hãy dùng “Sao chép công thức” rồi dán vào ô.")); return; }
            if (target_->text().trimmed().isEmpty()) { status_->setText(QStringLiteral("Hãy nhập ô kết quả hoặc bấm Lấy ô.")); return; }
            QString res, msg;
            if (!hooks().insertFormulaAt(target_->text(), result_->text(), &res, &msg)) { status_->setText(msg.isEmpty() ? QStringLiteral("Không chèn được (không có tab Excel đang mở?).") : msg); return; }
            QString note = QStringLiteral("Đã gửi công thức tới ô kết quả; giá trị đọc lại: ") + (res.isEmpty() ? QStringLiteral("(trống)") : res) + QStringLiteral(". Hoàn tác bằng Ctrl+Z.");
            if (res.startsWith(QLatin1Char('#')) || res.startsWith(QStringLiteral("Err:"))) {
                const QString fn = result_->text().mid(1, result_->text().indexOf(QLatin1Char('(')) - 1);
                const QString hint = replacementHint(fn);
                note += QStringLiteral(" Engine báo lỗi.") + (hint.isEmpty() ? QString() : QStringLiteral(" ") + hint);
            }
            status_->setText(note);
        });
        insert_->setEnabled(false);copy_->setEnabled(false);
        refill();
        lay->addStretch(1);
    }
    void sheetChanged(int part,const QString& name) {
        if (sheetPart_==part && sheetName_==name) return;
        const bool initial=sheetPart_<0;
        sheetPart_=part; sheetName_=name;
        if(initial)return;
        cancelPick();
        for(auto* e:edits_) { QSignalBlocker block(e);e->clear(); }
        rebuild();
        target_->clear(); quickA_->clear(); quickB_->clear();
        if(!cur_ || !cur_->args.isEmpty()){result_->clear();output_->hide();}
        status_->setText(QStringLiteral("Đã chuyển sheet. Chọn lại tham chiếu và ô kết quả để tránh ghi nhầm."));
    }
    void selectedReference(const QString& reference) {
        if (!isVisible() || (!pendingPick_ && !autoPick_->isChecked())) return;
        if(!pendingPick_ && (!cur_ || !result_->text().isEmpty())) return;
        if (reference.isEmpty()) {
            status_->setText(QStringLiteral("Chưa xác minh được địa chỉ vùng; nhập địa chỉ thủ công nếu vùng nằm ngoài màn hình."));
            return;
        }
        QLineEdit* e = pendingPick_;
        if (e==target_ || e==quickA_ || e==quickB_) {
            static const QRegularExpression cell(QStringLiteral("^\\$?[A-Z]{1,3}\\$?[1-9][0-9]*$"),QRegularExpression::CaseInsensitiveOption);
            if(!cell.match(reference).hasMatch()) {
                status_->setText(QStringLiteral("Chỉ chọn một ô, không chọn cả vùng."));
                return;
            }
            cancelPick();
            e->setText(reference);
            status_->setText(QStringLiteral("Đã lấy ô %1.").arg(reference));
            return;
        }
        if (!e && cur_) {
            for (auto* field : edits_) if (field->isVisible() && field->text().trimmed().isEmpty()) {
                if(field->property("referenceArg").toBool()) e=field;
                break; // never skip an earlier missing argument
            }
        }
        if (!e) return;
        cancelPick();
        e->setText(reference);
        if (!result_->text().isEmpty()) status_->setText(QStringLiteral("Đủ tham số. Chọn ô kết quả bằng nút Chọn ô."));
    }
private:
    void cancelPick() {pendingPick_=nullptr;if(cancel_)cancel_->hide();}
    void armPick(QLineEdit* field) {
        pendingPick_=field;
        cancel_->show();
        status_->setText(QStringLiteral("Bây giờ chọn ô hoặc kéo vùng trên bảng tính."));
    }
    void refill()
    {
        clearForm(); QSignalBlocker block(list_); shown_.clear(); list_->clear();
        const QString q = search_->text().trimmed();
        QVector<const FuncInfo*> items;
        if (!q.isEmpty()) items = search(q, 30);
        else for (const FuncInfo &f : catalog()) items.push_back(&f);
        const int cat = cat_->currentData().toInt();
        for (const FuncInfo *f : items) {
            if (cat >= 0 && int(f->cat) != cat) continue;
            shown_.push_back(f);
            list_->addItem(QStringLiteral("%1 — %2").arg(f->name, f->intent));
        }
        list_->setPlaceholderText(QStringLiteral("Chọn hàm"));
        list_->setCurrentIndex(-1);
        status_->setText(shown_.isEmpty()?QStringLiteral("Không tìm thấy hàm phù hợp."):QStringLiteral("Chọn một hàm để bắt đầu."));
    }
    void clearForm() {
        cancelPick(); cur_=nullptr; pairs_->hide(); desc_->hide(); autoPick_->hide();
        {QSignalBlocker block(optional_);optional_->setChecked(false);} optional_->hide();
        while(form_->rowCount())form_->removeRow(0);
        edits_.clear();args_.clear(); result_->clear();output_->hide();
    }
    void select(int r)
    {
        clearForm();
        if (r < 0 || r >= shown_.size()) return;
        cur_ = shown_[r];
        desc_->show(); autoPick_->show(); target_->clear();
        pairs_->setVisible(hasConditionPairs(*cur_));
        desc_->setText(QStringLiteral("<b>%1</b> (%2): %3<br>Ví dụ: <code>%4</code>%5").arg(cur_->name, categoryName(cur_->cat), cur_->description, cur_->example.toHtmlEscaped(),
            cur_->newerEngineOnly ? QStringLiteral("<br><i>Hàm mảng động/mới: engine Windows 26.8.3.1 đã kiểm thử; engine Linux cũ trả #NAME?. Nếu ô báo #NAME? thì engine chưa hỗ trợ — không phải lỗi của bạn.</i>") : QString()));
        populateArgs({});
    }
    void populateArgs(const QStringList& saved) {
        args_ = hasConditionPairs(*cur_) ? conditionArgs(*cur_, pairs_->value()) : cur_->args;
        for (const ArgInfo &a : args_) {
            auto *e = new QLineEdit; e->setObjectName(QStringLiteral("FormulaArgument%1").arg(edits_.size()));e->setPlaceholderText(a.hint);
            static const QRegularExpression referenceHint(QStringLiteral("^\\$?[A-Z]{1,3}\\$?[1-9][0-9]*(?::\\$?[A-Z]{1,3}\\$?[1-9][0-9]*)?$"), QRegularExpression::CaseInsensitiveOption);
            e->setProperty("referenceArg", referenceHint.match(a.hint).hasMatch());
            if (edits_.size() < saved.size()) e->setText(saved.at(edits_.size()));
            edits_.push_back(e);
            auto* field = new QWidget;
            auto* row = new QHBoxLayout(field); row->setContentsMargins(0, 0, 0, 0);
            row->addWidget(e, 1);
            auto* pick = new QPushButton(QStringLiteral("Chọn vùng"));
            pick->setToolTip(QStringLiteral("Bấm rồi chọn ô/vùng trên bảng; địa chỉ tự điền nếu xác minh được."));
            row->addWidget(pick);
            pick->setObjectName(QStringLiteral("FormulaPick%1").arg(edits_.size()-1));
            connect(pick, &QPushButton::clicked, this, [this, e] {
                armPick(e);
            });
            form_->addRow(a.name + (a.optional ? QStringLiteral(" (tuỳ chọn)") : QString()), field);
            connect(e, &QLineEdit::textChanged, this, [this] { rebuild(); });
        }
        rebuild();
    }
    void rebuild()
    {
        if (!cur_) return;
        QStringList v; bool prior=true,required=true,hasOptional=false;
        for(int i=0;i<edits_.size();++i) {
            const bool opt=args_[i].optional;
            const bool filled=!edits_[i]->text().trimmed().isEmpty();
            hasOptional|=opt; if(!opt && !filled)required=false;
            form_->setRowVisible(i,(!opt || optional_->isChecked()) && (prior || filled));
            if(!filled)prior=false;
            v << edits_[i]->text();
        }
        optional_->setVisible(hasOptional && required);
        if(pendingPick_ && edits_.contains(pendingPick_) && !pendingPick_->isVisibleTo(this))cancelPick();
        const BuildResult b = buildFormula(*cur_, v);
        if (b.ok) { result_->setText(b.formula); status_->setText(QStringLiteral("Đủ tham số. Chọn ô kết quả.")); }
        else { result_->clear();output_->hide();status_->setText(b.error); }
    }
    QLineEdit *search_{}, *result_{}, *target_{}, *quickA_{}, *quickB_{}, *pendingPick_{};
    int sheetPart_=-1;
    QString sheetName_;
    QComboBox *cat_{};
    QSpinBox *pairs_{};
    QCheckBox *autoPick_{};
    QComboBox *list_{};
    QWidget *output_{},*quickPanel_{};
    QCheckBox *optional_{},*quickToggle_{};
    QPushButton *cancel_{};
    QVector<ArgInfo> args_;
    QLabel *desc_{}, *status_{};
    QFormLayout *form_{};
    QPushButton *insert_{}, *copy_{};
    QVector<const FuncInfo*> shown_;
    const FuncInfo *cur_ = nullptr;
    QVector<QLineEdit*> edits_;
};

// ============================================================ Thống kê vùng / Lỗi / Biểu đồ nhanh
class RangeTools : public QWidget {
public:
    explicit RangeTools(QWidget *p, std::function<QString()> readSelection = {}, std::function<bool()> inputIdle = {}) : QWidget(p), readSelection_(std::move(readSelection)), inputIdle_(std::move(inputIdle))
    {
        auto *lay = new QVBoxLayout(this);
        lay->addWidget(new QLabel(QStringLiteral("<b>Công cụ vùng dữ liệu</b><br>Dán vùng ô (từ Excel) hoặc mở CSV/XLSX. Chỉ đọc — dữ liệu gốc không bị sửa.")));
        input_ = new QPlainTextEdit; input_->setPlaceholderText(QStringLiteral("Dán vùng ô tại đây (các ô cách nhau bằng Tab)"));
        lay->addWidget(input_, 1);
        auto *row = new QHBoxLayout;
        auto *open = new QPushButton(QStringLiteral("Mở CSV/XLSX…"));
        auto *fromSel = new QPushButton(QStringLiteral("Lấy vùng đang chọn"));
        vi_ = new QCheckBox(QStringLiteral("Số kiểu Việt (1.234,5)")); vi_->setChecked(true);
        row->addWidget(open); row->addWidget(fromSel);
        lay->addLayout(row);
        lay->addWidget(vi_);
        if (readSelection_) {
            live_ = new QCheckBox(QStringLiteral("Tự cập nhật thống kê vùng chọn"));
            live_->setToolTip(QStringLiteral("Chỉ đọc vùng khi panel đang hiện. Tắt nếu chọn vùng rất lớn."));
            lay->addWidget(live_);
            timer_ = new QTimer(this); timer_->setSingleShot(true); timer_->setInterval(220);
            connect(timer_, &QTimer::timeout, this, [this] {
                if(!isVisible()||!live_->isChecked())return;
                if(inputIdle_&&!inputIdle_()){timer_->start();return;}
                readAndSummarize();
            });
            connect(live_, &QCheckBox::toggled, this, [this](bool enabled) { if (enabled) scheduleRefresh(); else timer_->stop(); });
        }
        auto *row2 = new QGridLayout;
        for (auto &pr : {std::pair<QString, int>{QStringLiteral("Thống kê"), 0}, {QStringLiteral("Lỗi công thức"), 1}, {QStringLiteral("Dòng trùng"), 2}, {QStringLiteral("Tổng hợp nhóm"), 3}}) {
            auto *b = new QPushButton(pr.first); const int id = pr.second;
            connect(b, &QPushButton::clicked, this, [this, id] { run(id); });
            row2->addWidget(b,id/2,id%2);
        }
        lay->addLayout(row2);
        out_ = new QPlainTextEdit; out_->setReadOnly(true); lay->addWidget(out_, 1);
        connect(open, &QPushButton::clicked, this, [this] { openFile(); });
        connect(fromSel, &QPushButton::clicked, this, [this] {
            readAndSummarize();
        });
    }
    void scheduleRefresh() { if (timer_ && live_->isChecked() && isVisible()) timer_->start(); }
private:
    void readAndSummarize() {
        const auto reader = readSelection_ ? readSelection_ : hooks().selectedRangeText;
        if (!reader) { out_->setPlainText(QStringLiteral("Chưa nối với bảng tính.")); return; }
        const QString text = reader();
        if (text.size() > 200000) {
            input_->clear();
            out_->setPlainText(QStringLiteral("Vùng quá lớn để thống kê nhanh. Hãy chọn vùng nhỏ hơn."));
            return;
        }
        input_->setPlainText(text); run(0);
    }
    void openFile()
    {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Mở dữ liệu"), QString(), QStringLiteral("Bảng (*.csv *.xlsx)"));
        if (f.isEmpty()) return;
        mailmerge::Table t = f.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive) ? mailmerge::readXlsxFile(f, 0, false) : mailmerge::readCsvFile(f, false);
        if (!t.ok()) { out_->setPlainText(t.error); return; }
        QStringList lines;
        for (const QStringList &r : t.rows) lines << r.join(QLatin1Char('\t'));
        input_->setPlainText(lines.join(QLatin1Char('\n')));
        out_->setPlainText(t.warnings.join(QLatin1Char('\n')));
    }
    void groupDialog(const QVector<QStringList>& grid) {
        int width=0;
        for(const auto& r:grid)width=qMax(width,int(r.size()));
        if(width<1 || width>16384 || grid.size()>100001) {
            out_->setPlainText(QStringLiteral("Vùng vượt giới hạn tổng hợp."));return;
        }
        QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("Tổng hợp theo nhóm · Excel"));dialog.resize(850,530);
        auto* layout=new QVBoxLayout(&dialog);
        auto* note=new QLabel(QStringLiteral("Chọn cột nhóm và cột số để tổng hợp. Chỉ đọc dữ liệu hiển thị; không tạo đối tượng PivotTable và không sửa bảng tính."),&dialog);
        note->setWordWrap(true);layout->addWidget(note);
        auto* header=new QCheckBox(QStringLiteral("Dòng đầu là tiêu đề"),&dialog);header->setChecked(true);layout->addWidget(header);
        auto* form=new QFormLayout;
        auto* label=new QComboBox(&dialog);auto* value=new QComboBox(&dialog);
        form->addRow(QStringLiteral("Nhóm theo"),label);form->addRow(QStringLiteral("Cột tính toán"),value);layout->addLayout(form);
        auto* table=new QTableWidget(0,7,&dialog);
        table->setHorizontalHeaderLabels({QStringLiteral("Nhóm"),QStringLiteral("Số dòng"),QStringLiteral("Ô số"),QStringLiteral("Tổng"),QStringLiteral("Trung bình"),QStringLiteral("Nhỏ nhất"),QStringLiteral("Lớn nhất")});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        layout->addWidget(table,1);
        auto* status=new QLabel(&dialog);status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
        auto refresh=[&] {
            const auto result=summarizeGroups(grid,label->currentData().toInt(),value->currentData().toInt(),header->isChecked(),vi_->isChecked());
            table->setRowCount(0);
            if(!result.ok){status->setText(result.error);return;}
            table->setRowCount(int(result.groups.size()));
            for(int i=0;i<result.groups.size();++i) {
                const auto& g=result.groups[i];
                const QStringList cells{g.label,QString::number(g.rows),QString::number(g.numbers),
                    g.numbers?QString::number(g.sum,'g',15):QStringLiteral("—"),
                    g.numbers?QString::number(g.sum/g.numbers,'g',15):QStringLiteral("—"),
                    g.numbers?QString::number(g.min,'g',15):QStringLiteral("—"),
                    g.numbers?QString::number(g.max,'g',15):QStringLiteral("—")};
                for(int c=0;c<cells.size();++c)table->setItem(i,c,new QTableWidgetItem(cells[c]));
            }
            for(int c=2;c<7;++c)table->setColumnHidden(c,value->currentData().toInt()<0);
            status->setText(QStringLiteral("%1 nhóm. ").arg(result.groups.size())+result.warnings.join(QStringLiteral("\n")));
        };
        auto refill=[&] {
            QSignalBlocker a(label),b(value);
            const int selected=label->currentIndex(),measure=value->currentIndex();
            label->clear();value->clear();value->addItem(QStringLiteral("Chỉ đếm số dòng"),-1);
            for(int c=0;c<width;++c){
                const QString name=QStringLiteral("Cột %1").arg(c+1)+(header->isChecked()?QStringLiteral(" · ")+grid.first().value(c):QString());
                label->addItem(name,c);value->addItem(name,c);
            }
            label->setCurrentIndex(qBound(0,selected,width-1));value->setCurrentIndex(qBound(0,measure,width));
            refresh();
        };
        connect(header,&QCheckBox::toggled,&dialog,refill);
        connect(label,&QComboBox::currentIndexChanged,&dialog,refresh);
        connect(value,&QComboBox::currentIndexChanged,&dialog,refresh);
        auto* close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(close);
        auto* exportChart=close->addButton(QStringLiteral("Xuất XLSX + biểu đồ…"),QDialogButtonBox::ActionRole);
        connect(exportChart,&QPushButton::clicked,&dialog,[&]{
            const bool sum=value->currentData().toInt()>=0;
            const auto result=summarizeGroups(grid,label->currentData().toInt(),value->currentData().toInt(),header->isChecked(),vi_->isChecked());
            if(!result.ok||result.groups.size()>100){
                QMessageBox::warning(&dialog,QStringLiteral("Báo cáo Excel"),result.ok?QStringLiteral("Tối đa 100 nhóm mỗi biểu đồ. Hãy lọc bớt dữ liệu."):result.error);return;
            }
            if(!result.warnings.isEmpty() && QMessageBox::question(&dialog,QStringLiteral("Xác nhận cảnh báo"),result.warnings.join(QStringLiteral("\n"))+QStringLiteral("\nVẫn xuất báo cáo mới?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            QString dest=QFileDialog::getSaveFileName(&dialog,QStringLiteral("Xuất báo cáo có biểu đồ"),QStringLiteral("TongHop.xlsx"),QStringLiteral("Excel (*.xlsx)"),nullptr,QFileDialog::DontConfirmOverwrite);
            if(dest.isEmpty())return;
            if(!dest.endsWith(QLatin1String(".xlsx"),Qt::CaseInsensitive))dest+=QStringLiteral(".xlsx");
            const auto saved=exportGroupReport(dest,result,sum);
            if(!saved.ok)QMessageBox::warning(&dialog,QStringLiteral("Báo cáo Excel"),saved.error);
            else QMessageBox::information(&dialog,QStringLiteral("Báo cáo Excel"),QStringLiteral("Đã lưu %1 nhóm và biểu đồ cột Excel thật vào tệp mới. Dữ liệu nguồn không bị sửa.\n%2").arg(saved.rowsWritten).arg(saved.outPath));
        });
        connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        refill();dialog.exec();
    }
    void run(int what)
    {
        const auto grid = parseTsv(input_->toPlainText());
        if (grid.isEmpty()) { out_->setPlainText(QStringLiteral("Chưa có dữ liệu.")); return; }
        QStringList o;
        if (what == 0) {
            const RangeStats s = rangeStats(grid, vi_->isChecked());
            o << QStringLiteral("Số ô: %1 (trống %2, số %3, chữ %4, lỗi %5)").arg(s.cells).arg(s.empty).arg(s.numeric).arg(s.text).arg(s.errors);
            if (s.hasNumbers) o << QStringLiteral("Tổng: %1\nTrung bình: %2\nNhỏ nhất: %3\nLớn nhất: %4").arg(s.sum, 0, 'f', 2).arg(s.average, 0, 'f', 2).arg(s.min, 0, 'f', 2).arg(s.max, 0, 'f', 2);
            o << s.warnings;
        } else if (what == 1) {
            const auto hits = findFormulaErrors(grid);
            if (hits.isEmpty()) o << QStringLiteral("Không thấy ô lỗi trong vùng (chỉ kiểm tra giá trị hiển thị).");
            for (const auto &h : hits) o << QStringLiteral("Dòng %1, cột %2: %3 — %4").arg(h.row).arg(h.col).arg(h.text, h.meaning);
        } else if (what == 2) {
            tbl::Table t; t.header = grid[0];
            for (int i = 1; i < grid.size(); ++i) t.rows.push_back(grid[i]);
            QList<int> cols; for (int c = 0; c < t.header.size(); ++c) cols << c;
            const auto r = tbl::removeDuplicates(t, cols, true);
            o << QStringLiteral("Dòng đầu là tiêu đề. Dòng trùng hoàn toàn: %1 (chỉ liệt kê, KHÔNG xoá gì).").arg(r.removed.size());
            for (const auto &x : r.removed) o << QStringLiteral("  Dòng %1 trùng dòng %2: %3").arg(x.originalIndex + 2).arg(x.keptIndex + 2).arg(x.cells.join(QStringLiteral(" | ")));
            o << r.warnings;
        } else {
            groupDialog(grid);
            return;
        }
        out_->setPlainText(o.join(QLatin1Char('\n')));
    }
    QPlainTextEdit *input_{}, *out_{};
    QCheckBox *vi_{}, *live_{};
    QTimer* timer_{};
    std::function<bool()> inputIdle_;
    std::function<QString()> readSelection_;
};

// ============================================================ Xuất theo mẫu
class TemplateExportWidget : public QWidget {
public:
    explicit TemplateExportWidget(QWidget *p) : QWidget(p)
    {
        auto *lay = new QVBoxLayout(this);
        lay->addWidget(new QLabel(QStringLiteral("<b>Xuất Excel theo mẫu riêng</b><br>Chọn tệp mẫu .xlsx chính thức bạn lấy từ phần mềm đích (MISA, FAST, KiotViet, Sapo…) và tệp dữ liệu. "
                                                 "Ứng dụng ghép cột, ghi ra <b>tệp mới</b>; mẫu và dữ liệu gốc không đổi. Dữ liệu phải khớp cột bắt buộc của đúng phiên bản bạn dùng — ứng dụng không bảo đảm phần mềm đích chấp nhận tệp.")));
        target_ = new QComboBox; target_->addItems({QStringLiteral("MISA AMIS"), QStringLiteral("MISA SME"), QStringLiteral("FAST Accounting"), QStringLiteral("KiotViet"), QStringLiteral("Sapo"), QStringLiteral("Mẫu XLSX khác")});
        lay->addWidget(target_);
        auto* headerForm = new QFormLayout;
        headerRow_ = new QSpinBox; headerRow_->setRange(0, 1048576); headerRow_->setSpecialValueText(QStringLiteral("Tự nhận diện")); headerRow_->setKeyboardTracking(false);
        headerForm->addRow(QStringLiteral("Dòng tiêu đề mẫu đích"), headerRow_);
        locale_ = new QComboBox;
        locale_->addItems({QStringLiteral("Việt: 1.234,5"),QStringLiteral("Anh: 1,234.5")});
        headerForm->addRow(QStringLiteral("Cách đọc số nguồn"),locale_);
        lay->addLayout(headerForm);
        auto *r1 = new QHBoxLayout;
        auto *bt = new QPushButton(QStringLiteral("Chọn tệp mẫu…")), *bs = new QPushButton(QStringLiteral("Chọn dữ liệu nguồn (CSV/XLSX)…"));
        r1->addWidget(bt); r1->addWidget(bs); lay->addLayout(r1);
        info_ = new QLabel(QStringLiteral("Chưa chọn mẫu đích.")); info_->setWordWrap(true); info_->setTextFormat(Qt::PlainText); lay->addWidget(info_);
        sourceInfo_ = new QLabel(QStringLiteral("Chưa chọn dữ liệu nguồn.")); sourceInfo_->setWordWrap(true); sourceInfo_->setTextFormat(Qt::PlainText); lay->addWidget(sourceInfo_);
        map_ = new QTableWidget(0, 3); map_->setHorizontalHeaderLabels({QStringLiteral("Cột của mẫu"), QStringLiteral("Lấy từ cột nguồn"), QStringLiteral("Kiểu")});
        map_->horizontalHeader()->setStretchLastSection(true); lay->addWidget(map_, 1);
        auto *r2 = new QHBoxLayout;
        auto *chk = new QPushButton(QStringLiteral("Kiểm tra")), *exp = new QPushButton(QStringLiteral("Xuất bản mới…"));
        r2->addWidget(chk); r2->addWidget(exp); r2->addStretch(); lay->addLayout(r2);
        auto* profiles = new QHBoxLayout;
        auto* saveProfile = new QPushButton(QStringLiteral("Lưu cấu hình ghép cột…"));
        auto* loadProfile = new QPushButton(QStringLiteral("Nạp cấu hình…"));
        profiles->addWidget(saveProfile); profiles->addWidget(loadProfile); profiles->addStretch(); lay->addLayout(profiles);
        connect(saveProfile,&QPushButton::clicked,this,[this]{saveMapping();});
        connect(loadProfile,&QPushButton::clicked,this,[this]{loadMapping();});
        log_ = new QPlainTextEdit; log_->setReadOnly(true); lay->addWidget(log_, 1);
        connect(target_, &QComboBox::currentIndexChanged, this, [this] {
            // Target is descriptive only. Require its actual template again; never reuse the previous vendor's layout silently.
            tplPath_.clear(); ti_ = {}; templateSheet_ = 0; rebuildMap();
            info_->setText(QStringLiteral("Đã đổi phần mềm đích. Hãy chọn mẫu XLSX tương ứng."));
            log_->clear();
        });
        connect(headerRow_, &QSpinBox::valueChanged, this, [this] { if (!tplPath_.isEmpty()) reloadTemplate(); });
        connect(bt, &QPushButton::clicked, this, [this] { pickTemplate(); });
        connect(bs, &QPushButton::clicked, this, [this] { pickSource(); });
        connect(chk, &QPushButton::clicked, this, [this] { check(); });
        connect(exp, &QPushButton::clicked, this, [this] { doExport(); });
    }
private:
    int chooseSheet(const QString& f)
    {
        QString error;
        const auto names = mailmerge::xlsxSheetNames(f, &error);
        if (names.isEmpty()) { log_->setPlainText(error.isEmpty() ? QStringLiteral("Không đọc được danh sách sheet.") : error); return -1; }
        if (names.size() == 1) return 0;
        bool ok = false;
        const QString name = QInputDialog::getItem(this, QStringLiteral("Chọn sheet"), QFileInfo(f).fileName(), names, 0, false, &ok);
        return ok ? names.indexOf(name) : -1;
    }
    void reloadTemplate()
    {
        ti_ = loadTemplate(tplPath_, templateSheet_, headerRow_->value());
        if (!ti_.ok || ti_.headerRow < 1 || ti_.headers.isEmpty()) {
            ti_.ok = false;
            info_->setText(ti_.error.isEmpty() ? QStringLiteral("Chưa xác định được tiêu đề mẫu. Hãy chọn dòng tiêu đề.") : ti_.error);
            rebuildMap(); return;
        }
        info_->setText(QStringLiteral("Mẫu: %1 — sheet “%2”, tiêu đề dòng %3, %4 cột.")
            .arg(QFileInfo(tplPath_).fileName(), ti_.sheetNames.value(ti_.sheetIndex)).arg(ti_.headerRow).arg(ti_.headers.size()));
        log_->setPlainText(ti_.warnings.join(QLatin1Char('\n')));
        rebuildMap();
    }
    void pickTemplate()
    {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Tệp mẫu"), QString(), QStringLiteral("Excel (*.xlsx)"));
        if (f.isEmpty()) return;
        tplPath_.clear(); ti_ = {}; rebuildMap();
        info_->setText(QStringLiteral("Chưa nạp mẫu mới."));
        const int sheet = chooseSheet(f);
        if (sheet < 0) return;
        tplPath_ = f; templateSheet_ = sheet;
        { QSignalBlocker block(headerRow_); headerRow_->setValue(0); }
        reloadTemplate();
    }
    void pickSource()
    {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Dữ liệu nguồn"), QString(), QStringLiteral("Bảng (*.csv *.xlsx)"));
        if (f.isEmpty()) return;
        // Never leave the old data active after an invalid replacement or cancelled sheet choice.
        srcHeader_.clear(); srcRows_.clear(); rebuildMap();
        sourceInfo_->setText(QStringLiteral("Chưa nạp dữ liệu nguồn mới."));
        const bool xlsx = f.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive);
        const int sheet = xlsx ? chooseSheet(f) : 0;
        if (sheet < 0) return;
        mailmerge::Table t = xlsx ? mailmerge::readXlsxFile(f, sheet, true) : mailmerge::readCsvFile(f, true);
        if (!t.ok()) { log_->setPlainText(t.error); return; }
        srcHeader_ = t.header; srcRows_ = t.rows;
        sourceInfo_->setText(QStringLiteral("Nguồn: %1%2 — %3 dòng, %4 cột (dòng đầu là tiêu đề).")
            .arg(QFileInfo(f).fileName(), xlsx ? QStringLiteral(" · sheet %1").arg(sheet+1) : QString())
            .arg(srcRows_.size()).arg(srcHeader_.size()));
        log_->setPlainText(t.warnings.join(QLatin1Char('\n')));
        rebuildMap();
    }
    void rebuildMap()
    {
        map_->setRowCount(0);
        if (!ti_.ok) return;
        const QVector<int> m = srcHeader_.isEmpty() ? QVector<int>(ti_.headers.size(), -1) : autoMap(ti_.headers, srcHeader_);
        map_->setRowCount(ti_.headers.size());
        for (int i = 0; i < ti_.headers.size(); ++i) {
            map_->setItem(i, 0, new QTableWidgetItem(ti_.headers[i]));
            map_->item(i, 0)->setFlags(Qt::ItemIsEnabled);
            auto *cb = new QComboBox; cb->addItem(QStringLiteral("(để trống)"), -1);
            for (int j = 0; j < srcHeader_.size(); ++j) cb->addItem(srcHeader_[j], j);
            cb->setCurrentIndex(m[i] + 1);
            map_->setCellWidget(i, 1, cb);
            auto *ty = new QComboBox; ty->addItems({QStringLiteral("Chữ"), QStringLiteral("Số")});
            map_->setCellWidget(i, 2, ty);
        }
    }
    ExportOptions options() const
    {
        ExportOptions o;
        o.vietnameseNumbers = locale_->currentIndex()==0;
        for (int i = 0; i < map_->rowCount(); ++i) {
            o.mapping << static_cast<QComboBox*>(map_->cellWidget(i, 1))->currentData().toInt();
            o.colTypes << (static_cast<QComboBox*>(map_->cellWidget(i, 2))->currentIndex() == 1 ? ColType::Number : ColType::Text);
        }
        return o;
    }
    void check()
    {
        if (!ti_.ok || srcHeader_.isEmpty()) { log_->setPlainText(QStringLiteral("Cần chọn cả tệp mẫu và dữ liệu nguồn.")); return; }
        const auto v = validate(ti_, srcHeader_, srcRows_, options());
        QStringList o; for (const QString &e : v.errors) o << QStringLiteral("LỖI: ") + e; for (const QString &w : v.warnings) o << QStringLiteral("Lưu ý: ") + w;
        if (o.isEmpty()) o << QStringLiteral("Không có vấn đề phát hiện được (chưa phải bảo đảm phần mềm đích chấp nhận).");
        log_->setPlainText(o.join(QLatin1Char('\n')));
    }
    void saveMapping()
    {
        const auto data=encodeMappingProfile(target_->currentText(),ti_,srcHeader_,options());
        if(data.isEmpty()){log_->setPlainText(QStringLiteral("Cần nạp mẫu, nguồn và ghép cột hợp lệ trước khi lưu."));return;}
        QString dest=QFileDialog::getSaveFileName(this,QStringLiteral("Lưu quy tắc ghép cột"),QStringLiteral("ghep-cot.json"),QStringLiteral("JSON (*.json)"));
        if(dest.isEmpty())return;
        if(!dest.endsWith(QLatin1String(".json"),Qt::CaseInsensitive))dest+=QStringLiteral(".json");
        QFile file(dest);
        if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)){
            log_->setPlainText(QStringLiteral("Không tạo được tệp cấu hình mới. Không ghi đè tệp có sẵn."));return;
        }
        if(file.write(data)!=data.size()||!file.flush()){
            file.close();file.remove();log_->setPlainText(QStringLiteral("Không lưu được cấu hình."));return;
        }
        file.close();
        log_->setPlainText(QStringLiteral("Đã lưu cấu hình ghép cột, không chứa dữ liệu chứng từ: ")+dest);
    }
    void loadMapping()
    {
        if(!ti_.ok||srcHeader_.isEmpty()){check();return;}
        const QString src=QFileDialog::getOpenFileName(this,QStringLiteral("Nạp quy tắc ghép cột"),QString(),QStringLiteral("JSON (*.json)"));
        if(src.isEmpty())return;
        QFile file(src);
        if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024){log_->setPlainText(QStringLiteral("Không đọc được cấu hình hoặc tệp quá lớn."));return;}
        ExportOptions restored;QString error;
        if(!decodeMappingProfile(file.read(2*1024*1024+1),target_->currentText(),ti_,srcHeader_,&restored,&error)){
            log_->setPlainText(error);return;
        }
        for(int i=0;i<map_->rowCount();++i){
            static_cast<QComboBox*>(map_->cellWidget(i,1))->setCurrentIndex(restored.mapping[i]+1);
            static_cast<QComboBox*>(map_->cellWidget(i,2))->setCurrentIndex(restored.colTypes[i]==ColType::Number?1:0);
        }
        locale_->setCurrentIndex(restored.vietnameseNumbers?0:1);
        check();
    }
    void doExport()
    {
        if (!ti_.ok || srcHeader_.isEmpty()) { check(); return; }
        const auto opts = options();
        const auto verified = validate(ti_, srcHeader_, srcRows_, opts);
        if (!verified.errors.isEmpty()) { check(); return; }
        const auto current = loadTemplate(tplPath_, ti_.sheetIndex, ti_.headerRow);
        if (!current.ok || current.headers != ti_.headers || current.sheetNames != ti_.sheetNames ||
            current.existingDataRows != ti_.existingDataRows) {
            log_->setPlainText(QStringLiteral("Mẫu đã thay đổi kể từ khi ghép cột. Hãy chọn lại mẫu và kiểm tra dữ liệu."));
            ti_ = {}; rebuildMap(); return;
        }
        QStringList warnings = current.warnings + verified.warnings;
        if (!warnings.isEmpty()) {
            log_->setPlainText(warnings.join(QLatin1Char('\n')));
            if (QMessageBox::question(this, QStringLiteral("Xác nhận xuất dữ liệu"),
                QStringLiteral("Có %1 cảnh báo (xem nhật ký phía dưới). Bạn đã kiểm tra và muốn xuất bản mới?").arg(warnings.size()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        }
        const QString def = QFileInfo(tplPath_).absolutePath() + QStringLiteral("/") + QFileInfo(tplPath_).completeBaseName() + QStringLiteral("-xuat.xlsx");
        const QString out = QFileDialog::getSaveFileName(this, QStringLiteral("Lưu bản xuất mới"), def, QStringLiteral("Excel (*.xlsx)"), nullptr, QFileDialog::DontConfirmOverwrite);
        if (out.isEmpty()) return;
        const auto r = exportToTemplate(tplPath_, out, ti_, srcHeader_, srcRows_, opts);
        QStringList o;
        if (!r.ok) o << QStringLiteral("KHÔNG xuất được: ") + r.error;
        else o << QStringLiteral("Đã ghi %1 dòng vào tệp mới: %2").arg(r.rowsWritten).arg(r.outPath);
        for (const QString &w : r.warnings) o << QStringLiteral("Lưu ý: ") + w;
        log_->setPlainText(o.join(QLatin1Char('\n')));
    }
    QComboBox *target_{};
    QComboBox *locale_{};
    QLabel *info_{};
    QLabel *sourceInfo_{};
    QSpinBox *headerRow_{};
    int templateSheet_ = 0;
    QTableWidget *map_{};
    QPlainTextEdit *log_{};
    QString tplPath_;
    TemplateInfo ti_;
    QStringList srcHeader_;
    QVector<QStringList> srcRows_;
};

// ============================================================ Kho luật
class LawLinks : public QWidget {
public:
    explicit LawLinks(QWidget *p) : QWidget(p) {
        auto* lay = new QVBoxLayout(this);
        auto* notice = new QLabel(QStringLiteral("<b>Kho luật và nguồn chính thức</b><br>Danh mục liên kết, không phải toàn văn. Chưa xác minh hiệu lực từng điều khoản; kiểm tra văn bản sửa đổi, lịch sử và phạm vi áp dụng tại nguồn."));
        notice->setWordWrap(true); lay->addWidget(notice);
        auto* search = new QLineEdit; search->setPlaceholderText(QStringLiteral("Tìm số hiệu, tên luật, chủ đề…")); lay->addWidget(search);
        auto* group = new QComboBox; group->addItem(QStringLiteral("Tất cả lĩnh vực")); lay->addWidget(group);
        auto* tree = new QTreeWidget; tree->setColumnCount(3); tree->setHeaderLabels({QStringLiteral("Lĩnh vực"), QStringLiteral("Văn bản / nguồn"), QStringLiteral("Hướng dẫn")});
        tree->header()->setSectionResizeMode(1, QHeaderView::Stretch); tree->setRootIsDecorated(false); lay->addWidget(tree, 1);
        auto* status = new QLabel; status->setWordWrap(true); lay->addWidget(status);
        auto* open = new QPushButton(QStringLiteral("Mở nguồn chính thức")); lay->addWidget(open);
        QList<tpl::Source> items = tpl::sources(tpl::installed());
        if (items.isEmpty()) {
            items << tpl::Source{QStringLiteral("Tra cứu chung"), QStringLiteral("Cơ sở dữ liệu quốc gia về văn bản pháp luật"), QStringLiteral("https://vbpl.vn/"), QStringLiteral("Tra cứu hiệu lực")}
                  << tpl::Source{QStringLiteral("Tra cứu chung"), QStringLiteral("Cổng văn bản Chính phủ"), QStringLiteral("https://vanban.chinhphu.vn/"), QStringLiteral("Tải văn bản gốc")};
        }
        for (const auto& item : items) if (group->findText(item.group) < 0) group->addItem(item.group);
        auto fill = [=] {
            tree->clear();
            const QString q = foldVietnamese(search->text().trimmed());
            for (const auto& item : items) {
                if (group->currentIndex() > 0 && item.group != group->currentText()) continue;
                if (!foldVietnamese(item.group + QLatin1Char(' ') + item.document + QLatin1Char(' ') + item.guide).contains(q)) continue;
                auto* row = new QTreeWidgetItem(tree, {item.group, item.document, item.guide});
                row->setData(0, Qt::UserRole, item.url); row->setToolTip(1, item.url);
            }
            status->setText(QStringLiteral("%1 nguồn phù hợp. Kiểm tra hiệu lực trước khi áp dụng.").arg(tree->topLevelItemCount()));
        };
        auto launch = [=] {
            const auto* row = tree->currentItem(); if (!row) return;
            const QUrl url(row->data(0, Qt::UserRole).toString());
            if (url.scheme() != QStringLiteral("https")) { status->setText(QStringLiteral("Chỉ mở liên kết HTTPS.")); return; }
            if (!QDesktopServices::openUrl(url)) status->setText(QStringLiteral("Không mở được trình duyệt. ") + url.toString());
        };
        connect(search, &QLineEdit::textChanged, this, fill);
        connect(group, &QComboBox::currentIndexChanged, this, fill);
        connect(open, &QPushButton::clicked, this, launch);
        connect(tree, &QTreeWidget::itemDoubleClicked, this, launch);
        fill();
    }
};
}  // namespace

QWidget* createFormulaAssistantWidget(QWidget *p) { return new FormulaAssistant(p); }
void notifyFormulaSelection(QWidget* w, const QString& ref) { if (auto* p = dynamic_cast<FormulaAssistant*>(w)) p->selectedReference(ref); }
void notifyFormulaSheet(QWidget* w,int part,const QString& name) { if(auto* p=dynamic_cast<FormulaAssistant*>(w))p->sheetChanged(part,name); }
QWidget* createRangeToolsWidget(QWidget *p) { return new RangeTools(p); }
QWidget* createDocumentRangeToolsWidget(QWidget* p, std::function<QString()> readSelection, std::function<bool()> inputIdle) { return new RangeTools(p, std::move(readSelection), std::move(inputIdle)); }
void notifyDocumentRangeTools(QWidget* widget) { if (auto* r = dynamic_cast<RangeTools*>(widget)) r->scheduleRefresh(); }
QWidget* createTemplateExportWidget(QWidget *p) { return new TemplateExportWidget(p); }
QWidget* createLawLinksWidget(QWidget *p) { return new LawLinks(p); }
