// SPDX-License-Identifier: MPL-2.0
#include "Icons.h"

#include "IconData.h"

#include <QHash>
#include <QIconEngine>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace dung::ui {
namespace {
constexpr double kPi = 3.14159265358979323846;

// ---------- Bộ đọc đường dẫn kiểu SVG (M L H V C S Q Z A, hoa/thường) ----------
struct Cursor {
    const QString& s;
    int i = 0;
    explicit Cursor(const QString& str) : s(str) {}
    void skipSep() { while (i < s.size() && (s[i].isSpace() || s[i] == QLatin1Char(','))) ++i; }
    bool atNumber() {
        skipSep();
        if (i >= s.size()) return false;
        const QChar c = s[i];
        return c.isDigit() || c == QLatin1Char('-') || c == QLatin1Char('+') || c == QLatin1Char('.');
    }
    double number() {
        skipSep();
        const int start = i;
        if (i < s.size() && (s[i] == QLatin1Char('-') || s[i] == QLatin1Char('+'))) ++i;
        bool dot = false;
        while (i < s.size()) {
            if (s[i].isDigit()) ++i;
            else if (s[i] == QLatin1Char('.') && !dot) { dot = true; ++i; }
            else break;
        }
        return s.mid(start, i - start).toDouble();
    }
    bool flag() { // cờ cung: đúng 1 ký tự '0'/'1'
        skipSep();
        if (i >= s.size()) return false;
        return s[i++] == QLatin1Char('1');
    }
};

void arcTo(QPainterPath& path, QPointF from, double rx, double ry, bool large, bool sweep, QPointF to) {
    if (from == to) return;
    rx = qAbs(rx); ry = qAbs(ry);
    if (rx < 1e-9 || ry < 1e-9) { path.lineTo(to); return; }
    // Chuyển điểm cuối -> tâm (không hỗ trợ xoay trục; icon chỉ dùng cung tròn/elip thẳng trục).
    const double dx2 = (from.x() - to.x()) / 2, dy2 = (from.y() - to.y()) / 2;
    const double x1p = dx2, y1p = dy2;
    const double lam = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry);
    if (lam > 1) { const double k = qSqrt(lam); rx *= k; ry *= k; }
    const double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    const double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    double co = den == 0 ? 0 : qSqrt(qMax(0.0, num / den));
    if (large == sweep) co = -co;
    const double cxp = co * rx * y1p / ry, cyp = -co * ry * x1p / rx;
    const double cx = cxp + (from.x() + to.x()) / 2, cy = cyp + (from.y() + to.y()) / 2;
    auto ang = [](double ux, double uy, double vx, double vy) {
        const double a = qAtan2(uy, ux), b = qAtan2(vy, vx);
        return b - a;
    };
    const double ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry;
    const double vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;
    const double th1 = qAtan2(uy, ux);
    double dth = ang(ux, uy, vx, vy);
    if (!sweep && dth > 0) dth -= 2 * kPi;
    else if (sweep && dth < 0) dth += 2 * kPi;
    // Qt: góc dương = ngược chiều kim đồng hồ trên màn hình, y hướng xuống -> đảo dấu.
    path.arcTo(QRectF(cx - rx, cy - ry, 2 * rx, 2 * ry), -qRadiansToDegrees(th1), -qRadiansToDegrees(dth));
}

