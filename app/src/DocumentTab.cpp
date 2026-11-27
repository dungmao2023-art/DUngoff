// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "DocumentTab.h"
#include "SavePathPolicy.h"
#include "print/PrintDialog.h"
#include "print/PrintRender.h"
#include <QPixmap>
#include <QTemporaryDir>

#include "EngineHost.h"
#include "Theme.h"
#include "TileView.h"
#include "DataAssistWidgets.h"
#include "tools/dataassist/ExcelBridge.h"
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
#include <QEasingCurve>
#include <QVariantAnimation>
#include <QRegularExpression>
#include <QFile>
#include <QStandardPaths>
#include <QInputDialog>
#include <QShortcut>
#include <QKeySequence>
#include <QToolButton>
#include <QStyle>
#include "ui/Icons.h"
#include "word/WordCommands.h"
#include "word/WordActions.h"
#include "templates/TemplateLibrary.h"

using dung::DocKind;

namespace {
// Animate only panel width, never the document or engine state. The handle stays visible.
// Keep the target state separate from visibility, so rapid clicks can reverse an ongoing animation.
bool panelExpanded(QWidget* panel) {
    return panel && panel->property("panelExpanded").toBool();
}
void initCollapsed(QWidget* panel) {
    panel->setProperty("panelExpanded", false);
    panel->setFixedWidth(0);
    panel->hide();
}
void slidePanel(QWidget* panel, bool open, int fullWidth, QPushButton* handle,
                bool left, const QString& label) {
    if (!panel) return;
    panel->setProperty("panelExpanded", open);
    if (handle) {
        handle->setChecked(open);
        const QString arrow = left == open ? QStringLiteral("‹") : QStringLiteral("›");
        handle->setText(label.isEmpty() ? arrow : arrow + QLatin1Char('\n') + label);
        handle->setToolTip(open ? QObject::tr("Thu gọn") : QObject::tr("Mở rộng"));
    }
    auto* animation = panel->findChild<QVariantAnimation*>(QStringLiteral("PanelWidthAnimation"), Qt::FindDirectChildrenOnly);
    if (!animation) {
        animation = new QVariantAnimation(panel);
        animation->setObjectName(QStringLiteral("PanelWidthAnimation"));
        animation->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(animation, &QVariantAnimation::valueChanged, panel, [panel](const QVariant& value) {
            panel->setFixedWidth(value.toInt());
        });
        QObject::connect(animation, &QVariantAnimation::finished, panel, [panel] {
            if (!panelExpanded(panel)) panel->hide();
        });
    }
    animation->stop();
    const int from = panel->isHidden() ? 0 : panel->width();
    const int to = open ? fullWidth : 0;
    panel->setFixedWidth(from);
    if (open) panel->show();
    if (from == to) {
        if (!open) panel->hide();
        return;
    }
    animation->setDuration(qBound(80, 220 * qAbs(to - from) / qMax(1, fullWidth), 220));
    animation->setStartValue(from);
    animation->setEndValue(to);
    animation->start();
}
QPushButton* panelHandle(QWidget* rail, bool left, const QString& label, const QString& name) {
    auto* button = new QPushButton(rail);
    button->setObjectName(name);
    const QString arrow = left ? QStringLiteral("›") : QStringLiteral("‹");
    button->setText(label.isEmpty() ? arrow : arrow + QLatin1Char('\n') + label);
    button->setAccessibleName(name == QLatin1String("TemplatePanelHandle")
        ? QObject::tr("Mở hoặc ẩn kho mẫu") : QObject::tr("Mở hoặc ẩn Tôi muốn"));
    button->setToolTip(button->accessibleName());
    button->setCheckable(true);
    button->setFixedSize(label.isEmpty() ? 26 : 46, label.isEmpty() ? 64 : 78);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::TabFocus);
    button->setStyleSheet(QStringLiteral(R"(
QPushButton { background:#E8F0FF; color:#2864BA; border:1px solid #DCE5F2; border-radius:8px; font-weight:700; padding:2px; }
QPushButton:hover, QPushButton:focus { background:#DCE9FF; border-color:#2864BA; }
QPushButton:checked { background:#2864BA; color:white; border-color:#2864BA; }
)"));
    return button;
}
} // namespace


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

    if (m_doc->kind() == DocKind::Drawing && !m_doc->readOnly()) {
        auto* bar = new QWidget(this);
        auto* layout = new QHBoxLayout(bar); layout->setContentsMargins(12,6,12,6);
        auto* info = new QLabel(tr("Chỉnh trực tiếp trên trang: nhấp đúp vào chữ mà engine nhận diện; dùng Hộp chữ để thêm chữ mới. Ctrl+Z để hoàn tác. Lưu ODG hoặc xuất PDF mới."),bar);
        info->setWordWrap(true); layout->addWidget(info,1);
        m_pdfCompareButton = new QPushButton(tr("Đối chiếu PDF gốc"),bar);
        m_pdfCompareButton->setCheckable(true); m_pdfCompareButton->hide();
        m_pdfCompareButton->setToolTip(tr("Hiện trang gốc cạnh bản chỉnh sửa trực tiếp"));
        layout->addWidget(m_pdfCompareButton);
        bar->setStyleSheet(QStringLiteral("QWidget{background:#F4F8FF;} QLabel{color:#40577D;} QPushButton{background:white;border:1px solid #DCE5F2;border-radius:6px;padding:7px;} QPushButton:checked{background:#2864BA;color:white;}"));
        v->addWidget(bar);
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
        m_nameBox->setToolTip(tr("Nhập ô hoặc vùng trong sheet hiện tại (B12, A1:C5), rồi Enter"));
        auto* fx = new QPushButton(tr("ƒx · Tôi muốn"), fb);
        fx->setToolTip(tr("Mở hoặc thu gọn trợ lý công thức"));
        fx->setEnabled(!m_doc->readOnly());
        connect(fx, &QPushButton::clicked, this, [this] {
            if (m_excelTools) slidePanel(m_excelTools, !panelExpanded(m_excelTools), 340,
                findChild<QPushButton*>(QStringLiteral("ExcelPanelHandle")), false, tr("Tôi\nmuốn"));
        });
        fx->setObjectName(QStringLiteral("Fx"));
        m_formula = new QLineEdit(fb);
        m_formula->setPlaceholderText(tr("Nội dung hoặc công thức của ô"));
        m_formula->setToolTip(tr("Sửa nội dung một ô; Enter hoặc rời ô nhập để xác nhận, Esc để huỷ. Nếu vị trí đã thay đổi, nội dung sẽ không bị ghi sang ô khác."));
        m_formula->setReadOnly(m_doc->readOnly());
        fh->addWidget(m_nameBox);
        auto* cancelEdit=new QToolButton(fb);
        auto* acceptEdit=new QToolButton(fb);
        for(auto* button:{cancelEdit,acceptEdit}){
            button->setFixedSize(28,28);button->setAutoRaise(true);
            button->setFocusPolicy(Qt::NoFocus);button->setEnabled(false);
        }
        cancelEdit->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
        acceptEdit->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
        cancelEdit->setToolTip(tr("Hủy chỉnh sửa công thức (Esc)"));
        acceptEdit->setToolTip(tr("Xác nhận công thức (Enter)"));
        cancelEdit->setAccessibleName(cancelEdit->toolTip());acceptEdit->setAccessibleName(acceptEdit->toolTip());
        fh->addWidget(cancelEdit);fh->addWidget(acceptEdit);
        fh->addWidget(fx);
        fh->addWidget(m_formula, 1);
        v->addWidget(fb);
        connect(m_nameBox, &QLineEdit::returnPressed, this, [this] {
            if (!m_doc || !m_view) return;
            const auto addr=dataassist::bridgeRangeAddress(ss(m_nameBox->text()));
            if (addr.empty()) {
                m_nameBox->setText(qs(m_doc->lastPayload(LOK_CALLBACK_CELL_ADDRESS)));
                QMessageBox::warning(this,tr("Địa chỉ ô"),tr("Nhập địa chỉ hợp lệ trong sheet hiện tại, ví dụ B12 hoặc A1:C5."));
                m_nameBox->setFocus();
                m_nameBox->selectAll();
                return;
            }
            m_doc->command(".uno:GoToCell", dung::Document::stringArg("ToPoint",addr));
            m_view->setFocus();
        });
        // Bind an edit to its initial cell and sheet, not the selection that
        // happens to be current when the user eventually presses Enter.
        struct FormulaEdit {
            bool active = false;
            int part = -1;
            std::string sheet, cell;
        };
        auto edit = std::make_shared<FormulaEdit>();
        connect(m_formula, &QLineEdit::textEdited, this, [this, edit,cancelEdit,acceptEdit](const QString&) {
            if (edit->active || !m_doc || !m_view || m_doc->readOnly()) return;
            m_doc->waitInputIdle();
            edit->active = true;
            cancelEdit->setEnabled(true);acceptEdit->setEnabled(true);
            edit->part = m_doc->currentPart();
            edit->sheet = m_doc->partName(edit->part);
            edit->cell = dataassist::bridgeConfirmedCell(ss(m_view->selectedCellRange()),
                m_doc->lastPayload(LOK_CALLBACK_CELL_ADDRESS));
        });
        auto commitFormula = [this, edit,cancelEdit,acceptEdit](bool focusView) {
            if (!m_doc || !m_view || m_doc->readOnly() || !edit->active) return;
            const QString draft=m_formula->text();
            // Clear before invoking a modal dialog or changing focus to avoid double commits.
            edit->active=false;
            cancelEdit->setEnabled(false);acceptEdit->setEnabled(false);
            m_doc->waitInputIdle();
            const auto cell=dataassist::bridgeConfirmedCell(ss(m_view->selectedCellRange()),
                m_doc->lastPayload(LOK_CALLBACK_CELL_ADDRESS));
            if (edit->cell.empty() || cell!=edit->cell || m_doc->currentPart()!=edit->part ||
                m_doc->partName(edit->part)!=edit->sheet) {
                m_formula->setText(qs(m_doc->lastPayload(LOK_CALLBACK_CELL_FORMULA)));
                QMessageBox box(QMessageBox::Warning,tr("Thanh công thức"),
                    tr("Không xác minh được ô ban đầu. Chưa ghi dữ liệu vào ô khác. Bạn có thể sao chép nội dung vừa nhập trong phần Chi tiết."),
                    QMessageBox::Ok,this);
                box.setDetailedText(draft);
                box.exec();
                return;
            }
            m_doc->command(".uno:EnterString", dung::Document::stringArg("StringName",ss(draft)));
            m_doc->waitInputIdle();
            if(focusView) m_view->setFocus();
        };
        connect(m_formula,&QLineEdit::editingFinished,this,[commitFormula]{commitFormula(false);});
        connect(m_formula,&QLineEdit::returnPressed,this,[commitFormula]{commitFormula(true);});
        auto* cancelFormula=new QShortcut(QKeySequence(Qt::Key_Escape),m_formula);
        cancelFormula->setContext(Qt::WidgetShortcut);
        auto discardFormula=[this,edit,cancelEdit,acceptEdit]{
            edit->active=false;
            cancelEdit->setEnabled(false);acceptEdit->setEnabled(false);
            m_formula->setText(qs(m_doc->lastPayload(LOK_CALLBACK_CELL_FORMULA)));
            if(m_view)m_view->setFocus();
        };
        connect(cancelFormula,&QShortcut::activated,this,discardFormula);
        connect(cancelEdit,&QToolButton::clicked,this,discardFormula);
        connect(acceptEdit,&QToolButton::clicked,this,[commitFormula]{commitFormula(true);});
    }

    // ---- Vùng tài liệu (+ danh sách trang chiếu bên trái cho PowerPoint/PDF) ----
    auto* middle = new QHBoxLayout;
    middle->setSpacing(0);
    // Kho mẫu theo loại tài liệu. Chỉ đọc danh mục khi mở; không thay nội dung tài liệu hiện tại.
    const QString ext = m_doc->kind() == DocKind::Text ? QStringLiteral("docx")
                      : m_doc->kind() == DocKind::Spreadsheet ? QStringLiteral("xlsx")
                      : m_doc->kind() == DocKind::Presentation ? QStringLiteral("pptx")
                      : QString();
    if (!ext.isEmpty()) {
        auto* templates = new QWidget(this);
        templates->setObjectName(QStringLiteral("DocumentTemplates"));
        templates->setAttribute(Qt::WA_StyledBackground, true);
        templates->setFixedWidth(228);
        templates->setStyleSheet(QStringLiteral(R"(
QWidget#DocumentTemplates { background:#FBFCFF; border-right:1px solid #DCE5F2; }
QWidget#DocumentTemplates QListWidget { background:white; border:1px solid #DCE5F2; border-radius:8px; }
QWidget#DocumentTemplates QListWidget::item { padding:7px; }
QWidget#DocumentTemplates QListWidget::item:selected { background:#E8F0FF; color:#17294D; }
QWidget#DocumentTemplates QPushButton { background:#2864BA; color:white; border:none; border-radius:8px; padding:8px; font-weight:600; }
QWidget#DocumentTemplates QPushButton:disabled { background:#E8EDF5; color:#6B7A99; }
)"));
        auto* tv = new QVBoxLayout(templates);
        tv->setContentsMargins(10, 10, 10, 10);
        auto* title = new QLabel(tr("Mẫu %1").arg(ext.toUpper()), templates);
        title->setObjectName(QStringLiteral("SectionTitle"));
        tv->addWidget(title);
        auto* search = new QLineEdit(templates);
        search->setPlaceholderText(tr("Tìm mẫu…"));
        tv->addWidget(search);
        auto* list = new QListWidget(templates);
        list->setWordWrap(true);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tv->addWidget(list, 1);
        auto* hint = new QLabel(tr("Mẫu tham khảo. Tạo tệp mới, không thay tài liệu đang mở."), templates);
        hint->setWordWrap(true);
        hint->setObjectName(QStringLiteral("SectionNote"));
        auto* sourceNote=new QLabel(tr("Mẫu có thể thay đổi theo thời gian. <a href='https://csdl.dichvucong.gov.vn/'>Tra cứu mẫu hiện hành</a> tại cơ quan nhà nước."),templates);
        sourceNote->setWordWrap(true);sourceNote->setOpenExternalLinks(true);sourceNote->setTextFormat(Qt::RichText);
        tv->addWidget(hint);tv->addWidget(sourceNote);
        auto* use = new QPushButton(tr("Tạo từ mẫu"), templates);
        use->setEnabled(false);
        tv->addWidget(use);
        auto reload = [=] {
            list->clear();
            const auto lib = tpl::installed();
            if (lib.valid) {
                const QString root = QFileInfo(lib.root).canonicalFilePath();
                if (!root.isEmpty()) for (const auto& e : tpl::entries(lib)) {
                    const QFileInfo fi(QDir(lib.root).filePath(e.relPath));
                    if (fi.suffix().compare(ext, Qt::CaseInsensitive) != 0 || !fi.isFile()) continue;
                    const QString path = fi.canonicalFilePath();
                    if (!path.startsWith(root + QLatin1Char('/'))) continue;
                    QString title = fi.completeBaseName();
                    title.remove(QRegularExpression(QStringLiteral("^\\d+[_ ]*")));
                    title.replace(QLatin1Char('_'), QLatin1Char(' '));
                    if (!title.contains(search->text().trimmed(), Qt::CaseInsensitive) &&
                        !e.relPath.contains(search->text().trimmed(), Qt::CaseInsensitive)) continue;
                    auto* item = new QListWidgetItem(title, list);
                    item->setData(Qt::UserRole, e.relPath);
                    item->setToolTip(e.relPath + QStringLiteral(" · ") + e.status);
                }
            }
            hint->setText(list->count() ? tr("%1 mẫu tham khảo · tạo tệp mới").arg(list->count())
                                        : tr("Không có mẫu phù hợp. Kiểm tra gói mẫu tại Kho mẫu."));
        };
        auto create = [=] {
            auto* item = list->currentItem();
            if (!item) return;
            // Đọc lại gói đang cài: không sử dụng đường dẫn cũ sau khi cập nhật thư viện.
            const auto lib = tpl::installed();
            const QString rel = item->data(Qt::UserRole).toString();
            const QString root = QFileInfo(lib.root).canonicalFilePath();
            const QFileInfo fi(QDir(lib.root).filePath(rel));
            const QString src = fi.canonicalFilePath();
            if (!lib.valid || root.isEmpty() || !src.startsWith(root + QLatin1Char('/')) ||
                !fi.isFile() || fi.suffix().compare(ext, Qt::CaseInsensitive) != 0) {
                QMessageBox::warning(this, tr("Kho mẫu"), tr("Mẫu không còn hợp lệ. Hãy mở lại danh sách."));
                reload(); return;
            }
            QString dst = QFileDialog::getSaveFileName(this, tr("Lưu bản làm việc từ mẫu"),
                QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(fi.fileName()),
                tr("Tài liệu (*.%1)").arg(ext));
            if (dst.isEmpty()) return;
            if (!dst.endsWith(QLatin1Char('.') + ext, Qt::CaseInsensitive)) dst += QLatin1Char('.') + ext;
            QString error;
            if (!tpl::makeWorkingCopy(src, dst, &error)) {
                QMessageBox::warning(this, tr("Kho mẫu"), error); return;
            }
            emit templateCopyRequested(dst);
        };
        connect(search, &QLineEdit::textChanged, this, reload);
        connect(list, &QListWidget::currentItemChanged, this, [=](QListWidgetItem* item, QListWidgetItem*) {
            use->setEnabled(item != nullptr);
        });
        connect(use, &QPushButton::clicked, this, create);
        connect(list, &QListWidget::itemDoubleClicked, this, create);
        middle->addWidget(templates);
        auto* rail = new QWidget(this);
        auto* rv = new QVBoxLayout(rail);
        rv->setContentsMargins(0, 0, 0, 0);
        rv->addStretch(1);
        auto* toggle = panelHandle(rail, true, tr("Mẫu"), QStringLiteral("TemplatePanelHandle"));
        rv->addWidget(toggle);
        rv->addStretch(1);
        middle->addWidget(rail);
        initCollapsed(templates);
        connect(toggle, &QPushButton::clicked, this, [=] {
            const bool open = !panelExpanded(templates);
            if (open) reload();
            slidePanel(templates, open, 228, toggle, true, tr("Mẫu"));
        });
    }
    if (m_doc->kind() == DocKind::Presentation || m_doc->kind() == DocKind::Drawing) {
        m_slides = new QListWidget(this);
        m_slides->setFixedWidth(150);
        m_slides->setIconSize(QSize(128, 96));
        if (m_doc->kind()==DocKind::Presentation) {
            m_slides->setFixedWidth(174);
            m_slides->setViewMode(QListView::IconMode);
            m_slides->setFlow(QListView::TopToBottom);
            m_slides->setWrapping(false);
            m_slides->setMovement(QListView::Static);
            m_slides->setGridSize(QSize(148,132));
            m_slides->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_slideRefresh=new QTimer(this);
            m_slideRefresh->setSingleShot(true);
            m_slideRefresh->setInterval(250);
            connect(m_slideRefresh,&QTimer::timeout,this,[this]{
                const int part=m_slideRefreshPart;
                if(!m_pptNotes && part>=0 && part<m_slides->count() && part<m_doc->partCount())
                    pptSetThumbnail(m_slides->item(part),m_doc.get(),part);
            });
        }
        m_slides->setStyleSheet(QStringLiteral("QListWidget{background:#EEF2F7;border:none;border-right:1px solid #D9E1EC;}"
                                               "QListWidget::item{padding:10px;margin:4px;background:white;border-radius:6px;}"
                                               "QListWidget::item:selected{border:2px solid #1F5FBF;color:#1F5FBF;}"));
        middle->addWidget(m_slides);
        connect(m_slides, &QListWidget::currentRowChanged, this, [this](int row) {
            if (m_rebuildingParts || row < 0 || row == m_doc->currentPart()) return;
            if(m_slideRefresh && m_slideRefresh->isActive()) {
                m_slideRefresh->stop();
                const int old=m_slideRefreshPart;
                if(!m_pptNotes && old>=0 && old<m_slides->count() && old<m_doc->partCount())
                    pptSetThumbnail(m_slides->item(old),m_doc.get(),old);
            }
            m_doc->setPart(row);
            m_view->resetForPartChange();
            refreshPdfComparison();
            emit statusChanged();
        });
    }
    m_view = new TileView(m_doc.get(), this);
    middle->addWidget(m_view, 1);
    if (m_doc->kind() == DocKind::Spreadsheet && !m_doc->readOnly()) {
        m_excelTools = new QTabWidget(this);
        m_excelTools->setObjectName(QStringLiteral("ExcelTools"));
        m_excelTools->setStyleSheet(QStringLiteral(R"(
QTabWidget#ExcelTools::pane { border:1px solid #DCE5F2; background:#FBFCFF; }
QTabWidget#ExcelTools QTabBar::tab { background:#F4F8FF; color:#4A5A7C; padding:8px 10px; border:1px solid #DCE5F2; }
QTabWidget#ExcelTools QTabBar::tab:selected { background:white; color:#2864BA; border-bottom:2px solid #2864BA; font-weight:700; }
)"));
        auto addTool = [this](QWidget* tool, const QString& title) {
            auto* scroll = new QScrollArea(m_excelTools);
            scroll->setWidgetResizable(true);
            scroll->setWidget(tool);
            m_excelTools->addTab(scroll, title);
        };
        m_formulaAssistant = createFormulaAssistantWidget();
        notifyFormulaSheet(m_formulaAssistant,m_doc->currentPart(),qs(m_doc->partName(m_doc->currentPart())));
        addTool(m_formulaAssistant, tr("ƒx · Tôi muốn"));
        connect(m_view, &TileView::cellSelectionFinished, this, [this] {
            const int part = m_doc->currentPart();
            const auto sheet = m_doc->partName(part);
            const unsigned generation = ++m_pickGeneration;
            QTimer::singleShot(180, this, [this, part, sheet, generation] {
                auto current = [&] {
                    return generation == m_pickGeneration && isVisible() && m_doc && m_view &&
                        m_formulaAssistant && m_formulaAssistant->isVisible() &&
                        m_doc->currentPart() == part && m_doc->partName(part) == sheet;
                };
                if (!current()) return;
                m_doc->waitInputIdle(100, 700);
                if (!current()) return;
                const auto range = m_view->selectedCellRange(true);
                if (current() && !range.isEmpty()) notifyFormulaSelection(m_formulaAssistant, range);
            });
        });
        m_rangeTools = createDocumentRangeToolsWidget(this, [this] { return qs(m_doc->selectionAs("text/plain;charset=utf-8")); });
        addTool(m_rangeTools, tr("Vùng dữ liệu"));
        connect(m_excelTools, &QTabWidget::currentChanged, this, [this](int) { notifyDocumentRangeTools(m_rangeTools); });
        // Handle is on the document-facing edge of the right panel.
        auto* rail = new QWidget(this);
        auto* rv = new QVBoxLayout(rail);
        rv->setContentsMargins(0, 0, 0, 0);
        rv->addStretch(1);
        auto* toggle = panelHandle(rail, false, tr("Tôi\nmuốn"), QStringLiteral("ExcelPanelHandle"));
        rv->addWidget(toggle);
        rv->addStretch(1);
        middle->addWidget(rail);
        middle->addWidget(m_excelTools);
        initCollapsed(m_excelTools);
        connect(toggle, &QPushButton::clicked, this, [this, toggle] {
            slidePanel(m_excelTools, !panelExpanded(m_excelTools), 340, toggle, false, tr("Tôi\nmuốn"));
        });
    }
    if (m_doc->kind() == DocKind::Text && !m_doc->readOnly()) {
        auto* assistant = new QScrollArea(this);
        assistant->setWidgetResizable(true);
        assistant->setFixedWidth(282);
        assistant->setFrameShape(QFrame::NoFrame);
        auto* body = new QWidget;
        body->setObjectName(QStringLiteral("WordAssistant"));
        body->setAttribute(Qt::WA_StyledBackground, true);
        body->setStyleSheet(QStringLiteral(R"(
QWidget#WordAssistant { background:#FBFCFF; border-left:1px solid #DCE5F2; }
QWidget#WordAssistant QPushButton { background:white; border:1px solid #DCE5F2; border-radius:8px; padding:8px; text-align:left; color:#17294D; }
QWidget#WordAssistant QPushButton:hover, QWidget#WordAssistant QPushButton:focus { background:#E8F0FF; border-color:#2864BA; }
QLabel#AssistantGroup { color:#6B7A99; font-size:11px; font-weight:700; padding-top:8px; }
)"));
        auto* av = new QVBoxLayout(body);
        av->setContentsMargins(10, 10, 10, 10);
        av->setSpacing(5);
        auto* title = new QLabel(tr("Tôi muốn"), body);
        title->setObjectName(QStringLiteral("SectionTitle"));
        av->addWidget(title);
        auto group = [&](const QString& name) {
            auto* label = new QLabel(name, body);
            label->setObjectName(QStringLiteral("AssistantGroup"));
            av->addWidget(label);
        };
        auto action = [&](const QString& label, const char* id) {
            auto* button = new QPushButton(label, body);
            av->addWidget(button);
            const QString command = QString::fromLatin1(id);
            connect(button, &QPushButton::clicked, this, [this, command] {
                dung::word::handleAction(command, this, this);
            });
        };
        group(tr("TIÊU ĐỀ & TRANG"));
        action(tr("Tiêu đề 1 (đoạn hiện tại)"), "word:style1");
        action(tr("Tiêu đề 2 (đoạn hiện tại)"), "word:style2");
        action(tr("Tiêu đề 3 (đoạn hiện tại)"), "word:style3");
        for (bool header : {true, false}) {
            auto* button = new QPushButton(header ? tr("Bật đầu trang (Header)") : tr("Bật chân trang (Footer)"), body);
            av->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, header] {
                bool ok = false;
                const QString style = QInputDialog::getText(this, tr("Đầu / chân trang"),
                    tr("Kiểu trang cần bật (Page Style). Chỉ bật, không xóa nội dung đang có. Sau đó bấm vào vùng đầu / chân trang để nhập."),
                    QLineEdit::Normal, QStringLiteral("Default Page Style"), &ok).trimmed();
                if (!ok || style.isEmpty()) return;
                m_doc->waitInputIdle();
                const auto args = dung::word::join({dung::word::argString("PageStyle", ss(style)), dung::word::argBool("On", true)});
                m_doc->command(header ? ".uno:InsertPageHeader" : ".uno:InsertPageFooter", args);
                m_view->setFocus();
            });
        }
        group(tr("VĂN BẢN CHUẨN VIỆT"));
        action(tr("Căn lề hành chính Việt Nam"), "word:marginVN");
        action(tr("Quốc hiệu / đầu văn bản"), "word:adminHead");
        action(tr("Nơi nhận / chữ ký"), "word:adminTail");
        action(tr("Khung văn bản hành chính"), "word:adminFull");
        action(tr("Khổ A4 dọc"), "word:a4");
        group(tr("TIỆN ÍCH TIẾNG VIỆT"));
        action(tr("Số tiền thành chữ"), "word:vnMoney");
        action(tr("Chèn ngày tháng Việt Nam"), "word:vnDate");
        action(tr("Chuyển TCVN3 sang Unicode"), "word:vnTcvn3");
        action(tr("Kiểm tra âm tiết"), "word:vnSyllable");
        action(tr("Kiểm tra tài liệu"), "word:inspect");
        av->addStretch(1);
        assistant->setWidget(body);
        auto* rail = new QWidget(this);
        auto* rv = new QVBoxLayout(rail);
        rv->setContentsMargins(0, 0, 0, 0);
        rv->addStretch(1);
        auto* toggle = panelHandle(rail, false, tr("Tôi\nmuốn"), QStringLiteral("WordPanelHandle"));
        rv->addWidget(toggle);
        rv->addStretch(1);
        middle->addWidget(rail);
        middle->addWidget(assistant);
        initCollapsed(assistant);
        connect(toggle, &QPushButton::clicked, this, [=] {
            slidePanel(assistant, !panelExpanded(assistant), 282, toggle, false, tr("Tôi\nmuốn"));
        });
    }
    if (m_pdfCompareButton) {
        m_pdfComparison = new QWidget(this);
        m_pdfComparison->setMinimumWidth(230); m_pdfComparison->setMaximumWidth(360);
        auto* layout = new QVBoxLayout(m_pdfComparison);
        layout->setContentsMargins(8,8,8,8);
        auto* heading = new QLabel(tr("PDF gốc · chỉ đọc"),m_pdfComparison);
        heading->setStyleSheet(QStringLiteral("font-weight:700;color:#17294D;"));
        layout->addWidget(heading);
        auto* scroll = new QScrollArea(m_pdfComparison);
        scroll->setWidgetResizable(true);
        m_pdfOriginalImage = new QLabel(scroll);
        m_pdfOriginalImage->setAlignment(Qt::AlignTop|Qt::AlignHCenter);
        m_pdfOriginalImage->setWordWrap(true);
        scroll->setWidget(m_pdfOriginalImage); layout->addWidget(scroll,1);
        m_pdfComparison->hide(); middle->addWidget(m_pdfComparison);
        connect(m_pdfCompareButton,&QPushButton::toggled,this,[this](bool on) {
            m_pdfComparison->setVisible(on);
            if (on) { m_pdfComparedPart=-1; refreshPdfComparison(); }
        });
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
        sh->setContentsMargins(8,2,8,2);
        sh->setSpacing(4);
        m_sheets->setUsesScrollButtons(true);
        m_sheets->setElideMode(Qt::ElideRight);
        auto sheetArrow=[this](Qt::ArrowType type,const QString& name,int delta){
            auto* b=new QToolButton(this);b->setArrowType(type);b->setFixedSize(24,26);
            b->setFocusPolicy(Qt::NoFocus);b->setToolTip(name);b->setAccessibleName(name);
            connect(b,&QToolButton::clicked,this,[this,delta]{
                if(m_sheets->count())m_sheets->setCurrentIndex(qBound(0,m_sheets->currentIndex()+delta,m_sheets->count()-1));
            });
            return b;
        };
        sh->addWidget(sheetArrow(Qt::LeftArrow,tr("Sheet trước"),-1));
        sh->addWidget(sheetArrow(Qt::RightArrow,tr("Sheet sau"),1));
        sh->addWidget(m_sheets,1);
        auto* addSheet=new QPushButton(QStringLiteral("+"),this);
        addSheet->setFixedSize(28,28);addSheet->setToolTip(tr("Thêm sheet mới"));addSheet->setAccessibleName(addSheet->toolTip());
        addSheet->setEnabled(!readOnly());addSheet->setFocusPolicy(Qt::NoFocus);
        connect(addSheet,&QPushButton::clicked,this,[this]{excelAction(QStringLiteral("xl:sheetAdd"));});
        sh->addWidget(addSheet);
        v->addLayout(sh);
        m_sheets->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_sheets, &QTabBar::customContextMenuRequested, this, [this](const QPoint& p) { excelSheetMenu(p); });
        connect(m_sheets, &QTabBar::currentChanged, this, [this](int idx) {
            if (m_rebuildingParts || idx < 0 || idx == m_doc->currentPart()) return;
            ++m_pickGeneration;
            m_doc->setPart(idx);
            notifyFormulaSheet(m_formulaAssistant,m_doc->currentPart(),qs(m_doc->partName(m_doc->currentPart())));
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
    slidePanel(m_excelTools, true, 340, findChild<QPushButton*>(QStringLiteral("ExcelPanelHandle")), false, tr("Tôi\nmuốn"));
    if (page == 1 && m_rangeTools) notifyDocumentRangeTools(m_rangeTools);
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
    if (m_pdfCompareButton) m_pdfCompareButton->setVisible(m_recOrigin.endsWith(QLatin1String(".pdf"),Qt::CaseInsensitive));
}

void DocumentTab::setCopyOrigin(const QString& originalPath) {
    m_recOrigin = originalPath;
    m_recOriginSet = true;
    m_dirtySinceAutosave = true;  // bản sao chưa lưu ở đâu cả: tự lưu ngay ở nhịp tới
    if (m_pdfCompareButton) m_pdfCompareButton->setVisible(m_recOrigin.endsWith(QLatin1String(".pdf"),Qt::CaseInsensitive));
}

void DocumentTab::refreshPdfComparison() {
    if (!m_pdfComparison || m_pdfComparison->isHidden() || !m_pdfOriginalImage) return;
    const int part = m_doc->currentPart();
    if (part == m_pdfComparedPart) return;
    m_pdfComparedPart = part;
    QString error;
    const QImage image = printrender::renderPage(m_recOrigin,part+1,QSize(340,1200),&error);
    if (image.isNull()) {
        m_pdfOriginalImage->setPixmap(QPixmap());
        m_pdfOriginalImage->setText(tr("Không xem được trang gốc %1: %2").arg(part+1).arg(error));
    } else {
        m_pdfOriginalImage->setPixmap(QPixmap::fromImage(image));
        m_pdfOriginalImage->setToolTip(tr("Trang %1 của PDF gốc; bản sửa hiển thị ở vùng làm việc bên cạnh.").arg(part+1));
    }
}

void DocumentTab::dropRecovery() {
    if (!m_recId.isEmpty()) recovery::Session::instance().discard(m_recId);
    m_recId.clear();
    if (m_hasAdopted) { recovery::discardEntry(m_adopted); m_hasAdopted = false; }
}

void DocumentTab::autosaveTick() {
    auto& rs = recovery::Session::instance();
    if (!rs.active() || !m_doc || m_doc->readOnly()) return;
    if (!m_doc->modified()) return;
    // Even without an invalidate callback, a dirty document with no recovery
    // snapshot yet must be saved at the next tick.
    if (!m_dirtySinceAutosave && !m_recId.isEmpty()) return;
    if (!rs.hasRoom()) return;  // thư mục phục hồi đã đầy: bỏ qua, không làm gì khác
    const std::string ext = m_doc->recoveryExt();
    if (ext.empty()) return;
    m_doc->waitInputIdle();
    if (!m_doc->modified()) return;
    // Keep retry pending until both data and metadata have been committed.
    m_dirtySinceAutosave=true;
    if (m_recId.isEmpty()) m_recId = rs.newDocId();
    const QString e = qs(ext);
    std::string err;
    if (!m_doc->saveRecoveryCopy(toPath(rs.dataPath(m_recId, e)), &err)) return;
    const QString orig = m_doc->isNew() ? QString() : fromPath(m_doc->originalPath());
    const QString original = recovery::snapshotOrigin(m_savedPath,m_recOriginSet,m_recOrigin,orig);
    const QString name=m_savedPath.isEmpty()?qs(m_doc->displayName()):QFileInfo(m_savedPath).fileName();
    if (rs.commit(m_recId, e, name, original, original.isEmpty())) {
        m_dirtySinceAutosave = false;
        if (m_hasAdopted) { recovery::discardEntry(m_adopted); m_hasAdopted = false; }  // đã có bản mới trong phiên này
    }
}

QString DocumentTab::activeFilePath() const {
    if (!m_savedPath.isEmpty()) return m_savedPath;
    if (m_doc->isNew()) return {};
    return fromPath(m_doc->originalPath());
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
    case DocKind::Presentation: s += (m_pptNotes ? tr("  •  Ghi chú %1/%2") : tr("  •  Trang chiếu %1/%2")).arg(cur).arg(n); break;
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
        if(m_slideRefresh) m_slideRefresh->stop();
        m_slides->clear();
        for (int i = 0; i < n; ++i) {
            m_slides->addItem(m_doc->kind() == DocKind::Presentation ? tr("Trang chiếu %1").arg(i + 1)
                                                                    : tr("Trang %1").arg(i + 1));
            if (m_doc->kind() == DocKind::Presentation && !m_pptNotes) pptSetThumbnail(m_slides->item(i), m_doc.get(), i);
        }
        m_slides->setCurrentRow(cur);
    }
    m_rebuildingParts = false;
}

void DocumentTab::onEngineEvent(int type, const QString& payload) {
    m_view->handleEvent(type, payload);
    if (m_rangeTools && (type == LOK_CALLBACK_CELL_ADDRESS || type == LOK_CALLBACK_CELL_SELECTION_AREA ||
        type == LOK_CALLBACK_CELL_FORMULA || type == LOK_CALLBACK_SET_PART))
        notifyDocumentRangeTools(m_rangeTools);
    if(type==LOK_CALLBACK_INVALIDATE_TILES && m_slideRefresh && !m_pptNotes) {
        m_slideRefreshPart=m_doc->currentPart();
        m_slideRefresh->start();
    }
    const bool modified = m_doc->modified();
    // The first invalidation can arrive before ModifiedStatus=true.
    // A transition to dirty must independently schedule a recovery snapshot.
    if (modified && (type == LOK_CALLBACK_INVALIDATE_TILES || !m_lastMod))
        m_dirtySinceAutosave = true;
    if (modified != m_lastMod) {
        m_lastMod = modified;
        emit titleChanged();
        emit statusChanged();
    }
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
        ++m_pickGeneration;
        if(m_formulaAssistant) notifyFormulaSheet(m_formulaAssistant,m_doc->currentPart(),qs(m_doc->partName(m_doc->currentPart())));
        refreshPdfComparison();
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
// Do not issue a destructive Cut when the current selection cannot be
// transferred to the system clipboard using the supported formats.
static bool publishSelection(dung::Document* doc) {
    auto* mime = new QMimeData;
    const std::string html = doc->selectionAs("text/html");
    const std::string text = doc->selectionAs("text/plain;charset=utf-8");
    if (!html.empty()) mime->setHtml(qs(html));
    if (!text.empty()) mime->setText(qs(text));
    if (html.empty() && text.empty()) { delete mime; return false; }
    auto* clipboard = QApplication::clipboard();
    clipboard->setMimeData(mime);
    // The platform owns mime after setMimeData; do not dereference it.
    const auto* actual = clipboard->mimeData();
    return actual && ((!html.empty() && actual->hasHtml() && actual->html() == qs(html)) ||
                      (!text.empty() && actual->hasText() && actual->text() == qs(text)));
}

void DocumentTab::copy() {
    if (!m_doc) return;
    m_doc->waitInputIdle();
    if (publishSelection(m_doc.get())) m_doc->command(".uno:Copy");
}

void DocumentTab::cut() {
    if (!m_doc || m_doc->readOnly()) return;
    m_doc->waitInputIdle();
    if (!publishSelection(m_doc.get())) {
        QMessageBox::information(this, tr("Chưa cắt nội dung"),
            tr("Không sao chép được vùng chọn vào clipboard. Nội dung gốc được giữ nguyên. Hãy chọn văn bản hoặc ô có dữ liệu rồi thử lại."));
        return;
    }
    m_doc->command(".uno:Cut");
}

void DocumentTab::paste() {
    if (!m_doc || m_doc->readOnly()) return;
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime) return;
    // Capture both representations before calling the engine: clipboard
    // ownership may change during the first paste attempt.
    const QString html = mime->hasHtml() ? mime->html() : QString();
    const QString text = mime->hasText() ? mime->text() : QString();
    m_doc->waitInputIdle();
    bool ok = false;
    if (!html.isEmpty()) ok = m_doc->paste("text/html", ss(html));
    if (!ok && !text.isEmpty()) ok = m_doc->paste("text/plain;charset=utf-8", ss(text));
    if (!ok && (!html.isEmpty() || !text.isEmpty()))
        QMessageBox::warning(this, tr("Chưa dán được"), tr("Engine chưa xác nhận thao tác dán."));
    if (m_view) m_view->setFocus();
}

void DocumentTab::setZoom(int percent) { m_view->setZoom(percent); }

void DocumentTab::setPptNotesMode(bool notes) {
    if (m_doc->kind() != DocKind::Presentation || m_pptNotes == notes) return;
    m_doc->waitInputIdle();
    m_doc->setPartMode(notes ? 1 : 0); // LOK_PARTMODE_NOTES / SLIDES, verified in ppt_tests.cpp
    m_doc->waitInputIdle();
    m_pptNotes = notes;
    m_view->resetForPartChange();
    rebuildParts();
    m_view->setFocus();
    emit statusChanged();
}
void DocumentTab::stepPart(int delta) {
    if (!m_slides || m_slides->count() == 0) return;
    const int r = qBound(0, m_slides->currentRow() + delta, m_slides->count() - 1);
    m_slides->setCurrentRow(r);
}
int DocumentTab::zoom() const { return m_view->zoom(); }

// ---- Lưu an toàn ----
static bool samePathUi(const QString& a, const QString& b) {
    return savepath::samePathForProtection(a,b);
}

bool DocumentTab::checkSaveConflict(const QString& path) {
    if (!m_saveConflictCheck || !m_saveConflictCheck(path)) return true;
    QMessageBox::warning(this,tr("Tệp đang mở"),
        tr("Tệp đích đang được mở ở tab khác. Hãy lưu sang tên khác hoặc đóng tab đó trước. Chưa ghi đè dữ liệu."));
    return false;
}

bool DocumentTab::doSave(const QString& path, bool confirmedReplace) {
    if (!checkSaveConflict(path)) return false;
    dung::SaveOptions opts;
    const bool isPdf = path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive);
    const QString original = m_doc->isNew() ? QString() : fromPath(m_doc->originalPath());

    if (m_doc->kind() == DocKind::Drawing && m_recOriginSet && !m_recOrigin.isEmpty() && samePathUi(path, m_recOrigin)) {
        QMessageBox::warning(this, tr("Bảo vệ PDF gốc"), tr("Chọn tên mới, không ghi đè PDF nguồn của bản sao chỉnh sửa."));
        return false;
    }

    if ((!original.isEmpty() && samePathUi(path,original)) ||
        (m_recOriginSet && !m_recOrigin.isEmpty() && samePathUi(path,m_recOrigin))) {
        if(m_doc->readOnly() || !m_doc->featureLoss().empty()) {
            QMessageBox::warning(this,tr("Bảo vệ tệp gốc"),tr("Tài liệu chỉ đọc hoặc có thành phần chưa bảo toàn. Không được ghi đè tệp nguồn, kể cả khi đã đồng ý mất tính năng."));
            return false;
        }
    }
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

    if (!checkSaveConflict(path)) return false;
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
    QStringList nativeExtensions;
    for (const auto* f : natives) {
        filters << qs(f->label) + QStringLiteral(" (*.") + qs(f->ext) + QLatin1Char(')');
        nativeExtensions << qs(f->ext);
    }
    const QString suggested = m_savedPath.isEmpty() ? fromPath(m_doc->suggestSaveAsPath()) : m_savedPath;
    QString selected = filters.first();
    const QString picked = QFileDialog::getSaveFileName(this, tr("Lưu thành"), suggested,
        filters.join(QStringLiteral(";;")), &selected, QFileDialog::DontConfirmOverwrite);
    if (picked.isEmpty()) return false;
    const int i = filters.indexOf(selected);
    const QString path = savepath::resolve(picked, qs(natives[std::max(0, i)]->ext));
    // Save As must keep an editable format; PDF export is a separate action.
    if (!savepath::nativeSuffixAllowed(path,nativeExtensions)) {
        QMessageBox::warning(this,tr("Định dạng lưu không phù hợp"),
            tr("Chọn một định dạng tài liệu chỉnh sửa được: %1. Muốn tạo PDF, dùng Xuất PDF.").arg(nativeExtensions.join(QStringLiteral(", "))));
        return false;
    }
    if (!checkSaveConflict(path)) return false;
    const bool exists = savepath::needsConfirmation(path);
    if (exists && QMessageBox::question(this, tr("Xác nhận thay thế"),
        tr("Tệp đích đã tồn tại:\n%1\nBạn có muốn thay thế tệp này không?").arg(QDir::toNativeSeparators(path)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return false;
    return doSave(path, exists);
}

bool DocumentTab::exportPdf() {
    QString base = m_savedPath.isEmpty() ? fromPath(m_doc->suggestSaveAsPath("pdf"))
                                         : QFileInfo(m_savedPath).absolutePath() + QLatin1Char('/') +
                                               QFileInfo(m_savedPath).completeBaseName() + QStringLiteral(".pdf");
    const QString picked = QFileDialog::getSaveFileName(this, tr("Xuất PDF"), base,
        tr("PDF (*.pdf)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (picked.isEmpty()) return false;
    const QString path = savepath::resolve(picked, QStringLiteral("pdf"), true);
    if (!checkSaveConflict(path)) return false;
    const bool exists = savepath::needsConfirmation(path);
    if (exists && QMessageBox::question(this, tr("Xác nhận thay thế PDF"),
        tr("Tệp đích đã tồn tại:\n%1\nBạn có muốn thay thế tệp này không?").arg(QDir::toNativeSeparators(path)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return false;
    return doSave(path, exists);
}

bool DocumentTab::printDocument() {
    // Xem trước và in dùng CÙNG MỘT tệp PDF → không lệch giữa xem trước và bản in.
    QTemporaryDir tmp;
    QString pdfPath;
    const QString original = m_doc->isNew() ? QString() : fromPath(m_doc->originalPath());
    if (m_doc->kind() == DocKind::Drawing && m_doc->readOnly() && original.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive) && QFileInfo::exists(original)) {
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
    if (box.clickedButton() == saveBtn) {
        if (!save()) return false;
        // Never close while the document still has unsaved changes.
        if (m_doc->modified()) {
            QMessageBox::warning(this,tr("Chưa lưu xong"),tr("Tài liệu vẫn có thay đổi chưa lưu. Giữ thẻ mở để bảo vệ nội dung."));
            return false;
        }
        return true;
    }
    if (box.clickedButton() == discard) { dropRecovery(); return true; }
    return false;
}
