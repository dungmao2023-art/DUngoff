// SPDX-License-Identifier: MPL-2.0
#include "WordActions.h"

#include "../DocumentTab.h"
#include "WordCommands.h"
#include "PageMargins.h"

#include "dung/Engine.h"
#ifndef LOK_USE_UNSTABLE_API
#define LOK_USE_UNSTABLE_API
#endif
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <QPointer>
#include "../vntext/VnText.h"
#include "../vntext/SyllableCheck.h"
#include "../tools/mailmerge/MailMergeWidget.h"
#include <QGridLayout>
#include <QFormLayout>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QSettings>
#include "../TileView.h"
#include <QRegularExpression>
#include <QTextDocumentFragment>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QColorDialog>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStringList>
#include <QUrl>

namespace dung::word {

static std::string ss(const QString& s) { return s.toUtf8().toStdString(); }

static void send(DocumentTab* t, const Call& c) { t->document()->command(c.first, c.second); }

// Dialogs can run nested event loops. Capture both the selection contents
// and its position; identical text at a different location is not the same target.
struct WordInsertTarget {
    QPointer<DocumentTab> tab;
    Document* doc;
    std::string selected, caret, selection;
    int part;
    explicit WordInsertTarget(DocumentTab* t) : tab(t), doc(t->document()) {
        doc->waitInputIdle();
        part = doc->currentPart();
        selected = doc->selectionAs("text/plain;charset=utf-8");
        caret = doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR);
        selection = doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION);
    }
    bool verify() const {
        if (!tab || tab->document() != doc || tab->readOnly() || !tab->view()) return false;
        doc->waitInputIdle();
        if ((!caret.empty() || !selection.empty()) && doc->currentPart() == part &&
            doc->selectionAs("text/plain;charset=utf-8") == selected &&
            doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR) == caret &&
            doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION) == selection) return true;
        QMessageBox::warning(tab.data(), QObject::tr("Vị trí chèn"),
            QObject::tr("Không xác minh được vị trí con trỏ hoặc vùng chữ đã thay đổi. Chưa sửa tài liệu. Hãy chọn lại."));
        return false;
    }
};

// Đặt lề bằng cách vá XML trên BẢN SAO rồi mở bản sao trong thẻ mới (không dùng lệnh UNO/runMacro).
// An toàn: thẻ hiện tại, lịch sử hoàn tác và tệp gốc không bị đổi; bản sao được lấy từ trạng thái THẬT của tài liệu
// (bản phục hồi, gồm cả phần vừa sửa chưa lưu) nên không phụ thuộc cờ "đã sửa" của engine.
[[maybe_unused]] static bool applyMarginsByPatch(DocumentTab* tab, QWidget* parent, const MarginSpec& spec, const QString& label) {
    auto* doc = tab->document();
    if (doc->kind() != dung::DocKind::Text) return false;
    auto mm = [](const std::optional<qint32>& v) { return v ? QString::number(twipsToMm(*v), 'f', 1) : QStringLiteral("—"); };
    const QString base = QString::fromUtf8(doc->displayName().c_str());
    const bool notDocx = !doc->isNew() && !base.endsWith(QLatin1String(".docx"), Qt::CaseInsensitive);
    QString msg = QObject::tr("Đặt lề «%1»: trái %2 mm, phải %3 mm, trên %4 mm, dưới %5 mm — cho TẤT CẢ các phần của tài liệu.\n\n"
                              "Để áp dụng lề nhất quán và kiểm tra từng phần, Dũng Office sẽ tạo MỘT BẢN SAO của tài liệu hiện tại "
                              "(gồm cả phần bạn vừa sửa) với lề mới và mở trong thẻ mới.\n"
                              "• Thẻ hiện tại và tệp gốc KHÔNG bị đổi; thẻ hiện tại vẫn giữ lịch sử hoàn tác.\n"
                              "• Thẻ mới bắt đầu ở đầu tài liệu, chưa có lịch sử hoàn tác, và được đánh dấu chưa lưu (Lưu sẽ hỏi tên mới, không ghi đè tệp gốc).")
                      .arg(label, mm(spec.left), mm(spec.right), mm(spec.top), mm(spec.bottom));
    if (notDocx) msg += QObject::tr("\n• Bản sao được lưu ở định dạng .docx (tệp gốc không phải .docx).");
    msg += QObject::tr("\n\nTiếp tục?");
    QPointer<DocumentTab> guard(tab);
    if (QMessageBox::question(parent, QObject::tr("Lề trang"), msg,
          QMessageBox::Yes|QMessageBox::No,QMessageBox::No) != QMessageBox::Yes) return true;
    if (!guard || guard->readOnly() || guard->document()!=doc) return true;
    doc->waitInputIdle();

    QTemporaryDir td(QDir::tempPath() + QStringLiteral("/DungOffice-le-XXXXXX"));
    auto fail = [&](const QString& why) {
        QMessageBox::warning(parent, QObject::tr("Lề trang"),
            QObject::tr("Không đặt được lề: %1\n\nTài liệu hiện tại và tệp gốc không bị đổi.").arg(why));
        return true;
    };
    if (!td.isValid()) return fail(QObject::tr("không tạo được thư mục tạm."));
    const QString srcCopy = td.filePath(QStringLiteral("nguon.docx"));
    const QString patched = td.filePath(QStringLiteral("moi.docx"));
    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    std::string err;
    const bool saved = doc->saveRecoveryCopy(dung::fromUtf8(srcCopy.toUtf8().toStdString()), &err);
    PatchResult p;
    if (saved) p = patchDocxMargins(srcCopy, patched, spec);
    QGuiApplication::restoreOverrideCursor();
    if (!saved) return fail(QObject::tr("engine không tạo được bản sao (%1).").arg(QString::fromUtf8(err.c_str())));
    if (!p.ok) return fail(p.error);

    const QString orig = doc->isNew() ? QString() : QString::fromUtf8(dung::toUtf8(doc->originalPath()).c_str());
    const QFileInfo bi(base);
    const QString newName = bi.completeBaseName() + QObject::tr(" (lề %1)").arg(label) + QStringLiteral(".docx");
    emit tab->openCopyRequested(patched, orig, newName);   // kết nối trực tiếp: mở xong mới quay lại, rồi thư mục tạm bị xoá
    if (!p.warnings.isEmpty())
        QMessageBox::information(parent, QObject::tr("Lề trang"), p.warnings.join(QLatin1Char('\n')));
    return true;
}

