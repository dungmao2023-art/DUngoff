// SPDX-License-Identifier: MPL-2.0
#include "FileToolsWidgets.h"
#include "FileOps.h"
#include "LinkScan.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHash>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QSet>
#include <algorithm>
#include <climits>
#include <functional>
#include <memory>

namespace {

QString journalDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/filetools");
}

QPushButton* btn(const QString& t, QWidget* p = nullptr) { return new QPushButton(t, p); }

// Hộp tiến trình dùng chung cho các việc quét lâu; trả về callback Progress (false = người dùng huỷ).
struct ProgressUi {
    QProgressDialog dlg;
    explicit ProgressUi(QWidget* parent, const QString& label) : dlg(label, QObject::tr("Huỷ"), 0, 0, parent)
    {
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setMinimumDuration(300);
    }
    ft::Progress cb()
    {
        return [this](qint64 done, qint64 total) {
            if (total > 0) { dlg.setMaximum(int(qMin<qint64>(total / 1024 + 1, INT_MAX))); dlg.setValue(int(qMin<qint64>(done / 1024, INT_MAX - 1))); }
            QApplication::processEvents();
            return !dlg.wasCanceled();
        };
    }
};

QTableWidgetItem* ro(const QString& t)
{
    auto* i = new QTableWidgetItem(t);
    i->setFlags(i->flags() & ~Qt::ItemIsEditable);
    return i;
}

QWidget* pathRow(QLineEdit*& edit, const QString& placeholder, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    edit = new QLineEdit(w);
    edit->setPlaceholderText(placeholder);
    auto* b = btn(QObject::tr("Chọn…"), w);
    QLineEdit* e = edit;
    QObject::connect(b, &QPushButton::clicked, w, [e, w]() {
        const QString d = QFileDialog::getExistingDirectory(w, QObject::tr("Chọn thư mục"), e->text());
        if (!d.isEmpty()) e->setText(d);
    });
    l->addWidget(edit, 1);
    l->addWidget(b);
    return w;
}

} // namespace

