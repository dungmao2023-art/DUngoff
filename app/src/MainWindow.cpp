// SPDX-License-Identifier: MPL-2.0
#include "MainWindow.h"

#include "DocumentTab.h"
#include "EngineHost.h"
#include "HomePage.h"
#include "Ribbon.h"
#include "Theme.h"
#include "TileView.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QHash>
#include <QTabBar>
#include "pdf/PdfToolsDialog.h"
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using dung::DocKind;

static const char* kOpenFilter =
    "Tất cả tài liệu hỗ trợ (*.xlsx *.xls *.xlsm *.ods *.csv *.docx *.doc *.odt *.rtf *.txt *.pptx *.ppt *.odp *.pdf);;"
    "Excel (*.xlsx *.xls *.xlsm *.ods *.csv);;Word (*.docx *.doc *.odt *.rtf *.txt);;"
    "PowerPoint (*.pptx *.ppt *.odp);;PDF (*.pdf)";

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("Dũng Office"));
    setAcceptDrops(true);
    resize(1280, 800);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("Central"));
    auto* h = new QHBoxLayout(central);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    h->addWidget(buildSidebar());

    m_stack = new QStackedWidget(central);
    m_home = new HomePage(m_stack);
    m_docArea = new QWidget(m_stack);
    auto* dv = new QVBoxLayout(m_docArea);
    dv->setContentsMargins(0, 0, 0, 0);
    dv->setSpacing(0);
    m_ribbon = new Ribbon(m_docArea);
    m_tabs = new QTabWidget(m_docArea);
    m_tabs->setObjectName(QStringLiteral("DocTabs"));
    m_tabs->tabBar()->setObjectName(QStringLiteral("DocTabBar"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    dv->addWidget(m_ribbon);
    dv->addWidget(m_tabs, 1);
    m_stack->addWidget(m_home);
    m_stack->addWidget(m_docArea);
    h->addWidget(m_stack, 1);
    setCentralWidget(central);

    // ---- Thanh trạng thái ----
    m_status = new QLabel(this);
    m_roLabel = new QLabel(tr("CHỈ ĐỌC"), this);
    m_roLabel->setStyleSheet(QStringLiteral("color:#7A5B00;background:#FFF3C4;border-radius:4px;font-weight:600;"));
    m_roLabel->hide();
    m_zoom = new QSlider(Qt::Horizontal, this);
    m_zoom->setRange(25, 400);
    m_zoom->setValue(100);
    m_zoom->setFixedWidth(130);
    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_zoomLabel->setMinimumWidth(48);
    statusBar()->addWidget(m_status, 1);
    statusBar()->addPermanentWidget(m_roLabel);
    statusBar()->addPermanentWidget(m_zoom);
    statusBar()->addPermanentWidget(m_zoomLabel);
    connect(m_zoom, &QSlider::valueChanged, this, [this](int z) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(z));
        if (auto* t = current()) if (t->zoom() != z) t->setZoom(z);
    });

    // ---- Kết nối ----
    connect(m_home, &HomePage::newWorkbook, this, [this] { newDoc(DocKind::Spreadsheet); });
    connect(m_home, &HomePage::newDocument, this, [this] { newDoc(DocKind::Text); });
    connect(m_home, &HomePage::newPresentation, this, [this] { newDoc(DocKind::Presentation); });
    connect(m_home, &HomePage::openPdf, this, [this] { openDialog(QStringLiteral("PDF (*.pdf)")); });
    connect(m_home, &HomePage::openFile, this, [this] { openDialog(); });
    connect(m_home, &HomePage::openPath, this, &MainWindow::openPath);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_ribbon, &Ribbon::unoCommand, this, [this](const QString& c) { if (auto* t = current()) t->runCommand(c); });
    connect(m_ribbon, &Ribbon::appAction, this, &MainWindow::onAppAction);

    // ---- Phím tắt ứng dụng ----
    auto addShortcut = [this](const QKeySequence& ks, const QString& id) {
        auto* a = new QAction(this);
        a->setShortcut(ks);
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, [this, id] { onAppAction(id); });
        addAction(a);
    };
    addShortcut(QKeySequence::Open, QStringLiteral("open"));
    addShortcut(QKeySequence::Save, QStringLiteral("save"));
    addShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), QStringLiteral("saveAs"));
    addShortcut(QKeySequence(QStringLiteral("Ctrl+W")), QStringLiteral("close"));
    addShortcut(QKeySequence(QStringLiteral("Ctrl+N")), QStringLiteral("newWorkbook"));
    addShortcut(QKeySequence::Copy, QStringLiteral("copy"));
    addShortcut(QKeySequence::Cut, QStringLiteral("cut"));
    addShortcut(QKeySequence::Paste, QStringLiteral("paste"));
    addShortcut(QKeySequence(QStringLiteral("Ctrl+=")), QStringLiteral("zoomIn"));
    addShortcut(QKeySequence(QStringLiteral("Ctrl+-")), QStringLiteral("zoomOut"));

    m_home->setRecent(recent());
    m_home->setEngineStatus(tr("Đang khởi động engine tài liệu…"));
    showHome();

    // Khởi động engine sau khi cửa sổ đã hiện (màn hình Home xuất hiện ngay).
    QTimer::singleShot(50, this, [this] {
        QApplication::setOverrideCursor(Qt::BusyCursor);
        const bool ok = EngineHost::instance().start();
        QApplication::restoreOverrideCursor();
        m_home->setEngineStatus(ok ? EngineHost::instance().versionText() + tr(" — sẵn sàng")
                                   : tr("Lỗi engine: ") + EngineHost::instance().lastError());
    });
}

