// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "MainWindow.h"
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <functional>
#include "ppt/PptActions.h"

#include "DocumentTab.h"
#include "EngineHost.h"
#include "recovery/RecoveryDialog.h"
#include "recovery/RecoveryStore.h"
#include "word/WordActions.h"
#include "HomePage.h"
#include "extras/ProjectsPage.h"
#include "extras/SidePanel.h"
#include "templates/TemplateLibrary.h"
#include "templates/TemplatesPage.h"
#include "toolbox/ToolboxPage.h"
#include "DataAssistWidgets.h"
#include "ExcelBridge.h"
#include "Ribbon.h"
#include "Theme.h"
#include "TileView.h"
#include "shell/QuickPalette.h"
#include "shell/ShellIcons.h"
#include "shell/TopBar.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColor>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QFrame>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QHash>
#include <QTabBar>
#include <QTemporaryDir>
#include <QComboBox>
#include "pdf/PdfToolsDialog.h"
#include "tableutil/TableToolsDialog.h"
#include <QTabWidget>
#include <QTimer>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

using dung::DocKind;

static const char* kOpenFilter =
    "Tất cả tài liệu hỗ trợ (*.xlsx *.xls *.xlsm *.ods *.csv *.docx *.doc *.odt *.rtf *.txt *.pptx *.ppt *.odp *.pdf *.odg);;"
    "Excel (*.xlsx *.xls *.xlsm *.ods *.csv);;Word (*.docx *.doc *.odt *.rtf *.txt);;"
    "PowerPoint (*.pptx *.ppt *.odp);;PDF / bản vẽ (*.pdf *.odg)";

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

    auto* right = new QWidget(central);
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    m_top = new shell::TopBar(right);
    rv->addWidget(m_top);
    auto* content = new QWidget(right);
    auto* ch = new QHBoxLayout(content);
    ch->setContentsMargins(0, 0, 0, 0);
    ch->setSpacing(0);
    rv->addWidget(content, 1);
    h->addWidget(right, 1);

    m_stack = new QStackedWidget(content);
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
    m_tabs->tabBar()->setElideMode(Qt::ElideRight); // tên dài cắt phần cuối, giữ phần đầu
    m_tabs->tabBar()->setUsesScrollButtons(true);
    auto* groupRow = new QHBoxLayout;
    groupRow->setContentsMargins(8, 3, 8, 3);
    groupRow->addWidget(new QLabel(tr("Tài liệu:"), m_docArea));
    m_docGroup = new QComboBox(m_docArea);
    for (const auto& name : {tr("Word"), tr("Excel"), tr("PowerPoint"), tr("PDF"), tr("Khác"), tr("Tất cả")}) m_docGroup->addItem(name);
    groupRow->addWidget(m_docGroup); groupRow->addStretch(1);
    dv->addLayout(groupRow);
    dv->addWidget(m_ribbon);
    dv->addWidget(m_tabs, 1);
    connect(m_docGroup, &QComboBox::currentIndexChanged, this, [this](int group) {
        if (m_groupChanging) return;
        int match = -1;
        if (group == 5) { refreshDocumentGroups(); return; }
        for (int n = 0; n < m_tabs->count(); ++n) {
            auto* tab = qobject_cast<DocumentTab*>(m_tabs->widget(n));
            if (!tab) continue;
            const auto k = tab->kind();
            const int rank = k == DocKind::Text ? 0 : k == DocKind::Spreadsheet ? 1 : k == DocKind::Presentation ? 2 : k == DocKind::Drawing ? 3 : 4;
            if (rank == group) { match = n; break; }
        }
        if (match >= 0) selectDocumentTab(match);
        else {
            QSignalBlocker block(m_docGroup); m_docGroup->setCurrentIndex(5); refreshDocumentGroups();
            statusBar()->showMessage(tr("Chưa có tài liệu thuộc nhóm này."), 4000);
        }
    });
    m_stack->addWidget(m_home);
    m_stack->addWidget(m_docArea);
    m_projects = new ProjectsPage(m_stack);
    m_stack->addWidget(m_projects);
    tpl::seedFromBundled(QCoreApplication::applicationDirPath()); // cài/cập nhật gói mẫu đi kèm bộ cài (nếu mới hơn)
    m_templates = new TemplatesPage(m_stack);
    m_stack->addWidget(m_templates);
    connect(m_templates, &TemplatesPage::openPath, this, &MainWindow::openPath);
    m_toolbox = new ToolboxPage(m_stack);
    m_stack->addWidget(m_toolbox);
    // Nối tiện ích dữ liệu (Hộp công cụ) với tab Excel đang mở. Ghi đè ô có dữ liệu phải được xác nhận; engine hoàn tác được (Ctrl+Z).
    dataassist::hooks().selectedRangeText = [this]() -> QString {
        DocumentTab* t = current();
        if (!t || t->kind() != dung::DocKind::Spreadsheet) return {};
        return QString::fromStdString(dataassist::bridgeSelectedText(*t->document()));
    };
    dataassist::hooks().insertFormula = [this](const QString& f, QString* res, QString* msg) -> bool {
        DocumentTab* t = current();
        if (!t || t->kind() != dung::DocKind::Spreadsheet) { if (msg) *msg = tr("Chưa có tab Excel nào đang mở — mở một bảng tính rồi chọn ô cần chèn."); return false; }
        const auto o = dataassist::bridgeInsertFormula(*t->document(), f.toStdString(), [this](const std::string& old) {
            return QMessageBox::question(this, tr("Ghi đè ô đang có dữ liệu?"),
                                         tr("Ô đang chọn có nội dung:\n\n%1\n\nChèn công thức sẽ thay thế nội dung này (có thể hoàn tác bằng Ctrl+Z). Tiếp tục?").arg(QString::fromStdString(old).left(200)),
                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
        }, 250);
        if (!o.ok) { if (msg) *msg = QString::fromStdString(o.message); return false; }
        if (res) *res = QString::fromStdString(o.resultText);
        return true;
    };
    dataassist::hooks().selectedCellAddress = [this]() -> QString {
        DocumentTab* t = current();
        if (!t || t->kind() != dung::DocKind::Spreadsheet) return {};
        return QString::fromStdString(dataassist::bridgeCellAddress(t->document()->lastPayload(LOK_CALLBACK_CELL_ADDRESS)));
    };
    dataassist::hooks().insertFormulaAt = [this](const QString& target, const QString& f, QString* res, QString* msg) -> bool {
        DocumentTab* t = current();
        if (!t || t->kind() != dung::DocKind::Spreadsheet || t->readOnly()) {
            if (msg) *msg = tr("Cần mở một bảng tính có thể chỉnh sửa."); return false;
        }
        const auto addr = dataassist::bridgeCellAddress(target.toStdString());
        const auto out = dataassist::bridgeInsertFormulaAt(*t->document(), addr, f.toStdString(), LOK_CALLBACK_CELL_ADDRESS,
            [this, t, addr](const std::string& old) {
                const auto sheet = QString::fromStdString(t->document()->partName(t->document()->currentPart()));
                return QMessageBox::question(this, tr("Xác nhận ô kết quả"),
                    tr("Ghi công thức vào %1 — %2?\nGiá trị hiển thị trước khi ghi: %3\nCó thể thay thế công thức đang có. Hoàn tác bằng Ctrl+Z.")
                        .arg(sheet, QString::fromStdString(addr), QString::fromStdString(old).left(200)),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
            });
        if (!out.ok) { if (msg) *msg = QString::fromStdString(out.message); return false; }
        if (res) *res = QString::fromStdString(out.resultText);
        return true;
    };
    ch->addWidget(m_stack, 1);
    m_side = new SidePanel(content);
    ch->addWidget(m_side);
    m_side->setVisible(QSettings().value(QStringLiteral("ui/workPanel"), false).toBool());
    setCentralWidget(central);

    // ---- Thanh trạng thái ----
    m_status = new QLabel(this);
    m_roLabel = new QLabel(tr("CHỈ ĐỌC"), this);
    m_roLabel->setStyleSheet(QStringLiteral("color:#7A5B00;background:#FFF3C4;border-radius:4px;font-weight:600;"));
    m_roLabel->hide();
    m_toolLabel = new QLabel(this);
    m_toolLabel->setStyleSheet(QStringLiteral("color:#1F50BB;background:#EAF0FF;border-radius:4px;padding:1px 8px;font-weight:700;"));
    m_toolLabel->hide();
    m_saveLabel = new QLabel(this);
    m_saveLabel->hide();
    m_zoom = new QSlider(Qt::Horizontal, this);
    m_zoom->setRange(25, 400);
    m_zoom->setValue(100);
    m_zoom->setFixedWidth(130);
    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_zoomLabel->setMinimumWidth(48);
    statusBar()->addWidget(m_toolLabel);
    statusBar()->addWidget(m_status, 1);
    statusBar()->addPermanentWidget(m_saveLabel);
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
    connect(m_home, &HomePage::openTemplates, this, &MainWindow::showTemplates);
    connect(m_home, &HomePage::openToolbox, this, &MainWindow::showToolbox);
    connect(m_top, &shell::TopBar::tabClicked, this, &MainWindow::activateKind);
    connect(m_top, &shell::TopBar::searchRequested, this, &MainWindow::showPalette);
    connect(m_top, &shell::TopBar::quickLinkClicked, this, &MainWindow::runCommand);
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
    addShortcut(QKeySequence(QStringLiteral("Ctrl+K")), QStringLiteral("palette"));
    addShortcut(QKeySequence::Open, QStringLiteral("open"));
    addShortcut(QKeySequence::Save, QStringLiteral("save"));
    addShortcut(QKeySequence::Print, QStringLiteral("print"));
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
    m_top->setReadyText(tr("Đang khởi động…"));
    showHome();

    // Khởi động engine sau khi cửa sổ đã hiện (màn hình Home xuất hiện ngay).
    if (qEnvironmentVariableIsSet("DUNG_NO_ENGINE")) return; // chế độ chụp giao diện: không cần engine
    QTimer::singleShot(50, this, [this] {
        QApplication::setOverrideCursor(Qt::BusyCursor);
        const bool ok = EngineHost::instance().start();
        QApplication::restoreOverrideCursor();
        m_home->setEngineStatus(ok ? EngineHost::instance().versionText() + tr(" — sẵn sàng")
                                   : tr("Lỗi engine: ") + EngineHost::instance().lastError());
        m_top->setReadyText(ok ? tr("Sẵn sàng") : tr("Engine lỗi"));
        if (ok) QTimer::singleShot(0, this, [this] { offerRecovery(); });
    });
}

void MainWindow::offerRecovery() {
    recovery::pruneOld();
    const QList<recovery::Entry> orphans = recovery::findOrphans();
    if (orphans.isEmpty()) return;
    RecoveryDialog dlg(orphans, this);
    if (dlg.exec() != QDialog::Accepted) return;  // "Để sau": giữ nguyên, hỏi lại lần sau
    for (const auto& e : dlg.toDiscard()) recovery::discardEntry(e);
    for (const auto& e : dlg.toOpen()) {
        QString err;
        auto doc = EngineHost::instance().open(e.dataPath, &err);
        if (!doc) {
            QMessageBox::warning(this, tr("Không khôi phục được"),
                tr("Không mở được bản phục hồi của \"%1\".\nBản này vẫn được giữ lại.\n\n%2").arg(e.displayName, err));
            continue;
        }
        doc->setRecoveredOrigin(toPath(e.originalPath), ss(e.displayName));
        doc->markModified();
        auto* tab = new DocumentTab(std::move(doc));
        tab->adoptRecovered(e);
        addTab(tab);
    }
}

QWidget* MainWindow::buildSidebar() {
    auto* side = m_sideBar = new QWidget(this);
    side->setObjectName(QStringLiteral("Sidebar"));
    side->setAttribute(Qt::WA_StyledBackground, true);
    side->setFixedWidth(241);
    auto* v = new QVBoxLayout(side);
    v->setContentsMargins(12, 16, 12, 12);
    v->setSpacing(0);

    // ---- Logo ----
    auto* head = new QWidget(side);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(6, 0, 0, 0);
    hl->setSpacing(10);
    auto* logo = new QLabel(head);
    logo->setPixmap(shell::badge(QStringLiteral("D"), theme::kBlue, Qt::white, 42, devicePixelRatioF()));
    logo->setFixedSize(42, 42);
    hl->addWidget(logo);
    auto* names = new QWidget(head);
    auto* nl = new QVBoxLayout(names);
    nl->setContentsMargins(0, 0, 0, 0);
    nl->setSpacing(0);
    auto* brand = new QLabel(tr("Dũng<span style='color:#2864BA;font-weight:400'>Office</span>"), names);
    brand->setObjectName(QStringLiteral("SideBrand"));
    brand->setTextFormat(Qt::RichText);
    auto* sub = new QLabel(tr("FREE · VIỆT NAM"), names);
    sub->setObjectName(QStringLiteral("SideSub"));
    nl->addWidget(brand);
    nl->addWidget(sub);
    hl->addWidget(names, 1);
    m_navFullOnly << names;
    v->addWidget(head);
    v->addSpacing(8);

    // ---- Danh sách mục (cuộn được khi cửa sổ thấp) ----
    auto* scroll = new QScrollArea(side);
    scroll->setObjectName(QStringLiteral("SideScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("SideScrollBody"));
    scroll->setWidget(body);
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(0, 0, 0, 0);
    bv->setSpacing(2);
    v->addWidget(scroll, 1);

    auto* group = new QButtonGroup(side);
    auto section = [&](const QString& text) {
        auto* l = new QLabel(text, body);
        l->setObjectName(QStringLiteral("SideSection"));
        bv->addWidget(l);
        m_navFullOnly << l;
    };
    // Mọi biểu tượng của thanh bên đi qua shell::badge / shell::symbol (ShellIcons.h).
    auto make = [&](const QString& text, const QPixmap& pm, bool checkable) {
        auto* b = new QPushButton(text, body);
        b->setObjectName(QStringLiteral("NavItem"));
        b->setIcon(shell::icon(pm));
        b->setIconSize(QSize(28, 28));
        b->setCheckable(checkable);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
        b->setProperty("fullText", text);
        b->setToolTip(text);
        if (checkable) group->addButton(b);
        bv->addWidget(b);
        m_navButtons << b;
        return b;
    };
    const qreal dpr = devicePixelRatioF();
    const QColor soft(0xE8, 0xF0, 0xFF);
    auto sym = [&](const char* n) { return shell::symbol(QLatin1String(n), theme::kBlue, soft, 30, dpr); };
    auto app = [&](const QString& l, const QColor& c) { return shell::badge(l, c, Qt::white, 30, dpr); };

    section(tr("KHÔNG GIAN LÀM VIỆC"));
    auto* home = make(tr("Trang chủ"), sym("home"), true);
    auto* docs = make(tr("Tài liệu đang mở"), sym("docs"), true);
    auto* word = make(tr("Soạn thảo Word"), app(QStringLiteral("W"), theme::kWord), false);
    auto* excel = make(tr("Bảng tính Excel"), app(QStringLiteral("X"), theme::kExcel), false);
    auto* ppt = make(tr("PowerPoint"), app(QStringLiteral("P"), theme::kPowerPoint), false);
    auto* pdf = make(tr("Công cụ PDF"), app(QStringLiteral("PDF"), theme::kPdf), false);
    section(tr("LỊCH & CÔNG VIỆC"));
    auto* work = make(tr("Công việc · Ghi chú"), sym("work"), false);
    work->setCheckable(true);
    work->setChecked(QSettings().value(QStringLiteral("ui/workPanel"), false).toBool()); // mặc định ẩn như thiết kế
    work->setToolTip(tr("Hiện/ẩn bảng Ghi chú · Lịch · Liên hệ bên phải"));
    section(tr("TẬP TIN & DỰ ÁN"));
    auto* tmpl = make(tr("Kho mẫu"), sym("templates"), true);
    auto* proj = make(tr("Dự án"), sym("projects"), true);
    auto* tools = make(tr("Công cụ"), sym("tools"), true);
    auto* open = make(tr("Mở tệp"), sym("open"), false);
    auto* about = make(tr("Giới thiệu"), sym("about"), false);
    bv->addStretch(1);

    // ---- Thẻ riêng tư ----
    auto* card = new QFrame(side);
    card->setObjectName(QStringLiteral("PrivacyCard"));
    auto* cv = new QVBoxLayout(card);
    cv->setContentsMargins(12, 10, 12, 10);
    auto* ct = new QLabel(tr("<b>Ngoại tuyến · Riêng tư</b><br>Tài liệu ở lại trên máy bạn. Không tài khoản, không gửi lên mạng."), card);
    ct->setWordWrap(true);
    ct->setTextFormat(Qt::RichText);
    cv->addWidget(ct);
    v->addSpacing(8);
    v->addWidget(card);
    m_navFullOnly << card;

    connect(work, &QPushButton::toggled, this, [this](bool on) {
        m_side->setVisible(on);
        QSettings().setValue(QStringLiteral("ui/workPanel"), on);
    });
    home->setChecked(true);
    word->setToolTip(tr("Tạo tài liệu Word mới"));
    excel->setToolTip(tr("Tạo sổ làm việc Excel mới"));
    ppt->setToolTip(tr("Tạo bản trình chiếu mới"));
    pdf->setToolTip(tr("Mở tệp PDF để xem và dùng công cụ PDF"));
    connect(home, &QPushButton::clicked, this, &MainWindow::showHome);
    connect(docs, &QPushButton::clicked, this, &MainWindow::showDocuments);
    connect(proj, &QPushButton::clicked, this, &MainWindow::showProjects);
    connect(tmpl, &QPushButton::clicked, this, &MainWindow::showTemplates);
    connect(tools, &QPushButton::clicked, this, &MainWindow::showToolbox);
    connect(word, &QPushButton::clicked, this, [this] { newDoc(DocKind::Text); });
    connect(excel, &QPushButton::clicked, this, [this] { newDoc(DocKind::Spreadsheet); });
    connect(ppt, &QPushButton::clicked, this, [this] { newDoc(DocKind::Presentation); });
    connect(pdf, &QPushButton::clicked, this, [this] { openDialog(QStringLiteral("PDF (*.pdf)")); });
    connect(open, &QPushButton::clicked, this, [this] { openDialog(); });
    connect(about, &QPushButton::clicked, this, &MainWindow::showAbout);
    side->setProperty("homeBtn", QVariant::fromValue<QObject*>(home));
    side->setProperty("docsBtn", QVariant::fromValue<QObject*>(docs));
    side->setProperty("projBtn", QVariant::fromValue<QObject*>(proj));
    side->setProperty("tmplBtn", QVariant::fromValue<QObject*>(tmpl));
    side->setProperty("toolBtn", QVariant::fromValue<QObject*>(tools));
    side->setProperty("workBtn", QVariant::fromValue<QObject*>(work));
    setProperty("sidebar", QVariant::fromValue<QObject*>(side));
    return side;
}

void MainWindow::applyCompact(bool compact) {
    if (compact == m_compact) return;
    m_compact = compact;
    m_sideBar->setFixedWidth(compact ? 76 : 241);
    m_sideBar->setProperty("compact", compact);
    for (QWidget* w : m_navFullOnly) w->setVisible(!compact);
    for (QPushButton* b : m_navButtons) {
        b->setText(compact ? QString() : b->property("fullText").toString());
        b->setIconSize(QSize(30, 30));
    }
    m_top->setCompact(compact);
    m_sideBar->style()->unpolish(m_sideBar);
    m_sideBar->style()->polish(m_sideBar);
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    applyCompact(width() < 840);
}

void MainWindow::showPalette() {
    if (!m_palette) {
        m_palette = new shell::QuickPalette(this);
        connect(m_palette, &shell::QuickPalette::triggered, this, &MainWindow::runCommand);
    }
    QList<shell::PaletteEntry> e;
    auto add = [&](const char* id, const QString& t, const QString& hint = QString()) { e.append({QString::fromLatin1(id), t, hint}); };
    add("home", tr("Trang chủ"));
    add("docs", tr("Tài liệu đang mở"));
    add("newDoc", tr("Tài liệu Word mới"));
    add("newXlsx", tr("Sổ làm việc Excel mới"), QStringLiteral("Ctrl+N"));
    add("newPpt", tr("Bản trình chiếu mới"));
    add("openPdf", tr("Mở tệp PDF"));
    add("open", tr("Mở tệp…"), QStringLiteral("Ctrl+O"));
    add("save", tr("Lưu"), QStringLiteral("Ctrl+S"));
    add("saveAs", tr("Lưu thành bản sao…"), QStringLiteral("Ctrl+Shift+S"));
    add("exportPdf", tr("Xuất PDF"));
    add("print", tr("In / Xem trước"), QStringLiteral("Ctrl+P"));
    add("close", tr("Đóng tài liệu"), QStringLiteral("Ctrl+W"));
    add("templates", tr("Kho mẫu"));
    add("tools", tr("Công cụ"));
    add("projects", tr("Dự án"));
    add("work", tr("Công việc · Ghi chú · Lịch · Liên hệ"));
    add("zoomIn", tr("Phóng to"), QStringLiteral("Ctrl+="));
    add("zoomOut", tr("Thu nhỏ"), QStringLiteral("Ctrl+-"));
    add("about", tr("Giới thiệu Dũng Office"));
    m_palette->setEntries(e);
    m_palette->popup();
}

void MainWindow::runCommand(const QString& id) {
    auto sideBtn = [this](const char* prop) -> QPushButton* {
        return qobject_cast<QPushButton*>(m_sideBar->property(prop).value<QObject*>());
    };
    if (id == QLatin1String("home")) showHome();
    else if (id == QLatin1String("docs")) showDocuments();
    else if (id == QLatin1String("templates")) showTemplates();
    else if (id == QLatin1String("tools")) showToolbox();
    else if (id == QLatin1String("projects")) showProjects();
    else if (id == QLatin1String("work")) { if (auto* b = sideBtn("workBtn")) b->setChecked(!b->isChecked()); }
    else if (id == QLatin1String("newDoc")) newDoc(DocKind::Text);
    else if (id == QLatin1String("newXlsx")) newDoc(DocKind::Spreadsheet);
    else if (id == QLatin1String("newPpt")) newDoc(DocKind::Presentation);
    else if (id == QLatin1String("openPdf")) openDialog(QStringLiteral("PDF (*.pdf)"));
    else if (id == QLatin1String("about")) showAbout();
    else onAppAction(id);
}

void MainWindow::activateKind(const QString& kind) {
    if (kind == QLatin1String("home")) { showHome(); return; }
    DocKind want = DocKind::Text;
    if (kind == QLatin1String("excel")) want = DocKind::Spreadsheet;
    else if (kind == QLatin1String("ppt")) want = DocKind::Presentation;
    else if (kind == QLatin1String("pdf")) want = DocKind::Drawing;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* t = qobject_cast<DocumentTab*>(m_tabs->widget(i));
        if (t && t->document()->kind() == want) {
            selectDocumentTab(i);
            showDocuments();
            return;
        }
    }
    // Chưa có tài liệu loại này: về Trang chủ và nói rõ, KHÔNG tự tạo tài liệu.
    showHome();
    statusBar()->showMessage(tr("Chưa mở tài liệu %1 nào — chọn một thẻ ở Trang chủ để tạo hoặc mở.").arg(
        kind == QLatin1String("word") ? QStringLiteral("Word") : kind == QLatin1String("excel") ? QStringLiteral("Excel")
        : kind == QLatin1String("ppt") ? QStringLiteral("PowerPoint") : QStringLiteral("PDF")), 6000);
}

DocumentTab* MainWindow::current() const { return qobject_cast<DocumentTab*>(m_tabs->currentWidget()); }

void MainWindow::showHome() {
    m_home->setRecent(recent());
    m_stack->setCurrentWidget(m_home);
    if (auto* side = qobject_cast<QWidget*>(property("sidebar").value<QObject*>()))
        if (auto* b = qobject_cast<QPushButton*>(side->property("homeBtn").value<QObject*>())) b->setChecked(true);
    refreshStatus();
}

void MainWindow::showTemplates() {
    m_templates->reload();
    m_stack->setCurrentWidget(m_templates);
    if (auto* side = qobject_cast<QWidget*>(property("sidebar").value<QObject*>()))
        if (auto* b = qobject_cast<QPushButton*>(side->property("tmplBtn").value<QObject*>())) b->setChecked(true);
    refreshStatus();
}

void MainWindow::showToolbox() {
    m_toolbox->reload();
    m_stack->setCurrentWidget(m_toolbox);
    if (auto* side = qobject_cast<QWidget*>(property("sidebar").value<QObject*>()))
        if (auto* b = qobject_cast<QPushButton*>(side->property("toolBtn").value<QObject*>())) b->setChecked(true);
    refreshStatus();
}

void MainWindow::showProjects() {
    m_stack->setCurrentWidget(m_projects);
    if (auto* side = qobject_cast<QWidget*>(property("sidebar").value<QObject*>()))
        if (auto* b = qobject_cast<QPushButton*>(side->property("projBtn").value<QObject*>())) b->setChecked(true);
    refreshStatus();
}

void MainWindow::showDocuments() {
    if (m_tabs->count() == 0) { showHome(); return; }
    m_stack->setCurrentWidget(m_docArea);
    if (auto* side = qobject_cast<QWidget*>(property("sidebar").value<QObject*>()))
        if (auto* b = qobject_cast<QPushButton*>(side->property("docsBtn").value<QObject*>())) b->setChecked(true);
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
            selectDocumentTab(i);
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

namespace {
// Thứ tự nhóm tab: Word, Excel, PowerPoint, PDF, khác. Tab mới chèn vào cuối nhóm cùng loại.
int kindRank(dung::DocKind k) {
    switch (k) {
    case DocKind::Text: return 0;
    case DocKind::Spreadsheet: return 1;
    case DocKind::Presentation: return 2;
    case DocKind::Drawing: return 3;
    default: return 4;
    }
}
QColor kindColor(dung::DocKind k) {
    switch (k) {
    case DocKind::Text: return theme::kWord;
    case DocKind::Spreadsheet: return theme::kExcel;
    case DocKind::Presentation: return theme::kPowerPoint;
    case DocKind::Drawing: return theme::kPdf;
    default: return QColor(0x44, 0x44, 0x44);
    }
}
}  // namespace

void MainWindow::addTab(DocumentTab* tab) {
    const int rank = kindRank(tab->document()->kind());
    int pos = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* t = qobject_cast<DocumentTab*>(m_tabs->widget(i));
        if (t && kindRank(t->document()->kind()) <= rank) pos = i + 1;
    }
    const int idx = m_tabs->insertTab(pos, tab, tab->title());
    m_tabs->tabBar()->setTabTextColor(idx, kindColor(tab->document()->kind()));
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
    connect(tab, &DocumentTab::templateCopyRequested, this, &MainWindow::openPath);
    // Bản sao có lề mới (Word, Windows): mở trong thẻ mới, thẻ cũ giữ nguyên.
    connect(tab, &DocumentTab::openCopyRequested, this, [this](const QString& path, const QString& original, const QString& name) {
        QString err;
        auto doc = EngineHost::instance().open(path, &err);
        if (!doc) {
            QMessageBox::warning(this, tr("Lề trang"),
                tr("Không mở được bản sao có lề mới.\n%1\n\nTài liệu hiện tại và tệp gốc không bị đổi.").arg(err));
            return;
        }
        doc->setRecoveredOrigin(toPath(original), ss(name));
        doc->markModified();
        auto* nt = new DocumentTab(std::move(doc));
        nt->setCopyOrigin(original);
        addTab(nt);
    });
    connect(tab->view(), &TileView::zoomChanged, this, [this, tab](int z) {
        if (tab == current()) { QSignalBlocker b(m_zoom); m_zoom->setValue(z); m_zoomLabel->setText(QStringLiteral("%1%").arg(z)); }
    });
    selectDocumentTab(idx);
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
    else if (!current() || !m_tabs->isTabVisible(m_tabs->currentIndex())) selectDocumentTab(0);
    else onCurrentTabChanged();
    return true;
}

void MainWindow::selectDocumentTab(int index) {
    if (index < 0 || index >= m_tabs->count()) return;
    m_groupChanging = true;
    m_tabs->setTabVisible(index, true);
    m_tabs->setCurrentIndex(index);
    m_groupChanging = false;
    onCurrentTabChanged();
}
void MainWindow::refreshDocumentGroups() {
    if (!m_docGroup || m_groupChanging) return;
    m_groupChanging = true;
    const int group = m_docGroup->currentIndex();
    for (int n = 0; n < m_tabs->count(); ++n) {
        auto* tab = qobject_cast<DocumentTab*>(m_tabs->widget(n));
        m_tabs->setTabVisible(n, group == 5 || n == m_tabs->currentIndex() || (tab && kindRank(tab->kind()) == group));
    }
    m_groupChanging = false;
}
void MainWindow::onCurrentTabChanged() {
    if (m_groupChanging) return;
    auto* t = current();
    if (t && m_docGroup && m_docGroup->currentIndex() != 5) {
        QSignalBlocker b(m_docGroup);
        m_docGroup->setCurrentIndex(kindRank(t->kind()));
    }
    refreshDocumentGroups();
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
    {
        QString k = QStringLiteral("home");
        if (docs) {
            switch (t->document()->kind()) {
            case DocKind::Text: k = QStringLiteral("word"); break;
            case DocKind::Spreadsheet: k = QStringLiteral("excel"); break;
            case DocKind::Presentation: k = QStringLiteral("ppt"); break;
            case DocKind::Drawing: k = QStringLiteral("pdf"); break;
            default: k.clear(); break;
            }
        } else if (m_stack->currentWidget() != m_home) k.clear();
        m_top->setActive(k);
    }
    m_status->setText(docs ? t->statusText() : (m_stack->currentWidget() == m_projects ? tr("Dự án") : m_stack->currentWidget() == m_templates ? tr("Thư viện mẫu") : m_stack->currentWidget() == m_toolbox ? tr("Công cụ") : tr("Trang chủ")));
    m_toolLabel->setVisible(docs);
    m_saveLabel->setVisible(docs);
    if (docs) {
        m_toolLabel->setText(QString::fromUtf8(dung::kindName(t->document()->kind())));
        if (t->readOnly()) m_saveLabel->setText(tr("Chỉ đọc"));
        else if (t->document()->modified()) m_saveLabel->setText(tr("<span style='color:#AF791D'>● Chưa lưu</span>"));
        else m_saveLabel->setText(tr("<span style='color:#21866E'>✓ Đã lưu</span>"));
        const auto& fl = t->document()->featureLoss();
        if (!fl.empty()) {
            m_saveLabel->setText(m_saveLabel->text() +
                tr(" · <span style='color:#AF791D'>⚠ Lưu sẽ mất %1 loại thành phần</span>").arg(fl.size()));
            m_saveLabel->setToolTip(QString::fromStdString(dung::describeFeatureLoss(fl)));
        } else {
            m_saveLabel->setToolTip(QString());
        }
        m_saveLabel->setTextFormat(Qt::RichText);
    }
    if (docs) m_top->setSaveText(t->readOnly() ? tr("● Chỉ đọc") : t->document()->modified() ? tr("● Chưa lưu") : tr("● Đã lưu"));
    else m_top->setSaveText(tr("● Lưu trên máy bạn"));
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
    if (id == QLatin1String("palette")) showPalette();
    else if (id == QLatin1String("open")) openDialog();
    else if (id == QLatin1String("newWorkbook")) newDoc(DocKind::Spreadsheet);
    else if (id == QLatin1String("pdfEditCopy")) {
        if (!docVisible || t->kind() != DocKind::Drawing || !t->readOnly()) return;
        const QString original = fromPath(t->document()->originalPath());
        if (!original.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) return;
        if (QMessageBox::warning(this, tr("Tạo bản sao PDF chỉnh sửa"),
            tr("Nhập PDF thành bản vẽ ODG có thể sửa chữ/hình mà engine nhận diện. "
               "Font, bố cục, biểu mẫu, chữ ký, liên kết và các lớp có thể thay đổi hoặc mất. "
               "Ảnh quét vẫn là ảnh (không tự OCR). PDF gốc không bị sửa. "
               "Sau khi sửa, lưu ODG hoặc xuất PDF tên mới và xem trước để đối chiếu."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        QTemporaryDir tmp;
        if (!tmp.isValid()) { QMessageBox::warning(this, tr("PDF"), tr("Không tạo được thư mục tạm.")); return; }
        const QString path = tmp.filePath(QStringLiteral("editable.odg"));
        const int count = t->document()->partCount();
        std::string err;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const bool saved = t->document()->saveRecoveryCopy(toPath(path), &err);
        QString openError;
        auto copy = saved ? EngineHost::instance().open(path, &openError) : std::unique_ptr<dung::Document>();
        QApplication::restoreOverrideCursor();
        if (!copy || copy->kind() != DocKind::Drawing || copy->readOnly() || count < 1 || copy->partCount() != count) {
            QMessageBox::warning(this, tr("PDF"), tr("Không tạo được bản sao chỉnh sửa có cùng số trang. %1").arg(saved ? openError : qs(err)));
            return;
        }
        copy->setRecoveredOrigin(toPath(original), QFileInfo(original).completeBaseName().append(tr(" - chỉnh sửa.odg")).toStdString());
        auto* tab = new DocumentTab(std::move(copy));
        tab->setCopyOrigin(original);
        addTab(tab);
    }
    else if (id.startsWith(QLatin1String("pdfTools:"))) {
        // Công cụ PDF (app/src/pdf/): dùng tệp PDF đang mở nếu có, nếu không thì người dùng tự chọn tệp.
        static const QHash<QString, PdfToolsDialog::Tab> tabs{
            {QStringLiteral("merge"), PdfToolsDialog::Merge}, {QStringLiteral("split"), PdfToolsDialog::Split},
            {QStringLiteral("rotate"), PdfToolsDialog::Rotate}, {QStringLiteral("compress"), PdfToolsDialog::Compress},
            {QStringLiteral("info"), PdfToolsDialog::Info},
            {QStringLiteral("number"), PdfToolsDialog::Number}, {QStringLiteral("stamp"), PdfToolsDialog::Stamp},
            {QStringLiteral("images"), PdfToolsDialog::Images}, {QStringLiteral("blank"), PdfToolsDialog::Blank},
            {QStringLiteral("reorder"), PdfToolsDialog::Reorder}, {QStringLiteral("ocr"), PdfToolsDialog::Ocr}};
        QString path;
        if (docVisible && t->kind() == DocKind::Drawing && !t->document()->isNew()) {
            const QString p = fromPath(t->document()->originalPath());
            if (p.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) path = p;
        }
        PdfToolsDialog::run(this, tabs.value(id.mid(9), PdfToolsDialog::Merge), path);
    }
    else if (id.startsWith(QLatin1String("tableTools:"))) {
        // Tiện ích bảng (app/src/tableutil/): làm việc trên CSV, luôn ra bản sao. Dùng tệp CSV đang mở nếu có.
        const QString k = id.mid(11);
        const auto tab = k == QLatin1String("clean") ? TableToolsDialog::Clean : k == QLatin1String("compare") ? TableToolsDialog::Compare : TableToolsDialog::Dedup;
        QString path;
        if (docVisible && t->document() && !t->document()->isNew()) {
            const QString p = QString::fromStdU16String(t->document()->originalPath().u16string());
            if (p.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) path = p;
        }
        TableToolsDialog::run(this, tab, path);
    }
    else if (!docVisible) return;
    else if (handlePptAction(t, id)) {}
    else if (id == QLatin1String("pdfPrev")) t->stepPart(-1);
    else if (id == QLatin1String("pdfNext")) t->stepPart(1);
    else if (id == QLatin1String("xl:formulaAssistant")) t->showExcelTools(0);
    else if (id == QLatin1String("xl:rangeTools")) t->showExcelTools(1);
    else if (id.startsWith(QLatin1String("xl:"))) t->excelAction(id); // khối Excel: app/src/excel/
    else if (id == QLatin1String("save")) t->save();
    else if (id == QLatin1String("saveAs")) t->saveAs();
    else if (id == QLatin1String("exportPdf")) t->exportPdf();
    else if (id == QLatin1String("print")) t->printDocument();
    else if (id == QLatin1String("close")) closeTab(m_tabs->currentIndex());
    else if (id == QLatin1String("copy")) t->copy();
    else if (id == QLatin1String("cut")) t->cut();
    else if (id == QLatin1String("paste")) t->paste();
    else if (id == QLatin1String("zoomIn")) t->setZoom(t->zoom() + 10);
    else if (id == QLatin1String("zoomOut")) t->setZoom(t->zoom() - 10);
    else if (id == QLatin1String("zoom100")) t->setZoom(100);
    else if (dung::word::handleAction(id, t, this)) {}
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
    m_side->flush();
    m_projects->flush();
    while (m_tabs->count() > 0) {
        selectDocumentTab(0);
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


// ---------------------------------------------------------------- chụp giao diện (kiểm thử)
#include "shell/ShellIcons.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPixmap>
#include <QTextStream>

int MainWindow::captureShots(const QString& dir) {
    QDir().mkpath(dir);
    QFile logf(dir + QStringLiteral("/ui-check.log"));
    logf.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream log(&logf);
    int problems = 0;

    // Kiểm tra: (1) hai widget lá cùng cha chồng lên nhau, (2) nhãn/nút bị cắt chữ.
    auto check = [&](QWidget* root, const QString& where) {
        const auto all = root->findChildren<QWidget*>();
        for (QWidget* w : all) {
            if (!w->isVisibleTo(root) || w->width() <= 0) continue;
            const bool leaf = qobject_cast<QLabel*>(w) || qobject_cast<QPushButton*>(w);
            if (!leaf) continue;
            QString text;
            if (auto* l = qobject_cast<QLabel*>(w)) text = l->text(); else text = static_cast<QPushButton*>(w)->text();
            if (text.isEmpty() || text.contains(QLatin1Char('<'))) continue;
            if (auto* l = qobject_cast<QLabel*>(w); l && l->wordWrap()) continue;
            const int need = w->fontMetrics().horizontalAdvance(text);
            if (need > w->width() + 2 && !qobject_cast<QPushButton*>(w)) {
                log << "CAT-CHU [" << where << "] " << w->objectName() << " '" << text << "' cần " << need << "px, có " << w->width() << "px\n";
                ++problems;
            }
        }
        // chồng lấn giữa các anh em lá (cùng cha)
        for (QWidget* par : root->findChildren<QWidget*>()) {
            QList<QWidget*> kids;
            for (QObject* o : par->children()) {
                auto* c = qobject_cast<QWidget*>(o);
                if (c && c->isVisibleTo(root) && !c->isWindow() && (qobject_cast<QLabel*>(c) || qobject_cast<QPushButton*>(c))) kids << c;
            }
            for (int i = 0; i < kids.size(); ++i)
                for (int j = i + 1; j < kids.size(); ++j) {
                    // Bố cục QLayout không để chồng; chỉ báo khi giao nhau thực sự > 2px mỗi chiều.
                    const QRect inter = kids[i]->geometry().intersected(kids[j]->geometry());
                    if (inter.width() > 2 && inter.height() > 2) {
                        log << "CHONG [" << where << "] " << kids[i]->objectName() << " x " << kids[j]->objectName() << "\n";
                        ++problems;
                    }
                }
        }
    };

    struct Page { const char* name; std::function<void()> show; };
    const Page pages[] = {
        {"home", [this] { showHome(); }},
        {"templates", [this] { showTemplates(); }},
        {"toolbox", [this] { showToolbox(); }},
        {"projects", [this] { showProjects(); }},
    };
    const QSize sizes[] = {{1366, 768}, {1920, 1080}, {1100, 700}, {800, 600}};
    for (const QSize& sz : sizes) {
        resize(sz);
        show();
        for (const Page& pg : pages) {
            pg.show();
            QApplication::processEvents();
            QApplication::processEvents();
            const QString tag = QStringLiteral("%1-%2x%3").arg(QLatin1String(pg.name)).arg(sz.width()).arg(sz.height());
            grab().save(dir + QStringLiteral("/ui-") + tag + QStringLiteral(".png"));
            check(this, tag);
        }
        // Palette Ctrl+K
        if (sz.width() == 1366) {
            showHome();
            showPalette();
            QApplication::processEvents();
            m_palette->grab().save(dir + QStringLiteral("/ui-palette.png"));
            m_palette->hide();
        }
    }

    // Ribbon ở 4 loại tài liệu (không cần engine): dựng riêng một Ribbon.
    for (auto k : {DocKind::Text, DocKind::Spreadsheet, DocKind::Presentation, DocKind::Drawing}) {
        QWidget host;
        host.setObjectName(QStringLiteral("Central"));
        auto* hv = new QVBoxLayout(&host);
        hv->setContentsMargins(0, 0, 0, 0);
        auto* rb = new Ribbon(&host);
        hv->addWidget(rb);
        hv->addStretch(1);
        rb->setKind(k, false);
        host.resize(1366, 200);
        host.show();
        QApplication::processEvents();
        const QString nm = k == DocKind::Text ? QStringLiteral("word") : k == DocKind::Spreadsheet ? QStringLiteral("excel")
                          : k == DocKind::Presentation ? QStringLiteral("ppt") : QStringLiteral("pdf");
        host.grab().save(dir + QStringLiteral("/ui-ribbon-") + nm + QStringLiteral(".png"));
    }
    log << "TONG: " << problems << " van de\n";
    return problems;
}
