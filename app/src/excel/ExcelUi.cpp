// SPDX-License-Identifier: MPL-2.0
// Giao diện Excel: hành động "xl:*" từ ribbon và menu chuột phải trên thẻ sheet.
// Chỉ dùng lệnh UNO đã kiểm chứng (ExcelCommands.h). Mọi thao tác phá huỷ đều hỏi xác nhận.
#include "../DocumentTab.h"
#include "../EngineHost.h"
#include "../TileView.h"
#include "ExcelCommands.h"

#include <QColorDialog>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTabBar>

namespace ex = dung::excel;

static bool askName(QWidget* parent, const QString& title, const QString& initial, QString* out,
                    const QTabBar* tabs, int ignoreIndex) {
    for (;;) {
        bool ok = false;
        const QString n = QInputDialog::getText(parent, title, QObject::tr("Tên sheet (tối đa 31 ký tự, không chứa [ ] : * ? / \\):"),
                                                QLineEdit::Normal, initial, &ok).trimmed();
        if (!ok) return false;
        bool dup = false;
        for (int i = 0; i < tabs->count(); ++i)
            if (i != ignoreIndex && tabs->tabText(i).compare(n, Qt::CaseInsensitive) == 0) dup = true;
        if (!ex::validSheetName(ss(n))) {
            QMessageBox::warning(parent, title, QObject::tr("Tên sheet không hợp lệ."));
        } else if (dup) {
            QMessageBox::warning(parent, title, QObject::tr("Đã có sheet tên này."));
        } else { *out = n; return true; }
    }
}

void DocumentTab::excelSheetMenu(const QPoint& pos) {
    if (!m_sheets || readOnly()) return;
    const int i = m_sheets->tabAt(pos);
    if (i >= 0 && i != m_sheets->currentIndex()) m_sheets->setCurrentIndex(i);
    QMenu menu(this);
    menu.addAction(tr("Thêm sheet"), this, [this] { excelAction(QStringLiteral("xl:sheetAdd")); });
    if (i >= 0) {
        menu.addAction(tr("Đổi tên…"), this, [this] { excelAction(QStringLiteral("xl:sheetRename")); });
        menu.addAction(tr("Nhân đôi"), this, [this] { excelAction(QStringLiteral("xl:sheetDup")); });
        menu.addAction(tr("Xoá sheet…"), this, [this] { excelAction(QStringLiteral("xl:sheetDelete")); });
    }
    menu.exec(m_sheets->mapToGlobal(pos));
}

void DocumentTab::excelAction(const QString& id) {
    if (readOnly() || !m_doc || m_doc->kind() != dung::DocKind::Spreadsheet) return;
    const QString a = id.mid(3);
    auto cmd = [this](const char* c, const std::string& args = std::string()) { m_doc->command(c, args); };
    const int cur = m_doc->currentPart();
    const int n = m_doc->partCount();

    if (a == QLatin1String("sheetAdd")) {
        QString name;
        if (!askName(this, tr("Thêm sheet"), tr("Sheet%1").arg(n + 1), &name, m_sheets, -1)) return;
        cmd(".uno:Insert", ex::sheetInsert(ss(name), n + 1)); // thêm vào cuối
    } else if (a == QLatin1String("sheetRename")) {
        QString name;
        if (!askName(this, tr("Đổi tên sheet"), qs(m_doc->partName(cur)), &name, m_sheets, cur)) return;
        cmd(".uno:RenameTable", ex::sheetRename(ss(name)));
    } else if (a == QLatin1String("sheetDelete")) {
        if (n <= 1) { QMessageBox::information(this, tr("Xoá sheet"), tr("Không thể xoá sheet duy nhất của tệp.")); return; }
        if (QMessageBox::question(this, tr("Xoá sheet"),
                tr("Xoá sheet \"%1\" cùng toàn bộ dữ liệu, công thức, ghi chú trong đó?\nBản gốc trên đĩa không bị ảnh hưởng cho đến khi bạn Lưu thành.")
                    .arg(qs(m_doc->partName(cur))), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        cmd(".uno:Remove", ex::sheetRemove(cur + 1));
    } else if (a == QLatin1String("sheetDup")) {
        const QString stem = fromPath(m_doc->workingCopy().stem());
        cmd(".uno:Move", ex::sheetMove(ss(stem), cur + 2, true));
    } else if (a == QLatin1String("fontName")) {
        bool ok = false;
        const QString f = QInputDialog::getItem(this, tr("Phông chữ"), tr("Chọn phông:"), QFontDatabase::families(), 0, true, &ok);
        if (ok && !f.trimmed().isEmpty()) cmd(".uno:CharFontName", ex::fontName(ss(f.trimmed())));
    } else if (a == QLatin1String("fontSize")) {
        bool ok = false;
        const int pt = QInputDialog::getInt(this, tr("Cỡ chữ"), tr("Cỡ chữ (pt):"), 11, 4, 96, 1, &ok);
        if (ok) cmd(".uno:FontHeight", ex::fontHeight(pt));
    } else if (a == QLatin1String("textColor") || a == QLatin1String("fillColor")) {
        const QColor c = QColorDialog::getColor(Qt::black, this, a == QLatin1String("textColor") ? tr("Màu chữ") : tr("Màu nền ô"));
        if (!c.isValid()) return;
        const unsigned rgb = ((unsigned)c.red() << 16) | ((unsigned)c.green() << 8) | (unsigned)c.blue();
        if (a == QLatin1String("textColor")) cmd(".uno:Color", ex::textColor(rgb));
        else cmd(".uno:BackgroundColor", ex::fillColor(rgb));
    } else if (a == QLatin1String("colWidth")) {
        bool ok = false;
        const double cm = QInputDialog::getDouble(this, tr("Độ rộng cột"), tr("Độ rộng (cm):"), 2.5, 0.2, 50, 2, &ok);
        if (ok) cmd(".uno:ColumnWidth", ex::columnWidth(ex::cmToHundredthMm(cm)));
    } else if (a == QLatin1String("rowHeight")) {
        bool ok = false;
        const double cm = QInputDialog::getDouble(this, tr("Chiều cao hàng"), tr("Chiều cao (cm):"), 0.5, 0.1, 20, 2, &ok);
        if (ok) cmd(".uno:RowHeight", ex::rowHeight(ex::cmToHundredthMm(cm)));
    } else if (a == QLatin1String("replace")) {
        bool ok = false;
        const QString f = QInputDialog::getText(this, tr("Tìm và thay thế"), tr("Tìm:"), QLineEdit::Normal, QString(), &ok);
        if (!ok || f.isEmpty()) return;
        const QString r = QInputDialog::getText(this, tr("Tìm và thay thế"), tr("Thay bằng:"), QLineEdit::Normal, QString(), &ok);
        if (!ok) return;
        if (QMessageBox::question(this, tr("Tìm và thay thế"),
                tr("Thay tất cả \"%1\" bằng \"%2\" trong giá trị và văn bản của sheet này.\nCông thức KHÔNG bị thay đổi. Tiếp tục?").arg(f, r),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        cmd(".uno:ExecuteSearch", ex::replaceAll(ss(f), ss(r)));
    } else if (a == QLatin1String("find")) {
        bool ok = false;
        const QString f = QInputDialog::getText(this, tr("Tìm"), tr("Tìm:"), QLineEdit::Normal, QString(), &ok);
        if (ok && !f.isEmpty()) cmd(".uno:ExecuteSearch", ex::findNext(ss(f)));
    }
    if (a.startsWith(QLatin1String("sheet"))) { rebuildParts(); m_view->resetForPartChange(); emit statusChanged(); }
    m_view->setFocus();
}
