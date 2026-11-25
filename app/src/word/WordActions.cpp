// SPDX-License-Identifier: MPL-2.0
#include "WordActions.h"

#include "../DocumentTab.h"
#include "WordCommands.h"
#include "PageMargins.h"

#include "dung/Engine.h"
#include "../vntext/VnText.h"
#include "../vntext/SyllableCheck.h"

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
    const QString name = id.mid(5);

    if (name == QLatin1String("font")) {
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
    } else if (name == QLatin1String("vnMoney")) {
        bool ok = false;
        const QString in = QInputDialog::getText(parent, QObject::tr("Số tiền → chữ"),
            QObject::tr("Nhập số tiền (vd 1.500.000 hoặc 1500000):"), QLineEdit::Normal, QString(), &ok);
        if (!ok || in.trimmed().isEmpty()) return true;
        const auto r = vntext::moneyToWords(in.trimmed());
        if (!r.ok) {
            QMessageBox::warning(parent, QObject::tr("Số tiền → chữ"), QObject::tr("Không đổi được: %1\nKhông có gì được chèn vào văn bản.").arg(r.error));
            return true;
        }
        QString msg = QObject::tr("Xem trước:\n\n%1\n\nChèn vào vị trí con trỏ?").arg(r.text);
        if (!r.warnings.isEmpty()) msg += QObject::tr("\n\nLưu ý: ") + r.warnings.join(QStringLiteral("; "));
        if (QMessageBox::question(parent, QObject::tr("Số tiền → chữ"), msg) == QMessageBox::Yes)
            doc->paste("text/plain;charset=utf-8", ss(r.text));
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