bool handleAction(const QString& id, DocumentTab* tab, QWidget* parent) {
    if (!id.startsWith(QLatin1String("word:"))) return false;
    if (!tab || tab->readOnly()) return true;
    auto* doc = tab->document();
    if(!doc || doc->kind()!=DocKind::Text || !tab->view())return true;
    QString name = id.mid(5);
    if (name == QLatin1String("reviewSyllable")) name = QStringLiteral("vnSyllable");
    if (name == QLatin1String("reviewCount")) name = QStringLiteral("count");
    if (name == QLatin1String("adminMargin")) name = QStringLiteral("marginVN");

    if (name == QLatin1String("acceptAllReview") || name == QLatin1String("rejectAllReview")) {
        const bool accept = name == QLatin1String("acceptAllReview");
        if (QMessageBox::question(parent,QObject::tr("Xử lý toàn bộ thay đổi"),
            accept ? QObject::tr("Chấp nhận tất cả thay đổi được theo dõi trong tài liệu đang mở? Nội dung xóa được chấp nhận sẽ bị loại bỏ.") :
                     QObject::tr("Từ chối tất cả thay đổi được theo dõi trong tài liệu đang mở? Nội dung chèn được theo dõi sẽ bị loại bỏ."),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return true;
        if(tab->readOnly() || tab->document()!=doc) return true;
        doc->waitInputIdle();
        doc->command(accept ? ".uno:AcceptAllTrackedChanges" : ".uno:RejectAllTrackedChanges","",true);
        doc->waitInputIdle();
        tab->view()->setFocus();
    } else if (name == QLatin1String("addComment")) {
        doc->waitInputIdle();
        QPointer<DocumentTab> live(tab);
        const auto selected=doc->selectionAs("text/plain;charset=utf-8");
        const auto caret=doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR);
        const auto selection=doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION);
        if(selected.empty() && caret.empty()){
            QMessageBox::information(parent,QObject::tr("Bình luận Word"),QObject::tr("Đặt con trỏ vào văn bản trước khi thêm bình luận."));return true;
        }
        bool ok=false;
        const QString comment=QInputDialog::getMultiLineText(parent,QObject::tr("Bình luận Word"),
            QObject::tr("Nhập nội dung bình luận tại con trỏ hoặc vùng chữ đang chọn:"),QString(),&ok);
        if(!ok||comment.trimmed().isEmpty())return true;
        if(comment.size()>10000){QMessageBox::warning(parent,QObject::tr("Bình luận Word"),QObject::tr("Tối đa 10.000 ký tự."));return true;}
        if(!live||live->readOnly()||live->document()!=doc||!live->view())return true;
        doc->waitInputIdle();
        if(doc->selectionAs("text/plain;charset=utf-8")!=selected ||
           doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR)!=caret ||
           doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION)!=selection){
            QMessageBox::warning(live.data(),QObject::tr("Bình luận Word"),QObject::tr("Vị trí hoặc vùng chữ đã thay đổi. Chưa thêm bình luận; hãy chọn lại."));
            return true;
        }
        doc->command(".uno:InsertAnnotation",Document::stringArg("Text",ss(comment)));
        doc->waitInputIdle();
        if(live&&live->view())live->view()->setFocus();
    } else if (name == QLatin1String("headerEnable") || name == QLatin1String("footerEnable")) {
        const bool header = name == QLatin1String("headerEnable");
        bool ok = false;
        const QString style = QInputDialog::getText(parent,
            header ? QObject::tr("Bật đầu trang") : QObject::tr("Bật chân trang"),
            QObject::tr("Tên kiểu trang (Page Style) của phần cần sửa. Với tài liệu mới, thường là Default Page Style. "
                        "Lệnh chỉ bật, không tắt hay xóa nội dung đã có. Sau khi bật, bấm vào vùng đầu/chân trang để nhập chữ hoặc trường số trang."),
            QLineEdit::Normal, QStringLiteral("Default Page Style"), &ok).trimmed();
        if (!ok || style.isEmpty()) return true;
        doc->waitInputIdle();
        send(tab, enablePageBand(header, ss(style)));
        tab->view()->setFocus();
    } else if (name == QLatin1String("footnote") || name == QLatin1String("endnote")) {
        if (!doc->selectionAs("text/plain;charset=utf-8").empty()) {
            QMessageBox::information(parent, QObject::tr("Chú thích"), QObject::tr("Đặt con trỏ sau từ cần chú thích, bỏ vùng bôi đen rồi thực hiện lại."));
            return true;
        }
        doc->waitInputIdle();
        send(tab, insertFootnote(name == QLatin1String("endnote")));
        tab->view()->setFocus();
    } else if (name == QLatin1String("font")) {
        const WordInsertTarget target(tab);
        const auto fonts = parseFontList(doc->commandValues(".uno:CharFontName"));
        QStringList items;
        for (const auto& f : fonts) items << QString::fromUtf8(f.c_str());
        bool ok = false;
        const QString pick = items.isEmpty()
            ? QInputDialog::getText(parent, QObject::tr("Phông chữ"), QObject::tr("Tên phông:"), QLineEdit::Normal, QString(), &ok)
            : QInputDialog::getItem(parent, QObject::tr("Phông chữ"), QObject::tr("Chọn phông:"), items, 0, true, &ok);
        if (ok && !pick.trimmed().isEmpty() && target.verify()) {
            send(target.tab.data(),fontName(ss(pick.trimmed())));
            target.tab->view()->setFocus();
        }
    } else if (name == QLatin1String("size")) {
        const WordInsertTarget target(tab);
        QStringList items;
        for (double s : commonFontSizes()) items << QString::number(s);
        bool ok = false;
        const QString pick = QInputDialog::getItem(parent, QObject::tr("Cỡ chữ"), QObject::tr("Cỡ chữ (pt):"), items, 3, true, &ok);
        if (!ok) return true;
        double v = pick.toDouble(&ok);
        if (ok && v >= 1 && v <= 400 && target.verify()) {
            send(target.tab.data(),fontSize(v));
            target.tab->view()->setFocus();
        }
    } else if (name == QLatin1String("color") || name == QLatin1String("back")) {
        const WordInsertTarget target(tab);
        const QColor c = QColorDialog::getColor(Qt::black, parent, QObject::tr("Chọn màu"));
        if (c.isValid() && target.verify()) {
            const unsigned rgb = static_cast<unsigned>(c.red() << 16 | c.green() << 8 | c.blue());
            send(target.tab.data(), name == QLatin1String("color") ? fontColor(rgb) : backColor(rgb));
            target.tab->view()->setFocus();
        }
    } else if (name.startsWith(QLatin1String("style"))) {
        const auto& st = paragraphStyles();
        const int i = name.mid(5).toInt();
        if (i >= 0 && i < static_cast<int>(st.size())) send(tab, paragraphStyle(st[static_cast<size_t>(i)].styleName));
    } else if (name == QLatin1String("print")) {
        tab->printDocument();
    } else if (name == QLatin1String("exportPdf")) {
        tab->exportPdf();
    } else if (name == QLatin1String("symbols")) {
        doc->waitInputIdle();
        QPointer<DocumentTab> live(tab);
        const auto selected=doc->selectionAs("text/plain;charset=utf-8");
        const auto caret=doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR);
        const auto selection=doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION);
        if(selected.empty()&&caret.empty()){
            QMessageBox::information(parent,QObject::tr("Chèn ký hiệu"),QObject::tr("Đặt con trỏ vào văn bản trước khi chèn."));return true;
        }
        QDialog dlg(parent);
        dlg.setWindowTitle(QObject::tr("Chèn ký hiệu"));
        auto* lay = new QVBoxLayout(&dlg);
        auto* note = new QLabel(QObject::tr("Chọn ký hiệu hoặc nhập ký tự Unicode. Nếu đang bôi chọn, ký hiệu sẽ thay phần chọn."), &dlg);
        note->setWordWrap(true);
        lay->addWidget(note);
        auto* edit = new QLineEdit(&dlg);
        edit->setMaxLength(16);
        edit->setPlaceholderText(QObject::tr("Ký hiệu cần chèn"));
        auto* grid = new QGridLayout;
        const QStringList symbols = QStringLiteral("© ® ™ § ¶ ° ‰ € £ ¥ ₫ ¢ ± × ÷ ≠ ≤ ≥ ≈ ∞ √ ∑ π α β γ Δ Ω µ → ← ↑ ↓ ↔ ✓ ✗ ★ ☆ • … – — « » № ½ ¼ ¾ ² ³").split(QLatin1Char(' '));
        for (int n = 0; n < symbols.size(); ++n) {
            auto* btn = new QPushButton(symbols[n], &dlg);
            btn->setMinimumWidth(38);
            grid->addWidget(btn, n / 8, n % 8);
            QObject::connect(btn, &QPushButton::clicked, edit, [edit, value = symbols[n]] { edit->setText(value); });
        }
        lay->addLayout(grid);
        lay->addWidget(edit);
        auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        box->button(QDialogButtonBox::Ok)->setText(QObject::tr("Chèn"));
        QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(box);
        if (dlg.exec() == QDialog::Accepted && !edit->text().isEmpty()) {
            if(!live||live->readOnly()||live->document()!=doc||!live->view())return true;
            doc->waitInputIdle();
            auto unchanged=[&]{
                return live&&!live->readOnly()&&live->document()==doc&&live->view()&&
                    doc->selectionAs("text/plain;charset=utf-8")==selected&&
                    doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR)==caret&&
                    doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION)==selection;
            };
            if(!unchanged()){
                QMessageBox::warning(live.data(),QObject::tr("Chèn ký hiệu"),QObject::tr("Vị trí hoặc vùng chữ thay đổi. Chưa chèn ký hiệu."));return true;
            }
            if(!selected.empty()){
                if(QMessageBox::question(live.data(),QObject::tr("Thay vùng chữ"),
                    QObject::tr("Ký hiệu sẽ thay thế vùng chữ đang chọn. Tiếp tục?"),
                    QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return true;
                if(!live||live->document()!=doc||live->readOnly())return true;
                doc->waitInputIdle();
                if(!unchanged()){QMessageBox::warning(live.data(),QObject::tr("Chèn ký hiệu"),QObject::tr("Vùng chữ thay đổi. Chưa chèn."));return true;}
            }
            if(!doc->paste("text/plain;charset=utf-8",ss(edit->text())))
                QMessageBox::warning(live.data(),QObject::tr("Chèn ký hiệu"),QObject::tr("Engine không xác nhận chèn ký hiệu."));
            doc->waitInputIdle();
            if(live&&live->view())live->view()->setFocus();
        }
    } else if (name == QLatin1String("mailMerge")) {
        QDialog dlg(parent);
        dlg.setWindowTitle(QObject::tr("Trộn thư / Điền tên hàng loạt"));
        dlg.resize(940, 650);
        auto* lay = new QVBoxLayout(&dlg);
        auto* hint = new QLabel(QObject::tr("Chọn tệp mẫu đã lưu và danh sách người nhận. Công cụ tạo các tệp kết quả riêng; tài liệu đang mở không bị sửa."), &dlg);
        hint->setWordWrap(true);
        lay->addWidget(hint);
        lay->addWidget(createMailMergeWidget(&dlg), 1);
        auto* close = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
        QObject::connect(close, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(close);
        dlg.exec();
    } else if (name == QLatin1String("inspect") || name == QLatin1String("outline")) {
        doc->waitInputIdle();
        std::string plain = doc->selectionAs("text/plain;charset=utf-8");
        const bool whole = plain.empty();
        if (whole) {
            if (QMessageBox::question(parent, QObject::tr("Đọc tài liệu"),
                    QObject::tr("Đọc toàn văn bản để tạo báo cáo? Vị trí chọn sẽ chuyển về đầu tài liệu; nội dung không bị sửa.")) != QMessageBox::Yes)
                return true;
            doc->command(".uno:SelectAll");
            doc->waitInputIdle();
            plain = doc->selectionAs("text/plain;charset=utf-8");
        }
        const QString html = QString::fromStdString(doc->selectionAs("text/html"));
        if (whole) doc->command(".uno:GoToStartOfDoc");
        const auto content = QString::fromStdString(plain);
        const QRegularExpression rx(QStringLiteral("<h([1-6])(?:\\s[^>]*)?>(.*?)</h\\1\\s*>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QDialog dlg(parent);
        dlg.setWindowTitle(name == QLatin1String("outline") ? QObject::tr("Dàn ý tài liệu") : QObject::tr("Kiểm tra tài liệu"));
        dlg.resize(720, 460);
        auto* lay = new QVBoxLayout(&dlg);
        auto* summary = new QLabel(QObject::tr("%1 · %2 từ · %3 ký tự. Dàn ý lấy từ các tiêu đề H1–H6 do engine xuất; không phải mục lục tự động.")
            .arg(whole ? QObject::tr("Toàn văn bản") : QObject::tr("Vùng chọn")).arg(countWords(plain)).arg(content.size()), &dlg);
        summary->setWordWrap(true);
        lay->addWidget(summary);
        auto* list = new QListWidget(&dlg);
        auto matches = rx.globalMatch(html);
        while (matches.hasNext()) {
            const auto match = matches.next();
            const QString title = QTextDocumentFragment::fromHtml(match.captured(2)).toPlainText().simplified();
            if (title.isEmpty()) continue;
            const int level = match.captured(1).toInt();
            auto* item = new QListWidgetItem(QString(level * 2 - 2, QLatin1Char(' ')) + QStringLiteral("H%1 · ").arg(level) + title, list);
            item->setData(Qt::UserRole, title);
        }
        const bool hasHeadings = list->count() > 0;
        if (!hasHeadings) list->addItem(QObject::tr("Không tìm thấy tiêu đề H1–H6 trong dữ liệu engine trả về."));
        lay->addWidget(list, 1);
        auto* box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
        auto* jump = box->addButton(QObject::tr("Tìm tiêu đề"), QDialogButtonBox::ActionRole);
        jump->setEnabled(hasHeadings);
        if (hasHeadings) list->setCurrentRow(0);
        QObject::connect(jump, &QPushButton::clicked, &dlg, [&] {
            auto* item = list->currentItem();
            if (!item) return;
            const auto title = item->data(Qt::UserRole).toString();
            if (title.isEmpty()) return;
            dlg.accept();
            send(tab, findNext(ss(title), false));
        });
        QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(box);
        dlg.exec();
    } else if (name == QLatin1String("table")) {
        const WordInsertTarget target(tab);
        QDialog dialog(parent);
        dialog.setWindowTitle(QObject::tr("Chèn bảng"));
        auto* layout=new QVBoxLayout(&dialog);
        auto* form=new QFormLayout;
        auto* cols=new QSpinBox(&dialog);
        cols->setRange(1,63);cols->setValue(3);
        auto* rows=new QSpinBox(&dialog);
        rows->setRange(1,500);rows->setValue(3);
        form->addRow(QObject::tr("Số cột"),cols);
        form->addRow(QObject::tr("Số hàng"),rows);
        layout->addLayout(form);
        auto* preview=new QLabel(&dialog);preview->setWordWrap(true);
        layout->addWidget(preview);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
        auto* insert=buttons->button(QDialogButtonBox::Ok);
        insert->setText(QObject::tr("Chèn bảng"));insert->setAutoDefault(false);
        buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
        layout->addWidget(buttons);
        auto refresh=[&]{
            const int cells=cols->value()*rows->value();
            insert->setEnabled(cells<=5000);
            preview->setText(cells<=5000?QObject::tr("%1 cột × %2 hàng = %3 ô").arg(cols->value()).arg(rows->value()).arg(cells):
                QObject::tr("Chọn bảng không quá 5.000 ô để tránh làm chậm tài liệu."));
        };
        QObject::connect(cols,&QSpinBox::valueChanged,&dialog,[&](int){refresh();});
        QObject::connect(rows,&QSpinBox::valueChanged,&dialog,[&](int){refresh();});
        QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        refresh();
        if(dialog.exec()==QDialog::Accepted&&cols->value()*rows->value()<=5000&&target.verify()){
            send(target.tab.data(),insertTable(cols->value(),rows->value()));
            target.tab->view()->setFocus();
        }
    } else if (name == QLatin1String("image")) {
        const WordInsertTarget target(tab);
        const QString f = QFileDialog::getOpenFileName(parent, QObject::tr("Chèn ảnh"), QString(),
                                                       QObject::tr("Ảnh (*.png *.jpg *.jpeg *.gif *.bmp *.svg)"));
        if(!f.isEmpty() && target.verify()) {
            const QFileInfo info(f);
            if(!info.isFile() || !info.isReadable()) {
                QMessageBox::warning(target.tab.data(),QObject::tr("Chèn ảnh"),QObject::tr("Tệp ảnh không còn đọc được."));
                return true;
            }
            send(target.tab.data(),insertImage(ss(QUrl::fromLocalFile(f).toString())));
            target.tab->view()->setFocus();
        }
    } else if (name == QLatin1String("link")) {
        doc->waitInputIdle();
        QPointer<DocumentTab> live(tab);
        const auto selected=doc->selectionAs("text/plain;charset=utf-8");
        const auto caret=doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR);
        const auto selection=doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION);
        if(selected.empty()&&caret.empty()){
            QMessageBox::information(parent,QObject::tr("Liên kết"),QObject::tr("Đặt con trỏ hoặc chọn chữ trước khi chèn liên kết."));return true;
        }
        bool ok = false;
        const QString url = QInputDialog::getText(parent, QObject::tr("Liên kết"), QObject::tr("Địa chỉ (URL):"), QLineEdit::Normal,
                                                  QStringLiteral("https://"), &ok);
        if (!ok || url.trimmed().isEmpty()) return true;
        const QUrl target(url.trimmed(),QUrl::StrictMode);
        const bool web=(target.scheme()==QLatin1String("https")||target.scheme()==QLatin1String("http"))&&!target.host().isEmpty()&&target.userInfo().isEmpty();
        const bool mail=target.scheme()==QLatin1String("mailto")&&!target.path().isEmpty();
        if(!target.isValid()||(!web&&!mail)){
            QMessageBox::warning(parent,QObject::tr("Liên kết"),QObject::tr("Chỉ chèn địa chỉ HTTP, HTTPS hoặc mailto hợp lệ. Chưa chèn liên kết."));return true;
        }
        if(!live||live->readOnly()||live->document()!=doc||!live->view())return true;
        doc->waitInputIdle();
        if(doc->selectionAs("text/plain;charset=utf-8")!=selected||
           doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR)!=caret||
           doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION)!=selection){
            QMessageBox::warning(live.data(),QObject::tr("Liên kết"),QObject::tr("Vị trí hoặc vùng chữ thay đổi. Chưa chèn liên kết."));return true;
        }
        QString text=QString::fromUtf8(selected.c_str());
        if(text.isEmpty())text=url.trimmed();
        send(live.data(),hyperlink(ss(text),ss(target.toString(QUrl::FullyEncoded))));
        doc->waitInputIdle();
        if(live&&live->view())live->view()->setFocus();
    } else if (name == QLatin1String("find") || name == QLatin1String("replace")) {
        const QPointer<DocumentTab> live(tab);
        QDialog dlg(parent);
        dlg.setWindowTitle(QObject::tr("Tìm và thay thế"));
        auto* root=new QVBoxLayout(&dlg);
        auto* form=new QFormLayout;
        auto* what=new QLineEdit(&dlg);
        auto* with=new QLineEdit(&dlg);
        form->addRow(QObject::tr("Tìm:"),what);
        form->addRow(QObject::tr("Thay bằng:"),with);
        root->addLayout(form);
        auto* note=new QLabel(QObject::tr("Tìm trước/sau trong tài liệu. Thay tất cả cần xác nhận; để trống ô Thay bằng nếu muốn xóa nội dung tìm thấy."),&dlg);
        note->setWordWrap(true);root->addWidget(note);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dlg);
        auto* prev=buttons->addButton(QObject::tr("Tìm trước"),QDialogButtonBox::ActionRole);
        auto* next=buttons->addButton(QObject::tr("Tìm tiếp"),QDialogButtonBox::ActionRole);
        auto* replace=buttons->addButton(QObject::tr("Thay tất cả"),QDialogButtonBox::ActionRole);
        root->addWidget(buttons);
        auto ready=[&]{return live && live->document()==doc && !what->text().isEmpty();};
        auto refresh=[&]{prev->setEnabled(ready());next->setEnabled(ready());replace->setEnabled(ready()&&!live->readOnly());};
        QObject::connect(what,&QLineEdit::textChanged,&dlg,[&]{refresh();});
        auto find=[&](bool backward){
            if(!ready())return;
            doc->waitInputIdle();
            send(live.data(),findNext(ss(what->text()),backward));
            doc->waitInputIdle();
            if(doc->selectionAs("text/plain;charset=utf-8").empty())
                note->setText(QObject::tr("Engine không trả vùng chọn. Kiểm tra từ khóa."));
            else note->setText(QObject::tr("Đã gửi lệnh tìm; đóng hộp thoại để xem vùng chọn trong tài liệu."));
        };
        QObject::connect(prev,&QPushButton::clicked,&dlg,[&]{find(true);});
        QObject::connect(next,&QPushButton::clicked,&dlg,[&]{find(false);});
        QObject::connect(what,&QLineEdit::returnPressed,&dlg,[&]{find(false);});
        QObject::connect(replace,&QPushButton::clicked,&dlg,[&]{
            if(!ready()||live->readOnly())return;
            const QString needle=what->text(),replacement=with->text();
            if(QMessageBox::question(&dlg,QObject::tr("Thay tất cả"),
                QObject::tr("Thay mọi lần xuất hiện của «%1» bằng «%2» trong tài liệu?").arg(needle,replacement),
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            if(!live||live->document()!=doc||live->readOnly())return;
            doc->waitInputIdle();
            send(live.data(),replaceAll(ss(needle),ss(replacement)));
            doc->waitInputIdle();
            dlg.accept();
        });
        QObject::connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        refresh();
        if(name==QLatin1String("replace")) with->setPlaceholderText(QObject::tr("Nội dung thay thế"));
        what->setFocus();
        dlg.exec();
        if(live&&live->view())live->view()->setFocus();
    } else if (name == QLatin1String("a4") || name == QLatin1String("landscape")) {
        // Đơn vị 1/100 mm (đã kiểm chứng trong word_tests.cpp).
        const bool land = name == QLatin1String("landscape");
        doc->command(".uno:AttributePageSize",
                     join({argLong("AttributePageSize.Width", land ? 29700 : 21000),
                           argLong("AttributePageSize.Height", land ? 21000 : 29700)}));
    } else if (name.startsWith(QLatin1String("margin"))) {
        // Lề trang, đơn vị 1/100 mm. Lệnh PageLRMargin/PageULMargin đã kiểm chứng trên LibreOffice 24.2 (AttributePageLRSpace/ULSpace KHÔNG có tác dụng). "marginVN": thể thức văn bản hành chính (Nghị định 30/2020/NĐ-CP):
        // trên 20, dưới 20, trái 30, phải 15 mm. Các lề khác: thường 25,4 mm; hẹp 12,7 mm.
        long l = 3000, r = 1500, t = 2000, b = 2000;
        QString label = QObject::tr("hành chính");
        if (name == QLatin1String("marginNormal")) { l = r = t = b = 2540; label = QObject::tr("thường"); }
        else if (name == QLatin1String("marginNarrow")) { l = r = t = b = 1270; label = QObject::tr("hẹp"); }
        else if (name == QLatin1String("marginCustom")) {
            QPointer<DocumentTab> guard(tab);
            QDialog dialog(parent);
            dialog.setWindowTitle(QObject::tr("Lề trang tùy chỉnh"));
            auto* layout=new QVBoxLayout(&dialog);
            auto* hint=new QLabel(QObject::tr("Nhập lề theo mm. Giá trị ban đầu là mẫu hành chính, không phải lề hiện tại. Áp dụng cho toàn bộ tài liệu."),&dialog);
            hint->setWordWrap(true);
            layout->addWidget(hint);
            auto* form=new QFormLayout;
            auto make=[&](const QString& title,double value) {
                auto* field=new QDoubleSpinBox(&dialog);
                field->setRange(0,100);
                field->setDecimals(1);
                field->setSingleStep(1);
                field->setSuffix(QObject::tr(" mm"));
                field->setValue(value);
                form->addRow(title,field);
                return field;
            };
            auto* left=make(QObject::tr("Trái"),30);
            auto* right=make(QObject::tr("Phải"),15);
            auto* top=make(QObject::tr("Trên"),20);
            auto* bottom=make(QObject::tr("Dưới"),20);
            layout->addLayout(form);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
            QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&] {
                // A4 portrait/landscape: leave at least 20 mm content in both orientations.
                if(left->value()+right->value()>190 || top->value()+bottom->value()>190) {
                    QMessageBox::warning(&dialog,QObject::tr("Lề trang"),QObject::tr("Tổng hai lề đối diện không được vượt 190 mm."));
                    return;
                }
                dialog.accept();
            });
            layout->addWidget(buttons);
            if(dialog.exec()!=QDialog::Accepted || !guard || guard->readOnly() || guard->document()!=doc) return true;
            l=qRound(left->value()*100);r=qRound(right->value()*100);
            t=qRound(top->value()*100);b=qRound(bottom->value()*100);
            label=QObject::tr("tùy chỉnh");
            // Use the validated all-section copy path on both platforms.
            // Native Linux page style commands do not promise all sections.
            MarginSpec custom;
            custom.left=mmToTwips(l/100.0);custom.right=mmToTwips(r/100.0);
            custom.top=mmToTwips(t/100.0);custom.bottom=mmToTwips(b/100.0);
            return applyMarginsByPatch(tab,parent,custom,label);
        }
