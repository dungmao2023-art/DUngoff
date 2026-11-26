// SPDX-License-Identifier: MPL-2.0
#include "WordActions.h"

#include "../DocumentTab.h"
#include "WordCommands.h"
#include "PageMargins.h"

#include "dung/Engine.h"
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
                              "Engine trên Windows chưa nhận lề qua lệnh, nên Dũng Office sẽ tạo MỘT BẢN SAO của tài liệu hiện tại "
                              "(gồm cả phần bạn vừa sửa) với lề mới và mở trong thẻ mới.\n"
                              "• Thẻ hiện tại và tệp gốc KHÔNG bị đổi; thẻ hiện tại vẫn giữ lịch sử hoàn tác.\n"
                              "• Thẻ mới bắt đầu ở đầu tài liệu, chưa có lịch sử hoàn tác, và được đánh dấu chưa lưu (Lưu sẽ hỏi tên mới, không ghi đè tệp gốc).")
                      .arg(label, mm(spec.left), mm(spec.right), mm(spec.top), mm(spec.bottom));
    if (notDocx) msg += QObject::tr("\n• Bản sao được lưu ở định dạng .docx (tệp gốc không phải .docx).");
    msg += QObject::tr("\n\nTiếp tục?");
    if (QMessageBox::question(parent, QObject::tr("Lề trang"), msg) != QMessageBox::Yes) return true;

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
    QString name = id.mid(5);
    if (name == QLatin1String("reviewSyllable")) name = QStringLiteral("vnSyllable");
    if (name == QLatin1String("reviewCount")) name = QStringLiteral("count");
    if (name == QLatin1String("adminMargin")) name = QStringLiteral("marginVN");

    if (name == QLatin1String("addComment")) {
        doc->waitInputIdle();
        bool ok=false;
        const QString comment=QInputDialog::getMultiLineText(parent,QObject::tr("Bình luận Word"),
            QObject::tr("Nhập nội dung bình luận tại con trỏ hoặc vùng chữ đang chọn:"),QString(),&ok);
        if(!ok||comment.trimmed().isEmpty())return true;
        if(comment.size()>10000){QMessageBox::warning(parent,QObject::tr("Bình luận Word"),QObject::tr("Tối đa 10.000 ký tự."));return true;}
        doc->command(".uno:InsertAnnotation",Document::stringArg("Text",ss(comment)));
        doc->waitInputIdle();
        tab->view()->setFocus();
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
        const auto fonts = parseFontList(doc->commandValues(".uno:CharFontName"));
        QStringList items;
        for (const auto& f : fonts) items << QString::fromUtf8(f.c_str());
        bool ok = false;
        const QString pick = items.isEmpty()
            ? QInputDialog::getText(parent, QObject::tr("Phông chữ"), QObject::tr("Tên phông:"), QLineEdit::Normal, QString(), &ok)
            : QInputDialog::getItem(parent, QObject::tr("Phông chữ"), QObject::tr("Chọn phông:"), items, 0, true, &ok);
        if (ok && !pick.trimmed().isEmpty()) send(tab, fontName(ss(pick.trimmed())));
    } else if (name == QLatin1String("size")) {
        QStringList items;
        for (double s : commonFontSizes()) items << QString::number(s);
        bool ok = false;
        const QString pick = QInputDialog::getItem(parent, QObject::tr("Cỡ chữ"), QObject::tr("Cỡ chữ (pt):"), items, 3, true, &ok);
        double v = pick.toDouble(&ok);
        if (ok && v >= 1 && v <= 400) send(tab, fontSize(v));
    } else if (name == QLatin1String("color") || name == QLatin1String("back")) {
        const QColor c = QColorDialog::getColor(Qt::black, parent, QObject::tr("Chọn màu"));
        if (c.isValid()) {
            const unsigned rgb = static_cast<unsigned>(c.red() << 16 | c.green() << 8 | c.blue());
            send(tab, name == QLatin1String("color") ? fontColor(rgb) : backColor(rgb));
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
            doc->waitInputIdle();
            if (!doc->paste("text/plain;charset=utf-8", ss(edit->text())))
                QMessageBox::warning(parent, QObject::tr("Chèn ký hiệu"), QObject::tr("Engine không xác nhận chèn ký hiệu."));
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
        bool ok = false;
        const int cols = QInputDialog::getInt(parent, QObject::tr("Chèn bảng"), QObject::tr("Số cột:"), 3, 1, 63, 1, &ok);
        if (!ok) return true;
        const int rows = QInputDialog::getInt(parent, QObject::tr("Chèn bảng"), QObject::tr("Số hàng:"), 3, 1, 500, 1, &ok);
        if (ok) send(tab, insertTable(cols, rows));
    } else if (name == QLatin1String("image")) {
        const QString f = QFileDialog::getOpenFileName(parent, QObject::tr("Chèn ảnh"), QString(),
                                                       QObject::tr("Ảnh (*.png *.jpg *.jpeg *.gif *.bmp *.svg)"));
        if (!f.isEmpty()) send(tab, insertImage(ss(QUrl::fromLocalFile(f).toString())));
    } else if (name == QLatin1String("link")) {
        bool ok = false;
        const QString url = QInputDialog::getText(parent, QObject::tr("Liên kết"), QObject::tr("Địa chỉ (URL):"), QLineEdit::Normal,
                                                  QStringLiteral("https://"), &ok);
        if (!ok || url.trimmed().isEmpty()) return true;
        QString text = QString::fromUtf8(doc->selectionAs("text/plain;charset=utf-8").c_str());
        if (text.isEmpty()) text = url.trimmed();
        send(tab, hyperlink(ss(text), ss(url.trimmed())));
    } else if (name == QLatin1String("find") || name == QLatin1String("replace")) {
        bool ok = false;
        const QString what = QInputDialog::getText(parent, QObject::tr("Tìm"), QObject::tr("Tìm:"), QLineEdit::Normal, QString(), &ok);
        if (!ok || what.isEmpty()) return true;
        if (name == QLatin1String("find")) { send(tab, findNext(ss(what), false)); return true; }
        const QString with = QInputDialog::getText(parent, QObject::tr("Thay thế"), QObject::tr("Thay bằng:"), QLineEdit::Normal, QString(), &ok);
        if (ok) send(tab, replaceAll(ss(what), ss(with)));
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
        if (dlg.exec() != QDialog::Accepted) return true;
        doc->waitInputIdle();
        if (!doc->selectionAs("text/plain;charset=utf-8").empty() &&
            QMessageBox::question(parent, QObject::tr("Thay vùng chọn?"), QObject::tr("Đang có văn bản được chọn. Chèn sẽ thay vùng này. Tiếp tục?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return true;
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
        doc->waitInputIdle();
        const QString originalSelection = QString::fromStdString(doc->selectionAs("text/plain;charset=utf-8"));
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
            doc->waitInputIdle();
            if (QString::fromStdString(doc->selectionAs("text/plain;charset=utf-8")) != originalSelection) {
                QMessageBox::warning(parent, QObject::tr("Số → chữ"), QObject::tr("Vùng chọn đã thay đổi; chưa chèn kết quả.")); return true;
            }
            if (!doc->paste("text/plain;charset=utf-8", ss(r.text)))
                QMessageBox::warning(parent, QObject::tr("Số → chữ"), QObject::tr("Engine chưa xác nhận chèn."));
        }
    } else if (name == QLatin1String("vnDate")) {
        const QString t = vntext::formatDateVn(QDate::currentDate(), vntext::DateStyle::Admin);
        if (QMessageBox::question(parent, QObject::tr("Chèn ngày Việt"),
                QObject::tr("Xem trước:\n\n%1\n\nChèn vào vị trí con trỏ?").arg(t)) == QMessageBox::Yes)
            doc->paste("text/plain;charset=utf-8", ss(t));
    } else if (name == QLatin1String("vnTcvn3")) {
        const QString sel = QString::fromUtf8(doc->selectionAs("text/plain;charset=utf-8").c_str());
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
        if (QMessageBox::question(parent, QObject::tr("TCVN3 → Unicode"), msg) == QMessageBox::Yes)
            doc->paste("text/plain;charset=utf-8", ss(r.text));
    } else if (name == QLatin1String("vnSyllable")) {
        // Chỉ liệt kê âm tiết nghi sai, KHÔNG tự sửa. Kiểm vùng chọn; nếu không chọn gì thì kiểm toàn văn bản.
        std::string raw = doc->selectionAs("text/plain;charset=utf-8");
        const bool wholeDoc = raw.empty();
        if (wholeDoc) {
            doc->command(".uno:SelectAll");
            raw = doc->selectionAs("text/plain;charset=utf-8");
            doc->command(".uno:GoToStartOfDoc");
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
        std::string text = doc->selectionAs("text/plain;charset=utf-8");
        const bool sel = !text.empty();
        if (!sel) {
            doc->command(".uno:SelectAll");
            text = doc->selectionAs("text/plain;charset=utf-8");
        }
        QMessageBox::information(parent, QObject::tr("Đếm từ"),
            QObject::tr("%1 từ (%2 ký tự) trong %3.").arg(countWords(text)).arg(QString::fromUtf8(text.c_str()).size())
                .arg(sel ? QObject::tr("phần đã chọn") : QObject::tr("toàn văn bản")));
    }
    return true;
}

}  // namespace dung::word
