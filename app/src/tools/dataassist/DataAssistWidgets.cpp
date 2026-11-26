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
#include <QFormLayout>
#include <QHBoxLayout>
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
        auto* intro = new QLabel(QStringLiteral("<b>ƒx · Tôi muốn…</b><br>Gõ việc cần làm (vd. “cộng tổng”, “tìm theo mã”) hoặc chọn nhóm hàm. Engine bảng tính tính kết quả."));
        intro->setWordWrap(true);
        lay->addWidget(intro);
        auto *top = new QVBoxLayout;
        search_ = new QLineEdit; search_->setPlaceholderText(QStringLiteral("Tôi muốn…"));
        cat_ = new QComboBox; cat_->addItem(QStringLiteral("Tất cả nhóm"), -1);
        for (Category c : allCategories()) cat_->addItem(categoryName(c), int(c));
        top->addWidget(search_); top->addWidget(cat_);
        lay->addLayout(top);
        list_ = new QListWidget; lay->addWidget(list_, 1);
        desc_ = new QLabel; desc_->setWordWrap(true); lay->addWidget(desc_);
        pairs_ = new QSpinBox; pairs_->setRange(1, 8); pairs_->setValue(2);
        pairs_->setPrefix(QStringLiteral("Số cặp điều kiện: ")); pairs_->hide(); lay->addWidget(pairs_);
        autoPick_ = new QCheckBox(QStringLiteral("Tự lấy vùng khi chọn trên bảng tính"));
        autoPick_->setChecked(true); lay->addWidget(autoPick_);
        form_ = new QFormLayout; lay->addLayout(form_);
        connect(pairs_, &QSpinBox::valueChanged, this, [this](int) {
            if (!cur_ || !hasConditionPairs(*cur_)) return;
            QStringList saved;
            for (auto* e : edits_) saved << e->text();
            while (form_->rowCount()) form_->removeRow(0);
            edits_.clear();
            populateArgs(saved);
        });
        auto* targetRow = new QHBoxLayout;
        target_ = new QLineEdit; target_->setPlaceholderText(QStringLiteral("VD: C6"));
        auto* useTarget = new QPushButton(QStringLiteral("Lấy ô"));
        targetRow->addWidget(target_, 1); targetRow->addWidget(useTarget);
        lay->addWidget(new QLabel(QStringLiteral("Ô kết quả (sheet hiện tại)")));
        lay->addLayout(targetRow);
        connect(useTarget, &QPushButton::clicked, this, [this] {
            const QString addr = hooks().selectedCellAddress ? hooks().selectedCellAddress() : QString();
            if (addr.isEmpty()) { status_->setText(QStringLiteral("Chưa nhận được địa chỉ ô từ engine.")); return; }
            target_->setText(addr);
        });
        result_ = new QLineEdit; result_->setPlaceholderText(QStringLiteral("Công thức sẽ hiện ở đây — sửa tự do được"));
        lay->addWidget(result_);
        auto* quick = new QFormLayout;
        auto* qa = new QLineEdit; qa->setPlaceholderText(QStringLiteral("A1 hoặc số"));
        auto* qb = new QLineEdit; qb->setPlaceholderText(QStringLiteral("B1 hoặc số"));
        auto* op = new QComboBox;
        for (const auto& s : {QStringLiteral("+"), QStringLiteral("-"), QStringLiteral("*"), QStringLiteral("/"), QStringLiteral("^"), QStringLiteral("Tỷ lệ %")}) op->addItem(s);
        auto* make = new QPushButton(QStringLiteral("Tạo phép tính nhanh"));
        quick->addRow(QStringLiteral("Giá trị A"), qa); quick->addRow(QStringLiteral("Phép tính"), op); quick->addRow(QStringLiteral("Giá trị B"), qb);
        quick->addRow(make); lay->addLayout(quick);
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
        lay->addLayout(btns);
        status_ = new QLabel; status_->setWordWrap(true); lay->addWidget(status_);
        connect(search_, &QLineEdit::textChanged, this, [this] { refill(); });
        connect(cat_, &QComboBox::currentIndexChanged, this, [this] { refill(); });
        connect(list_, &QListWidget::currentRowChanged, this, [this](int r) { select(r); });
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
        insert_->setEnabled(true);
        refill();
    }
    void selectedReference(const QString& reference) {
        if (!isVisible() || !autoPick_->isChecked() || !cur_) return;
        if (reference.isEmpty()) {
            status_->setText(QStringLiteral("Chưa xác minh được địa chỉ vùng; nhập địa chỉ thủ công nếu vùng nằm ngoài màn hình."));
            return;
        }
        QLineEdit* e = nullptr;
        for (auto* field : edits_) if (field->property("pickNext").toBool()) { e = field; break; }
        if (!e) {
            for (auto* field : edits_) if (field->text().trimmed().isEmpty() && field->property("referenceArg").toBool()) { e = field; break; }
        }
        if (!e) return;
        e->setProperty("pickNext", false);
        e->setText(reference);
        for (auto* next : edits_) if (next->text().trimmed().isEmpty()) {
            status_->setText(QStringLiteral("Đã lấy %1. Đối số tiếp theo: %2").arg(reference, next->placeholderText()));
            break;
        }
    }
