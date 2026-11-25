// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "DocumentTab.h"
#include "print/PrintDialog.h"
#include <QTemporaryDir>

#include "EngineHost.h"
#include "Theme.h"
#include "TileView.h"
#include "DataAssistWidgets.h"
#include <QScrollArea>
#include <QTabWidget>
#include "ppt/SlideThumbs.h"
#include "recovery/RecoveryStore.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

using dung::DocKind;

DocumentTab::DocumentTab(std::unique_ptr<dung::Document> doc, QWidget* parent)
    : QWidget(parent), m_doc(std::move(doc)) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ---- Băng cảnh báo (chỉ đọc / định dạng có rủi ro) ----
    if (m_doc->readOnly() || !m_doc->warnings().empty()) {
        auto* banner = new QFrame(this);
        banner->setObjectName(QStringLiteral("Banner"));
        auto* bh = new QHBoxLayout(banner);
        bh->setContentsMargins(12, 6, 12, 6);
        QString text;
        for (const auto& w : m_doc->warnings()) text += qs(w) + QLatin1Char(' ');
        if (m_doc->readOnly()) text.prepend(tr("CHỈ ĐỌC — "));
        auto* lbl = new QLabel(text.trimmed(), banner);
        lbl->setObjectName(QStringLiteral("BannerText"));
        lbl->setWordWrap(true);
        bh->addWidget(lbl, 1);
        if (m_doc->readOnly() && m_doc->kind() != DocKind::Drawing) {
            auto* btn = new QPushButton(tr("Lưu thành bản có thể chỉnh sửa…"), banner);
            connect(btn, &QPushButton::clicked, this, [this] { saveAs(); });
            bh->addWidget(btn);
        }
        v->addWidget(banner);
    }

    // ---- Thanh công thức (Excel) ----
    if (m_doc->kind() == DocKind::Spreadsheet) {
        auto* fb = new QWidget(this);
        fb->setObjectName(QStringLiteral("FormulaBar"));
        fb->setAttribute(Qt::WA_StyledBackground, true);
        auto* fh = new QHBoxLayout(fb);
        fh->setContentsMargins(8, 4, 8, 4);
        m_nameBox = new QLineEdit(fb);
        m_nameBox->setFixedWidth(90);
        m_nameBox->setToolTip(tr("Ô hiện tại — nhập địa chỉ (vd. B12) rồi Enter để đến ô đó"));
        auto* fx = new QPushButton(tr("ƒx · Tôi muốn"), fb);
        fx->setToolTip(tr("Mở hoặc thu gọn trợ lý công thức"));
        fx->setEnabled(!m_doc->readOnly());
        connect(fx, &QPushButton::clicked, this, [this] {
            if (m_excelTools) m_excelTools->setVisible(m_excelTools->isHidden());
        });
        fx->setObjectName(QStringLiteral("Fx"));
        m_formula = new QLineEdit(fb);
        m_formula->setPlaceholderText(tr("Nội dung hoặc công thức của ô"));
        m_formula->setReadOnly(m_doc->readOnly());
        fh->addWidget(m_nameBox);
        fh->addWidget(fx);
        fh->addWidget(m_formula, 1);
        v->addWidget(fb);
        connect(m_nameBox, &QLineEdit::returnPressed, this, [this] {
            const QString addr = m_nameBox->text().trimmed().toUpper();
            if (!addr.isEmpty()) m_doc->command(".uno:GoToCell", dung::Document::stringArg("ToPoint", ss(addr)));
            m_view->setFocus();
        });
        connect(m_formula, &QLineEdit::returnPressed, this, [this] {
            if (m_doc->readOnly()) return;
            m_doc->command(".uno:EnterString", dung::Document::stringArg("StringName", ss(m_formula->text())));
            m_view->setFocus();
        });
    }

    // ---- Vùng tài liệu (+ danh sách trang chiếu bên trái cho PowerPoint/PDF) ----
    auto* middle = new QHBoxLayout;
    middle->setSpacing(0);
    if (m_doc->kind() == DocKind::Presentation || m_doc->kind() == DocKind::Drawing) {
        m_slides = new QListWidget(this);
        m_slides->setFixedWidth(150);
        m_slides->setIconSize(QSize(128, 96));
        m_slides->setStyleSheet(QStringLiteral("QListWidget{background:#EEF2F7;border:none;border-right:1px solid #D9E1EC;}"
                                               "QListWidget::item{padding:10px;margin:4px;background:white;border-radius:6px;}"
                                               "QListWidget::item:selected{border:2px solid #1F5FBF;color:#1F5FBF;}"));
        middle->addWidget(m_slides);
        connect(m_slides, &QListWidget::currentRowChanged, this, [this](int row) {
            if (m_rebuildingParts || row < 0 || row == m_doc->currentPart()) return;
            m_doc->setPart(row);
            m_view->resetForPartChange();
            emit statusChanged();
        });
    }
    m_view = new TileView(m_doc.get(), this);
    middle->addWidget(m_view, 1);
    if (m_doc->kind() == DocKind::Spreadsheet && !m_doc->readOnly()) {
        m_excelTools = new QTabWidget(this);
        m_excelTools->setObjectName(QStringLiteral("ExcelTools"));
        m_excelTools->setMinimumWidth(280);
        m_excelTools->setMaximumWidth(420);
        auto addTool = [this](QWidget* tool, const QString& title) {
            auto* scroll = new QScrollArea(m_excelTools);
            scroll->setWidgetResizable(true);
            scroll->setWidget(tool);
            m_excelTools->addTab(scroll, title);
        };
        addTool(createFormulaAssistantWidget(), tr("ƒx · Tôi muốn"));
        addTool(createRangeToolsWidget(), tr("Vùng dữ liệu"));
        middle->addWidget(m_excelTools);
    }
    v->addLayout(middle, 1);

    // ---- Thanh sheet (Excel) ----
    if (m_doc->kind() == DocKind::Spreadsheet) {
        m_sheets = new QTabBar(this);
        m_sheets->setObjectName(QStringLiteral("SheetTabs"));
        m_sheets->setShape(QTabBar::RoundedSouth);
        m_sheets->setDrawBase(false);
        m_sheets->setExpanding(false);
        auto* sh = new QHBoxLayout;
        sh->setContentsMargins(52, 0, 0, 0);
        sh->addWidget(m_sheets);
        sh->addStretch(1);
        v->addLayout(sh);
        m_sheets->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_sheets, &QTabBar::customContextMenuRequested, this, [this](const QPoint& p) { excelSheetMenu(p); });
        connect(m_sheets, &QTabBar::currentChanged, this, [this](int idx) {
            if (m_rebuildingParts || idx < 0 || idx == m_doc->currentPart()) return;
            m_doc->setPart(idx);
            m_view->resetForPartChange();
            emit statusChanged();
        });
    }
    rebuildParts();
    connect(m_view, &TileView::zoomChanged, this, &DocumentTab::statusChanged);

    // Sự kiện engine đến từ luồng LibreOffice -> chuyển sang luồng giao diện.
    QPointer<DocumentTab> self(this);
    m_doc->setEventSink([self](const dung::EngineEvent& e) {
        const int type = e.type;
        const QString payload = qs(e.payload);
        QMetaObject::invokeMethod(qApp, [self, type, payload] {
            if (self) self->onEngineEvent(type, payload);
        }, Qt::QueuedConnection);
    });
    QTimer::singleShot(0, m_view, qOverload<>(&QWidget::setFocus));

    // Tự lưu bản phục hồi mỗi ~2 phút, chỉ khi có thay đổi. Không bao giờ ghi vào tệp gốc.
    m_autosave = new QTimer(this);
    m_autosave->setInterval(2 * 60 * 1000);
    connect(m_autosave, &QTimer::timeout, this, &DocumentTab::autosaveTick);
    m_autosave->start();
}

