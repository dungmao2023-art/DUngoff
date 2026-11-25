// SPDX-License-Identifier: MPL-2.0
#include "ShellIcons.h"

#include "../Theme.h"
#include "../ui/Icons.h"
#include <QHash>

#include <QFont>
#include <QPainter>
#include <QPainterPath>

namespace shell {

static QPixmap makePm(int size, qreal dpr) {
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    return pm;
}

// Mọi hình đều lấy từ bộ icon vector chung (dung::ui::icon) — không dùng font biểu tượng.
static void drawIcon(QPainter& p, const QString& name, const QColor& color, qreal box, qreal frac, qreal dpr) {
    const int px = qMax(8, int(box * frac));
    const QPixmap ic = dung::ui::pixmap(name, color, px, dpr);
    p.drawPixmap(QPointF((box - px) / 2.0, (box - px) / 2.0), ic);
}

QPixmap badge(const QString& letter, const QColor& bg, const QColor& fg, int size, qreal dpr) {
    QPixmap pm = makePm(size, dpr);
    QPainter p(&pm);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.28, size * 0.28);
    QString name;
    if (letter == QLatin1String("W")) name = QStringLiteral("word");
    else if (letter == QLatin1String("X")) name = QStringLiteral("excel");
    else if (letter == QLatin1String("P")) name = QStringLiteral("ppt");
    else if (letter == QLatin1String("PDF")) name = QStringLiteral("pdf");
    if (!name.isEmpty()) {
        drawIcon(p, name, fg, size, 0.66, dpr);
    } else { // chữ cái (logo "D")
        QFont f(QStringLiteral("Segoe UI"));
        f.setBold(true);
        f.setPixelSize(size * 0.48);
        p.setFont(f);
        p.setPen(fg);
        p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, letter);
    }
    return pm;
}

QPixmap symbol(const QString& name, const QColor& color, const QColor& bg, int size, qreal dpr) {
    QPixmap pm = makePm(size, dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.28, size * 0.28);
    if (name == QLatin1String("arrow")) { // mũi tên chéo nhỏ ở góc thẻ
        const qreal s = size / 24.0;
        p.scale(s, s);
        p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(8, 16), QPointF(16, 8));
        p.drawLine(QPointF(9.5, 8), QPointF(16, 8));
        p.drawLine(QPointF(16, 8), QPointF(16, 14.5));
        return pm;
    }
    static const QHash<QString, QString> map = {
        {QStringLiteral("docs"), QStringLiteral("folder")},     {QStringLiteral("work"), QStringLiteral("tasks")},
        {QStringLiteral("templates"), QStringLiteral("template")}, {QStringLiteral("projects"), QStringLiteral("folder")},
        {QStringLiteral("about"), QStringLiteral("info")},
    };
    const QString n = map.value(name, name);
    drawIcon(p, dung::ui::hasIcon(n) ? n : QStringLiteral("info"), color, size, bg.alpha() == 0 ? 0.9 : 0.62, dpr);
    return pm;
}

QIcon icon(const QPixmap& pm) { return QIcon(pm); }

} // namespace shell
