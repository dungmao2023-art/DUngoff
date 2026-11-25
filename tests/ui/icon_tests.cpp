// SPDX-License-Identifier: MPL-2.0
// Kiểm thử hệ biểu tượng (không cần màn hình thật). Chạy với QT_QPA_PLATFORM=offscreen:
//   dung-icontest [thư-mục-báo-cáo]   -> ghi icons-contact-sheet.png, icons-small-sizes.png
#include "Icons.h"

#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QSet>
#include <cstdio>

using namespace dung::ui;
static int g_fail = 0, g_pass = 0, g_warn = 0;
static void check(bool ok, const QString& name, const QString& extra = QString()) {
    if (ok) { ++g_pass; printf("[ĐẠT]  %s\n", name.toUtf8().constData()); }
    else { ++g_fail; printf("[LỖI]  %s\n", name.toUtf8().constData()); }
    if (!extra.isEmpty()) printf("       %s\n", extra.toUtf8().constData());
}
static void warn(const QString& name, const QString& why) { ++g_warn; printf("[CẢNH BÁO] %s\n       %s\n", name.toUtf8().constData(), why.toUtf8().constData()); }

static const char* kRequired[] = {
    "home","word","excel","ppt","pdf","calendar","save","open","new","info","note","law","template","transfer","search","minimize","close",
    "undo","redo","cut","copy","paste","format-painter","bold","italic","underline","strikethrough","subscript","superscript","font-color",
    "highlight","clear-format","align-left","align-center","align-right","justify","bullets","numbering","indent","outdent","line-spacing",
    "table","image","link","chart","shape","textbox","header","footer","page-number","symbol","equation","page-break","margins","orientation",
    "paper-size","columns","toc","footnote","citation","mail-merge","spell","word-count","comment","track-changes","compare","protect",
    "zoom-in","zoom-out","print","export","tools","screenshot","folder-search","rename","duplicate-files","compare-folders","disk-usage",
    "link-check","organize","path","timer","merge-pdf","split","rotate","delete","move-up","move-down","add","sum","filter","sort","pivot",
    "function","freeze","gridlines","slide-new","slide-duplicate","play","theme","transition","animation","notes","update","settings","star",
    "pin","check","warning","error"};
// Cặp tên cùng hình được khai báo là bí danh (hiện không có).
static const QSet<QPair<QString, QString>> kAliases = {};

