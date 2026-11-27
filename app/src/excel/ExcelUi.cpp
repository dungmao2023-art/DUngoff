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
#include <QPushButton>
#include <QTabBar>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include "../vntext/VnText.h"
#include "../tools/dataassist/ExcelBridge.h"

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
    if (!m_doc || readOnly() || m_doc->kind() != dung::DocKind::Spreadsheet) return;
    m_doc->waitInputIdle();
    if (readOnly() || !m_sheets || !m_view) return;
    const QString a = id.mid(3);
    auto cmd = [this](const char* c, const std::string& args = std::string()) { m_doc->command(c, args); };
    const int cur = m_doc->currentPart();
    const int n = m_doc->partCount();
    const auto* originalDoc = m_doc.get();
    const auto originalSheet = m_doc->partName(cur);
    auto sameSheet = [&] {
        if (!m_doc || m_doc.get() != originalDoc || readOnly()) return false;
        m_doc->waitInputIdle();
        return m_doc->currentPart() == cur && m_doc->partCount() == n && m_doc->partName(cur) == originalSheet;
    };

    if (a==QLatin1String("fillDown") || a==QLatin1String("fillRight")) {
        const bool down=a==QLatin1String("fillDown");
        const QString range=m_view->selectedCellRange();
        const QString title=down?tr("Điền xuống"):tr("Điền sang phải");
        const auto address=dataassist::bridgeRangeAddress(ss(range));
        const auto ends=range.split(QLatin1Char(':'));
        if(address.empty() || ends.size()!=2) {
            QMessageBox::information(this,title,tr("Chọn vùng ô liền nhau gồm ô nguồn và ô đích trong vùng nhìn thấy."));return;
        }
        auto coords=[](const QString& cell){
            int pos=0,col=0;
            while(pos<cell.size()&&cell[pos].unicode()>='A'&&cell[pos].unicode()<='Z'){col=col*26+cell[pos].unicode()-'A'+1;++pos;}
            return QPair<int,int>(cell.mid(pos).toInt(),col);
        };
        const auto first=coords(ends.first()),last=coords(ends.last());
        if(first.first<1||first.second<1||last.first<first.first||last.second<first.second||
           (down?last.first==first.first:last.second==first.second)){
            QMessageBox::information(this,title,down?tr("Chọn ít nhất hai hàng."):tr("Chọn ít nhất hai cột."));return;
        }
        const qint64 targets=down?qint64(last.first-first.first)*(last.second-first.second+1):
            qint64(last.second-first.second)*(last.first-first.first+1);
        if(targets>100000){
            QMessageBox::warning(this,title,tr("Mỗi lần điền tối đa 100.000 ô đích. Hãy chia vùng nhỏ hơn."));return;
        }
        const auto before=m_doc->selectionAs("text/plain;charset=utf-8");
        if(QMessageBox::question(this,title,
           tr("Sheet: %1 · vùng %2\nSao chép %3 vào %4 ô đích. Công thức tương đối sẽ được điều chỉnh; dữ liệu và định dạng ở ô đích có thể bị thay thế, kể cả ô hiển thị trống. Tiếp tục?")
            .arg(qs(originalSheet),range,down?tr("hàng đầu"):tr("cột đầu")).arg(targets),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!sameSheet()||!m_view||m_view->selectedCellRange()!=range||
           m_doc->selectionAs("text/plain;charset=utf-8")!=before){
            QMessageBox::warning(this,title,tr("Vùng hoặc nội dung đã thay đổi trong lúc xác nhận. Chưa điền dữ liệu."));return;
        }
        cmd(down?".uno:FillDown":".uno:FillRight");
        m_doc->waitInputIdle();m_view->setFocus();return;
    } else if (a == QLatin1String("insertChart")) {
        m_doc->waitInputIdle();
        const QString range=m_view->selectedCellRange();
        if(dataassist::bridgeRangeAddress(ss(range)).empty()||!range.contains(QLatin1Char(':'))) {
            QMessageBox::information(this,tr("Biểu đồ"),tr("Chọn vùng dữ liệu liền nhau gồm nhãn và giá trị ngay trên sheet, sau đó bấm Biểu đồ cột."));
            return;
        }
        // Native Calc chart insertion, not an exported copy, screenshot or manual XML patch.
        // Do not dispatch another edit while the engine is processing this command.
        m_view->setFocus();
        cmd(".uno:InsertObjectChart");
        return;
    } else if (a == QLatin1String("vnMoney")) {
        m_doc->waitInputIdle();
        const QString range = m_view->selectedCellRange();
        if (dataassist::bridgeRangeAddress(ss(range)).empty()) {
            QMessageBox::information(this, tr("Số → chữ"), tr("Hãy chọn một ô hoặc vùng số liền nhau, nằm trong màn hình.")); return;
        }
        const QString raw = qs(m_doc->selectionAs("text/plain;charset=utf-8"));
        const QString value = raw.trimmed();
        bool ok = false;
        const QStringList modes{tr("Tiền đồng · dấu phẩy thập phân"), tr("Tiền đồng · dấu chấm thập phân"),
                                tr("Số thường · dấu phẩy thập phân"), tr("Số thường · dấu chấm thập phân")};
        const auto mode = QInputDialog::getItem(this, tr("Số → chữ"), tr("Vùng %1 · chọn định dạng số hiển thị:").arg(range), modes, 0, false, &ok);
        if (!ok) return;
        const int style = modes.indexOf(mode);
        vntext::MoneyOptions options; options.currency = style < 2; options.chan = options.currency;
        options.decimalSeparator = style % 2 == 0 ? QLatin1Char(',') : QLatin1Char('.');
        if (range.contains(QLatin1Char(':'))) {
            auto coords = [](const QString& address) {
                int i = 0, col = 0;
                while (i < address.size() && address.at(i).isLetter()) { col = col * 26 + address.at(i).unicode() - 'A' + 1; ++i; }
                return QPair<int,int>(address.mid(i).toInt(), col);
            };
            const auto ends = range.split(QLatin1Char(':'));
            const auto p = coords(ends.first()), q = coords(ends.last());
            const int rows = q.first - p.first + 1, cols = q.second - p.second + 1;
            const auto result = vntext::rangeToWords(raw, rows, cols, options);
            if (!result.ok) { QMessageBox::warning(this, tr("Số → chữ"), result.error); return; }
            QDialog dialog(this); dialog.setWindowTitle(tr("Xem trước số → chữ · %1").arg(range)); dialog.resize(760, 420);
            auto* layout = new QVBoxLayout(&dialog);
            auto* note = new QLabel(tr("%1 ô đã đổi, %2 ô có lưu ý về số 0 hoặc định dạng. Sao chép kết quả rồi dán vào vùng đích mới; công thức và dữ liệu nguồn không bị sửa.").arg(result.converted).arg(result.warnings), &dialog);
            note->setWordWrap(true); layout->addWidget(note);
            auto* preview = new QPlainTextEdit(result.text, &dialog); preview->setReadOnly(true); preview->setLineWrapMode(QPlainTextEdit::NoWrap); layout->addWidget(preview, 1);
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
            auto* copy = buttons->addButton(tr("Sao chép vùng kết quả"), QDialogButtonBox::AcceptRole);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            QObject::connect(copy, &QPushButton::clicked, &dialog, &QDialog::accept);
            layout->addWidget(buttons);
            if (dialog.exec() == QDialog::Accepted) QApplication::clipboard()->setText(result.text);
            return;
        }
        const auto result = vntext::moneyToWords(value, options);
        if (!result.ok) { QMessageBox::warning(this, tr("Số → chữ"), result.error); return; }
        QMessageBox box(this);
        box.setWindowTitle(tr("Số → chữ")); box.setText(result.text);
        box.setInformativeText(tr("Sao chép để dán vào ô khác (giữ nguyên số/công thức nguồn), hoặc thay ô đang chọn.") + (result.warnings.isEmpty() ? QString() : QStringLiteral("\n") + result.warnings.join(QStringLiteral("; "))));
        auto* copy = box.addButton(tr("Sao chép"), QMessageBox::AcceptRole);
        auto* replace = box.addButton(tr("Thay ô"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel); box.setDefaultButton(copy); box.exec();
        if (box.clickedButton() == copy) QApplication::clipboard()->setText(result.text);
        else if (box.clickedButton() == replace) {
            if (m_doc->currentPart() != cur || m_view->selectedCellRange() != range || qs(m_doc->selectionAs("text/plain;charset=utf-8")).trimmed() != value) {
                QMessageBox::warning(this, tr("Số → chữ"), tr("Vùng chọn đã đổi. Chưa ghi dữ liệu.")); return;
            }
            if (QMessageBox::question(this, tr("Thay số hoặc công thức?"), tr("Thay %1 bằng chữ? Công thức gốc (nếu có) sẽ bị thay thế.").arg(range), QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
                m_doc->waitInputIdle();
                if (!sameSheet() || m_view->selectedCellRange() != range ||
                    qs(m_doc->selectionAs("text/plain;charset=utf-8")).trimmed() != value) {
                    QMessageBox::warning(this, tr("Số → chữ"), tr("Ô hoặc giá trị đã đổi trong lúc xác nhận; chưa ghi dữ liệu.")); return;
                }
                if (!m_doc->paste("text/plain;charset=utf-8", ss(result.text))) QMessageBox::warning(this, tr("Số → chữ"), tr("Engine chưa xác nhận chèn."));
            }
        }
    } else if (a == QLatin1String("sheetAdd")) {
        QString name;
        if (!askName(this, tr("Thêm sheet"), tr("Sheet%1").arg(n + 1), &name, m_sheets, -1)) return;
        if (!sameSheet()) return;
        cmd(".uno:Insert", ex::sheetInsert(ss(name), n + 1)); // thêm vào cuối
    } else if (a == QLatin1String("sheetRename")) {
        QString name;
        if (!askName(this, tr("Đổi tên sheet"), qs(m_doc->partName(cur)), &name, m_sheets, cur)) return;
        if (!sameSheet()) return;
        cmd(".uno:RenameTable", ex::sheetRename(ss(name)));
    } else if (a == QLatin1String("sheetDelete")) {
        if (n <= 1) { QMessageBox::information(this, tr("Xoá sheet"), tr("Không thể xoá sheet duy nhất của tệp.")); return; }
        if (QMessageBox::question(this, tr("Xoá sheet"),
                tr("Xoá sheet \"%1\" cùng toàn bộ dữ liệu, công thức, ghi chú trong đó?\nThao tác sẽ thay đổi tài liệu đang mở. Kiểm tra lại trước khi lưu.")
                    .arg(qs(m_doc->partName(cur))), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        if (!sameSheet()) { QMessageBox::warning(this, tr("Xoá sheet"), tr("Sheet đã thay đổi; chưa xoá dữ liệu.")); return; }
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
        if (!sameSheet()) { QMessageBox::warning(this, tr("Tìm và thay thế"), tr("Sheet đã thay đổi; chưa thay dữ liệu.")); return; }
        cmd(".uno:ExecuteSearch", ex::replaceAll(ss(f), ss(r)));
        m_doc->waitInputIdle();
    } else if (a == QLatin1String("find")) {
        bool ok = false;
        const QString f = QInputDialog::getText(this, tr("Tìm"), tr("Tìm:"), QLineEdit::Normal, QString(), &ok);
        if (ok && !f.isEmpty()) cmd(".uno:ExecuteSearch", ex::findNext(ss(f)));
    }
    if (a.startsWith(QLatin1String("sheet"))) { m_doc->waitInputIdle(); rebuildParts(); m_view->resetForPartChange(); emit statusChanged(); }
    m_view->setFocus();
}
