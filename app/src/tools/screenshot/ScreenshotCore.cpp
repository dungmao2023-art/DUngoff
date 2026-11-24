// SPDX-License-Identifier: MPL-2.0
#include "ScreenshotCore.h"

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QLineF>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QPdfWriter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtMath>
#include <algorithm>

namespace shot {

static QImage argb(const QImage& in) {
    return in.format() == QImage::Format_ARGB32 ? in : in.convertToFormat(QImage::Format_ARGB32);
}

void mosaicRegion(QImage& img, const QRect& regionIn, int block) {
    if (img.format() != QImage::Format_ARGB32) img = argb(img);
    block = std::max(block, kMinRedactBlock);
    const QRect r = regionIn.normalized().intersected(img.rect());
    if (r.isEmpty()) return;
    for (int by = r.top(); by <= r.bottom(); by += block) {
        for (int bx = r.left(); bx <= r.right(); bx += block) {
            const int x1 = std::min(bx + block - 1, r.right()), y1 = std::min(by + block - 1, r.bottom());
            quint64 sa = 0, sr = 0, sg = 0, sb = 0, n = 0;
            for (int y = by; y <= y1; ++y) {
                const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                for (int x = bx; x <= x1; ++x) { sa += qAlpha(row[x]); sr += qRed(row[x]); sg += qGreen(row[x]); sb += qBlue(row[x]); ++n; }
            }
            const QRgb c = qRgba(int(sr / n), int(sg / n), int(sb / n), int(sa / n));
            for (int y = by; y <= y1; ++y) {
                QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = bx; x <= x1; ++x) row[x] = c;
            }
        }
    }
}

void blurRegion(QImage& img, const QRect& regionIn, int block) {
    if (img.format() != QImage::Format_ARGB32) img = argb(img);
    block = std::max(block, kMinRedactBlock);
    const QRect r = regionIn.normalized().intersected(img.rect());
    if (r.isEmpty()) return;
    mosaicRegion(img, r, block);               // bước phá thông tin: mọi chi tiết nhỏ hơn khối biến mất
    // Làm mịn biên khối (hộp 3 lần, bán kính ~block/2) chỉ để trông như mờ; thông tin đã mất ở bước trên.
    const int rad = std::max(2, block / 2);
    const int w = r.width(), h = r.height();
    QVector<QRgb> buf(w * h), tmp(w * h);
    for (int y = 0; y < h; ++y) std::copy_n(reinterpret_cast<const QRgb*>(img.constScanLine(r.top() + y)) + r.left(), w, buf.begin() + y * w);
    auto pass = [&](const QVector<QRgb>& src, QVector<QRgb>& dst, bool horiz) {
        const int len = horiz ? w : h, lines = horiz ? h : w;
        for (int l = 0; l < lines; ++l)
            for (int i = 0; i < len; ++i) {
                const int lo = std::max(0, i - rad), hi = std::min(len - 1, i + rad);
                quint32 a = 0, rr = 0, g = 0, b = 0; const int n = hi - lo + 1;
                for (int k = lo; k <= hi; ++k) {
                    const QRgb p = horiz ? src[l * w + k] : src[k * w + l];
                    a += qAlpha(p); rr += qRed(p); g += qGreen(p); b += qBlue(p);
                }
                const QRgb o = qRgba(rr / n, g / n, b / n, a / n);
                if (horiz) dst[l * w + i] = o; else dst[i * w + l] = o;
            }
    };
    for (int it = 0; it < 3; ++it) { pass(buf, tmp, true); pass(tmp, buf, false); }
    for (int y = 0; y < h; ++y) std::copy_n(buf.begin() + y * w, w, reinterpret_cast<QRgb*>(img.scanLine(r.top() + y)) + r.left());
}

Document::Document(const QImage& base) : m_base(argb(base)) {}

bool Document::add(const Op& op) {
    if (m_base.isNull()) return false;
    if (op.type == OpType::Crop || op.type == OpType::Blur || op.type == OpType::Mosaic) {
        const QRect r = op.region.normalized().intersected(m_base.rect());
        if (r.width() < 2 || r.height() < 2) return false;
        if (op.type == OpType::Crop && currentCrop().intersected(r).isEmpty()) return false;
    }
    m_ops.resize(m_cursor);     // xoá nhánh redo
    m_ops.push_back(op);
    ++m_cursor;
    return true;
}
bool Document::undo() { if (!canUndo()) return false; --m_cursor; return true; }
bool Document::redo() { if (!canRedo()) return false; ++m_cursor; return true; }