#ifdef Q_OS_WIN
        // Engine Windows 26.8.3: lệnh PageLRMargin/PageULMargin không đổi lề (CI đã thử 6 bộ lệnh). Dùng đường vá XML trên bản sao.
        MarginSpec spec;
        spec.left = mmToTwips(l / 100.0); spec.right = mmToTwips(r / 100.0);
        spec.top = mmToTwips(t / 100.0);  spec.bottom = mmToTwips(b / 100.0);
        applyMarginsByPatch(tab, parent, spec, label);
#else
        (void)label;
        doc->command(".uno:PageLRMargin", join({argLong("PageLRMargin.Left", l), argLong("PageLRMargin.Right", r)}));
        doc->command(".uno:PageULMargin", join({argLong("PageULMargin.Upper", t), argLong("PageULMargin.Lower", b)}));
#endif
    } else if (name == QLatin1String("adminHead") || name == QLatin1String("adminTail") || name == QLatin1String("adminFull")) {
        const WordInsertTarget target(tab);
        const bool head = name != QLatin1String("adminTail"), tail = name != QLatin1String("adminHead");
        QDialog dlg(parent);
        dlg.setWindowTitle(QObject::tr("Chèn khối văn bản hành chính"));
        dlg.resize(820, 700);
        auto* lay = new QVBoxLayout(&dlg);
        auto* hint = new QLabel(QObject::tr("Khối nội dung tham khảo Phụ lục I Nghị định 30/2020/NĐ-CP. Chèn tại con trỏ, không phải đầu/chân trang lặp. Kiểm tra bố cục, lề và số trang sau khi chèn."), &dlg);
        hint->setWordWrap(true); lay->addWidget(hint);
        auto* form = new QFormLayout;
        QSettings settings;
        auto* agency = new QLineEdit(settings.value(QStringLiteral("admin/agency")).toString(), &dlg);
        auto* parentAgency = new QLineEdit(settings.value(QStringLiteral("admin/parentAgency")).toString(), &dlg);
        auto* number = new QLineEdit(&dlg);
        auto* place = new QLineEdit(settings.value(QStringLiteral("admin/place")).toString(), &dlg);
        auto* title = new QLineEdit(&dlg);
        auto* subject = new QLineEdit(&dlg);
        auto* recipients = new QPlainTextEdit(&dlg);
        recipients->setPlainText(QStringLiteral("- …;\n- Lưu: VT."));
        recipients->setMaximumHeight(64);
        auto* role = new QLineEdit(settings.value(QStringLiteral("admin/role")).toString(), &dlg);
        auto* signer = new QLineEdit(settings.value(QStringLiteral("admin/signer")).toString(), &dlg);
        title->setPlaceholderText(QObject::tr("Ví dụ: THÔNG BÁO; bỏ trống nếu là công văn"));
        if (head) {
            form->addRow(QObject::tr("Cơ quan chủ quản:"), parentAgency);
            form->addRow(QObject::tr("Cơ quan ban hành:"), agency);
            form->addRow(QObject::tr("Số / ký hiệu:"), number);
            form->addRow(QObject::tr("Địa danh:"), place);
            form->addRow(QObject::tr("Tên loại:"), title);
            form->addRow(QObject::tr("Trích yếu:"), subject);
        } else { for (auto* w : {agency, parentAgency, number, place, title, subject}) w->hide(); }
        if (tail) {
            form->addRow(QObject::tr("Nơi nhận:"), recipients);
            form->addRow(QObject::tr("Chức vụ ký:"), role);
            form->addRow(QObject::tr("Người ký:"), signer);
        } else { recipients->hide(); role->hide(); signer->hide(); }
        lay->addLayout(form);
        auto* remember = new QCheckBox(QObject::tr("Lưu thông tin cơ quan, địa danh và người ký trên máy này"), &dlg);
        lay->addWidget(remember);
        auto* preview = new QTextBrowser(&dlg);
        preview->setOpenExternalLinks(false); lay->addWidget(preview, 1);
        auto render = [&] {
            AdminBlock b;
            b.agency = ss(agency->text().trimmed().toUpper()); b.parentAgency = ss(parentAgency->text().trimmed().toUpper());
            b.number = ss(number->text().trimmed()); b.placeDate = ss((place->text().trimmed().isEmpty() ? QStringLiteral("…") : place->text().trimmed()) + QStringLiteral(", ") + vntext::formatDateVn(QDate::currentDate(), vntext::DateStyle::Admin));
            b.title = ss(title->text().trimmed().toUpper()); b.subject = ss(subject->text().trimmed());
            b.recipient = ss(recipients->toPlainText()); b.signerRole = ss(role->text().trimmed().toUpper()); b.signerName = ss(signer->text().trimmed());
            return adminBlockHtml(b, head, tail);
        };
        auto refresh = [&] { preview->setHtml(QString::fromStdString(render())); };
        for (auto* e : {agency, parentAgency, number, place, title, subject, role, signer})
            QObject::connect(e, &QLineEdit::textChanged, &dlg, refresh);
        QObject::connect(recipients, &QPlainTextEdit::textChanged, &dlg, refresh);
        refresh();
        auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        box->button(QDialogButtonBox::Ok)->setText(QObject::tr("Chèn vào văn bản"));
        QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(box);
        if (dlg.exec() != QDialog::Accepted || !target.verify()) return true;
        if (!target.selected.empty() &&
            QMessageBox::question(parent, QObject::tr("Thay vùng chọn?"), QObject::tr("Đang có văn bản được chọn. Chèn sẽ thay vùng này. Tiếp tục?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return true;
        if (!target.verify()) return true;
        if (!doc->paste("text/html", render())) {
            QMessageBox::warning(parent, QObject::tr("Chèn khối"), QObject::tr("Engine không xác nhận chèn. Hãy kiểm tra văn bản trước khi thử lại."));
            return true;
        }
        if (remember->isChecked()) {
            if (head) { settings.setValue(QStringLiteral("admin/agency"), agency->text()); settings.setValue(QStringLiteral("admin/parentAgency"), parentAgency->text()); settings.setValue(QStringLiteral("admin/place"), place->text()); }
            if (tail) { settings.setValue(QStringLiteral("admin/role"), role->text()); settings.setValue(QStringLiteral("admin/signer"), signer->text()); }
        }
        tab->view()->setFocus();
    } else if (name == QLatin1String("vnMoney")) {
        bool ok = false;
        const WordInsertTarget target(tab);
        const QString originalSelection = QString::fromStdString(target.selected);
        const QString selected = originalSelection.trimmed();
        const QString in = QInputDialog::getText(parent, QObject::tr("Số → chữ"),
            QObject::tr("Số đang chọn (hoặc nhập số):"), QLineEdit::Normal, selected, &ok);
        if (!ok || in.trimmed().isEmpty()) return true;
        const QStringList modes{QObject::tr("Tiền đồng · dấu phẩy thập phân"), QObject::tr("Tiền đồng · dấu chấm thập phân"),
                                QObject::tr("Số thường · dấu phẩy thập phân"), QObject::tr("Số thường · dấu chấm thập phân")};
        const auto mode = QInputDialog::getItem(parent, QObject::tr("Kiểu đọc"), QObject::tr("Chọn kiểu và định dạng số:"), modes, 0, false, &ok);
        if (!ok) return true;
        const int style = modes.indexOf(mode);
        vntext::MoneyOptions options; options.currency = style < 2; options.chan = options.currency;
        options.decimalSeparator = style % 2 == 0 ? QLatin1Char(',') : QLatin1Char('.');
        const auto r = vntext::moneyToWords(in.trimmed(), options);
        if (!r.ok) {
            QMessageBox::warning(parent, QObject::tr("Số tiền → chữ"), QObject::tr("Không đổi được: %1\nKhông có gì được chèn vào văn bản.").arg(r.error));
            return true;
        }
        QString msg = QObject::tr("Xem trước:\n\n%1\n\n%2").arg(r.text, selected.isEmpty() ? QObject::tr("Chèn vào vị trí con trỏ?") : QObject::tr("Thay văn bản đang chọn bằng kết quả?"));
        if (!r.warnings.isEmpty()) msg += QObject::tr("\n\nLưu ý: ") + r.warnings.join(QStringLiteral("; "));
        if (QMessageBox::question(parent, QObject::tr("Số tiền → chữ"), msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            if (!target.verify()) return true;
            if (!doc->paste("text/plain;charset=utf-8", ss(r.text)))
                QMessageBox::warning(parent, QObject::tr("Số → chữ"), QObject::tr("Engine chưa xác nhận chèn."));
        }
    } else if (name == QLatin1String("vnDate")) {
        const WordInsertTarget target(tab);
        const QString t = vntext::formatDateVn(QDate::currentDate(), vntext::DateStyle::Admin);
        const QString prompt = target.selected.empty() ? QObject::tr("Chèn vào vị trí con trỏ?") :
            QObject::tr("Thay vùng chữ đang chọn bằng ngày tháng?");
        if (QMessageBox::question(parent, QObject::tr("Chèn ngày Việt"),
                QObject::tr("Xem trước:\n\n%1\n\n%2").arg(t, prompt),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes && target.verify()) {
            if (!doc->paste("text/plain;charset=utf-8", ss(t)))
                QMessageBox::warning(parent, QObject::tr("Chèn ngày Việt"), QObject::tr("Engine chưa xác nhận chèn."));
        }
    } else if (name == QLatin1String("vnTcvn3")) {
        const WordInsertTarget target(tab);
        const QString sel = QString::fromStdString(target.selected);
        if (sel.isEmpty()) {
            QMessageBox::information(parent, QObject::tr("TCVN3 → Unicode"), QObject::tr("Hãy chọn đoạn văn bản cần đổi trước."));
            return true;
        }
        const auto r = vntext::legacyToUnicode(sel, vntext::LegacyEncoding::TCVN3);
        if (r.verdict != vntext::Verdict::Converted) {
            QString why;
            switch (r.verdict) {
            case vntext::Verdict::AlreadyUnicode: why = QObject::tr("Văn bản đã là Unicode."); break;
            case vntext::Verdict::NothingToConvert: why = QObject::tr("Không có ký tự thuộc bảng mã TCVN3."); break;
            case vntext::Verdict::Unsupported: why = QObject::tr("Bảng mã này chưa được hỗ trợ."); break;
            default: why = QObject::tr("Không đủ độ tin cậy (%1%) để tự đổi.").arg(int(r.confidence * 100)); break;
            }
            QMessageBox::warning(parent, QObject::tr("TCVN3 → Unicode"),
                QObject::tr("Không đổi gì. %1\n%2").arg(why, r.warnings.join(QStringLiteral("\n"))));
            return true;
        }
        QString msg = QObject::tr("Độ tin cậy %1%. Xem trước:\n\n%2\n\nÁp dụng thay phần đã chọn? (Hoàn tác được bằng Ctrl+Z)")
                          .arg(int(r.confidence * 100)).arg(r.text.left(400));
        if (!r.warnings.isEmpty()) msg += QObject::tr("\n\nLưu ý: ") + r.warnings.join(QStringLiteral("; "));
        if (QMessageBox::question(parent, QObject::tr("TCVN3 → Unicode"), msg,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes && target.verify()) {
            if (!doc->paste("text/plain;charset=utf-8", ss(r.text)))
                QMessageBox::warning(parent, QObject::tr("TCVN3 → Unicode"), QObject::tr("Engine chưa xác nhận chèn."));
        }
    } else if (name == QLatin1String("vnSyllable")) {
        // Chỉ liệt kê âm tiết nghi sai, KHÔNG tự sửa. Kiểm vùng chọn; nếu không chọn gì thì kiểm toàn văn bản.
        doc->waitInputIdle();
        std::string raw = doc->selectionAs("text/plain;charset=utf-8");
        const bool wholeDoc = raw.empty();
        if (wholeDoc) {
            doc->command(".uno:SelectAll");
            doc->waitInputIdle();
            raw = doc->selectionAs("text/plain;charset=utf-8");
            doc->command(".uno:GoToStartOfDoc");
            doc->waitInputIdle();
        }
        const QString text = QString::fromUtf8(raw.c_str());
        const auto issues = vntext::spell::checkText(text);
        const QString note = QObject::tr("Chỉ bắt âm tiết KHÔNG tồn tại trong tiếng Việt (gõ sai dấu/chữ). Không bắt lỗi dùng sai từ "
                                         "(ví dụ \"xử dụng\"), không kiểm từ mượn/viết tắt, và có thể báo nhầm tên riêng hiếm. "
                                         "Không có gì bị tự sửa.");
        if (issues.isEmpty()) {
            QMessageBox::information(parent, QObject::tr("Kiểm tra âm tiết"),
                QObject::tr("Không thấy âm tiết nào sai trong %1.\n\n%2")
                    .arg(wholeDoc ? QObject::tr("toàn văn bản") : QObject::tr("phần đã chọn"), note));
            return true;
        }
        QDialog dlg(parent);
        dlg.setWindowTitle(QObject::tr("Kiểm tra âm tiết (%1 nghi sai)").arg(issues.size()));
        dlg.resize(760, 460);
        auto* lay = new QVBoxLayout(&dlg);
        auto* hint = new QLabel(note, &dlg);
        hint->setWordWrap(true);
        lay->addWidget(hint);
        auto* list = new QListWidget(&dlg);
        for (const auto& it : issues) list->addItem(vntext::spell::describeIssue(text, it));
        list->setCurrentRow(0);
        lay->addWidget(list, 1);
        auto* box = new QDialogButtonBox(&dlg);
        auto* jump = box->addButton(QObject::tr("Tìm trong văn bản"), QDialogButtonBox::ActionRole);
        box->addButton(QObject::tr("Đóng"), QDialogButtonBox::RejectRole);
        lay->addWidget(box);
        // Tìm lần xuất hiện kế tiếp của âm tiết đang chọn (bắt đầu từ vị trí con trỏ hiện tại). Không sửa văn bản.
        QObject::connect(jump, &QPushButton::clicked, &dlg, [&]() {
            const int row = list->currentRow();
            if (row >= 0 && row < issues.size()) send(tab, findNext(ss(issues[row].text), false));
        });
        QObject::connect(list, &QListWidget::itemDoubleClicked, jump, &QPushButton::click);
        QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        dlg.exec();
    } else if (name == QLatin1String("count")) {
        doc->waitInputIdle();
        std::string text = doc->selectionAs("text/plain;charset=utf-8");
        const bool sel = !text.empty();
        if (!sel) {
            doc->command(".uno:SelectAll");
            doc->waitInputIdle();
            text = doc->selectionAs("text/plain;charset=utf-8");
            // Do not leave the entire document selected after a read-only report.
            doc->command(".uno:GoToStartOfDoc");
            doc->waitInputIdle();
        }
        QMessageBox::information(parent, QObject::tr("Đếm từ"),
            QObject::tr("%1 từ (%2 ký tự) trong %3.").arg(countWords(text)).arg(QString::fromUtf8(text.c_str()).size())
                .arg(sel ? QObject::tr("phần đã chọn") : QObject::tr("toàn văn bản")));
    }
    return true;
}

}  // namespace dung::word
