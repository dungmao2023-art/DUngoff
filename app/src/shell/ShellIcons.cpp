// SPDX-License-Identifier: MPL-2.0
#include "ShellIcons.h"

#include "../Theme.h"

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

QPixmap badge(const QString& letter, const QColor& bg, const QColor& fg, int size, qreal dpr) {
    QPixmap pm = makePm(size, dpr);
    QPainter p(&pm);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.28, size * 0.28);
    QFont f(QStringLiteral("Segoe UI"));
    f.setBold(true);
    f.setPixelSize(letter.size() > 2 ? size * 0.30 : size * 0.48);
    p.setFont(f);
    p.setPen(fg);
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, letter);
    return pm;
}

// Hình vẽ bằng nét (không phụ thuộc font biểu tượng, nên giống nhau trên mọi máy).
QPixmap symbol(const QString& name, const QColor& color, const QColor& bg, int size, qreal dpr) {
    QPixmap pm = makePm(size, dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.28, size * 0.28);
    const qreal s = size / 24.0; // vẽ trên lưới 24
    p.scale(s, s);
    QPen pen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (name == QLatin1String("home")) {
        QPainterPath h;
        h.moveTo(5, 12); h.lineTo(12, 5.5); h.lineTo(19, 12);
        h.moveTo(7, 11); h.lineTo(7, 18.5); h.lineTo(17, 18.5); h.lineTo(17, 11);
        p.drawPath(h);
    } else if (name == QLatin1String("docs")) {
        p.drawRoundedRect(QRectF(6.5, 5, 11, 14), 2, 2);
        p.drawLine(QPointF(9.5, 9.5), QPointF(14.5, 9.5));
        p.drawLine(QPointF(9.5, 12.5), QPointF(14.5, 12.5));
        p.drawLine(QPointF(9.5, 15.5), QPointF(12.5, 15.5));
    } else if (name == QLatin1String("work")) {
        p.drawRoundedRect(QRectF(5.5, 6.5, 13, 12), 2, 2);
        p.drawLine(QPointF(5.5, 10.5), QPointF(18.5, 10.5));
        p.drawLine(QPointF(9, 4.5), QPointF(9, 8));
        p.drawLine(QPointF(15, 4.5), QPointF(15, 8));
    } else if (name == QLatin1String("templates")) {
        p.drawRoundedRect(QRectF(5.5, 5.5, 5.5, 5.5), 1.2, 1.2);
        p.drawRoundedRect(QRectF(13, 5.5, 5.5, 5.5), 1.2, 1.2);
        p.drawRoundedRect(QRectF(5.5, 13, 5.5, 5.5), 1.2, 1.2);
        p.drawRoundedRect(QRectF(13, 13, 5.5, 5.5), 1.2, 1.2);
    } else if (name == QLatin1String("projects")) {
        QPainterPath f;
        f.moveTo(5, 8); f.lineTo(5, 18); f.lineTo(19, 18); f.lineTo(19, 9.5);
        f.lineTo(12, 9.5); f.lineTo(10.5, 7); f.lineTo(5, 7); f.closeSubpath();
        p.drawPath(f);
    } else if (name == QLatin1String("tools")) {
        p.drawRoundedRect(QRectF(5, 9, 14, 9.5), 2, 2);
        p.drawLine(QPointF(9.5, 9), QPointF(9.5, 7));
        p.drawLine(QPointF(9.5, 7), QPointF(14.5, 7));
        p.drawLine(QPointF(14.5, 7), QPointF(14.5, 9));
        p.drawLine(QPointF(5, 13.5), QPointF(19, 13.5));
    } else if (name == QLatin1String("open")) {
        QPainterPath f;
        f.moveTo(5, 18); f.lineTo(5, 7); f.lineTo(10.5, 7); f.lineTo(12, 9); f.lineTo(18, 9); f.lineTo(18, 11);
        f.moveTo(5, 18); f.lineTo(7.5, 11.5); f.lineTo(20, 11.5); f.lineTo(17.5, 18); f.closeSubpath();
        p.drawPath(f);
    } else if (name == QLatin1String("about")) {
        p.drawEllipse(QPointF(12, 12), 7, 7);
        p.drawLine(QPointF(12, 11), QPointF(12, 16));
        p.drawPoint(QPointF(12, 8.3));
    } else if (name == QLatin1String("search")) {
        p.drawEllipse(QPointF(11, 11), 5.5, 5.5);
        p.drawLine(QPointF(15, 15), QPointF(19, 19));
    } else if (name == QLatin1String("arrow")) {
        p.drawLine(QPointF(8, 16), QPointF(16, 8));
        p.drawLine(QPointF(9.5, 8), QPointF(16, 8));
        p.drawLine(QPointF(16, 8), QPointF(16, 14.5));
    } else { // mặc định: chấm tròn
        p.setBrush(color);
        p.drawEllipse(QPointF(12, 12), 3, 3);
    }
    return pm;
}

QIcon icon(const QPixmap& pm) { return QIcon(pm); }

} // namespace shell