static int inkCount(const QImage& im) {
    int n = 0;
    for (int y = 0; y < im.height(); ++y)
        for (int x = 0; x < im.width(); ++x) if (qAlpha(im.pixel(x, y)) > 24) ++n;
    return n;
}
static QRect inkBox(const QImage& im) {
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    for (int y = 0; y < im.height(); ++y)
        for (int x = 0; x < im.width(); ++x)
            if (qAlpha(im.pixel(x, y)) > 24) { x0 = qMin(x0, x); y0 = qMin(y0, y); x1 = qMax(x1, x); y1 = qMax(y1, y); }
    return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
}
static int diffCount(const QImage& a, const QImage& b) {
    int n = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) if (a.pixel(x, y) != b.pixel(x, y)) ++n;
    return n;
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    QDir().mkpath(outDir);
    const QStringList names = iconNames();
    const QColor blue(0x16, 0x47, 0x8F);

    // 1. Đủ danh sách tên bắt buộc, không trùng tên
    QStringList missing;
    QSet<QString> uniq(names.begin(), names.end());
    check(uniq.size() == names.size(), QStringLiteral("tên icon không trùng nhau (%1 icon)").arg(names.size()));
    for (const char* r : kRequired) if (!hasIcon(QString::fromLatin1(r))) missing << QString::fromLatin1(r);
    check(missing.isEmpty(), QStringLiteral("đủ %1 tên bắt buộc").arg(int(sizeof(kRequired) / sizeof(kRequired[0]))), missing.join(QLatin1Char(' ')));
    check(!hasIcon(QStringLiteral("khong-co-icon-nay")), QStringLiteral("tên lạ: hasIcon=false"));
    check(inkCount(renderImage(QStringLiteral("khong-co-icon-nay"), blue, 24)) > 0, QStringLiteral("tên lạ vẫn vẽ khung trống (không im lặng, không sập)"));

    // 2. Mỗi icon: có nét, không bị tô kín, nằm trong khung, ở cỡ 16/24/32
    QStringList bad;
    for (const QString& n : names) {
        for (int px : {16, 24, 32}) {
            const QImage im = renderImage(n, blue, px);
            const int ink = inkCount(im);
            const QRect box = inkBox(im);
            const double cover = double(ink) / (px * px);
            const bool ok = im.size() == QSize(px, px) && ink >= px / 2 && cover < 0.80 && qMax(box.width(), box.height()) >= px * 0.4;
            if (!ok) bad << QStringLiteral("%1@%2(ink=%3,%4%%)").arg(n).arg(px).arg(ink).arg(int(cover * 100));
        }
    }
    check(bad.isEmpty(), QStringLiteral("mọi icon vẽ ra có nét, không kín, đủ lớn ở 16/24/32 px"), bad.join(QLatin1Char(' ')));

    // 3. Các icon khác nhau từng đôi (ở 24 px và 48 px)
    QStringList same, close;
    for (int px : {24, 48}) {
        QList<QImage> imgs; for (const QString& n : names) imgs << renderImage(n, blue, px);
        for (int i = 0; i < names.size(); ++i)
            for (int j = i + 1; j < names.size(); ++j) {
                if (kAliases.contains({names[i], names[j]})) continue;
                const int d = diffCount(imgs[i], imgs[j]);
                if (d == 0) same << QStringLiteral("%1=%2@%3").arg(names[i], names[j]).arg(px);
                else if (px == 48 && d < 48 * 48 * 0.012) close << QStringLiteral("%1~%2(%3px)").arg(names[i], names[j]).arg(d);
            }
    }
    check(same.isEmpty(), QStringLiteral("không có hai icon trùng hệt nhau"), same.join(QLatin1Char(' ')));
    if (!close.isEmpty()) warn(QStringLiteral("icon rất giống nhau (<1.2% điểm ảnh khác)"), close.join(QLatin1Char(' ')));

    // 4. Qua QIcon: Disabled khác Normal (nhạt hơn); màu truyền vào được áp dụng
    QStringList notDim;
    for (const QString& n : names) {
        const QIcon ic = icon(n, blue);
        const QImage a = ic.pixmap(QSize(24, 24), 1.0, QIcon::Normal, QIcon::Off).toImage();
        const QImage b = ic.pixmap(QSize(24, 24), 1.0, QIcon::Disabled, QIcon::Off).toImage();
        if (a.isNull() || b.isNull() || diffCount(a, b) == 0 || inkCount(a) == 0 || inkCount(b) == 0) notDim << n;
    }
    check(notDim.isEmpty(), QStringLiteral("trạng thái Disabled khác Normal ở mọi icon"), notDim.join(QLatin1Char(' ')));
    {
        const QImage red = renderImage(QStringLiteral("check"), Qt::red, 24);
        long r = 0, b = 0;
        for (int y = 0; y < 24; ++y) for (int x = 0; x < 24; ++x) { const QRgb p = red.pixel(x, y); r += qRed(p); b += qBlue(p); }
        check(r > 0 && b == 0, QStringLiteral("màu truyền vào được dùng để tô icon (đỏ không lẫn xanh)"));
        const QImage white = renderImage(QStringLiteral("home"), Qt::white, 24);
        check(inkCount(white) > 20 && qRed(white.pixel(white.rect().center())) >= 0, QStringLiteral("icon trắng (thanh bên) vẽ được"));
    }

    // 5. Màn hình HiDPI: DPR 2 và 3 cho ảnh 2x/3x, vẫn nét (có nhiều điểm ảnh hơn cỡ 1x)
    {
        QStringList badDpr;
        for (const QString& n : names) {
            const QPixmap p1 = pixmap(n, blue, 24, 1.0), p2 = pixmap(n, blue, 24, 2.0), p3 = pixmap(n, blue, 24, 3.0);
            if (p1.size() != QSize(24, 24) || p2.size() != QSize(48, 48) || p3.size() != QSize(72, 72)
                || !qFuzzyCompare(p2.devicePixelRatio(), 2.0) || inkCount(p2.toImage()) <= inkCount(p1.toImage()))
                badDpr << n;
            const QPixmap viaIcon = icon(n, blue).pixmap(QSize(24, 24), 2.0, QIcon::Normal, QIcon::Off);
            if (n == QLatin1String("bold"))
                printf("       thông tin: QIcon::pixmap(24x24, dpr=2) -> %dx%d vật lý, dpr=%.1f, logic %dx%d\n", viaIcon.width(), viaIcon.height(),
                       viaIcon.devicePixelRatio(), viaIcon.deviceIndependentSize().toSize().width(), viaIcon.deviceIndependentSize().toSize().height());
            if (viaIcon.deviceIndependentSize().toSize() != QSize(24, 24) || inkCount(viaIcon.toImage()) == 0) badDpr << n + QStringLiteral("(QIcon)");
        }
        check(badDpr.isEmpty(), QStringLiteral("DPR 2/3: ảnh đúng kích thước vật lý, có nét, qua cả QIcon::pixmap"), badDpr.join(QLatin1Char(' ')));
    }

    // 6. Vẽ trực tiếp vào widget/painter (đường paint của QIcon)
    {
        QImage canvas(64, 64, QImage::Format_ARGB32_Premultiplied); canvas.fill(Qt::white);
        QPainter p(&canvas);
        icon(QStringLiteral("bold"), blue).paint(&p, QRect(8, 8, 48, 48));
        p.end();
        int dark = 0; for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) if (qGray(canvas.pixel(x, y)) < 128) ++dark;
        check(dark > 40, QStringLiteral("QIcon::paint vẽ ra nét"));
        check(icon(QStringLiteral("bold")).pixmap(QSize(24, 24)).size() == QSize(24, 24), QStringLiteral("icon không truyền màu dùng màu mặc định"));
        check(iconOrEmpty(QStringLiteral("timer")).isNull() == false && iconOrEmpty(QStringLiteral("⏱")).isNull(), QStringLiteral("iconOrEmpty: tên icon -> icon, chữ khác -> rỗng"));
    }

    // 7. Ánh xạ mã lệnh cơ bản
    check(iconNameForCommand(QStringLiteral(".uno:Bold")) == QLatin1String("bold") && iconNameForCommand(QStringLiteral("khong-co")).isEmpty(),
          QStringLiteral("iconNameForCommand: có/không có ánh xạ"));

    // 8. Tờ tổng hợp để mắt người soát lỗi hình
    {
        const int cols = 12, cell = 104, icon_px = 56, pad = 8;
        const int rows = (names.size() + cols - 1) / cols;
        QImage sheet(cols * cell + pad * 2, rows * cell + pad * 2, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::white);
        QPainter p(&sheet);
        QFont f(QStringLiteral("DejaVu Sans")); f.setPixelSize(10); p.setFont(f);
        for (int i = 0; i < names.size(); ++i) {
            const int cx = pad + (i % cols) * cell, cy = pad + (i / cols) * cell;
            p.setPen(QColor(0xE2E8F0)); p.drawRect(cx, cy, cell - 2, cell - 2);
            p.drawImage(cx + (cell - icon_px) / 2, cy + 8, renderImage(names[i], blue, icon_px));
            p.setPen(QColor(0x475569));
            p.drawText(QRect(cx, cy + icon_px + 12, cell - 2, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, names[i]);
        }
        p.end();
        const QString path = QDir(outDir).filePath(QStringLiteral("icons-contact-sheet.png"));
        check(sheet.save(path), QStringLiteral("ghi tờ tổng hợp: ") + path);
        // Cỡ thật 16/24/32 phóng 3 lần (điểm ảnh gần nhất) để soát độ đọc được ở cỡ nhỏ
        const int z = 2, scell = 72 * z + 8;
        const int c2 = 9, r2 = (names.size() + c2 - 1) / c2;
        QImage small(c2 * scell, r2 * scell, QImage::Format_ARGB32_Premultiplied);
        small.fill(Qt::white);
        QPainter q(&small);
        for (int i = 0; i < names.size(); ++i) {
            const int cx = (i % c2) * scell, cy = (i / c2) * scell;
            int x = cx + 2;
            for (int px : {16, 24, 32}) {
                q.drawImage(QRect(x, cy + 2, px * z, px * z), renderImage(names[i], blue, px));
                x += px * z + 2;
            }
        }
        q.end();
        const QString p2 = QDir(outDir).filePath(QStringLiteral("icons-small-sizes.png"));
        check(small.save(p2), QStringLiteral("ghi tờ cỡ nhỏ: ") + p2);
    }

    printf("\nTổng: %d đạt, %d lỗi, %d cảnh báo\n", g_pass, g_fail, g_warn);
    return g_fail ? 1 : 0;
}