// =====================================================================================
// 1) Đổi tên hàng loạt
// =====================================================================================
QWidget* createBatchRenameWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    auto* hint = new QLabel(QObject::tr("Thêm tệp/thư mục, đặt mẫu tên, xem trước. Không có gì bị đổi cho tới khi bạn bấm “Đổi tên”. Có thể hoàn tác."), w);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* files = new QListWidget(w);
    files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto* addRow = new QHBoxLayout;
    auto* bAddFiles = btn(QObject::tr("Thêm tệp…"), w);
    auto* bAddDir = btn(QObject::tr("Thêm nội dung thư mục…"), w);
    auto* bRemove = btn(QObject::tr("Bỏ khỏi danh sách"), w);
    addRow->addWidget(bAddFiles); addRow->addWidget(bAddDir); addRow->addWidget(bRemove); addRow->addStretch(1);

    auto* box = new QGroupBox(QObject::tr("Mẫu tên"), w);
    auto* form = new QFormLayout(box);
    auto* pattern = new QLineEdit(QStringLiteral("{name}"), box);
    pattern->setToolTip(QObject::tr("{name} tên cũ · {n} số thứ tự · {date} ngày sửa · {today} hôm nay · {parent} thư mục cha · {ext} đuôi"));
    auto* start = new QSpinBox(box); start->setRange(-999999, 999999); start->setValue(1);
    auto* step = new QSpinBox(box); step->setRange(-9999, 9999); step->setValue(1);
    auto* pad = new QSpinBox(box); pad->setRange(0, 12); pad->setValue(3);
    auto* numRow = new QWidget(box); auto* nl = new QHBoxLayout(numRow); nl->setContentsMargins(0, 0, 0, 0);
    nl->addWidget(new QLabel(QObject::tr("bắt đầu"))); nl->addWidget(start);
    nl->addWidget(new QLabel(QObject::tr("bước"))); nl->addWidget(step);
    nl->addWidget(new QLabel(QObject::tr("số chữ số"))); nl->addWidget(pad); nl->addStretch(1);
    auto* find = new QLineEdit(box); auto* repl = new QLineEdit(box);
    auto* cs = new QCheckBox(QObject::tr("phân biệt hoa/thường khi thay"), box);
    auto* nod = new QCheckBox(QObject::tr("bỏ dấu tiếng Việt (đ → d)"), box);
    auto* sp = new QCheckBox(QObject::tr("chuẩn hoá khoảng trắng"), box);
    auto* kext = new QCheckBox(QObject::tr("giữ nguyên đuôi tệp"), box); kext->setChecked(true);
    auto* cm = new QComboBox(box);
    cm->addItems({QObject::tr("giữ nguyên chữ hoa/thường"), QObject::tr("chữ thường"), QObject::tr("CHỮ HOA"), QObject::tr("Viết Hoa Đầu Từ")});
    form->addRow(QObject::tr("Mẫu:"), pattern);
    form->addRow(QObject::tr("Số thứ tự:"), numRow);
    form->addRow(QObject::tr("Tìm:"), find);
    form->addRow(QObject::tr("Thay bằng:"), repl);
    form->addRow(QString(), cs); form->addRow(QString(), nod); form->addRow(QString(), sp); form->addRow(QString(), kext);
    form->addRow(QObject::tr("Kiểu chữ:"), cm);

    auto* table = new QTableWidget(0, 3, w);
    table->setHorizontalHeaderLabels({QObject::tr("Tên cũ"), QObject::tr("Tên mới"), QObject::tr("Trạng thái")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* summary = new QLabel(w);
    auto* actRow = new QHBoxLayout;
    auto* bApply = btn(QObject::tr("Đổi tên"), w);
    auto* bUndo = btn(QObject::tr("Hoàn tác lần đổi gần nhất"), w);
    bApply->setEnabled(false); bUndo->setEnabled(false);
    actRow->addWidget(bApply); actRow->addWidget(bUndo); actRow->addStretch(1);

    root->addLayout(addRow); root->addWidget(files, 1); root->addWidget(box); root->addWidget(table, 2); root->addWidget(summary); root->addLayout(actRow);

    auto lastJournal = std::make_shared<QString>();
    auto plan = std::make_shared<ft::RenamePlan>();
    auto paths = [files]() { QStringList l; for (int i = 0; i < files->count(); ++i) l << files->item(i)->data(Qt::UserRole).toString(); return l; };
    auto refresh = [=]() {
        ft::RenameOptions o;
        o.pattern = pattern->text(); o.start = start->value(); o.step = step->value(); o.pad = pad->value();
        o.find = find->text(); o.replace = repl->text(); o.caseSensitive = cs->isChecked();
        o.removeDiacritics = nod->isChecked(); o.normalizeSpaces = sp->isChecked(); o.keepExtension = kext->isChecked();
        o.caseMode = static_cast<ft::CaseMode>(cm->currentIndex());
        *plan = ft::planRename(paths(), o);
        table->setRowCount(plan->items.size());
        int bad = 0;
        for (int i = 0; i < plan->items.size(); ++i) {
            const auto& it = plan->items[i];
            QString st;
            switch (it.status) {
            case ft::RenameStatus::Ok: st = QObject::tr("sẽ đổi"); break;
            case ft::RenameStatus::Unchanged: st = QObject::tr("không đổi"); break;
            case ft::RenameStatus::Conflict: st = QObject::tr("XUNG ĐỘT: %1").arg(it.note); ++bad; break;
            case ft::RenameStatus::Invalid: st = QObject::tr("KHÔNG HỢP LỆ: %1").arg(it.note); ++bad; break;
            }
            table->setItem(i, 0, ro(it.oldName)); table->setItem(i, 1, ro(it.newName)); table->setItem(i, 2, ro(st));
            if (bad && (it.status == ft::RenameStatus::Conflict || it.status == ft::RenameStatus::Invalid))
                for (int c = 0; c < 3; ++c) table->item(i, c)->setBackground(QColor(255, 225, 225));
        }
        summary->setText(plan->items.isEmpty() ? QObject::tr("Chưa có tệp nào.")
            : QObject::tr("%1 mục sẽ đổi, %2 có vấn đề.").arg(plan->changeCount()).arg(bad));
        bApply->setEnabled(plan->changeCount() > 0 && !plan->hasProblems());
    };
    auto addPaths = [=](const QStringList& ps) {
        QSet<QString> have; for (const auto& p : paths()) have.insert(p);
        for (const QString& p : ps) if (!have.contains(p)) { auto* i = new QListWidgetItem(QFileInfo(p).fileName() + QStringLiteral("   —   ") + QFileInfo(p).absolutePath(), files); i->setData(Qt::UserRole, p); }
        refresh();
    };
    QObject::connect(bAddFiles, &QPushButton::clicked, w, [=]() {
        const QStringList f = QFileDialog::getOpenFileNames(w, QObject::tr("Chọn tệp"));
        QStringList abs; for (const auto& x : f) abs << QFileInfo(x).absoluteFilePath();
        std::sort(abs.begin(), abs.end(), [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });
        addPaths(abs);
    });
    QObject::connect(bAddDir, &QPushButton::clicked, w, [=]() {
        const QString d = QFileDialog::getExistingDirectory(w, QObject::tr("Chọn thư mục"));
        if (d.isEmpty()) return;
        QStringList l;
        for (const QFileInfo& fi : QDir(d).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name | QDir::LocaleAware)) l << fi.absoluteFilePath();
        addPaths(l);
    });
    QObject::connect(bRemove, &QPushButton::clicked, w, [=]() { qDeleteAll(files->selectedItems()); refresh(); });
    for (QLineEdit* e : {pattern, find, repl}) QObject::connect(e, &QLineEdit::textChanged, w, refresh);
    for (QSpinBox* s : {start, step, pad}) QObject::connect(s, qOverload<int>(&QSpinBox::valueChanged), w, refresh);
    for (QCheckBox* c : {cs, nod, sp, kext}) QObject::connect(c, &QCheckBox::toggled, w, refresh);
    QObject::connect(cm, qOverload<int>(&QComboBox::currentIndexChanged), w, refresh);
    QObject::connect(bApply, &QPushButton::clicked, w, [=]() {
        if (QMessageBox::question(w, QObject::tr("Đổi tên"), QObject::tr("Đổi tên %1 mục? Bạn có thể hoàn tác sau đó.").arg(plan->changeCount())) != QMessageBox::Yes) return;
        const auto r = ft::applyRename(*plan, journalDir());
        if (!r.ok) { QMessageBox::warning(w, QObject::tr("Đổi tên"), r.error + QLatin1Char('\n') + r.warnings.join(QLatin1Char('\n'))); refresh(); return; }
        *lastJournal = r.journalPath; bUndo->setEnabled(!r.journalPath.isEmpty());
        for (int i = 0; i < files->count() && i < plan->items.size(); ++i)
            if (plan->items[i].status == ft::RenameStatus::Ok) files->item(i)->setData(Qt::UserRole, QDir(plan->items[i].dir).filePath(plan->items[i].newName));
        for (int i = 0; i < files->count(); ++i) { const QString p = files->item(i)->data(Qt::UserRole).toString(); files->item(i)->setText(QFileInfo(p).fileName() + QStringLiteral("   —   ") + QFileInfo(p).absolutePath()); }
        pattern->setText(QStringLiteral("{name}")); // tránh áp lại mẫu số thứ tự lên tên đã đổi
        refresh();
        QMessageBox::information(w, QObject::tr("Đổi tên"), QObject::tr("Đã đổi %1 mục.").arg(r.done));
    });
    QObject::connect(bUndo, &QPushButton::clicked, w, [=]() {
        const auto r = ft::undoJournal(*lastJournal);
        QMessageBox::information(w, QObject::tr("Hoàn tác"), r.ok ? QObject::tr("Đã hoàn tác %1 mục.").arg(r.done) : r.error + QLatin1Char('\n') + r.warnings.join(QLatin1Char('\n')));
        // đồng bộ lại danh sách theo nhật ký: người dùng thêm lại tệp nếu cần
        files->clear(); refresh(); bUndo->setEnabled(false);
    });
    refresh();
    return w;
}