private:
    void refill()
    {
        shown_.clear(); list_->clear();
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
        if (!shown_.isEmpty()) list_->setCurrentRow(0); else { desc_->setText(q.isEmpty() ? QString() : QStringLiteral("Không có hàm khớp “%1”.").arg(q)); clearForm(); }
    }
    void clearForm() { cur_ = nullptr; pairs_->hide(); while (form_->rowCount()) form_->removeRow(0); edits_.clear(); result_->clear(); }
    void select(int r)
    {
        clearForm();
        if (r < 0 || r >= shown_.size()) return;
        cur_ = shown_[r];
        pairs_->setVisible(hasConditionPairs(*cur_));
        desc_->setText(QStringLiteral("<b>%1</b> (%2): %3<br>Ví dụ: <code>%4</code>%5").arg(cur_->name, categoryName(cur_->cat), cur_->description, cur_->example.toHtmlEscaped(),
            cur_->newerEngineOnly ? QStringLiteral("<br><i>Hàm mảng động/mới: engine Windows 26.8.3.1 đã kiểm thử; engine Linux cũ trả #NAME?. Nếu ô báo #NAME? thì engine chưa hỗ trợ — không phải lỗi của bạn.</i>") : QString()));
        populateArgs({});
    }
    void populateArgs(const QStringList& saved) {
        const auto args = hasConditionPairs(*cur_) ? conditionArgs(*cur_, pairs_->value()) : cur_->args;
        for (const ArgInfo &a : args) {
            auto *e = new QLineEdit; e->setPlaceholderText(a.hint);
            static const QRegularExpression referenceHint(QStringLiteral("\\b[A-Z]{1,3}[1-9][0-9]*\\b"), QRegularExpression::CaseInsensitiveOption);
            e->setProperty("referenceArg", referenceHint.match(a.hint).hasMatch());
            if (edits_.size() < saved.size()) e->setText(saved.at(edits_.size()));
            edits_.push_back(e);
            auto* field = new QWidget;
            auto* row = new QHBoxLayout(field); row->setContentsMargins(0, 0, 0, 0);
            row->addWidget(e, 1);
            auto* pick = new QPushButton(QStringLiteral("Chọn vùng"));
            pick->setToolTip(QStringLiteral("Bấm rồi chọn ô/vùng trên bảng; địa chỉ tự điền nếu xác minh được."));
            row->addWidget(pick);
            connect(pick, &QPushButton::clicked, this, [this, e] {
                for (auto* field : edits_) field->setProperty("pickNext", false);
                e->setProperty("pickNext", true);
                autoPick_->setChecked(true);
                status_->setText(QStringLiteral("Bây giờ chọn ô hoặc kéo vùng trên bảng tính."));
            });
            form_->addRow(a.name + (a.optional ? QStringLiteral(" (tuỳ chọn)") : QString()), field);
            connect(e, &QLineEdit::textChanged, this, [this] { rebuild(); });
        }
        rebuild();
    }
    void rebuild()
    {
        if (!cur_) return;
        QStringList v; for (auto *e : edits_) v << e->text();
        const BuildResult b = buildFormula(*cur_, v);
        if (b.ok) { result_->setText(b.formula); status_->clear(); }
        else { result_->clear(); status_->setText(b.error); }
    }
    QLineEdit *search_{}, *result_{}, *target_{};
    QComboBox *cat_{};
    QSpinBox *pairs_{};
    QCheckBox *autoPick_{};
    QListWidget *list_{};
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
    explicit RangeTools(QWidget *p, std::function<QString()> readSelection = {}) : QWidget(p), readSelection_(std::move(readSelection))
    {
        auto *lay = new QVBoxLayout(this);
        lay->addWidget(new QLabel(QStringLiteral("<b>Công cụ vùng dữ liệu</b><br>Dán vùng ô (từ Excel) hoặc mở CSV/XLSX. Chỉ đọc — dữ liệu gốc không bị sửa.")));
        input_ = new QPlainTextEdit; input_->setPlaceholderText(QStringLiteral("Dán vùng ô tại đây (các ô cách nhau bằng Tab)"));
        lay->addWidget(input_, 1);
        auto *row = new QHBoxLayout;
        auto *open = new QPushButton(QStringLiteral("Mở CSV/XLSX…"));
        auto *fromSel = new QPushButton(QStringLiteral("Lấy vùng đang chọn"));
        vi_ = new QCheckBox(QStringLiteral("Số kiểu Việt (1.234,5)")); vi_->setChecked(true);
        row->addWidget(open); row->addWidget(fromSel); row->addWidget(vi_); row->addStretch();
        lay->addLayout(row);
        if (readSelection_) {
            live_ = new QCheckBox(QStringLiteral("Tự cập nhật thống kê vùng chọn"));
            live_->setToolTip(QStringLiteral("Chỉ đọc vùng khi panel đang hiện. Tắt nếu chọn vùng rất lớn."));
            lay->addWidget(live_);
            timer_ = new QTimer(this); timer_->setSingleShot(true); timer_->setInterval(220);
            connect(timer_, &QTimer::timeout, this, [this] { if (isVisible() && live_->isChecked()) readAndSummarize(); });
            connect(live_, &QCheckBox::toggled, this, [this](bool enabled) { if (enabled) scheduleRefresh(); else timer_->stop(); });
        }
        auto *row2 = new QHBoxLayout;
        for (auto &pr : {std::pair<QString, int>{QStringLiteral("Thống kê"), 0}, {QStringLiteral("Kiểm tra lỗi công thức"), 1}, {QStringLiteral("Tìm dòng trùng"), 2}, {QStringLiteral("Biểu đồ nhanh (đếm theo cột 1)"), 3}}) {
            auto *b = new QPushButton(pr.first); const int id = pr.second;
            connect(b, &QPushButton::clicked, this, [this, id] { run(id); });
            row2->addWidget(b);
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
            const ChartData d = groupForChart(grid, 0, -1, true, vi_->isChecked());
            double mx = 0; for (const auto &p : d.points) mx = qMax(mx, p.value);
            for (const auto &p : d.points) o << QStringLiteral("%1 | %2 %3").arg(p.label.leftJustified(18)).arg(QString(int(24 * p.value / (mx > 0 ? mx : 1)), QChar(0x2588))).arg(p.value);
            o << d.warnings;
        }
        out_->setPlainText(o.join(QLatin1Char('\n')));
    }
    QPlainTextEdit *input_{}, *out_{};
    QCheckBox *vi_{}, *live_{};
    QTimer* timer_{};
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
        auto *r1 = new QHBoxLayout;
        auto *bt = new QPushButton(QStringLiteral("Chọn tệp mẫu…")), *bs = new QPushButton(QStringLiteral("Chọn dữ liệu nguồn (CSV/XLSX)…"));
        r1->addWidget(bt); r1->addWidget(bs); lay->addLayout(r1);
        info_ = new QLabel; info_->setWordWrap(true); lay->addWidget(info_);
        map_ = new QTableWidget(0, 3); map_->setHorizontalHeaderLabels({QStringLiteral("Cột của mẫu"), QStringLiteral("Lấy từ cột nguồn"), QStringLiteral("Kiểu")});
        map_->horizontalHeader()->setStretchLastSection(true); lay->addWidget(map_, 1);
        auto *r2 = new QHBoxLayout;
        auto *chk = new QPushButton(QStringLiteral("Kiểm tra")), *exp = new QPushButton(QStringLiteral("Xuất bản mới…"));
        r2->addWidget(chk); r2->addWidget(exp); r2->addStretch(); lay->addLayout(r2);
        log_ = new QPlainTextEdit; log_->setReadOnly(true); lay->addWidget(log_, 1);
        connect(bt, &QPushButton::clicked, this, [this] { pickTemplate(); });
        connect(bs, &QPushButton::clicked, this, [this] { pickSource(); });
        connect(chk, &QPushButton::clicked, this, [this] { check(); });
        connect(exp, &QPushButton::clicked, this, [this] { doExport(); });
    }