QPainterPath parsePath(const QString& d) {
    QPainterPath path;
    Cursor c(d);
    QPointF cur(0, 0), start(0, 0), lastCtrl(0, 0);
    QChar cmd, prevCmd;
    while (true) {
        c.skipSep();
        if (c.i >= d.size()) break;
        if (d[c.i].isLetter()) cmd = d[c.i++];
        else if (cmd.isNull()) break;
        else if (cmd == QLatin1Char('M')) cmd = QLatin1Char('L');
        else if (cmd == QLatin1Char('m')) cmd = QLatin1Char('l');
        const bool rel = cmd.isLower();
        const QChar u = cmd.toUpper();
        auto pt = [&](double x, double y) { return rel ? QPointF(cur.x() + x, cur.y() + y) : QPointF(x, y); };
        if (u == QLatin1Char('Z')) {
            path.closeSubpath(); cur = start;
        } else if (u == QLatin1Char('M')) {
            const double x = c.number(), y = c.number();
            cur = pt(x, y); start = cur; path.moveTo(cur);
        } else if (u == QLatin1Char('L')) {
            const double x = c.number(), y = c.number();
            cur = pt(x, y); path.lineTo(cur);
        } else if (u == QLatin1Char('H')) {
            const double x = c.number();
            cur = QPointF(rel ? cur.x() + x : x, cur.y()); path.lineTo(cur);
        } else if (u == QLatin1Char('V')) {
            const double y = c.number();
            cur = QPointF(cur.x(), rel ? cur.y() + y : y); path.lineTo(cur);
        } else if (u == QLatin1Char('C')) {
            const double a = c.number(), b = c.number(), e = c.number(), f = c.number(), g = c.number(), h = c.number();
            const QPointF p1 = pt(a, b), p2 = pt(e, f), p3 = pt(g, h);
            path.cubicTo(p1, p2, p3); lastCtrl = p2; cur = p3;
        } else if (u == QLatin1Char('S')) {
            const double e = c.number(), f = c.number(), g = c.number(), h = c.number();
            const bool smooth = prevCmd.toUpper() == QLatin1Char('C') || prevCmd.toUpper() == QLatin1Char('S');
            const QPointF p1 = smooth ? QPointF(2 * cur.x() - lastCtrl.x(), 2 * cur.y() - lastCtrl.y()) : cur;
            const QPointF p2 = pt(e, f), p3 = pt(g, h);
            path.cubicTo(p1, p2, p3); lastCtrl = p2; cur = p3;
        } else if (u == QLatin1Char('Q')) {
            const double a = c.number(), b = c.number(), e = c.number(), f = c.number();
            const QPointF p1 = pt(a, b), p2 = pt(e, f);
            path.quadTo(p1, p2); lastCtrl = p1; cur = p2;
        } else if (u == QLatin1Char('A')) {
            const double rx = c.number(), ry = c.number(); c.number(); // trục xoay: bỏ qua
            const bool large = c.flag(), sweep = c.flag();
            const double x = c.number(), y = c.number();
            const QPointF to = pt(x, y);
            arcTo(path, cur, rx, ry, large, sweep, to); cur = to;
        } else {
            break; // lệnh lạ: dừng, không đoán
        }
        prevCmd = cmd;
        if (u == QLatin1Char('Z')) { c.skipSep(); if (c.i < d.size() && !d[c.i].isLetter()) break; }
    }
    return path;
}

// ---------- Phần tử đã phân tích ----------
struct Elem {
    enum Mode { Stroke, Fill, Tint } mode = Stroke;
    QPainterPath path;
};

QVector<Elem> parseDsl(const QString& dsl) {
    QVector<Elem> out;
    for (const QString& part : dsl.split(QLatin1Char('|'), Qt::SkipEmptyParts)) {
        const int colon = part.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        const QString kind = part.left(colon), data = part.mid(colon + 1);
        Elem e;
        auto nums = [&] {
            QList<double> v;
            Cursor cu(data);
            while (cu.atNumber()) v << cu.number();
            return v;
        };
        if (kind == QLatin1String("p") || kind == QLatin1String("f") || kind == QLatin1String("tp")) {
            e.path = parsePath(data);
            e.mode = kind == QLatin1String("p") ? Elem::Stroke : (kind == QLatin1String("f") ? Elem::Fill : Elem::Tint);
        } else if (kind == QLatin1String("c") || kind == QLatin1String("fc")) {
            const auto v = nums();
            if (v.size() < 3) continue;
            e.path.addEllipse(QPointF(v[0], v[1]), v[2], v[2]);
            e.mode = kind == QLatin1String("c") ? Elem::Stroke : Elem::Fill;
        } else if (kind == QLatin1String("r") || kind == QLatin1String("fr") || kind == QLatin1String("tr")) {
            const auto v = nums();
            if (v.size() < 4) continue;
            const double rx = v.size() > 4 ? v[4] : 0;
            if (rx > 0) e.path.addRoundedRect(QRectF(v[0], v[1], v[2], v[3]), rx, rx);
            else e.path.addRect(QRectF(v[0], v[1], v[2], v[3]));
            e.mode = kind == QLatin1String("r") ? Elem::Stroke : (kind == QLatin1String("fr") ? Elem::Fill : Elem::Tint);
        } else continue;
        out << e;
    }
    return out;
}

const char* kPlaceholder = "r:4 4 16 16 3|p:M9 9l6 6";