QWidget* MainWindow::buildSidebar() {
    auto* side = new QWidget(this);
    side->setObjectName(QStringLiteral("Sidebar"));
    side->setAttribute(Qt::WA_StyledBackground, true);
    side->setFixedWidth(76);
    auto* v = new QVBoxLayout(side);
    v->setContentsMargins(6, 12, 6, 12);
    v->setSpacing(4);
    auto* logo = new QLabel(QStringLiteral("DŨNG"), side);
    logo->setObjectName(QStringLiteral("SideLogo"));
    logo->setAlignment(Qt::AlignCenter);
    v->addWidget(logo);
    v->addSpacing(10);

    auto* group = new QButtonGroup(side);
    auto make = [&](const QString& text, char16_t mdl2, const QString& fallback, bool checkable) {
        auto* b = new QToolButton(side);
        b->setObjectName(QStringLiteral("SideButton"));
        b->setText(theme::glyph(mdl2, fallback) + QLatin1Char('\n') + text);
        if (!theme::iconFontFamily().isEmpty()) {
            QFont f = b->font();
            f.setFamilies({QStringLiteral("Segoe UI"), theme::iconFontFamily()});
            b->setFont(f);
        }
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setCheckable(checkable);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(52);
        if (checkable) group->addButton(b);
        v->addWidget(b);
        return b;
    };
    auto* home = make(tr("Trang chủ"), 0xE80F, QStringLiteral("⌂"), true);
    auto* docs = make(tr("Tài liệu"), 0xE8A5, QStringLiteral("▤"), true);
    v->addSpacing(8);
    auto* word = make(tr("Word"), 0xE8A5, QStringLiteral("W"), false);
    auto* excel = make(tr("Excel"), 0xE80A, QStringLiteral("X"), false);
    auto* ppt = make(tr("PowerPoint"), 0xE786, QStringLiteral("P"), false);
    auto* pdf = make(tr("PDF"), 0xEA90, QStringLiteral("PDF"), false);
    v->addStretch(1);
    auto* open = make(tr("Mở tệp"), 0xE8E5, QStringLiteral("📂"), false);
    auto* about = make(tr("Giới thiệu"), 0xE946, QStringLiteral("i"), false);
    home->setChecked(true);
    word->setToolTip(tr("Tạo tài liệu Word mới"));
    excel->setToolTip(tr("Tạo sổ làm việc Excel mới"));
    ppt->setToolTip(tr("Tạo bản trình chiếu mới"));
    pdf->setToolTip(tr("Mở tệp PDF để xem"));
    connect(home, &QToolButton::clicked, this, &MainWindow::showHome);
    connect(docs, &QToolButton::clicked, this, &MainWindow::showDocuments);
    connect(word, &QToolButton::clicked, this, [this] { newDoc(DocKind::Text); });
    connect(excel, &QToolButton::clicked, this, [this] { newDoc(DocKind::Spreadsheet); });
    connect(ppt, &QToolButton::clicked, this, [this] { newDoc(DocKind::Presentation); });
    connect(pdf, &QToolButton::clicked, this, [this] { openDialog(QStringLiteral("PDF (*.pdf)")); });
    connect(open, &QToolButton::clicked, this, [this] { openDialog(); });
    connect(about, &QToolButton::clicked, this, &MainWindow::showAbout);
    side->setProperty("homeBtn", QVariant::fromValue<QObject*>(home));
    side->setProperty("docsBtn", QVariant::fromValue<QObject*>(docs));
    setProperty("sidebar", QVariant::fromValue<QObject*>(side));
    return side;
}