// =====================================================================================
// 2) Tìm tệp trùng
// =====================================================================================
QWidget* createDuplicateFinderWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    auto* hint = new QLabel(QObject::tr("Quét theo kích thước rồi so nội dung (SHA-256). Công cụ KHÔNG xoá tệp: tệp chọn chỉ được chuyển vào thư mục “_to_delete” và có thể hoàn tác. Luôn giữ lại ít nhất một bản mỗi nhóm."), w);
    hint->setWordWrap(true);
    root->addWidget(hint);
    auto* roots = new QListWidget(w);
    roots->setMaximumHeight(90);
    auto* rr = new QHBoxLayout;
    auto* bAdd = btn(QObject::tr("Thêm thư mục…"), w); auto* bRm = btn(QObject::tr("Bỏ"), w);
    auto* minKb = new QSpinBox(w); minKb->setRange(0, 100000000); minKb->setValue(1); minKb->setSuffix(QStringLiteral(" KB"));
    auto* bScan = btn(QObject::tr("Quét"), w);
    rr->addWidget(bAdd); rr->addWidget(bRm); rr->addWidget(new QLabel(QObject::tr("Bỏ qua tệp nhỏ hơn:"))); rr->addWidget(minKb); rr->addStretch(1); rr->addWidget(bScan);
    auto* tree = new QTreeWidget(w);
    tree->setHeaderLabels({QObject::tr("Tệp"), QObject::tr("Kích thước"), QObject::tr("Sửa lần cuối")});
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    auto* summary = new QLabel(w);
    auto* act = new QHBoxLayout;
    auto* bAuto = btn(QObject::tr("Chọn mọi bản trừ bản đầu của mỗi nhóm"), w);
    auto* bClear = btn(QObject::tr("Bỏ chọn hết"), w);
    auto* bMove = btn(QObject::tr("Chuyển tệp đã chọn vào _to_delete"), w);
    auto* bUndo = btn(QObject::tr("Hoàn tác lần chuyển gần nhất"), w);
    bMove->setEnabled(false); bUndo->setEnabled(false);
    act->addWidget(bAuto); act->addWidget(bClear); act->addStretch(1); act->addWidget(bMove); act->addWidget(bUndo);
    root->addWidget(roots); root->addLayout(rr); root->addWidget(tree, 1); root->addWidget(summary); root->addLayout(act);

    auto result = std::make_shared<ft::DupResult>();
    auto lastJournal = std::make_shared<QString>();
    QObject::connect(bAdd, &QPushButton::clicked, w, [=]() { const QString d = QFileDialog::getExistingDirectory(w, QObject::tr("Chọn thư mục")); if (!d.isEmpty()) roots->addItem(d); });
    QObject::connect(bRm, &QPushButton::clicked, w, [=]() { qDeleteAll(roots->selectedItems()); });
    QObject::connect(bScan, &QPushButton::clicked, w, [=]() {
        QStringList rs; for (int i = 0; i < roots->count(); ++i) rs << roots->item(i)->text();
        if (rs.isEmpty()) { QMessageBox::information(w, QObject::tr("Tìm tệp trùng"), QObject::tr("Hãy thêm ít nhất một thư mục.")); return; }
        ProgressUi pu(w, QObject::tr("Đang quét và so nội dung…"));
        ft::DupOptions o; o.minSize = qMax<qint64>(1, qint64(minKb->value()) * 1024);
        *result = ft::findDuplicates(rs, o, pu.cb());
        pu.dlg.close();
        tree->clear();
        if (result->cancelled) { summary->setText(QObject::tr("Đã huỷ.")); return; }
        int gi = 0;
        for (const auto& g : result->groups) {
            auto* top = new QTreeWidgetItem(tree, {QObject::tr("Nhóm %1 — %2 tệp giống nhau").arg(++gi).arg(g.files.size()), ft::humanSize(g.size), QString()});
            top->setFlags(top->flags() & ~Qt::ItemIsUserCheckable);
            for (const QString& f : g.files) {
                auto* c = new QTreeWidgetItem(top, {f, ft::humanSize(g.size), QFileInfo(f).lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"))});
                c->setFlags(c->flags() | Qt::ItemIsUserCheckable); c->setCheckState(0, Qt::Unchecked); c->setData(0, Qt::UserRole, f);
            }
            top->setExpanded(true);
        }
        summary->setText(QObject::tr("%1 nhóm trùng, có thể giải phóng khoảng %2. %3 tệp không đọc được.%4")
            .arg(result->groups.size()).arg(ft::humanSize(result->wastedBytes)).arg(result->skipped.size())
            .arg(result->skipped.isEmpty() ? QString() : QObject::tr("\nVí dụ: ") + result->skipped.first()));
        bMove->setEnabled(!result->groups.isEmpty());
    });
    QObject::connect(bAuto, &QPushButton::clicked, w, [=]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) { auto* t = tree->topLevelItem(i); for (int j = 0; j < t->childCount(); ++j) t->child(j)->setCheckState(0, j == 0 ? Qt::Unchecked : Qt::Checked); }
    });
    QObject::connect(bClear, &QPushButton::clicked, w, [=]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) { auto* t = tree->topLevelItem(i); for (int j = 0; j < t->childCount(); ++j) t->child(j)->setCheckState(0, Qt::Unchecked); }
    });
    QObject::connect(bMove, &QPushButton::clicked, w, [=]() {
        QStringList sel;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) { auto* t = tree->topLevelItem(i); for (int j = 0; j < t->childCount(); ++j) if (t->child(j)->checkState(0) == Qt::Checked) sel << t->child(j)->data(0, Qt::UserRole).toString(); }
        if (sel.isEmpty()) { QMessageBox::information(w, QObject::tr("Tìm tệp trùng"), QObject::tr("Chưa chọn tệp nào.")); return; }
        const QString dest = QDir(QFileInfo(sel.first()).absolutePath()).filePath(QStringLiteral("_to_delete"));
        if (QMessageBox::question(w, QObject::tr("Chuyển tệp trùng"), QObject::tr("Chuyển %1 tệp vào “%2”? Tệp không bị xoá và có thể hoàn tác.").arg(sel.size()).arg(dest)) != QMessageBox::Yes) return;
        // thư mục đích nằm cạnh từng tệp đầu tiên; các tệp ở ổ khác sẽ báo lỗi rõ ràng và mọi thứ được khôi phục
        const auto r = ft::moveDuplicates(*result, sel, dest, journalDir());
        if (!r.ok) { QMessageBox::warning(w, QObject::tr("Chuyển tệp trùng"), r.error + QLatin1Char('\n') + r.warnings.join(QLatin1Char('\n'))); return; }
        *lastJournal = r.journalPath; bUndo->setEnabled(true);
        QMessageBox::information(w, QObject::tr("Chuyển tệp trùng"), QObject::tr("Đã chuyển %1 tệp vào “%2”. Hãy quét lại để cập nhật danh sách.").arg(r.done).arg(dest));
        tree->clear(); bMove->setEnabled(false);
    });
    QObject::connect(bUndo, &QPushButton::clicked, w, [=]() {
        const auto r = ft::undoJournal(*lastJournal);
        QMessageBox::information(w, QObject::tr("Hoàn tác"), r.ok ? QObject::tr("Đã trả %1 tệp về chỗ cũ.").arg(r.done) : r.error + QLatin1Char('\n') + r.warnings.join(QLatin1Char('\n')));
        bUndo->setEnabled(false);
    });
    return w;
}

