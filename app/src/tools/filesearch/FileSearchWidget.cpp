#include "FileSearchWidget.h"
#include "FileSearchCore.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QThread>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <atomic>
#include <memory>

namespace {
QString humanSize(qint64 b)
{
    if (b < 1024) return QStringLiteral("%1 B").arg(b);
    if (b < 1024 * 1024) return QStringLiteral("%1 KB").arg(b / 1024.0, 0, 'f', 1);
    if (b < 1024LL * 1024 * 1024) return QStringLiteral("%1 MB").arg(b / 1048576.0, 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(b / 1073741824.0, 0, 'f', 2);
}

class FileSearchWidget : public QWidget {
public:
    explicit FileSearchWidget(QWidget *parent)
        : QWidget(parent)
    {
        auto *lay = new QVBoxLayout(this);
        m_name = new QLineEdit;
        m_name->setPlaceholderText(tr("Tên tệp, ví dụ: bao cao -nhap ext:docx,xlsx size>1mb after:2026-01-01"));
        m_name->setToolTip(tr("Không phân biệt hoa/thường và dấu. -từ để loại trừ; \"cụm từ\" trong ngoặc kép; ext:docx,xlsx; size>1mb; size<10mb; after:yyyy-mm-dd; before:yyyy-mm-dd"));
        m_content = new QLineEdit;
        m_content->setPlaceholderText(tr("Từ khoá trong NỘI DUNG (tuỳ chọn): txt, csv, md, docx, xlsx, pptx, pdf"));
        m_folder = new QLineEdit(QDir::homePath());
        auto *pick = new QPushButton(tr("Chọn thư mục…"));
        auto *row1 = new QHBoxLayout;
        row1->addWidget(new QLabel(tr("Tìm trong:")));
        row1->addWidget(m_folder, 1);
        row1->addWidget(pick);
        m_diac = new QCheckBox(tr("Phân biệt dấu"));
        m_hidden = new QCheckBox(tr("Cả tệp ẩn"));
        m_useIdx = new QCheckBox(tr("Dùng chỉ mục tên tệp khi có"));
        m_useIdx->setChecked(true);
        m_go = new QPushButton(tr("Tìm"));
        m_stop = new QPushButton(tr("Dừng"));
        m_stop->setEnabled(false);
        m_build = new QPushButton(tr("Lập chỉ mục…"));
        m_build->setToolTip(tr("Tuỳ chọn. Chỉ lưu tên, dung lượng, ngày sửa — không lưu nội dung. Chạy khi bạn bấm, không chạy nền."));
        m_drop = new QPushButton(tr("Xoá chỉ mục"));
        auto *row2 = new QHBoxLayout;
        row2->addWidget(m_diac);
        row2->addWidget(m_hidden);
        row2->addWidget(m_useIdx);
        row2->addStretch(1);
        row2->addWidget(m_build);
        row2->addWidget(m_drop);
        row2->addWidget(m_stop);
        row2->addWidget(m_go);
        m_tree = new QTreeWidget;
        m_tree->setHeaderLabels({tr("Tên"), tr("Thư mục"), tr("Dung lượng"), tr("Ngày sửa"), tr("Vị trí"), tr("Trích đoạn")});
        m_tree->setRootIsDecorated(false);
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
        m_tree->setColumnWidth(0, 240);
        m_tree->setColumnWidth(1, 300);
        m_status = new QLabel(tr("Nhập từ khoá hoặc bộ lọc rồi bấm Tìm."));
        m_skipBtn = new QPushButton(tr("Xem tệp bị bỏ qua…"));
        m_skipBtn->setVisible(false);
        auto *row3 = new QHBoxLayout;
        row3->addWidget(m_status, 1);
        row3->addWidget(m_skipBtn);
        lay->addWidget(m_name);
        lay->addWidget(m_content);
        lay->addLayout(row1);
        lay->addLayout(row2);
        lay->addWidget(m_tree, 1);
        lay->addLayout(row3);

        connect(pick, &QPushButton::clicked, this, [this] {
            const QString d = QFileDialog::getExistingDirectory(this, tr("Chọn thư mục để tìm"), m_folder->text());
            if (!d.isEmpty()) m_folder->setText(d);
        });
        connect(m_go, &QPushButton::clicked, this, [this] { startSearch(); });
        connect(m_name, &QLineEdit::returnPressed, this, [this] { startSearch(); });
        connect(m_content, &QLineEdit::returnPressed, this, [this] { startSearch(); });
        connect(m_stop, &QPushButton::clicked, this, [this] { if (m_cancel) m_cancel->store(true); });
        connect(m_build, &QPushButton::clicked, this, [this] { buildIndex(); });
        connect(m_drop, &QPushButton::clicked, this, [this] {
            const bool ok = filesearch::removeIndex(m_folder->text(), indexDir());
            m_status->setText(ok ? tr("Đã xoá chỉ mục của thư mục này.") : tr("Không xoá được chỉ mục."));
        });
        connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it) { openFile(it->data(0, Qt::UserRole).toString()); });
        connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &p) {
            auto *it = m_tree->itemAt(p);
            if (!it) return;
            const QString path = it->data(0, Qt::UserRole).toString();
            QMenu m;
            m.addAction(tr("Mở tệp"), [this, path] { openFile(path); });
            m.addAction(tr("Mở thư mục chứa"), [this, path] { openFolder(path); });
            m.addAction(tr("Sao chép đường dẫn"), [path] { QApplication::clipboard()->setText(QDir::toNativeSeparators(path)); });
            m.exec(m_tree->viewport()->mapToGlobal(p));
        });
        connect(m_skipBtn, &QPushButton::clicked, this, [this] { showSkipped(); });
    }
    ~FileSearchWidget() override
    {
        if (m_cancel) m_cancel->store(true);
        if (m_thread) { m_thread->wait(); delete m_thread; }
    }