DocumentTab* MainWindow::current() const { return qobject_cast<DocumentTab*>(m_tabs->currentWidget()); }

void MainWindow::showHome() {
    m_home->setRecent(recent());
    m_stack->setCurrentWidget(m_home);
    if (auto* side = qobject_cast<QWidget*>(property("sidebar").value<QObject*>()))
        if (auto* b = qobject_cast<QToolButton*>(side->property("homeBtn").value<QObject*>())) b->setChecked(true);
    refreshStatus();
}

void MainWindow::showDocuments() {
    if (m_tabs->count() == 0) { showHome(); return; }
    m_stack->setCurrentWidget(m_docArea);
    if (auto* side = qobject_cast<QWidget*>(property("sidebar").value<QObject*>()))
        if (auto* b = qobject_cast<QToolButton*>(side->property("docsBtn").value<QObject*>())) b->setChecked(true);
    if (auto* t = current()) t->view()->setFocus();
    refreshStatus();
}

void MainWindow::openDialog(const QString& filter) {
    QSettings s;
    const QString dir = s.value(QStringLiteral("lastDir"), QDir::homePath()).toString();
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Mở tệp"), dir,
                                                            filter.isEmpty() ? tr(kOpenFilter) : filter);
    if (!files.isEmpty()) s.setValue(QStringLiteral("lastDir"), QFileInfo(files.first()).absolutePath());
    openFiles(files);
}

void MainWindow::openFiles(const QStringList& paths) {
    for (const QString& p : paths) openPath(p);
}

void MainWindow::openPath(const QString& path) {
    if (path.isEmpty()) return;
    // Đã mở rồi -> chuyển tới thẻ đó
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* t = qobject_cast<DocumentTab*>(m_tabs->widget(i));
        if (t && !t->document()->isNew() &&
            QFileInfo(fromPath(t->document()->originalPath())).canonicalFilePath() == QFileInfo(path).canonicalFilePath()) {
            m_tabs->setCurrentIndex(i);
            showDocuments();
            return;
        }
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    auto doc = EngineHost::instance().open(path, &err);
    QApplication::restoreOverrideCursor();
    if (!doc) {
        QMessageBox::warning(this, tr("Không mở được tệp"), err);
        return;
    }
    addRecent(path);
    addTab(new DocumentTab(std::move(doc)));
}

void MainWindow::newDoc(DocKind kind) {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    auto doc = EngineHost::instance().createNew(kind, &err);
    QApplication::restoreOverrideCursor();
    if (!doc) {
        QMessageBox::warning(this, tr("Không tạo được tài liệu"), err);
        return;
    }
    addTab(new DocumentTab(std::move(doc)));
}

void MainWindow::addTab(DocumentTab* tab) {
    const int idx = m_tabs->addTab(tab, tab->title());
    m_tabs->setTabToolTip(idx, tab->document()->isNew() ? tab->title()
                                                        : QDir::toNativeSeparators(fromPath(tab->document()->originalPath())));
    connect(tab, &DocumentTab::titleChanged, this, [this, tab] {
        const int i = m_tabs->indexOf(tab);
        if (i >= 0) m_tabs->setTabText(i, tab->title());
    });
    connect(tab, &DocumentTab::statusChanged, this, [this, tab] { if (tab == current()) refreshStatus(); });
    connect(tab, &DocumentTab::commandStateChanged, this, [this, tab](const QString& c, const QString& v) {
        if (tab == current()) m_ribbon->setCommandState(c, v);
    });
    connect(tab, &DocumentTab::savedTo, this, &MainWindow::addRecent);
    connect(tab->view(), &TileView::zoomChanged, this, [this, tab](int z) {
        if (tab == current()) { QSignalBlocker b(m_zoom); m_zoom->setValue(z); m_zoomLabel->setText(QStringLiteral("%1%").arg(z)); }
    });
    m_tabs->setCurrentIndex(idx);
    showDocuments();
    onCurrentTabChanged();
}

bool MainWindow::closeTab(int index) {
    auto* t = qobject_cast<DocumentTab*>(m_tabs->widget(index));
    if (!t) return true;
    if (!t->maybeClose()) return false;
    m_tabs->removeTab(index);
    delete t; // giải phóng tài liệu trong engine và xoá bản làm việc tạm
    if (m_tabs->count() == 0) showHome();
    return true;
}

void MainWindow::onCurrentTabChanged() {
    auto* t = current();
    if (!t) { refreshStatus(); return; }
    m_ribbon->setKind(t->kind(), t->readOnly());
    m_ribbon->resetStates(t->states());
    QSignalBlocker b(m_zoom);
    m_zoom->setValue(t->zoom());
    m_zoomLabel->setText(QStringLiteral("%1%").arg(t->zoom()));
    refreshStatus();
}