void DocumentTab::showExcelTools(int page) {
    if (!m_excelTools || readOnly()) return;
    m_excelTools->setCurrentIndex(page);
    m_excelTools->show();
}

DocumentTab::~DocumentTab() {
    if (m_doc) m_doc->setEventSink(nullptr);
    dropRecovery();
}

void DocumentTab::adoptRecovered(const recovery::Entry& e) {
    m_adopted = e;
    m_hasAdopted = true;
    m_recOrigin = e.originalPath;
    m_recOriginSet = true;
    m_dirtySinceAutosave = true;  // bản này chưa nằm trong phiên mới: tự lưu ngay ở nhịp tới
}

void DocumentTab::setCopyOrigin(const QString& originalPath) {
    m_recOrigin = originalPath;
    m_recOriginSet = true;
    m_dirtySinceAutosave = true;  // bản sao chưa lưu ở đâu cả: tự lưu ngay ở nhịp tới
}

void DocumentTab::dropRecovery() {
    if (!m_recId.isEmpty()) recovery::Session::instance().discard(m_recId);
    m_recId.clear();
    if (m_hasAdopted) { recovery::discardEntry(m_adopted); m_hasAdopted = false; }
}

void DocumentTab::autosaveTick() {
    auto& rs = recovery::Session::instance();
    if (!rs.active() || !m_doc || m_doc->readOnly()) return;
    if (!m_doc->modified() || !m_dirtySinceAutosave) return;
    if (!rs.hasRoom()) return;  // thư mục phục hồi đã đầy: bỏ qua, không làm gì khác
    const std::string ext = m_doc->recoveryExt();
    if (ext.empty()) return;
    if (m_recId.isEmpty()) m_recId = rs.newDocId();
    const QString e = qs(ext);
    std::string err;
    if (!m_doc->saveRecoveryCopy(toPath(rs.dataPath(m_recId, e)), &err)) return;  // lỗi tự lưu không làm phiền người dùng
    const QString orig = m_doc->isNew() ? QString() : fromPath(m_doc->originalPath());
    const QString original = m_recOriginSet ? m_recOrigin : orig;
    if (rs.commit(m_recId, e, qs(m_doc->displayName()), original, original.isEmpty())) {
        m_dirtySinceAutosave = false;
        if (m_hasAdopted) { recovery::discardEntry(m_adopted); m_hasAdopted = false; }  // đã có bản mới trong phiên này
    }
}