private:
    static QString indexDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/filesearch-index"); }

    void setBusy(bool b)
    {
        m_go->setEnabled(!b);
        m_build->setEnabled(!b);
        m_drop->setEnabled(!b);
        m_stop->setEnabled(b);
    }
    void waitOldThread()
    {
        if (m_thread) { m_thread->wait(); delete m_thread; m_thread = nullptr; }
    }

    void startSearch()
    {
        if (m_thread && m_thread->isRunning()) return;
        waitOldThread();
        QStringList warnings;
        filesearch::Query q = filesearch::parseQuery(m_name->text(), &warnings);
        q.root = m_folder->text().trimmed();
        q.contentTerms = filesearch::splitTerms(m_content->text());
        q.ignoreDiacritics = !m_diac->isChecked();
        q.includeHidden = m_hidden->isChecked();
        m_tree->clear();
        m_skipped.clear();
        m_skipBtn->setVisible(false);
        if (!warnings.isEmpty()) QMessageBox::information(this, tr("Tìm tệp"), tr("Một số điều kiện không được hiểu và đã bị bỏ qua:\n\n%1").arg(warnings.join(QLatin1Char('\n'))));
        auto idx = std::make_shared<filesearch::Index>();
        bool haveIdx = false;
        if (m_useIdx->isChecked()) {
            QString err;
            haveIdx = filesearch::loadIndex(q.root, indexDir(), idx.get(), &err);
        }
        m_cancel = std::make_shared<std::atomic_bool>(false);
        auto cancel = m_cancel;
        m_status->setText(tr("Đang tìm…"));
        setBusy(true);
        m_thread = QThread::create([this, q, idx, haveIdx, cancel] {
            auto out = filesearch::search(
                q, haveIdx ? idx.get() : nullptr, cancel.get(),
                [this](const filesearch::Result &r) {
                    QMetaObject::invokeMethod(this, [this, r] { addRow(r); }, Qt::QueuedConnection);
                },
                [this](qint64 n) {
                    QMetaObject::invokeMethod(this, [this, n] { m_status->setText(tr("Đang tìm… đã duyệt %1 tệp").arg(n)); }, Qt::QueuedConnection);
                });
            QMetaObject::invokeMethod(this, [this, out, haveIdx] { finished(out, haveIdx); }, Qt::QueuedConnection);
        });
        m_thread->start();
    }

    void addRow(const filesearch::Result &r)
    {
        const QFileInfo fi(r.path);
        auto *it = new QTreeWidgetItem(m_tree);
        it->setText(0, fi.fileName());
        it->setText(1, QDir::toNativeSeparators(fi.absolutePath()));
        it->setText(2, humanSize(r.size));
        it->setText(3, r.modified.toString(QStringLiteral("dd/MM/yyyy HH:mm")));
        it->setText(4, r.location);
        it->setText(5, r.snippet);
        it->setData(0, Qt::UserRole, r.path);
    }

    void finished(const filesearch::Output &out, bool usedIndex)
    {
        setBusy(false);
        if (!out.error.isEmpty()) { m_status->setText(out.error); return; }
        m_tree->clear(); // kết quả cuối đã sắp xếp theo đường dẫn
        for (const auto &r : out.results) addRow(r);
        m_skipped = out.skipped;
        QString s = tr("%1 kết quả trong %2 ms, duyệt %3 tệp").arg(out.stats.matched).arg(out.stats.elapsedMs).arg(out.stats.filesSeen);
        if (usedIndex) s += tr(" (dùng chỉ mục tên tệp)");
        if (out.stats.canceled) s += tr(" — đã dừng theo yêu cầu, kết quả chưa đầy đủ");
        if (out.stats.truncatedResults) s += tr(" — đạt giới hạn số kết quả, hãy thu hẹp bộ lọc");
        if (out.stats.staleIndexEntries > 0) s += tr(" — %1 mục chỉ mục đã cũ (tệp không còn)").arg(out.stats.staleIndexEntries);
        if (out.stats.pdfUnavailable) s += tr(" — KHÔNG tìm nội dung PDF được vì máy bản dựng này không có bộ đọc PDF (Qt6::Pdf) và máy chưa có pdftotext");
        if (!out.stats.unsupportedByExt.isEmpty()) {
            QStringList l;
            for (auto it = out.stats.unsupportedByExt.cbegin(); it != out.stats.unsupportedByExt.cend(); ++it) l << QStringLiteral(".%1 (%2)").arg(it.key()).arg(it.value());
            l.sort();
            s += tr(" — chưa tìm nội dung được trong: %1").arg(l.join(QStringLiteral(", ")));
        }
        if (out.stats.skipped > 0) { s += tr(" — %1 tệp bị bỏ qua").arg(out.stats.skipped); m_skipBtn->setVisible(true); }
        m_status->setText(s);
    }

    void showSkipped()
    {
        QDialog d(this);
        d.setWindowTitle(tr("Tệp bị bỏ qua và lý do"));
        auto *l = new QVBoxLayout(&d);
        auto *t = new QPlainTextEdit;
        t->setReadOnly(true);
        QStringList lines;
        for (const auto &s : m_skipped) lines << QDir::toNativeSeparators(s.path) + QStringLiteral("\n    → ") + s.reason;
        t->setPlainText(lines.join(QStringLiteral("\n")));
        l->addWidget(t);
        d.resize(720, 420);
        d.exec();
    }

    void buildIndex()
    {
        if (m_thread && m_thread->isRunning()) return;
        waitOldThread();
        const QString root = m_folder->text().trimmed();
        if (!QFileInfo(root).isDir()) { m_status->setText(tr("Hãy chọn một thư mục tồn tại.")); return; }
        m_cancel = std::make_shared<std::atomic_bool>(false);
        auto cancel = m_cancel;
        const bool hidden = m_hidden->isChecked();
        m_status->setText(tr("Đang lập chỉ mục tên tệp… (bấm Dừng để huỷ)"));
        setBusy(true);
        m_thread = QThread::create([this, root, hidden, cancel] {
            QString err;
            qint64 n = 0;
            const bool ok = filesearch::buildIndex(root, indexDir(), hidden, cancel.get(), &err, &n);
            QMetaObject::invokeMethod(this, [this, ok, err, n] {
                setBusy(false);
                m_status->setText(ok ? tr("Đã lập chỉ mục %1 tệp (chỉ tên, dung lượng, ngày sửa). Xoá được bất cứ lúc nào.").arg(n) : tr("Không lập được chỉ mục: %1").arg(err));
            }, Qt::QueuedConnection);
        });
        m_thread->start();
    }

    void openFile(const QString &path)
    {
        if (!QFileInfo::exists(path)) { QMessageBox::warning(this, tr("Tìm tệp"), tr("Tệp không còn tồn tại (kết quả có thể từ chỉ mục cũ).")); return; }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
    void openFolder(const QString &path)
    {
#ifdef Q_OS_WIN
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
    }

    QLineEdit *m_name, *m_content, *m_folder;
    QCheckBox *m_diac, *m_hidden, *m_useIdx;
    QPushButton *m_go, *m_stop, *m_build, *m_drop, *m_skipBtn;
    QTreeWidget *m_tree;
    QLabel *m_status;
    QThread *m_thread = nullptr;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QVector<filesearch::Skipped> m_skipped;
};
} // namespace

QWidget *createFileSearchWidget(QWidget *parent) { return new FileSearchWidget(parent); }