void MainWindow::refreshStatus() {
    auto* t = current();
    const bool docs = m_stack->currentWidget() == m_docArea && t;
    m_status->setText(docs ? t->statusText() : tr("Trang chủ"));
    m_roLabel->setVisible(docs && t->readOnly());
    m_zoom->setVisible(docs);
    m_zoomLabel->setVisible(docs);
    QString title = tr("Dũng Office");
    if (docs) title = t->title() + QStringLiteral(" — ") + title;
    setWindowTitle(title);
}

void MainWindow::onAppAction(const QString& id) {
    auto* t = current();
    const bool docVisible = t && m_stack->currentWidget() == m_docArea;
    if (id == QLatin1String("open")) openDialog();
    else if (id == QLatin1String("newWorkbook")) newDoc(DocKind::Spreadsheet);
    else if (id.startsWith(QLatin1String("pdfTools:"))) {
        // Công cụ PDF (app/src/pdf/): dùng tệp PDF đang mở nếu có, nếu không thì người dùng tự chọn tệp.
        static const QHash<QString, PdfToolsDialog::Tab> tabs{
            {QStringLiteral("merge"), PdfToolsDialog::Merge}, {QStringLiteral("split"), PdfToolsDialog::Split},
            {QStringLiteral("rotate"), PdfToolsDialog::Rotate}, {QStringLiteral("compress"), PdfToolsDialog::Compress},
            {QStringLiteral("info"), PdfToolsDialog::Info}};
        QString path;
        if (docVisible && t->document() && !t->document()->isNew()) path = QString::fromStdU16String(t->document()->originalPath().u16string());
        PdfToolsDialog::run(this, tabs.value(id.mid(9), PdfToolsDialog::Merge), path);
    }
    else if (!docVisible) return;
    else if (id == QLatin1String("pdfPrev")) t->stepPart(-1);
    else if (id == QLatin1String("pdfNext")) t->stepPart(1);
    else if (id == QLatin1String("save")) t->save();
    else if (id == QLatin1String("saveAs")) t->saveAs();
    else if (id == QLatin1String("exportPdf")) t->exportPdf();
    else if (id == QLatin1String("close")) closeTab(m_tabs->currentIndex());
    else if (id == QLatin1String("copy")) t->copy();
    else if (id == QLatin1String("cut")) t->cut();
    else if (id == QLatin1String("paste")) t->paste();
    else if (id == QLatin1String("zoomIn")) t->setZoom(t->zoom() + 10);
    else if (id == QLatin1String("zoomOut")) t->setZoom(t->zoom() - 10);
    else if (id == QLatin1String("zoom100")) t->setZoom(100);
}

QStringList MainWindow::recent() const { return QSettings().value(QStringLiteral("recent")).toStringList(); }

void MainWindow::addRecent(const QString& path) {
    QStringList r = recent();
    r.removeAll(path);
    r.prepend(path);
    while (r.size() > 15) r.removeLast();
    QSettings().setValue(QStringLiteral("recent"), r);
}

void MainWindow::showAbout() {
    QMessageBox::about(this, tr("Giới thiệu Dũng Office"),
        tr("<h3>Dũng Office %1</h3>"
           "<p>Bộ ứng dụng văn phòng miễn phí, ngoại tuyến, không cần tài khoản.</p>"
           "<p><b>Nguyên tắc dữ liệu:</b> tệp gốc không bao giờ bị ghi đè mặc định; mọi thay đổi được lưu "
           "vào tệp mới (Lưu thành). Định dạng chưa bảo toàn được sẽ mở ở chế độ chỉ đọc.</p>"
           "<p>Xử lý tài liệu dựa trên các module của LibreOffice (MPL-2.0) qua LibreOfficeKit.<br>%2<br>"
           "Runtime: %3</p>"
           "<p>Dũng Office không liên kết với The Document Foundation. LibreOffice là nhãn hiệu của "
           "The Document Foundation.</p>")
            .arg(QStringLiteral(DUNG_VERSION), EngineHost::instance().versionText().toHtmlEscaped(),
                 QDir::toNativeSeparators(EngineHost::instance().runtimeDir()).toHtmlEscaped()));
}

void MainWindow::closeEvent(QCloseEvent* e) {
    while (m_tabs->count() > 0) {
        m_tabs->setCurrentIndex(0);
        showDocuments();
        if (!closeTab(0)) { e->ignore(); return; }
    }
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    QStringList files;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile()) files << u.toLocalFile();
    openFiles(files);
}
