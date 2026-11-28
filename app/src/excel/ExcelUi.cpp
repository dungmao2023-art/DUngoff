// SPDX-License-Identifier: MPL-2.0
// Giao diện Excel: hành động "xl:*" từ ribbon và menu chuột phải trên thẻ sheet.
// Chỉ dùng lệnh UNO đã kiểm chứng (ExcelCommands.h). Mọi thao tác phá huỷ đều hỏi xác nhận.
#include "../DocumentTab.h"
#include "../EngineHost.h"
#include "../TileView.h"
#include "ExcelCommands.h"
#ifndef LOK_USE_UNSTABLE_API
#define LOK_USE_UNSTABLE_API
#endif
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

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
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QLineEdit>
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
        auto* left=menu.addAction(tr("Dời sang trái"),this,[this]{excelAction(QStringLiteral("xl:sheetMoveLeft"));});
        left->setEnabled(i>0);
        auto* right=menu.addAction(tr("Dời sang phải"),this,[this]{excelAction(QStringLiteral("xl:sheetMoveRight"));});
        right->setEnabled(i+1<m_sheets->count());
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
    if (cur < 0 || cur >= n) return;
    const auto* originalDoc = m_doc.get();
    const auto originalSheet = m_doc->partName(cur);
    auto sameSheet = [&] {
        if (!m_doc || m_doc.get() != originalDoc || readOnly()) return false;
        m_doc->waitInputIdle();
        return m_doc->currentPart() == cur && m_doc->partCount() == n && m_doc->partName(cur) == originalSheet;
    };

    // Modal dialogs run a nested event loop. Never apply their result to a
    // different sheet or cell range if the selection changes while open.
    const auto confirmTarget = [&](const QString& range, const QString& title) {
        if (range.isEmpty() || dataassist::bridgeRangeAddress(ss(range)).empty() ||
            !sameSheet() || !m_view || m_view->selectedCellRange() != range || !sameSheet()) {
            QMessageBox::warning(this, title, tr("Không xác minh được vùng chọn ban đầu hoặc sheet đã thay đổi. Chưa áp dụng thao tác. Hãy chọn lại ô."));
            return false;
        }
        return true;
    };

    if(a==QLatin1String("deleteRow")||a==QLatin1String("deleteColumn")){
        const bool row=a==QLatin1String("deleteRow");
        const QString title=row?tr("Xóa hàng"):tr("Xóa cột");
        const auto confirmedCell=[&]{
            return dataassist::bridgeConfirmedCell(ss(m_view->selectedCellRange()),
                m_doc->lastPayload(LOK_CALLBACK_CELL_ADDRESS));
        };
        const auto cell=confirmedCell();
        if(cell.empty()){
            QMessageBox::information(this,title,tr("Chọn một ô trong hàng hoặc cột cần xóa. Không chọn nhiều ô."));return;
        }
        const auto before=m_doc->selectionAs("text/plain;charset=utf-8");
        if(QMessageBox::question(this,title,
            tr("Sheet %1 · ô %2. Xóa toàn bộ %3 chứa ô này, kể cả công thức và dữ liệu ngoài vùng nhìn thấy? Các tham chiếu có thể thay đổi. Có thể dùng Ctrl+Z để hoàn tác.")
                .arg(qs(originalSheet),qs(cell),row?tr("hàng"):tr("cột")),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!sameSheet()||!m_view||confirmedCell()!=cell||
           m_doc->selectionAs("text/plain;charset=utf-8")!=before){
            QMessageBox::warning(this,title,tr("Sheet, ô hoặc dữ liệu đã thay đổi. Chưa xóa."));return;
        }
        cmd(row?".uno:DeleteRows":".uno:DeleteColumns");
        m_doc->waitInputIdle();m_view->setFocus();return;
    }else if(a==QLatin1String("fillSeries")){
        const QString title=tr("Điền dãy số");
        const QString range=m_view->selectedCellRange();
        const auto ends=range.split(QLatin1Char(':'));
        if(ends.size()!=2||dataassist::bridgeRangeAddress(ss(range)).empty()){
            QMessageBox::information(this,title,tr("Chọn một dải ô liên tục trong cùng một hàng hoặc cột."));return;
        }
        auto coords=[](const QString& cell){
            int pos=0,col=0;
            while(pos<cell.size()&&cell[pos].unicode()>='A'&&cell[pos].unicode()<='Z'){
                col=col*26+cell[pos].unicode()-'A'+1;++pos;
            }
            return QPair<int,int>(cell.mid(pos).toInt(),col);
        };
        const auto first=coords(ends.first()),last=coords(ends.last());
        const bool vertical=first.second==last.second;
        const int count=vertical?last.first-first.first+1:last.second-first.second+1;
        if(first.first<1||first.second<1||(!vertical&&first.first!=last.first)||count<2||count>10000){
            QMessageBox::information(this,title,tr("Chọn 2–10.000 ô trong đúng một hàng hoặc một cột."));return;
        }
        const auto before=m_doc->selectionAs("text/plain;charset=utf-8");
        QDialog dialog(this);
        dialog.setWindowTitle(title);
        auto* layout=new QVBoxLayout(&dialog);
        auto* context=new QLabel(tr("Sheet %1 · %2 · %3 ô").arg(qs(originalSheet),range).arg(count),&dialog);
        context->setTextFormat(Qt::PlainText);
        context->setWordWrap(true);
        layout->addWidget(context);
        auto* form=new QFormLayout;
        auto* startField=new QSpinBox(&dialog);
        startField->setRange(-1000000000,1000000000);
        startField->setValue(1);
        auto* stepField=new QSpinBox(&dialog);
        stepField->setRange(-1000000,1000000);
        stepField->setValue(1);
        form->addRow(tr("Bắt đầu"),startField);
        form->addRow(tr("Bước tăng/giảm"),stepField);
        layout->addLayout(form);
        auto* preview=new QLabel(&dialog);
        preview->setWordWrap(true);
        layout->addWidget(preview);
        auto* warning=new QLabel(tr("Toàn bộ vùng chọn có thể bị ghi đè, kể cả ô đầu. Chỉ tiếp tục khi đúng vùng."),&dialog);
        warning->setWordWrap(true);
        layout->addWidget(warning);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
        auto* apply=buttons->button(QDialogButtonBox::Ok);
        apply->setText(tr("Điền dãy"));
        apply->setAutoDefault(false);
        auto* cancel=buttons->button(QDialogButtonBox::Cancel);
        cancel->setDefault(true);
        layout->addWidget(buttons);
        const auto updatePreview=[&]{
            const qint64 start=startField->value(),step=stepField->value();
            long long end=0;
            const bool valid=ex::linearSeriesEnd(start,step,count,end);
            apply->setEnabled(valid);
            preview->setText(valid?tr("Ô đầu: %1 · Ô thứ hai: %2 · Ô cuối: %3").arg(start).arg(start+step).arg(end):
                tr("Bước phải khác 0 và dãy phải nằm trong khoảng ±1 tỷ."));
        };
        connect(startField,&QSpinBox::valueChanged,&dialog,[&](int){updatePreview();});
        connect(stepField,&QSpinBox::valueChanged,&dialog,[&](int){updatePreview();});
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        updatePreview();
        if(dialog.exec()!=QDialog::Accepted||!apply->isEnabled())return;
        const qint64 start=startField->value(),step=stepField->value();
        long long end=0;
        if(!ex::linearSeriesEnd(start,step,count,end))return;
        if(!sameSheet()||!m_view||m_view->selectedCellRange()!=range||
           m_doc->selectionAs("text/plain;charset=utf-8")!=before){
            QMessageBox::warning(this,title,tr("Vùng hoặc nội dung thay đổi. Chưa điền dãy."));return;
        }
        cmd(".uno:FillSeries",ex::fillLinear(vertical,start,step,end));
        m_doc->waitInputIdle();m_view->setFocus();return;
    }else if(a==QLatin1String("fillDown")||a==QLatin1String("fillRight")||
       a==QLatin1String("fillUp")||a==QLatin1String("fillLeft")){
        const bool down=a==QLatin1String("fillDown"),up=a==QLatin1String("fillUp"),left=a==QLatin1String("fillLeft");
        const bool vertical=down||up;
        const QString range=m_view->selectedCellRange();
        const QString title=down?tr("Điền xuống"):up?tr("Điền lên"):left?tr("Điền sang trái"):tr("Điền sang phải");
        const QString sourceEdge=down?tr("hàng đầu"):up?tr("hàng cuối"):left?tr("cột cuối"):tr("cột đầu");
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
           (vertical?last.first==first.first:last.second==first.second)){
            QMessageBox::information(this,title,vertical?tr("Chọn ít nhất hai hàng."):tr("Chọn ít nhất hai cột."));return;
        }
        const qint64 targets=vertical?qint64(last.first-first.first)*(last.second-first.second+1):
            qint64(last.second-first.second)*(last.first-first.first+1);
        if(targets>100000){
            QMessageBox::warning(this,title,tr("Mỗi lần điền tối đa 100.000 ô đích. Hãy chia vùng nhỏ hơn."));return;
        }
        const auto before=m_doc->selectionAs("text/plain;charset=utf-8");
        if(QMessageBox::question(this,title,
           tr("Sheet: %1 · vùng %2\nSao chép %3 vào %4 ô đích. Công thức tương đối sẽ được điều chỉnh; dữ liệu và định dạng ở ô đích có thể bị thay thế, kể cả ô hiển thị trống. Tiếp tục?")
            .arg(qs(originalSheet),range,sourceEdge).arg(targets),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!sameSheet()||!m_view||m_view->selectedCellRange()!=range||
           m_doc->selectionAs("text/plain;charset=utf-8")!=before){
            QMessageBox::warning(this,title,tr("Vùng hoặc nội dung đã thay đổi trong lúc xác nhận. Chưa điền dữ liệu."));return;
        }
        cmd(down?".uno:FillDown":up?".uno:FillUp":left?".uno:FillLeft":".uno:FillRight");
        m_doc->waitInputIdle();m_view->setFocus();return;
    } else if(a==QLatin1String("sortAscending")||a==QLatin1String("sortDescending")){
        const bool descending=a==QLatin1String("sortDescending");
        const QString title=descending?tr("Sắp xếp giảm dần"):tr("Sắp xếp tăng dần");
        const QString cell=m_view->selectedCellRange();
        // An explicitly selected multi-column range can split related rows.
        // Use the existing native auto-region sort from one confirmed key cell.
        if(dataassist::bridgeConfirmedCell(ss(cell),m_doc->lastPayload(LOK_CALLBACK_CELL_ADDRESS)).empty()){
            QMessageBox::information(this,title,tr("Chọn một ô dữ liệu trong cột cần sắp xếp. Không chọn riêng một dải ô; engine sẽ tự xác định vùng dữ liệu liên tục."));
            return;
        }
        const auto before=m_doc->selectionAs("text/plain;charset=utf-8");
        if(before.empty()){
            QMessageBox::information(this,title,tr("Chọn ô có dữ liệu trong cột khóa, không chọn ô trống."));return;
        }
        if(QMessageBox::question(this,title,
            tr("Sheet: %1 · ô khóa %2.\nEngine sẽ tự nhận diện vùng dữ liệu liên tục quanh ô khóa và sắp xếp các dòng. Hãy kiểm tra bảng có tiêu đề, không có dòng/cột trống ngắt vùng; dữ liệu ngoài vùng tự nhận diện có thể không di chuyển theo. Nên lưu bản sao trước. Tiếp tục?")
            .arg(qs(originalSheet),cell),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!sameSheet()||!m_view||m_view->selectedCellRange()!=cell||m_doc->selectionAs("text/plain;charset=utf-8")!=before){
            QMessageBox::warning(this,title,tr("Ô khóa, sheet hoặc nội dung đã thay đổi. Chưa sắp xếp."));return;
        }
        cmd(descending?".uno:SortDescending":".uno:SortAscending");
        m_doc->waitInputIdle();m_view->setFocus();return;
    } else if(a==QLatin1String("refreshPivot")){
        const auto range=m_view->selectedCellRange();
        if(dataassist::bridgeCellAddress(ss(range)).empty()){
            QMessageBox::information(this,tr("Cập nhật PivotTable"),tr("Chọn một ô bên trong PivotTable hiện có, không chọn vùng nguồn hoặc nhiều ô."));return;
        }
        const auto before=m_doc->selectionAs("text/plain;charset=utf-8");
        if(QMessageBox::question(this,tr("Cập nhật PivotTable"),
            tr("Sheet %1 · ô %2. Cập nhật PivotTable hiện có từ nguồn dữ liệu của bảng; kết quả tổng hợp có thể thay đổi. Tiếp tục?").arg(qs(originalSheet),range),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!sameSheet()||!m_view||m_view->selectedCellRange()!=range||m_doc->selectionAs("text/plain;charset=utf-8")!=before){
            QMessageBox::warning(this,tr("Cập nhật PivotTable"),tr("Vùng chọn hoặc dữ liệu thay đổi. Chưa gửi lệnh cập nhật."));return;
        }
        cmd(".uno:RecalcPivotTable");m_doc->waitInputIdle();m_view->setFocus();return;
    } else if (a == QLatin1String("insertChart")) {
        m_doc->waitInputIdle();
        const QString range=m_view->selectedCellRange();
        const auto normalized=dataassist::bridgeRangeAddress(ss(range));
        const auto ends=qs(normalized).split(QLatin1Char(':'));
        if(ends.size()!=2){
            QMessageBox::information(this,tr("Biểu đồ"),tr("Chọn vùng dữ liệu liền nhau gồm nhãn và giá trị ngay trên sheet, sau đó bấm Biểu đồ cột."));
            return;
        }
        auto coords=[](const QString& cell){
            int i=0,col=0;
            while(i<cell.size()&&cell[i].unicode()>='A'&&cell[i].unicode()<='Z'){
                col=col*26+cell[i].unicode()-'A'+1;++i;
            }
            return QPair<int,int>(cell.mid(i).toInt(),col);
        };
        const auto first=coords(ends.first()),last=coords(ends.last());
        const int rows=last.first-first.first+1,cols=last.second-first.second+1;
        if(rows<2||cols<2||qint64(rows)*cols>50000||cols>100){
            QMessageBox::information(this,tr("Biểu đồ"),tr("Chọn bảng từ 2 hàng và 2 cột, tối đa 50.000 ô và 100 cột; hàng/cột nhãn cần nằm trong vùng chọn."));return;
        }
        const auto before=m_doc->selectionAs("text/plain;charset=utf-8");
        if(dataassist::bridgeTrim(before).empty()){
            QMessageBox::information(this,tr("Biểu đồ"),tr("Không đọc được dữ liệu nguồn trong vùng chọn."));return;
        }
        if(QMessageBox::question(this,tr("Biểu đồ cột"),
            tr("Tạo biểu đồ cột từ vùng %1 trên sheet %2 (%3 hàng × %4 cột)? Engine tự nhận diện nhãn và chuỗi số; biểu đồ liên kết trực tiếp với nguồn.").arg(range,qs(originalSheet)).arg(rows).arg(cols),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!sameSheet()||!m_view||m_view->selectedCellRange()!=range||m_doc->selectionAs("text/plain;charset=utf-8")!=before){
            QMessageBox::warning(this,tr("Biểu đồ"),tr("Vùng hoặc dữ liệu nguồn đã đổi. Chưa tạo biểu đồ."));return;
        }
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
            if (!sameSheet() || !m_view || m_view->selectedCellRange() != range || qs(m_doc->selectionAs("text/plain;charset=utf-8")).trimmed() != value) {
                QMessageBox::warning(this, tr("Số → chữ"), tr("Vùng chọn đã đổi. Chưa ghi dữ liệu.")); return;
            }
            if (QMessageBox::question(this, tr("Thay số hoặc công thức?"), tr("Thay %1 bằng chữ? Công thức gốc (nếu có) sẽ bị thay thế.").arg(range), QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
                m_doc->waitInputIdle();
                if (!sameSheet() || !m_view || m_view->selectedCellRange() != range ||
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
    } else if(a==QLatin1String("sheetMoveLeft")||a==QLatin1String("sheetMoveRight")) {
        const bool left=a==QLatin1String("sheetMoveLeft");
        if(!sameSheet())return;
        if((left&&cur==0)||(!left&&cur+1>=n)){
            QMessageBox::information(this,tr("Di chuyển sheet"),left?tr("Sheet đã ở đầu sổ làm việc."):tr("Sheet đã ở cuối sổ làm việc."));
            return;
        }
        // Forward insertion indices differ between engine builds.
        // Swap adjacent sheets using only the verified backward move:
        // for a rightward move, move the next sheet before this one.
        const QString stem=fromPath(m_doc->workingCopy().stem());
        if(!left){
            const auto neighbour=m_doc->partName(cur+1);
            m_doc->setPart(cur+1);
            m_doc->waitInputIdle();
            if(m_doc->partCount()!=n||m_doc->currentPart()!=cur+1||
               m_doc->partName(cur+1)!=neighbour||m_doc->partName(cur)!=originalSheet){
                QMessageBox::warning(this,tr("Di chuyển sheet"),tr("Không xác minh được sheet bên cạnh. Chưa di chuyển."));
                return;
            }
        }
        cmd(".uno:Move",ex::sheetMove(ss(stem),left?cur:cur+1,false));
        m_doc->waitInputIdle();
        const int expected=cur+(left?-1:1);
        if(m_doc->partCount()!=n||m_doc->partName(expected)!=originalSheet){
            QMessageBox::warning(this,tr("Di chuyển sheet"),tr("Chưa xác minh được thứ tự sheet sau thao tác. Hãy kiểm tra lại trước khi lưu."));
        }else if(m_doc->currentPart()!=expected){
            m_doc->setPart(expected);
        }
    } else if (a == QLatin1String("sheetDup")) {
        const QString stem = fromPath(m_doc->workingCopy().stem());
        cmd(".uno:Move", ex::sheetMove(ss(stem), cur + 2, true));
    } else if (a == QLatin1String("fontName")) {
        const QString targetRange = m_view->selectedCellRange();
        bool ok = false;
        const QStringList fonts=documentFontFamilies(true);
        const QString f=fonts.isEmpty()
            ? QInputDialog::getText(this,tr("Phông chữ"),tr("Engine chưa trả danh sách; nhập tên phông:"),QLineEdit::Normal,QString(),&ok)
            : QInputDialog::getItem(this,tr("Phông chữ"),tr("Chọn phông engine nhận diện:"),fonts,0,false,&ok);
        if (ok && !f.trimmed().isEmpty() && confirmTarget(targetRange, tr("Phông chữ"))) cmd(".uno:CharFontName", ex::fontName(ss(f.trimmed())));
    } else if (a == QLatin1String("fontSize")) {
        const QString targetRange = m_view->selectedCellRange();
        bool ok = false;
        const int pt = QInputDialog::getInt(this, tr("Cỡ chữ"), tr("Cỡ chữ (pt):"), 11, 4, 96, 1, &ok);
        if (ok && confirmTarget(targetRange, tr("Cỡ chữ"))) cmd(".uno:FontHeight", ex::fontHeight(pt));
    } else if (a == QLatin1String("textColor") || a == QLatin1String("fillColor")) {
        const QString targetRange = m_view->selectedCellRange();
        const QColor c = QColorDialog::getColor(Qt::black, this, a == QLatin1String("textColor") ? tr("Màu chữ") : tr("Màu nền ô"));
        if (!c.isValid() || !confirmTarget(targetRange, tr("Màu ô"))) return;
        const unsigned rgb = ((unsigned)c.red() << 16) | ((unsigned)c.green() << 8) | (unsigned)c.blue();
        if (a == QLatin1String("textColor")) cmd(".uno:Color", ex::textColor(rgb));
        else cmd(".uno:BackgroundColor", ex::fillColor(rgb));
    } else if (a == QLatin1String("colWidth")) {
        const QString targetRange = m_view->selectedCellRange();
        bool ok = false;
        const double cm = QInputDialog::getDouble(this, tr("Độ rộng cột"), tr("Độ rộng (cm):"), 2.5, 0.2, 50, 2, &ok);
        if (ok && confirmTarget(targetRange, tr("Độ rộng cột"))) cmd(".uno:ColumnWidth", ex::columnWidth(ex::cmToHundredthMm(cm)));
    } else if (a == QLatin1String("rowHeight")) {
        const QString targetRange = m_view->selectedCellRange();
        bool ok = false;
        const double cm = QInputDialog::getDouble(this, tr("Chiều cao hàng"), tr("Chiều cao (cm):"), 0.5, 0.1, 20, 2, &ok);
        if (ok && confirmTarget(targetRange, tr("Chiều cao hàng"))) cmd(".uno:RowHeight", ex::rowHeight(ex::cmToHundredthMm(cm)));
    } else if (a==QLatin1String("replace") || a==QLatin1String("find")) {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Tìm và thay thế — %1").arg(qs(originalSheet)));
        auto* root=new QVBoxLayout(&dlg);
        auto* form=new QFormLayout;
        auto* needle=new QLineEdit(&dlg);
        auto* replacement=new QLineEdit(&dlg);
        form->addRow(tr("Tìm:"),needle);
        form->addRow(tr("Thay bằng:"),replacement);
        root->addLayout(form);
        auto* matchCase=new QCheckBox(tr("Phân biệt chữ hoa/thường"),&dlg);
        matchCase->setChecked(false);root->addWidget(matchCase);
        auto* note=new QLabel(tr("Tìm chữ thuần trong sheet hiện tại, không dùng biểu thức chính quy. Thay tất cả dùng chế độ giá trị/văn bản; cần xác nhận trước khi thay."),&dlg);
        note->setWordWrap(true);root->addWidget(note);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dlg);
        auto* next=buttons->addButton(tr("Tìm tiếp"),QDialogButtonBox::ActionRole);
        auto* replace=buttons->addButton(tr("Thay tất cả"),QDialogButtonBox::ActionRole);
        root->addWidget(buttons);
        auto refresh=[&]{const bool enabled=!needle->text().isEmpty();next->setEnabled(enabled);replace->setEnabled(enabled);};
        QObject::connect(needle,&QLineEdit::textChanged,&dlg,[&]{refresh();});
        auto validate=[&]{
            if(needle->text().isEmpty())return false;
            if(sameSheet())return true;
            QMessageBox::warning(&dlg,tr("Tìm và thay thế"),tr("Sheet đã thay đổi. Hãy đóng và mở lại công cụ."));
            return false;
        };
        auto find=[&]{
            if(!validate())return;
            cmd(".uno:ExecuteSearch",ex::findNext(ss(needle->text()),matchCase->isChecked()));
            m_doc->waitInputIdle();
            note->setText(tr("Đã gửi lệnh tìm. Đóng hộp thoại để xem ô được chọn; engine chưa xác nhận số kết quả."));
        };
        QObject::connect(next,&QPushButton::clicked,&dlg,find);
        QObject::connect(needle,&QLineEdit::returnPressed,&dlg,find);
        QObject::connect(replace,&QPushButton::clicked,&dlg,[&]{
            if(!validate())return;
            const QString from=needle->text(),to=replacement->text();
            const bool exact=matchCase->isChecked();
            if(QMessageBox::question(&dlg,tr("Thay tất cả"),
                tr("Sheet %1: thay mọi lần xuất hiện của «%2» bằng «%3» trong giá trị/văn bản?\n%4").arg(qs(originalSheet),from,to,exact?tr("Có phân biệt hoa/thường"):tr("Không phân biệt hoa/thường")),
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            if(!validate()||needle->text()!=from||replacement->text()!=to||matchCase->isChecked()!=exact)return;
            cmd(".uno:ExecuteSearch",ex::replaceAll(ss(from),ss(to),exact));
            m_doc->waitInputIdle();dlg.accept();
        });
        QObject::connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        refresh();needle->setFocus();dlg.exec();
    }
    if (!m_doc || m_doc.get() != originalDoc || !m_view || readOnly()) return;
    if (a.startsWith(QLatin1String("sheet"))) { m_doc->waitInputIdle(); rebuildParts(); m_view->resetForPartChange(); emit statusChanged(); }
    m_view->setFocus();
}