// =====================================================================================
// 3) So sánh hai thư mục
// =====================================================================================
QWidget* createFolderCompareWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    QLineEdit *ea = nullptr, *eb = nullptr;
    auto* form = new QFormLayout;
    form->addRow(QObject::tr("Thư mục A:"), pathRow(ea, QObject::tr("Thư mục gốc (ví dụ bản cũ)"), w));
    form->addRow(QObject::tr("Thư mục B:"), pathRow(eb, QObject::tr("Thư mục cần so (ví dụ bản mới)"), w));
    auto* content = new QCheckBox(QObject::tr("So sánh nội dung (chậm hơn, chắc chắn hơn; nếu tắt chỉ so kích thước + giờ sửa)"), w);
    auto* filter = new QComboBox(w);
    filter->addItems({QObject::tr("Hiện tất cả"), QObject::tr("Chỉ khác biệt"), QObject::tr("Chỉ có ở A"), QObject::tr("Chỉ có ở B"), QObject::tr("A mới hơn"), QObject::tr("B mới hơn"), QObject::tr("Khác nhau")});
    auto* bGo = btn(QObject::tr("So sánh"), w);
    auto* bExport = btn(QObject::tr("Xuất báo cáo CSV…"), w); bExport->setEnabled(false);
    auto* row = new QHBoxLayout; row->addWidget(filter); row->addStretch(1); row->addWidget(bGo); row->addWidget(bExport);
    auto* table = new QTableWidget(0, 5, w);
    table->setHorizontalHeaderLabels({QObject::tr("Đường dẫn"), QObject::tr("Trạng thái"), QObject::tr("Kích thước A"), QObject::tr("Kích thước B"), QObject::tr("Sửa A / B")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* summary = new QLabel(w);
    summary->setWordWrap(true);
    root->addLayout(form); root->addWidget(content); root->addLayout(row); root->addWidget(table, 1); root->addWidget(summary);

    auto res = std::make_shared<ft::CompareResult>();
    auto show = [=]() {
        const int f = filter->currentIndex();
        table->setRowCount(0);
        for (const auto& e : res->entries) {
            const bool keep = f == 0 || (f == 1 && e.state != ft::DiffState::Same) || (f == 2 && e.state == ft::DiffState::OnlyA) || (f == 3 && e.state == ft::DiffState::OnlyB)
                || (f == 4 && e.state == ft::DiffState::NewerA) || (f == 5 && e.state == ft::DiffState::NewerB) || (f == 6 && e.state == ft::DiffState::Different);
            if (!keep) continue;
            const int r = table->rowCount(); table->insertRow(r);
            table->setItem(r, 0, ro(e.rel)); table->setItem(r, 1, ro(ft::diffStateText(e.state)));
            table->setItem(r, 2, ro(e.sizeA >= 0 ? ft::humanSize(e.sizeA) : QString())); table->setItem(r, 3, ro(e.sizeB >= 0 ? ft::humanSize(e.sizeB) : QString()));
            auto t = [](const QDateTime& d) { return d.isValid() ? d.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("—"); };
            table->setItem(r, 4, ro(t(e.timeA) + QStringLiteral("  /  ") + t(e.timeB)));
        }
    };
    QObject::connect(filter, qOverload<int>(&QComboBox::currentIndexChanged), w, show);
    QObject::connect(bGo, &QPushButton::clicked, w, [=]() {
        if (!QFileInfo(ea->text()).isDir() || !QFileInfo(eb->text()).isDir()) { QMessageBox::information(w, QObject::tr("So sánh"), QObject::tr("Hãy chọn hai thư mục hợp lệ.")); return; }
        ProgressUi pu(w, QObject::tr("Đang so sánh…"));
        ft::CompareOptions o; o.compareContent = content->isChecked();
        *res = ft::compareFolders(ea->text(), eb->text(), o, pu.cb());
        pu.dlg.close();
        if (res->cancelled) { summary->setText(QObject::tr("Đã huỷ.")); return; }
        int c[6] = {0, 0, 0, 0, 0, 0};
        for (const auto& e : res->entries) ++c[int(e.state)];
        summary->setText(QObject::tr("Giống nhau %1 · Chỉ ở A %2 · Chỉ ở B %3 · Khác nhau %4 · A mới hơn %5 · B mới hơn %6.%7")
            .arg(c[0]).arg(c[1]).arg(c[2]).arg(c[3]).arg(c[4]).arg(c[5])
            .arg(res->skipped.isEmpty() ? QString() : QObject::tr("\n%1 tệp không đọc được, ví dụ: %2").arg(res->skipped.size()).arg(res->skipped.first())));
        bExport->setEnabled(true);
        show();
    });
    QObject::connect(bExport, &QPushButton::clicked, w, [=]() {
        const QString p = QFileDialog::getSaveFileName(w, QObject::tr("Lưu báo cáo"), QStringLiteral("so-sanh-thu-muc.csv"), QStringLiteral("CSV (*.csv)"));
        if (p.isEmpty()) return;
        const auto r = ft::exportCompareCsv(p, *res, ea->text(), eb->text());
        QMessageBox::information(w, QObject::tr("Báo cáo"), r.ok ? QObject::tr("Đã lưu %1 dòng.").arg(r.done) : r.error);
    });
    return w;
}

// =====================================================================================
// 4) Dung lượng thư mục
// =====================================================================================
QWidget* createDiskUsageWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    QLineEdit* ep = nullptr;
    auto* row = new QHBoxLayout;
    auto* pr = pathRow(ep, QObject::tr("Thư mục cần xem dung lượng"), w);
    auto* bGo = btn(QObject::tr("Quét"), w);
    row->addWidget(pr, 1); row->addWidget(bGo);
    auto* tabs = new QTabWidget(w);
    auto* t1 = new QTableWidget(0, 4, tabs);
    t1->setHorizontalHeaderLabels({QObject::tr("Mục"), QObject::tr("Dung lượng"), QObject::tr("Số tệp"), QObject::tr("Tỷ lệ")});
    t1->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t1->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* t2 = new QTableWidget(0, 3, tabs);
    t2->setHorizontalHeaderLabels({QObject::tr("Tệp lớn"), QObject::tr("Dung lượng"), QObject::tr("Thư mục chứa")});
    t2->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t2->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabs->addTab(t1, QObject::tr("Theo thư mục con")); tabs->addTab(t2, QObject::tr("Tệp lớn nhất"));
    auto* summary = new QLabel(QObject::tr("Nhấp đúp một dòng để mở thư mục chứa. Công cụ chỉ đọc, không xoá gì."), w);
    summary->setWordWrap(true);
    root->addLayout(row); root->addWidget(tabs, 1); root->addWidget(summary);
    auto res = std::make_shared<ft::UsageResult>();
    QObject::connect(bGo, &QPushButton::clicked, w, [=]() {
        if (!QFileInfo(ep->text()).isDir()) { QMessageBox::information(w, QObject::tr("Dung lượng"), QObject::tr("Hãy chọn một thư mục hợp lệ.")); return; }
        ProgressUi pu(w, QObject::tr("Đang đếm dung lượng…"));
        *res = ft::diskUsage(ep->text(), 100, pu.cb());
        pu.dlg.close();
        t1->setRowCount(res->children.size());
        for (int i = 0; i < res->children.size(); ++i) {
            const auto& n = res->children[i];
            t1->setItem(i, 0, ro((n.isDir ? QStringLiteral("📁 ") : QStringLiteral("📄 ")) + n.name)); t1->item(i, 0)->setData(Qt::UserRole, n.path);
            t1->setItem(i, 1, ro(ft::humanSize(n.size))); t1->setItem(i, 2, ro(QString::number(n.files)));
            auto* bar = new QProgressBar; bar->setRange(0, 1000); bar->setValue(res->total > 0 ? int(n.size * 1000 / res->total) : 0);
            bar->setFormat(QStringLiteral("%1%").arg(res->total > 0 ? double(n.size) * 100.0 / double(res->total) : 0.0, 0, 'f', 1)); t1->setCellWidget(i, 3, bar);
        }
        t2->setRowCount(res->largestFiles.size());
        for (int i = 0; i < res->largestFiles.size(); ++i) {
            const auto& n = res->largestFiles[i];
            t2->setItem(i, 0, ro(n.name)); t2->item(i, 0)->setData(Qt::UserRole, n.path);
            t2->setItem(i, 1, ro(ft::humanSize(n.size))); t2->setItem(i, 2, ro(QFileInfo(n.path).absolutePath()));
        }
        summary->setText(res->cancelled ? QObject::tr("Đã huỷ — số liệu chưa đầy đủ.") : QObject::tr("Tổng: %1 trong %2 tệp. Liên kết (symlink) không được tính.").arg(ft::humanSize(res->total)).arg(res->totalFiles));
    });
    auto openFolder = [](QTableWidget* t, int row) {
        const QString p = t->item(row, 0)->data(Qt::UserRole).toString();
        const QFileInfo fi(p);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? p : fi.absolutePath()));
    };
    QObject::connect(t1, &QTableWidget::cellDoubleClicked, w, [=](int r, int) { openFolder(t1, r); });
    QObject::connect(t2, &QTableWidget::cellDoubleClicked, w, [=](int r, int) { openFolder(t2, r); });
    return w;
}