private:
    void pickTemplate()
    {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Tệp mẫu"), QString(), QStringLiteral("Excel (*.xlsx)"));
        if (f.isEmpty()) return;
        tplPath_ = f; ti_ = loadTemplate(f);
        if (!ti_.ok) { info_->setText(ti_.error); return; }
        info_->setText(QStringLiteral("Mẫu: %1 — sheet “%2”, tiêu đề ở dòng %3, %4 cột.").arg(QFileInfo(f).fileName(), ti_.sheetNames[ti_.sheetIndex]).arg(ti_.headerRow).arg(ti_.headers.size()));
        log_->setPlainText(ti_.warnings.join(QLatin1Char('\n')));
        rebuildMap();
    }
    void pickSource()
    {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Dữ liệu nguồn"), QString(), QStringLiteral("Bảng (*.csv *.xlsx)"));
        if (f.isEmpty()) return;
        mailmerge::Table t = f.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive) ? mailmerge::readXlsxFile(f, 0, true) : mailmerge::readCsvFile(f, true);
        if (!t.ok()) { log_->setPlainText(t.error); return; }
        srcHeader_ = t.header; srcRows_ = t.rows;
        log_->setPlainText(QStringLiteral("Nguồn: %1 dòng, %2 cột.\n%3").arg(srcRows_.size()).arg(srcHeader_.size()).arg(t.warnings.join(QLatin1Char('\n'))));
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
    void doExport()
    {
        if (!ti_.ok || srcHeader_.isEmpty()) { check(); return; }
        const QString def = QFileInfo(tplPath_).absolutePath() + QStringLiteral("/") + QFileInfo(tplPath_).completeBaseName() + QStringLiteral("-xuat.xlsx");
        const QString out = QFileDialog::getSaveFileName(this, QStringLiteral("Lưu bản xuất mới"), def, QStringLiteral("Excel (*.xlsx)"), nullptr, QFileDialog::DontConfirmOverwrite);
        if (out.isEmpty()) return;
        const auto r = exportToTemplate(tplPath_, out, ti_, srcHeader_, srcRows_, options());
        QStringList o;
        if (!r.ok) o << QStringLiteral("KHÔNG xuất được: ") + r.error;
        else o << QStringLiteral("Đã ghi %1 dòng vào tệp mới: %2").arg(r.rowsWritten).arg(r.outPath);
        for (const QString &w : r.warnings) o << QStringLiteral("Lưu ý: ") + w;
        log_->setPlainText(o.join(QLatin1Char('\n')));
    }
    QComboBox *target_{};
    QLabel *info_{};
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
QWidget* createRangeToolsWidget(QWidget *p) { return new RangeTools(p); }
QWidget* createDocumentRangeToolsWidget(QWidget* p, std::function<QString()> readSelection) { return new RangeTools(p, std::move(readSelection)); }
void notifyDocumentRangeTools(QWidget* widget) { if (auto* r = dynamic_cast<RangeTools*>(widget)) r->scheduleRefresh(); }
QWidget* createTemplateExportWidget(QWidget *p) { return new TemplateExportWidget(p); }
QWidget* createLawLinksWidget(QWidget *p) { return new LawLinks(p); }