int Document::nextStepNumber() const {
    int n = 0;
    for (int i = 0; i < m_cursor; ++i) if (m_ops[i].type == OpType::Step) ++n;
    return n + 1;
}

QRect Document::currentCrop() const {
    QRect c = m_base.rect();
    for (int i = 0; i < m_cursor; ++i)
        if (m_ops[i].type == OpType::Crop) c = c.intersected(m_ops[i].region.normalized().intersected(m_base.rect()));
    return c;
}

static void drawArrow(QPainter& p, const Op& o) {
    QPen pen(o.color, o.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.drawLine(o.a, o.b);
    const QLineF l(o.b, o.a);
    if (l.length() < 1.0) return;
    const double head = std::max(14.0, o.width * 4.0), ang = qDegreesToRadians(28.0), base = qDegreesToRadians(l.angle());
    const QPointF p1 = o.b + QPointF(qCos(base + ang) * head, -qSin(base + ang) * head);
    const QPointF p2 = o.b + QPointF(qCos(base - ang) * head, -qSin(base - ang) * head);
    QPainterPath path; path.moveTo(o.b); path.lineTo(p1); path.lineTo(p2); path.closeSubpath();
    p.setBrush(o.color); p.drawPath(path);
}

QImage Document::render() const {
    QImage img = m_base.copy();
    for (int i = 0; i < m_cursor; ++i) {
        const Op& o = m_ops[i];
        if (o.type == OpType::Mosaic) { mosaicRegion(img, o.region, o.width); continue; }
        if (o.type == OpType::Blur)   { blurRegion(img, o.region, o.width);   continue; }
        if (o.type == OpType::Crop)   continue;
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        switch (o.type) {
        case OpType::Arrow: drawArrow(p, o); break;
        case OpType::Rect:
            p.setPen(QPen(o.color, o.width, Qt::SolidLine, Qt::MiterJoin)); p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(o.a, o.b).normalized()); break;
        case OpType::Highlight: {
            QColor c = o.color.isValid() ? o.color : QColor(255, 235, 0);
            p.setCompositionMode(QPainter::CompositionMode_Multiply);   // chữ đen bên dưới vẫn đọc được
            p.setPen(Qt::NoPen); p.setBrush(c);
            p.drawRect(QRectF(o.a, o.b).normalized()); break;
        }
        case OpType::Pen: {
            if (o.path.size() < 2) { if (o.path.size() == 1) { p.setPen(Qt::NoPen); p.setBrush(o.color); p.drawEllipse(o.path[0], o.width / 2.0, o.width / 2.0); } break; }
            p.setPen(QPen(o.color, o.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
            QPainterPath pp; pp.moveTo(o.path[0]); for (int k = 1; k < o.path.size(); ++k) pp.lineTo(o.path[k]);
            p.drawPath(pp); break;
        }
        case OpType::Text: {
            QFont f; f.setPixelSize(std::max(8, o.fontPx)); f.setBold(true);
            p.setFont(f); p.setPen(o.color);
            const QRectF box(o.a, QSizeF(img.width() - o.a.x(), img.height() - o.a.y()));
            p.drawText(box, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, o.text); break;
        }
        case OpType::Step: {
            const double r = std::max(12.0, o.width * 3.0);
            p.setPen(QPen(Qt::white, 2)); p.setBrush(o.color); p.drawEllipse(o.a, r, r);
            QFont f; f.setPixelSize(int(r * 1.15)); f.setBold(true); p.setFont(f); p.setPen(Qt::white);
            p.drawText(QRectF(o.a.x() - r, o.a.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(o.number)); break;
        }
        default: break;
        }
    }
    return img.copy(currentCrop());
}

Measure measure(const QPointF& a, const QPointF& b) {
    return {b.x() - a.x(), b.y() - a.y(), QLineF(a, b).length()};
}

// ---------------- lưu tệp ----------------
QString extensionFor(Format f) { return f == Format::Png ? "png" : f == Format::Jpg ? "jpg" : "pdf"; }

QString defaultSaveDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (base.isEmpty()) base = QDir::homePath();
    return QDir(base).filePath(QStringLiteral("Dung Office/Chụp màn hình"));
}

QString timestampStem(const QDateTime& t) { return QStringLiteral("ChupManHinh_") + t.toString("yyyyMMdd_HHmmss"); }

QString uniquePath(const QString& dir, const QString& stem, const QString& ext) {
    const QDir d(dir);
    QString p = d.filePath(stem + "." + ext);
    for (int i = 2; QFileInfo::exists(p) && i < 100000; ++i) p = d.filePath(QStringLiteral("%1_%2.%3").arg(stem).arg(i).arg(ext));
    return p;
}

static SaveResult writeTo(const QImage& imgIn, const QString& path, Format fmt, int q) {
    SaveResult r; r.path = path;
    if (imgIn.isNull() || imgIn.width() < 1 || imgIn.height() < 1) { r.message = QObject::tr("Ảnh trống, không có gì để lưu."); return r; }
    if (QFileInfo::exists(path)) { r.message = QObject::tr("Tệp đích đã tồn tại, không ghi đè."); return r; }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { r.message = QObject::tr("Không tạo được thư mục đích."); return r; }
    QSaveFile f(path);   // ghi vào tệp tạm, chỉ đổi tên khi xong: lỗi giữa chừng không để lại tệp hỏng
    if (!f.open(QIODevice::WriteOnly)) { r.message = QObject::tr("Không mở được tệp để ghi: ") + f.errorString(); return r; }
    bool ok = false;
    if (fmt == Format::Png) ok = imgIn.save(&f, "PNG");
    else if (fmt == Format::Jpg) {
        QImage flat(imgIn.size(), QImage::Format_RGB32); flat.fill(Qt::white);
        QPainter p(&flat); p.drawImage(0, 0, imgIn); p.end();
        ok = flat.save(&f, "JPG", q);
    } else {
        const int dpi = 150;
        QPdfWriter pdf(&f);
        pdf.setResolution(dpi);
        pdf.setPageSize(QPageSize(QSizeF(imgIn.width() * 25.4 / dpi, imgIn.height() * 25.4 / dpi), QPageSize::Millimeter));
        pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter p(&pdf);
        ok = p.isActive();
        if (ok) { p.drawImage(QRect(0, 0, pdf.width(), pdf.height()), imgIn); p.end(); }
    }
    if (!ok) { f.cancelWriting(); r.message = QObject::tr("Không ghi được ảnh."); return r; }
    if (QFileInfo::exists(path)) { f.cancelWriting(); r.message = QObject::tr("Tệp đích vừa xuất hiện, không ghi đè."); return r; }
    if (!f.commit()) { r.message = QObject::tr("Không hoàn tất được việc ghi tệp: ") + f.errorString(); return r; }
    // đọc lại để chắc chắn
    QFile chk(path);
    if (!chk.open(QIODevice::ReadOnly) || chk.size() <= 0) { r.message = QObject::tr("Đã ghi nhưng đọc lại thất bại."); return r; }
    if (fmt == Format::Pdf) { if (chk.read(4) != "%PDF") { r.message = QObject::tr("Tệp PDF ghi ra không hợp lệ."); return r; } }
    else { chk.close(); QImage back(path); if (back.isNull() || back.size() != imgIn.size()) { r.message = QObject::tr("Đọc lại ảnh không khớp kích thước."); return r; } }
    r.ok = true; r.message = QObject::tr("Đã lưu.");
    return r;
}

SaveResult saveImage(const QImage& img, const QString& dir, Format fmt, const QDateTime& now, int q) {
    return writeTo(img, uniquePath(dir, timestampStem(now), extensionFor(fmt)), fmt, q);
}

SaveResult saveImageAs(const QImage& img, const QString& wantedPath, int q) {
    const QFileInfo fi(wantedPath);
    const QString ext = fi.suffix().toLower();
    Format fmt = ext == "jpg" || ext == "jpeg" ? Format::Jpg : ext == "pdf" ? Format::Pdf : Format::Png;
    QString target = wantedPath;
    bool renamed = false;
    if (fi.exists()) { target = uniquePath(fi.absolutePath(), fi.completeBaseName(), fi.suffix().isEmpty() ? extensionFor(fmt) : fi.suffix()); renamed = true; }
    SaveResult r = writeTo(img, target, fmt, q);
    if (r.ok && renamed) r.message = QObject::tr("Tệp đã tồn tại nên lưu thành tên khác: ") + QFileInfo(target).fileName();
    return r;
}

} // namespace shot