// =====================================================================================
// 5) Liên kết
// =====================================================================================
QWidget* createLinkToolsWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    auto* hint = new QLabel(QObject::tr("Dán văn bản hoặc danh sách liên kết. Công cụ gom link, bỏ tham số theo dõi (utm_*, fbclid, gclid…), bỏ trùng. Chỉ khi bạn bấm “Kiểm tra sống/chết” thì máy mới gửi yêu cầu mạng tới các trang đó."), w);
    hint->setWordWrap(true);
    auto* in = new QPlainTextEdit(w); in->setPlaceholderText(QObject::tr("Dán văn bản hoặc danh sách liên kết vào đây…"));
    auto* strip = new QCheckBox(QObject::tr("bỏ tham số theo dõi"), w); strip->setChecked(true);
    auto* frag = new QCheckBox(QObject::tr("bỏ phần #… ở cuối"), w);
    auto* bClean = btn(QObject::tr("Gom && làm sạch"), w);
    auto* opts = new QHBoxLayout; opts->addWidget(strip); opts->addWidget(frag); opts->addStretch(1); opts->addWidget(bClean);
    auto* table = new QTableWidget(0, 2, w);
    table->setHorizontalHeaderLabels({QObject::tr("Liên kết"), QObject::tr("Kết quả kiểm tra")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* summary = new QLabel(w); summary->setWordWrap(true);
    auto* act = new QHBoxLayout;
    auto* bCopy = btn(QObject::tr("Sao chép danh sách"), w);
    auto* bExport = btn(QObject::tr("Xuất ra tệp…"), w);
    auto* bCheck = btn(QObject::tr("Kiểm tra sống/chết (gửi yêu cầu mạng)…"), w);
    auto* bStop = btn(QObject::tr("Dừng"), w); bStop->setEnabled(false);
    bCopy->setEnabled(false); bExport->setEnabled(false); bCheck->setEnabled(false);
    act->addWidget(bCopy); act->addWidget(bExport); act->addStretch(1); act->addWidget(bCheck); act->addWidget(bStop);
    root->addWidget(hint); root->addWidget(in, 1); root->addLayout(opts); root->addWidget(table, 2); root->addWidget(summary); root->addLayout(act);

    auto urls = std::make_shared<QStringList>();
    QObject::connect(bClean, &QPushButton::clicked, w, [=]() {
        ft::LinkOptions o; o.stripTracking = strip->isChecked(); o.dropFragment = frag->isChecked();
        const QStringList found = ft::extractUrls(in->toPlainText());
        const auto r = ft::cleanLinks(found, o);
        *urls = r.urls;
        table->setRowCount(urls->size());
        for (int i = 0; i < urls->size(); ++i) { table->setItem(i, 0, ro(urls->at(i))); table->setItem(i, 1, ro(QString())); }
        summary->setText(QObject::tr("Tìm thấy %1 liên kết · làm sạch %2 · bỏ trùng %3 · còn lại %4.").arg(found.size()).arg(r.cleaned).arg(r.duplicates).arg(urls->size()));
        const bool any = !urls->isEmpty();
        bCopy->setEnabled(any); bExport->setEnabled(any); bCheck->setEnabled(any);
    });
    QObject::connect(bCopy, &QPushButton::clicked, w, [=]() { QApplication::clipboard()->setText(urls->join(QLatin1Char('\n'))); });
    QObject::connect(bExport, &QPushButton::clicked, w, [=]() {
        const QString p = QFileDialog::getSaveFileName(w, QObject::tr("Lưu danh sách"), QStringLiteral("lien-ket.txt"), QStringLiteral("Văn bản (*.txt);;CSV (*.csv)"));
        if (p.isEmpty()) return;
        const auto r = ft::exportLinks(p, *urls);
        QMessageBox::information(w, QObject::tr("Xuất danh sách"), r.ok ? QObject::tr("Đã lưu %1 liên kết.").arg(r.done) : r.error);
    });
    // Kiểm tra sống/chết: chỉ khi bấm; tối đa 4 yêu cầu song song; HEAD, hết hạn 8 giây.
    struct Checker { QNetworkAccessManager nam; QStringList queue; int next = 0, running = 0; bool stop = false; std::function<void()> pump; };
    auto chk = std::make_shared<Checker>();
    QObject::connect(bStop, &QPushButton::clicked, w, [=]() { chk->stop = true; });
    QObject::connect(bCheck, &QPushButton::clicked, w, [=]() {
        QSet<QString> hosts; for (const QString& u : *urls) hosts.insert(QUrl(u).host());
        if (QMessageBox::question(w, QObject::tr("Kiểm tra liên kết"),
                QObject::tr("Máy sẽ gửi yêu cầu mạng (HEAD) tới %1 liên kết trên %2 máy chủ. Máy chủ đó sẽ thấy địa chỉ IP của bạn. Tiếp tục?").arg(urls->size()).arg(hosts.size())) != QMessageBox::Yes) return;
        chk->queue = *urls; chk->next = 0; chk->running = 0; chk->stop = false;
        bCheck->setEnabled(false); bStop->setEnabled(true); bClean->setEnabled(false);
        Checker* c = chk.get(); // con trỏ thô: tránh vòng tham chiếu shared_ptr
        c->pump = [=]() {
            while (!c->stop && c->running < 4 && c->next < c->queue.size()) {
                const int idx = c->next++;
                QNetworkRequest rq{QUrl(c->queue[idx])};
                rq.setTransferTimeout(8000);
                rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
                rq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("DungOffice-LinkCheck/1.0"));
                QNetworkReply* rep = c->nam.head(rq);
                ++c->running;
                QObject::connect(rep, &QNetworkReply::finished, w, [=]() {
                    const int code = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    QString text;
                    if (code >= 200 && code < 400) text = QObject::tr("Sống (%1)").arg(code);
                    else if (code == 404 || code == 410) text = QObject::tr("Chết (%1)").arg(code);
                    else if (code > 0) text = QObject::tr("Không rõ (mã %1: máy chủ có thể chặn kiểm tra)").arg(code);
                    else text = QObject::tr("Lỗi mạng: %1").arg(rep->errorString());
                    if (idx < table->rowCount()) table->item(idx, 1)->setText(text);
                    rep->deleteLater();
                    --c->running;
                    if (c->running == 0 && (c->stop || c->next >= c->queue.size())) {
                        bCheck->setEnabled(true); bStop->setEnabled(false); bClean->setEnabled(true);
                        summary->setText(summary->text() + QObject::tr("\nĐã kiểm tra xong (kết quả chỉ mang tính tham khảo)."));
                    } else c->pump();
                });
            }
        };
        c->pump();
    });
    return w;
}

