// SPDX-License-Identifier: MPL-2.0
#include "WordActions.h"

#include "../DocumentTab.h"
#include "WordCommands.h"

#include "dung/Engine.h"

#include <QColorDialog>
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