QString DocumentTab::title() const {
    QString t = m_savedPath.isEmpty() ? qs(m_doc->displayName()) : QFileInfo(m_savedPath).fileName();
    if (m_doc->readOnly()) t += tr(" (chỉ đọc)");
    if (m_doc->modified()) t += QStringLiteral(" •");
    return t;
}

QString DocumentTab::statusText() const {
    QString s = QString::fromUtf8(dung::kindName(m_doc->kind()));
    const int n = m_doc->partCount(), cur = m_doc->currentPart() + 1;
    switch (m_doc->kind()) {
    case DocKind::Spreadsheet: s += tr("  •  Sheet %1/%2").arg(cur).arg(n); break;
    case DocKind::Presentation: s += tr("  •  Trang chiếu %1/%2").arg(cur).arg(n); break;
    case DocKind::Drawing: s += tr("  •  Trang %1/%2").arg(cur).arg(n); break;
    default: break;
    }
    if (!m_cellAddress.isEmpty()) s += tr("  •  Ô %1").arg(m_cellAddress);
    if (!m_savedPath.isEmpty()) s += tr("  •  Lưu vào: %1").arg(QDir::toNativeSeparators(m_savedPath));
    return s;
}

void DocumentTab::rebuildParts() {
    m_rebuildingParts = true;
    const int n = m_doc->partCount(), cur = m_doc->currentPart();
    if (m_sheets) {
        while (m_sheets->count() > n) m_sheets->removeTab(m_sheets->count() - 1);
        for (int i = 0; i < n; ++i) {
            const QString name = qs(m_doc->partName(i));
            if (i < m_sheets->count()) m_sheets->setTabText(i, name);
            else m_sheets->addTab(name);
        }
        m_sheets->setCurrentIndex(cur);
    }
    if (m_slides) {
        m_slides->clear();
        for (int i = 0; i < n; ++i) {
            m_slides->addItem(m_doc->kind() == DocKind::Presentation ? tr("Trang chiếu %1").arg(i + 1)
                                                                    : tr("Trang %1").arg(i + 1));
            if (m_doc->kind() == DocKind::Presentation) pptSetThumbnail(m_slides->item(i), m_doc.get(), i);
        }
        m_slides->setCurrentRow(cur);
    }
    m_rebuildingParts = false;
}