// =====================================================================================
// 6) Sắp xếp tệp vào thư mục
// =====================================================================================
QWidget* createOrganizeWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    auto* hint = new QLabel(QObject::tr("Chuyển tệp vào các thư mục con theo loại, đuôi, năm hoặc tháng. Luôn XEM TRƯỚC trước khi chuyển; không ghi đè (trùng tên thì thêm số); có nhật ký để HOÀN TÁC."), w);
    hint->setWordWrap(true);
    QLineEdit *src = nullptr, *dst = nullptr;
    auto* srcRow = pathRow(src, QObject::tr("Thư mục chứa tệp cần sắp xếp"), w);
    auto* dstRow = pathRow(dst, QObject::tr("Thư mục đích (để trống = ngay trong thư mục nguồn)"), w);
    auto* mode = new QComboBox(w);
    mode->addItem(QObject::tr("Theo loại (Tài liệu, Bảng tính, Ảnh…)"), int(ft::OrganizeMode::ByCategory));
    mode->addItem(QObject::tr("Theo đuôi tệp (DOCX, XLSX…)"), int(ft::OrganizeMode::ByExtension));
    mode->addItem(QObject::tr("Theo năm sửa"), int(ft::OrganizeMode::ByYear));
    mode->addItem(QObject::tr("Theo năm/tháng sửa"), int(ft::OrganizeMode::ByYearMonth));
    auto* rec = new QCheckBox(QObject::tr("gồm cả thư mục con"), w);
    auto* bPrev = btn(QObject::tr("Xem trước"), w);
    auto* opts = new QHBoxLayout; opts->addWidget(mode, 1); opts->addWidget(rec); opts->addWidget(bPrev);
    auto* table = new QTableWidget(0, 3, w);
    table->setHorizontalHeaderLabels({QObject::tr("Từ"), QObject::tr("Đến"), QObject::tr("Ghi chú")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    auto* summary = new QLabel(w); summary->setWordWrap(true);
    auto* bApply = btn(QObject::tr("Chuyển tệp…"), w); bApply->setEnabled(false);
    auto* bUndo = btn(QObject::tr("Hoàn tác lần vừa rồi"), w); bUndo->setEnabled(false);
    auto* bUndoFile = btn(QObject::tr("Hoàn tác từ nhật ký…"), w);
    auto* act = new QHBoxLayout; act->addWidget(bUndo); act->addWidget(bUndoFile); act->addStretch(1); act->addWidget(bApply);
    root->addWidget(hint); root->addWidget(srcRow); root->addWidget(dstRow); root->addLayout(opts); root->addWidget(table, 1); root->addWidget(summary); root->addLayout(act);

    auto plan = std::make_shared<ft::OrganizePlan>();
    auto lastJournal = std::make_shared<QString>();
    QObject::connect(bPrev, &QPushButton::clicked, w, [=]() {
        if (src->text().trimmed().isEmpty() || !QFileInfo(src->text()).isDir()) { QMessageBox::warning(w, QObject::tr("Sắp xếp tệp"), QObject::tr("Hãy chọn thư mục nguồn hợp lệ.")); return; }
        *plan = ft::planOrganize(src->text().trimmed(), rec->isChecked(), ft::OrganizeMode(mode->currentData().toInt()), dst->text().trimmed());
        table->setRowCount(plan->items.size());
        for (int i = 0; i < plan->items.size(); ++i) {
            const auto& it = plan->items[i];
            table->setItem(i, 0, ro(it.from)); table->setItem(i, 1, ro(it.to)); table->setItem(i, 2, ro(it.note));
        }
        summary->setText(QObject::tr("Sẽ chuyển %1 tệp · %2 tệp bỏ qua · %3 tệp phải thêm số vì trùng tên.%4")
                             .arg(plan->items.size()).arg(plan->skipped.size()).arg(plan->renamedForConflict)
                             .arg(plan->skipped.isEmpty() ? QString() : QStringLiteral("\n") + plan->skipped.mid(0, 5).join(QLatin1Char('\n'))));
        bApply->setEnabled(!plan->items.isEmpty());
    });
    QObject::connect(bApply, &QPushButton::clicked, w, [=]() {
        if (QMessageBox::question(w, QObject::tr("Chuyển tệp"), QObject::tr("Chuyển %1 tệp theo bảng xem trước? Có thể hoàn tác bằng nút “Hoàn tác”.").arg(plan->items.size())) != QMessageBox::Yes) return;
        const auto r = ft::applyOrganize(*plan, journalDir());
        if (!r.ok) { QMessageBox::critical(w, QObject::tr("Sắp xếp tệp"), r.error + (r.warnings.isEmpty() ? QString() : QStringLiteral("\n") + r.warnings.join(QLatin1Char('\n')))); return; }
        *lastJournal = r.journalPath;
        bUndo->setEnabled(!r.journalPath.isEmpty());
        bApply->setEnabled(false);
        table->setRowCount(0);
        summary->setText(QObject::tr("Đã chuyển %1 tệp. Nhật ký: %2").arg(r.done).arg(r.journalPath));
    });
    auto doUndo = [=](const QString& jp) {
        const auto r = ft::undoJournal(jp);
        QMessageBox::information(w, QObject::tr("Hoàn tác"), r.ok ? QObject::tr("Đã đưa %1 tệp về chỗ cũ.").arg(r.done) : r.error + QStringLiteral("\n") + r.warnings.join(QLatin1Char('\n')));
        if (r.ok) bUndo->setEnabled(false);
    };
    QObject::connect(bUndo, &QPushButton::clicked, w, [=]() { if (!lastJournal->isEmpty()) doUndo(*lastJournal); });
    QObject::connect(bUndoFile, &QPushButton::clicked, w, [=]() {
        const QString jp = QFileDialog::getOpenFileName(w, QObject::tr("Chọn nhật ký"), journalDir(), QStringLiteral("Nhật ký (*.json)"));
        if (!jp.isEmpty()) doUndo(jp);
    });
    return w;
}

// =====================================================================================
// 7) Đường dẫn
// =====================================================================================
QWidget* createPathToolsWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    auto* hint = new QLabel(QObject::tr("Chọn tệp/thư mục để sao chép đường dẫn ở nhiều dạng. Windows cũ lỗi với đường dẫn từ 260 ký tự trở lên — công cụ cảnh báo và có thể dò cả thư mục."), w);
    hint->setWordWrap(true);
    auto* path = new QLineEdit(w); path->setPlaceholderText(QObject::tr("Đường dẫn tệp hoặc thư mục"));
    auto* bFile = btn(QObject::tr("Chọn tệp…"), w);
    auto* bDir = btn(QObject::tr("Chọn thư mục…"), w);
    auto* top = new QHBoxLayout; top->addWidget(path, 1); top->addWidget(bFile); top->addWidget(bDir);
    auto* form = new QFormLayout;
    auto* warn = new QLabel(w); warn->setWordWrap(true);
    struct Row { QString label; QLineEdit* edit; };
    auto rows = std::make_shared<QVector<Row>>();
    const QStringList labels = {QObject::tr("Đầy đủ"), QObject::tr("Có ngoặc kép"), QObject::tr("Dấu /"), QObject::tr("Địa chỉ file://"), QObject::tr("Thư mục chứa"), QObject::tr("Tên"), QObject::tr("Dạng dài \\\\?\\")};
    for (const QString& l : labels) {
        auto* line = new QLineEdit(w); line->setReadOnly(true);
        auto* b = btn(QObject::tr("Chép"), w);
        QObject::connect(b, &QPushButton::clicked, w, [line]() { QApplication::clipboard()->setText(line->text()); });
        auto* h = new QHBoxLayout; h->addWidget(line, 1); h->addWidget(b);
        form->addRow(l, h);
        rows->push_back({l, line});
    }
    auto refresh = [=]() {
        const QString t = path->text().trimmed();
        if (t.isEmpty()) { for (auto& r : *rows) r.edit->clear(); warn->clear(); return; }
        const ft::PathForms f = ft::pathForms(t);
        const QStringList vals = {f.full, f.quoted, f.slashes, f.fileUrl, f.folder, f.name, f.extended};
        for (int i = 0; i < rows->size(); ++i) (*rows)[i].edit->setText(vals[i]);
        if (f.tooLong) warn->setText(QObject::tr("⚠ Đường dẫn dài %1 ký tự (≥ %2): nhiều chương trình Windows sẽ không mở/sao chép được. Hãy rút ngắn tên thư mục hoặc dùng dạng dài \\\\?\\ trong công cụ hỗ trợ.").arg(f.length).arg(ft::kWinMaxPath));
        else if (f.nearLimit) warn->setText(QObject::tr("Lưu ý: đường dẫn dài %1 ký tự, gần giới hạn %2.").arg(f.length).arg(ft::kWinMaxPath));
        else warn->setText(QObject::tr("Đường dẫn dài %1 ký tự — an toàn.").arg(f.length));
    };
    QObject::connect(path, &QLineEdit::textChanged, w, refresh);
    QObject::connect(bFile, &QPushButton::clicked, w, [=]() { const QString p = QFileDialog::getOpenFileName(w, QObject::tr("Chọn tệp")); if (!p.isEmpty()) path->setText(p); });
    QObject::connect(bDir, &QPushButton::clicked, w, [=]() { const QString p = QFileDialog::getExistingDirectory(w, QObject::tr("Chọn thư mục")); if (!p.isEmpty()) path->setText(p); });

    auto* gb = new QGroupBox(QObject::tr("Dò đường dẫn quá dài trong một thư mục"), w);
    auto* gl = new QVBoxLayout(gb);
    QLineEdit* scanRoot = nullptr;
    auto* scanRow = pathRow(scanRoot, QObject::tr("Thư mục cần dò"), gb);
    auto* bScan = btn(QObject::tr("Dò"), gb);
    auto* list = new QListWidget(gb);
    auto* res = new QLabel(gb); res->setWordWrap(true);
    auto* bCopyList = btn(QObject::tr("Chép danh sách"), gb); bCopyList->setEnabled(false);
    auto* gh = new QHBoxLayout; gh->addWidget(scanRow, 1); gh->addWidget(bScan);
    gl->addLayout(gh); gl->addWidget(list, 1); gl->addWidget(res); gl->addWidget(bCopyList, 0, Qt::AlignRight);
    QObject::connect(bScan, &QPushButton::clicked, w, [=]() {
        if (!QFileInfo(scanRoot->text()).isDir()) { QMessageBox::warning(w, QObject::tr("Dò đường dẫn"), QObject::tr("Hãy chọn thư mục hợp lệ.")); return; }
        ProgressUi ui(w, QObject::tr("Đang dò…"));
        const auto r = ft::findLongPaths(scanRoot->text(), ft::kWinMaxPath, [&](qint64, qint64) { QApplication::processEvents(); return !ui.dlg.wasCanceled(); });
        list->clear(); list->addItems(r.paths);
        res->setText(r.cancelled ? QObject::tr("Đã huỷ — kết quả chưa đầy đủ (%1 mục đã dò).").arg(r.scanned)
                                 : QObject::tr("Đã dò %1 mục · %2 đường dẫn từ %3 ký tự trở lên.").arg(r.scanned).arg(r.paths.size()).arg(ft::kWinMaxPath));
        bCopyList->setEnabled(!r.paths.isEmpty());
    });
    QObject::connect(bCopyList, &QPushButton::clicked, w, [=]() {
        QStringList l; for (int i = 0; i < list->count(); ++i) l << list->item(i)->text();
        QApplication::clipboard()->setText(l.join(QLatin1Char('\n')));
    });
    root->addWidget(hint); root->addLayout(top); root->addLayout(form); root->addWidget(warn); root->addWidget(gb, 1);
    return w;
}

