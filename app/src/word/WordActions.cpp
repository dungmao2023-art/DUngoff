// SPDX-License-Identifier: MPL-2.0
#include "WordActions.h"

#include "../DocumentTab.h"
#include "WordCommands.h"

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
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStringList>
#include <QUrl>

namespace dung::word {

static std::string ss(const QString& s) { return s.toUtf8().toStdString(); }

static void send(DocumentTab* t, const Call& c) { t->document()->command(c.first, c.second); }

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
        if (name == QLatin1String("marginNormal")) l = r = t = b = 2540;
        else if (name == QLatin1String("marginNarrow")) l = r = t = b = 1270;
        doc->command(".uno:PageLRMargin", join({argLong("PageLRMargin.Left", l), argLong("PageLRMargin.Right", r)}));
        doc->command(".uno:PageULMargin", join({argLong("PageULMargin.Upper", t), argLong("PageULMargin.Lower", b)}));
#ifdef Q_OS_WIN
        // Engine Windows 26.8.3: CI đã thử 6 bộ lệnh lề (PageLRMargin, LeftRightMargin... ) và tệp DOCX vẫn giữ lề 20 mm.
        // Không giả vờ thành công: báo rõ để người dùng đặt lề bằng hộp thoại Định dạng trang.
        QMessageBox::warning(parent, QObject::tr("Lề trang"),
            QObject::tr("Phiên bản engine hiện tại trên Windows CHƯA xác nhận được việc đặt lề bằng nút này (kiểm thử tự động cho thấy lề không đổi).\n"
                        "Vui lòng kiểm tra lại lề trong hộp thoại định dạng trang trước khi in hoặc gửi."));
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