void DocumentTab::onEngineEvent(int type, const QString& payload) {
    m_view->handleEvent(type, payload);
    if (type == LOK_CALLBACK_INVALIDATE_TILES && m_doc->modified()) m_dirtySinceAutosave = true;
    if (m_doc->modified() != m_lastMod) { m_lastMod = m_doc->modified(); emit statusChanged(); } // đổi trạng thái "chưa lưu"
    switch (type) {
    case LOK_CALLBACK_CELL_ADDRESS:
        m_cellAddress = payload;
        if (m_nameBox && !m_nameBox->hasFocus()) m_nameBox->setText(payload);
        emit statusChanged();
        break;
    case LOK_CALLBACK_CELL_FORMULA:
        if (m_formula && !m_formula->hasFocus()) m_formula->setText(payload);
        break;
    case LOK_CALLBACK_STATE_CHANGED: {
        QString cmd, value;
        if (payload.startsWith(QLatin1Char('{'))) {
            const QJsonObject o = QJsonDocument::fromJson(payload.toUtf8()).object();
            cmd = o.value(QStringLiteral("commandName")).toString();
            value = o.value(QStringLiteral("state")).toVariant().toString();
        } else {
            const int eq = payload.indexOf(QLatin1Char('='));
            if (eq < 0) break;
            cmd = payload.left(eq);
            value = payload.mid(eq + 1);
        }
        if (cmd.isEmpty()) break;
        m_states.insert(cmd, value);
        if (cmd == QLatin1String(".uno:ModifiedStatus")) emit titleChanged();
        emit commandStateChanged(cmd, value);
        break;
    }
    case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED:
    case LOK_CALLBACK_SET_PART:
        if ((m_sheets && m_sheets->count() != m_doc->partCount()) ||
            (m_slides && m_slides->count() != m_doc->partCount()) ||
            (m_sheets && m_sheets->currentIndex() != m_doc->currentPart()) ||
            (m_slides && m_slides->currentRow() != m_doc->currentPart())) {
            rebuildParts();
            m_view->resetForPartChange();
        }
        emit statusChanged();
        break;
    default:
        break;
    }
}

void DocumentTab::runCommand(const QString& uno) {
    // Dạng ".uno:Lệnh#{json}" mang đối số UNO (dùng cho ribbon PowerPoint)
    const int hash = uno.indexOf(QLatin1Char('#'));
    if (hash > 0) m_doc->command(ss(uno.left(hash)), ss(uno.mid(hash + 1)));
    else m_doc->command(ss(uno));
    m_view->setFocus();
}

// ---- Clipboard: chuyển giữa clipboard nội bộ của engine và clipboard Windows ----
void DocumentTab::copy() {
    auto* mime = new QMimeData;
    const std::string html = m_doc->selectionAs("text/html");
    const std::string text = m_doc->selectionAs("text/plain;charset=utf-8");
    if (!html.empty()) mime->setHtml(qs(html));
    if (!text.empty()) mime->setText(qs(text));
    if (html.empty() && text.empty()) { delete mime; return; }
    QApplication::clipboard()->setMimeData(mime);
    m_doc->command(".uno:Copy");
}

void DocumentTab::cut() {
    if (m_doc->readOnly()) return;
    copy();
    m_doc->command(".uno:Cut");
}

void DocumentTab::paste() {
    if (m_doc->readOnly()) return;
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime) return;
    bool ok = false;
    if (mime->hasHtml()) ok = m_doc->paste("text/html", ss(mime->html()));
    if (!ok && mime->hasText()) ok = m_doc->paste("text/plain;charset=utf-8", ss(mime->text()));
    m_view->setFocus();
}

void DocumentTab::setZoom(int percent) { m_view->setZoom(percent); }

void DocumentTab::stepPart(int delta) {
    if (!m_slides || m_slides->count() == 0) return;
    const int r = qBound(0, m_slides->currentRow() + delta, m_slides->count() - 1);
    m_slides->setCurrentRow(r);
}
int DocumentTab::zoom() const { return m_view->zoom(); }

