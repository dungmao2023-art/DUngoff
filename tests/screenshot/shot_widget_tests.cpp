// SPDX-License-Identifier: MPL-2.0
// Kiểm thử khói cho widget chụp màn hình (nền offscreen): dựng được, nạp ảnh, kéo chuột vẽ/che, hoàn tác, chụp toàn màn hình.
#include "ScreenshotWidget.h"
#include <QAbstractButton>
#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QWidget>
#include <cstdio>

static int g_fail = 0, g_pass = 0, g_warn = 0;
static void check(bool ok, const char* name) { if (ok) { ++g_pass; printf("[ĐẠT]  %s\n", name); } else { ++g_fail; printf("[LỖI]  %s\n", name); } }
static int diffCount(const QImage& a, const QImage& b) {
    if (a.size() != b.size()) return -1;
    int n = 0; for (int y = 0; y < a.height(); ++y) for (int x = 0; x < a.width(); ++x) if (a.pixel(x, y) != b.pixel(x, y)) ++n;
    return n;
}
static void mouse(QWidget* w, QEvent::Type t, QPointF p, Qt::MouseButtons btns) {
    QMouseEvent e(t, p, w->mapToGlobal(p.toPoint()), t == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, btns, Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}
static void drag(QWidget* c, QPointF from, QPointF to) {
    mouse(c, QEvent::MouseButtonPress, from, Qt::LeftButton);
    mouse(c, QEvent::MouseMove, (from + to) / 2, Qt::LeftButton);
    mouse(c, QEvent::MouseMove, to, Qt::LeftButton);
    mouse(c, QEvent::MouseButtonRelease, to, Qt::NoButton);
}
static void wait(int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) { QApplication::processEvents(); } }

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    printf("== Kiểm thử khói widget chụp màn hình ==\n");
    QWidget* w = createScreenshotWidget(nullptr);
    w->resize(1000, 760); w->show(); wait(100);
    auto* canvas = w->findChild<QWidget*>("canvas");
    check(canvas != nullptr, "widget dựng được, có khung vẽ");
    for (const char* n : {"capRegion", "capFull", "capWindow", "undoBtn", "redoBtn", "saveBtn", "tool_rect", "tool_mosaic", "tool_blur", "tool_crop"})
        if (!w->findChild<QAbstractButton*>(n)) { printf("[LỖI]  thiếu nút %s\n", n); ++g_fail; }
    check(!w->findChild<QAbstractButton*>("undoBtn")->isEnabled(), "chưa có ảnh: nút Hoàn tác tắt");
    check(!w->findChild<QAbstractButton*>("saveBtn")->isEnabled(), "chưa có ảnh: nút Lưu tắt (không lưu ảnh rỗng)");
    if (!canvas) return 1;

    QImage base(500, 300, QImage::Format_ARGB32); QRandomGenerator g(5);
    for (int y = 0; y < 300; ++y) for (int x = 0; x < 500; ++x) base.setPixel(x, y, qRgb(g.bounded(256), g.bounded(256), g.bounded(256)));
    screenshotWidgetLoadImage(w, base); wait(50);
    canvas->grab();      // ép vẽ để khung vẽ tính xong toạ độ
    check(diffCount(screenshotWidgetRender(w), base) == 0, "nạp ảnh: ảnh ra giống hệt ảnh nạp vào");
    check(w->findChild<QAbstractButton*>("saveBtn")->isEnabled(), "có ảnh: nút Lưu bật");

    const QPointF c(canvas->width() / 2.0, canvas->height() / 2.0);
    w->findChild<QAbstractButton*>("tool_mosaic")->click();
    drag(canvas, c - QPointF(60, 40), c + QPointF(60, 40)); wait(30);
    const QImage after = screenshotWidgetRender(w);
    check(diffCount(after, base) > 500, "kéo chuột với Mosaic: vùng chọn bị che (nhiều điểm ảnh đổi)");
    check(w->findChild<QAbstractButton*>("undoBtn")->isEnabled(), "sau khi che: nút Hoàn tác bật");
    w->findChild<QAbstractButton*>("undoBtn")->click(); wait(30);
    check(diffCount(screenshotWidgetRender(w), base) == 0, "bấm Hoàn tác: ảnh trở lại đúng như ban đầu");
    w->findChild<QAbstractButton*>("redoBtn")->click(); wait(30);
    check(diffCount(screenshotWidgetRender(w), after) == 0, "bấm Làm lại: ảnh che trở lại đúng như trước");

    w->findChild<QAbstractButton*>("tool_crop")->click();
    drag(canvas, c - QPointF(50, 30), c + QPointF(50, 30)); wait(30);
    const QImage cropped = screenshotWidgetRender(w);
    check(cropped.width() < base.width() && cropped.height() < base.height(), "công cụ Cắt: ảnh nhỏ hơn sau khi kéo chọn");

    // chụp toàn màn hình qua nền offscreen
    QWidget* w2 = createScreenshotWidget(nullptr); w2->resize(900, 700); w2->show(); wait(50);
    w2->findChild<QAbstractButton*>("capFull")->click();
    wait(1200);
    const QImage shot = screenshotWidgetRender(w2);
    if (shot.isNull()) { ++g_warn; printf("[CẢNH BÁO] chụp toàn màn hình trên nền offscreen không cho ảnh\n       Nền offscreen có thể không hỗ trợ chụp; chụp thật chỉ kiểm được trên máy có màn hình.\n"); }
    else check(shot.width() > 0 && shot.height() > 0, "chụp toàn màn hình (offscreen): có ảnh kích thước hợp lệ");
    delete w2; delete w;
    printf("\nTổng: %d đạt, %d lỗi, %d cảnh báo\nKẾT QUẢ: %s\n", g_pass, g_fail, g_warn, g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT");
    return g_fail ? 1 : 0;
}