// =====================================================================================
// 8) Quét liên kết trong tệp (.url, .lnk, docx/xlsx/pptx)
// =====================================================================================
QWidget* createLinkScanWidget(QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* root = new QVBoxLayout(w);
    auto* hint = new QLabel(QObject::tr("Quét lối tắt (.url, .lnk) và liên kết ngoài trong Word/Excel/PowerPoint (docx, xlsx, pptx…) để tìm liên kết tới tệp đã mất. Chỉ ĐỌC, không sửa tệp nào. Liên kết web chỉ được kiểm tra khi bạn bấm nút riêng. Chưa đọc: công thức HYPERLINK() của Excel, tệp .doc/.xls/.ppt cũ."), w);
    hint->setWordWrap(true);
    QLineEdit* dir = nullptr;
    auto* dirRow = pathRow(dir, QObject::tr("Thư mục cần quét"), w);
    auto* rec = new QCheckBox(QObject::tr("gồm cả thư mục con"), w); rec->setChecked(true);
    auto* net = new QCheckBox(QObject::tr("kiểm tra cả đường dẫn mạng (\\\\máy\\…, có thể chậm)"), w);
    auto* onlyBad = new QCheckBox(QObject::tr("chỉ hiện liên kết hỏng"), w);
    auto* bScan = btn(QObject::tr("Quét"), w);
    auto* opts = new QHBoxLayout; opts->addWidget(rec); opts->addWidget(net); opts->addWidget(onlyBad); opts->addStretch(1); opts->addWidget(bScan);
    auto* table = new QTableWidget(0, 6, w);
    table->setHorizontalHeaderLabels({QObject::tr("Tệp"), QObject::tr("Phần"), QObject::tr("Loại"), QObject::tr("Đích"), QObject::tr("Trạng thái"), QObject::tr("Ghi chú")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* summary = new QLabel(w); summary->setWordWrap(true);
    auto* bExport = btn(QObject::tr("Xuất CSV…"), w); bExport->setEnabled(false);
    auto* bWeb = btn(QObject::tr("Kiểm tra liên kết web (gửi yêu cầu mạng)…"), w); bWeb->setEnabled(false);
    auto* bStop = btn(QObject::tr("Dừng"), w); bStop->setEnabled(false);
    auto* act = new QHBoxLayout; act->addWidget(bExport); act->addStretch(1); act->addWidget(bWeb); act->addWidget(bStop);
    root->addWidget(hint); root->addWidget(dirRow); root->addLayout(opts); root->addWidget(table, 1); root->addWidget(summary); root->addLayout(act);

    auto result = std::make_shared<ft::ScanLinksResult>();
    auto show = [=]() {
        table->setRowCount(0);
        int row = 0;
        for (int i = 0; i < result->findings.size(); ++i) {
            const auto& f = result->findings[i];
            if (onlyBad->isChecked() && f.state != ft::LinkState::Broken) continue;
            table->insertRow(row);
            const QStringList cells = {f.source, f.where, f.kind, f.target, ft::linkStateText(f.state), f.note};
            for (int c = 0; c < 6; ++c) table->setItem(row, c, ro(cells[c]));
            table->item(row, 0)->setData(Qt::UserRole, i);
            if (f.state == ft::LinkState::Broken) for (int c = 0; c < 6; ++c) table->item(row, c)->setForeground(QColor(0xB0, 0x1E, 0x1E));
            ++row;
        }
    };
    QObject::connect(onlyBad, &QCheckBox::toggled, w, show);
    QObject::connect(bScan, &QPushButton::clicked, w, [=]() {
        if (!QFileInfo(dir->text()).isDir()) { QMessageBox::warning(w, QObject::tr("Quét liên kết"), QObject::tr("Hãy chọn thư mục hợp lệ.")); return; }
        ProgressUi ui(w, QObject::tr("Đang quét…"));
        ft::ScanLinksOptions o; o.recursive = rec->isChecked(); o.checkNetworkPaths = net->isChecked();
        *result = ft::scanLinks({dir->text()}, o, [&](qint64 d, qint64 t) {
            ui.dlg.setMaximum(int(qMax<qint64>(t, 1))); ui.dlg.setValue(int(qMin<qint64>(d, t)));
            QApplication::processEvents(); return !ui.dlg.wasCanceled();
        });
        show();
        int web = 0; for (const auto& f : result->findings) if (f.type == ft::TargetType::Web) ++web;
        summary->setText(QObject::tr("Đã quét %1 tệp · %2 liên kết · %3 HỎNG · %4 liên kết web chưa kiểm tra · %5 tệp không đọc được.%6")
                             .arg(result->filesScanned).arg(result->findings.size()).arg(result->brokenCount()).arg(web).arg(result->skipped.size())
                             .arg(result->cancelled ? QObject::tr(" (đã huỷ — chưa đầy đủ)") : QString())
                         + (result->skipped.isEmpty() ? QString() : QStringLiteral("\n") + result->skipped.mid(0, 4).join(QLatin1Char('\n'))));
        bExport->setEnabled(!result->findings.isEmpty());
        bWeb->setEnabled(web > 0);
    });
    QObject::connect(bExport, &QPushButton::clicked, w, [=]() {
        const QString p = QFileDialog::getSaveFileName(w, QObject::tr("Lưu báo cáo"), QStringLiteral("bao-cao-lien-ket.csv"), QStringLiteral("CSV (*.csv)"));
        if (p.isEmpty()) return;
        const auto r = ft::exportFindingsCsv(p, result->findings);
        QMessageBox::information(w, QObject::tr("Xuất báo cáo"), r.ok ? QObject::tr("Đã lưu %1 dòng.").arg(r.done) : r.error);
    });
    // Kiểm tra web: chỉ khi bấm, sau khi hỏi; mỗi URL một lần; HEAD; hết hạn 8 giây; tối đa 4 yêu cầu song song.
    struct Checker { QNetworkAccessManager nam; QStringList urls; QHash<QString, QString> out; int next = 0, running = 0; bool stop = false; std::function<void()> pump; };
    auto chk = std::make_shared<Checker>();
    QObject::connect(bStop, &QPushButton::clicked, w, [=]() { chk->stop = true; });
    QObject::connect(bWeb, &QPushButton::clicked, w, [=]() {
        QSet<QString> uniq, hosts;
        for (const auto& f : result->findings) if (f.type == ft::TargetType::Web && f.target.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) { uniq.insert(f.target); hosts.insert(QUrl(f.target).host()); }
        if (uniq.isEmpty()) return;
        if (QMessageBox::question(w, QObject::tr("Kiểm tra liên kết web"),
                QObject::tr("Máy sẽ gửi yêu cầu mạng (HEAD) tới %1 liên kết trên %2 máy chủ. Máy chủ đó sẽ thấy địa chỉ IP của bạn. Tiếp tục?").arg(uniq.size()).arg(hosts.size())) != QMessageBox::Yes) return;
        chk->urls = QStringList(uniq.begin(), uniq.end()); chk->urls.sort(); chk->out.clear(); chk->next = 0; chk->running = 0; chk->stop = false;
        bWeb->setEnabled(false); bStop->setEnabled(true); bScan->setEnabled(false);
        Checker* c = chk.get();
        c->pump = [=]() {
            while (!c->stop && c->running < 4 && c->next < c->urls.size()) {
                const QString url = c->urls[c->next++];
                QNetworkRequest rq{QUrl(url)};
                rq.setTransferTimeout(8000);
                rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
                rq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("DungOffice-LinkCheck/1.0"));
                QNetworkReply* rep = c->nam.head(rq);
                ++c->running;
                QObject::connect(rep, &QNetworkReply::finished, w, [=]() {
                    const int code = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    ft::LinkState st = ft::LinkState::Unchecked; QString note;
                    if (code >= 200 && code < 400) { st = ft::LinkState::Ok; note = QObject::tr("Sống (%1)").arg(code); }
                    else if (code == 404 || code == 410) { st = ft::LinkState::Broken; note = QObject::tr("Chết (%1)").arg(code); }
                    else if (code > 0) note = QObject::tr("Không rõ (mã %1: máy chủ có thể chặn kiểm tra)").arg(code);
                    else note = QObject::tr("Lỗi mạng: %1").arg(rep->errorString());
                    for (auto& f : result->findings) if (f.target == url && f.type == ft::TargetType::Web) { f.state = st; f.note = note; }
                    rep->deleteLater();
                    --c->running;
                    if (c->running == 0 && (c->stop || c->next >= c->urls.size())) {
                        bWeb->setEnabled(true); bStop->setEnabled(false); bScan->setEnabled(true);
                        show();
                        summary->setText(summary->text() + QObject::tr("\nĐã kiểm tra web xong (chỉ mang tính tham khảo: một số máy chủ chặn kiểm tra tự động)."));
                    } else c->pump();
                });
            }
        };
        c->pump();
    });
    return w;
}