// ---- Lưu an toàn ----
static bool samePathUi(const QString& a, const QString& b) {
    const QString ca = QFileInfo(a).canonicalFilePath(), cb = QFileInfo(b).canonicalFilePath();
    if (!ca.isEmpty() && !cb.isEmpty()) return ca.compare(cb, Qt::CaseInsensitive) == 0;
    return QFileInfo(a).absoluteFilePath().compare(QFileInfo(b).absoluteFilePath(), Qt::CaseInsensitive) == 0;
}

bool DocumentTab::doSave(const QString& path, bool confirmedReplace) {
    dung::SaveOptions opts;
    const bool isPdf = path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive);
    const QString original = m_doc->isNew() ? QString() : fromPath(m_doc->originalPath());

    if (isPdf) {
        if (QMessageBox::question(this, tr("Xuất PDF"),
                tr("Bản PDF chỉ là bản in: không giữ công thức và không chỉnh sửa lại được như tài liệu gốc.\n"
                   "Tài liệu đang mở không bị thay đổi. Tiếp tục xuất?")) != QMessageBox::Yes)
            return false;
        opts.allowLossyExport = true;
    } else if (m_doc->readOnly()) {
        QString w;
        for (const auto& s : m_doc->warnings()) w += qs(s) + QLatin1Char('\n');
        if (QMessageBox::warning(this, tr("Lưu từ tệp chỉ đọc"),
                w + tr("\nLưu sang định dạng mới có thể không giữ được mọi thành phần của tệp gốc. "
                       "Tệp gốc sẽ KHÔNG bị thay đổi. Tiếp tục?"),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return false;
        opts.allowLossyExport = true;
    }
    if (!isPdf && !m_doc->featureLoss().empty()) {
        if (!m_featureLossConfirmed) {
            // Nêu CHÍNH XÁC từng thành phần sẽ mất; chưa ghi gì cho tới khi người dùng đồng ý.
            QMessageBox box(QMessageBox::Warning, tr("Bản lưu sẽ mất một số thành phần"),
                tr("Tệp gốc có những thành phần mà Dũng Office KHÔNG giữ được khi lưu:\n\n%1\n"
                   "Tệp gốc sẽ KHÔNG bị thay đổi; chỉ bản sao mới thiếu các thành phần trên.")
                    .arg(qs(dung::describeFeatureLoss(m_doc->featureLoss()))),
                QMessageBox::NoButton, this);
            auto* yes = box.addButton(tr("Vẫn lưu bản sao"), QMessageBox::AcceptRole);
            box.addButton(tr("Không lưu"), QMessageBox::RejectRole);
            box.setDefaultButton(qobject_cast<QPushButton*>(box.buttons().last()));
            box.exec();
            if (box.clickedButton() != yes) return false;
            m_featureLossConfirmed = true;
        }
        opts.allowFeatureLoss = true;
    }
    if (!original.isEmpty() && samePathUi(path, original)) {
        if (QMessageBox::warning(this, tr("Ghi đè tệp gốc?"),
                tr("Bạn đang chọn ghi đè lên chính tệp gốc.\n\nDũng Office sẽ tạo một bản sao lưu của tệp gốc "
                   "trước khi ghi (tên.dung-backup-ngày-giờ). Bạn vẫn muốn ghi đè?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return false;
        opts.allowOverwriteOriginal = true;
    } else if (QFileInfo::exists(path)) {
        opts.allowReplaceExisting = confirmedReplace;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const dung::SaveResult r = m_doc->saveAs(toPath(path), opts);
    QApplication::restoreOverrideCursor();
    if (!r.ok) {
        QMessageBox::warning(this, tr("Chưa lưu được"), qs(r.message));
        return false;
    }
    if (!isPdf) {
        m_savedPath = path;
        m_dirtySinceAutosave = false;
        dropRecovery();  // đã lưu: bản phục hồi không còn cần
    }
    emit titleChanged();
    emit statusChanged();
    emit savedTo(path);
    if (!r.backup.empty())
        QMessageBox::information(this, tr("Đã lưu"),
            tr("Đã lưu. Bản cũ của tệp được giữ lại tại:\n%1").arg(QDir::toNativeSeparators(fromPath(r.backup))));
    return true;
}

bool DocumentTab::save() {
    if (m_savedPath.isEmpty()) return saveAs();
    return doSave(m_savedPath, true);
}

bool DocumentTab::saveAs() {
    const auto natives = dung::nativeSaveFormats(m_doc->kind());
    if (natives.empty()) {
        QMessageBox::information(this, tr("Lưu thành"),
            tr("Loại tài liệu này chưa hỗ trợ lưu chỉnh sửa. Bạn có thể dùng \"Xuất PDF\" để tạo bản sao."));
        return false;
    }
    QStringList filters;
    for (const auto* f : natives) filters << qs(f->label) + QStringLiteral(" (*.") + qs(f->ext) + QLatin1Char(')');
    const QString suggested = m_savedPath.isEmpty() ? fromPath(m_doc->suggestSaveAsPath()) : m_savedPath;
    QString selected = filters.first();
    QString path = QFileDialog::getSaveFileName(this, tr("Lưu thành"), suggested, filters.join(QStringLiteral(";;")), &selected);
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty()) {
        const int i = filters.indexOf(selected);
        path += QLatin1Char('.') + qs(natives[std::max(0, i)]->ext);
    }
    // Hộp thoại hệ thống đã hỏi xác nhận nếu tệp tồn tại -> coi như đã xác nhận thay thế.
    return doSave(path, true);
}

bool DocumentTab::exportPdf() {
    QString base = m_savedPath.isEmpty() ? fromPath(m_doc->suggestSaveAsPath("pdf"))
                                         : QFileInfo(m_savedPath).absolutePath() + QLatin1Char('/') +
                                               QFileInfo(m_savedPath).completeBaseName() + QStringLiteral(".pdf");
    QString path = QFileDialog::getSaveFileName(this, tr("Xuất PDF"), base, tr("PDF (*.pdf)"));
    if (path.isEmpty()) return false;
    if (!path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) path += QStringLiteral(".pdf");
    return doSave(path, true);
}

bool DocumentTab::printDocument() {
    // Xem trước và in dùng CÙNG MỘT tệp PDF → không lệch giữa xem trước và bản in.
    QTemporaryDir tmp;
    QString pdfPath;
    const QString original = m_doc->isNew() ? QString() : fromPath(m_doc->originalPath());
    if (m_doc->kind() == DocKind::Drawing && original.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive) && QFileInfo::exists(original)) {
        pdfPath = original;  // PDF đang mở: in đúng tệp gốc, không xuất lại
    } else {
        if (!tmp.isValid()) {
            QMessageBox::warning(this, tr("In"), tr("Không tạo được thư mục tạm để chuẩn bị bản in."));
            return false;
        }
        pdfPath = tmp.filePath(QStringLiteral("ban-in.pdf"));
        dung::SaveOptions opts;
        opts.allowLossyExport = true;  // PDF tạm chỉ để in, không thay thế tài liệu
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const dung::SaveResult r = m_doc->saveAs(toPath(pdfPath), opts);
        QApplication::restoreOverrideCursor();
        if (!r.ok) {
            QMessageBox::warning(this, tr("In"), tr("Không chuẩn bị được bản in: %1").arg(qs(r.message)));
            return false;
        }
    }
    // Chỉ so số trang với Word/PowerPoint (Excel: phần là sheet, không tương ứng số trang in).
    const int display = (m_doc->kind() == DocKind::Text || m_doc->kind() == DocKind::Presentation) ? m_doc->partCount() : -1;
    PrintDialog::run(this, pdfPath, display, qs(m_doc->displayName()));
    return true;
}

bool DocumentTab::maybeClose() {
    if (!m_doc->modified()) return true;
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Đóng tài liệu"));
    box.setText(tr("Lưu thay đổi của \"%1\"?").arg(qs(m_doc->displayName())));
    box.setInformativeText(m_doc->isNew() ? QString()
                                          : tr("Tệp gốc không bị thay đổi. Thay đổi sẽ được lưu vào một tệp mới."));
    QPushButton* saveBtn = box.addButton(m_savedPath.isEmpty() ? tr("Lưu thành…") : tr("Lưu"), QMessageBox::AcceptRole);
    QPushButton* discard = box.addButton(tr("Không lưu"), QMessageBox::DestructiveRole);
    box.addButton(tr("Huỷ"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == saveBtn) return save();
    if (box.clickedButton() == discard) { dropRecovery(); return true; }
    return false;
}