const IconDef* findDef(const QString& name) {
    static const QHash<QString, const IconDef*> index = [] {
        QHash<QString, const IconDef*> h;
        for (int i = 0; i < kIconDefCount; ++i) h.insert(QString::fromLatin1(kIconDefs[i].name), &kIconDefs[i]);
        return h;
    }();
    return index.value(name, nullptr);
}

QVector<Elem> elementsFor(const QString& name) {
    static QMutex mu;
    static QHash<QString, QVector<Elem>> cache;
    QMutexLocker lock(&mu);
    auto it = cache.find(name);
    if (it != cache.end()) return it.value();
    const IconDef* d = findDef(name);
    const QVector<Elem> v = parseDsl(QString::fromLatin1(d ? d->dsl : kPlaceholder));
    cache.insert(name, v);
    return v;
}

void paintInto(QPainter& p, const QString& name, const QColor& color, const QRectF& target) {
    const QVector<Elem> els = elementsFor(name);
    const qreal px = qMin(target.width(), target.height()) * p.device()->devicePixelRatioF();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(target.topLeft());
    p.scale(target.width() / 24.0, target.height() / 24.0);
    // Nét 1.6 trên lưới 24; icon rất nhỏ thì dày hơn một chút để không mờ.
    const qreal strokeGrid = qMax(1.6, 24.0 / qMax<qreal>(px, 1) * 1.05);
    QPen pen(color, strokeGrid, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QColor tint = color;
    tint.setAlphaF(color.alphaF() * 0.30);
    for (const Elem& e : els) {
        switch (e.mode) {
        case Elem::Stroke: p.setPen(pen); p.setBrush(Qt::NoBrush); break;
        case Elem::Fill: p.setPen(Qt::NoPen); p.setBrush(color); break;
        case Elem::Tint: p.setPen(Qt::NoPen); p.setBrush(tint); break;
        }
        p.drawPath(e.path);
    }
    p.restore();
}

QColor colorFor(const QColor& base, QIcon::Mode mode) {
    if (mode == QIcon::Disabled) return disabledIconColor();
    return base.isValid() ? base : defaultIconColor();
}

class DungIconEngine : public QIconEngine {
public:
    DungIconEngine(QString n, QColor c) : m_name(std::move(n)), m_color(c) {}
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State) override {
        return render(size, mode, 1.0);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State, qreal scale) override {
        return render(size, mode, scale);
    }
    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine* clone() const override { return new DungIconEngine(m_name, m_color); }
    QString key() const override { return QStringLiteral("dung-icon"); }
private:
    QPixmap render(const QSize& logical, QIcon::Mode mode, qreal dpr) const {
        const QSize px(qMax(1, qRound(logical.width() * dpr)), qMax(1, qRound(logical.height() * dpr)));
        QPixmap pm(px);
        pm.fill(Qt::transparent);
        pm.setDevicePixelRatio(1.0);
        {
            QPainter p(&pm);
            paintInto(p, m_name, colorFor(m_color, mode), QRectF(QPointF(0, 0), QSizeF(px)));
        }
        pm.setDevicePixelRatio(dpr);
        return pm;
    }
    QString m_name;
    QColor m_color;
};

} // namespace

QColor defaultIconColor() { return QColor(0x16, 0x47, 0x8F); }
QColor disabledIconColor() { return QColor(0xA0, 0xAE, 0xC0); }

QImage renderImage(const QString& name, const QColor& color, int pixelSize) {
    const int px = qMax(1, pixelSize);
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    paintInto(p, name, color.isValid() ? color : defaultIconColor(), QRectF(0, 0, px, px));
    return img;
}

QIcon icon(const QString& name, const QColor& color, int /*logicalSize*/) {
    return QIcon(new DungIconEngine(name, color));
}

QPixmap pixmap(const QString& name, const QColor& color, int logicalSize, qreal dpr) {
    DungIconEngine e(name, color);
    return e.scaledPixmap(QSize(logicalSize, logicalSize), QIcon::Normal, QIcon::Off, dpr);
}

bool hasIcon(const QString& name) { return findDef(name) != nullptr; }

QStringList iconNames() {
    QStringList l;
    for (int i = 0; i < kIconDefCount; ++i) l << QString::fromLatin1(kIconDefs[i].name);
    return l;
}

QIcon iconOrEmpty(const QString& nameOrText, const QColor& color) {
    return hasIcon(nameOrText) ? icon(nameOrText, color) : QIcon();
}

} // namespace dung::ui
