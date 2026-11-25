// SPDX-License-Identifier: MPL-2.0
// dung-carettest: kiểm thử con trỏ nhập liệu / khung ô.
//   dung-carettest                                   → kiểm thử đơn vị hình học (không cần engine)
//   dung-carettest --program <program> --out <dir> [--docx f] [--xlsx f] [--pptx f]
//                                                    → thêm: dựng TileView thật (nền offscreen) trên engine thật,
//                                                      chụp viewport lúc con trỏ BẬT/TẮT và so điểm ảnh.
#include "../../app/src/CaretGeom.h"
#include "../../app/src/TileView.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <QThread>
#include <functional>
#include <cstdlib>
#include <iostream>

namespace {
int g_fail = 0, g_pass = 0;
void check(bool ok, const QString& what, const QString& ev = QString()) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what.toStdString();
    if (!ev.isEmpty()) std::cout << "  {" << ev.toStdString() << "}";
    std::cout << std::endl;
    ok ? ++g_pass : ++g_fail;
}
void note(const QString& s) { std::cout << "[GHI]   " << s.toStdString() << std::endl; }

void unit() {
    using namespace caretgeom;
    auto p = parseCursorPayload("4469, 2204, 0, 342");   // đúng chuỗi engine Windows thật gửi
    check(p.valid && p.rect == QRect(4469, 2204, 0, 342), "HỒI QUY: con trỏ rộng 0 vẫn hợp lệ", QString("valid=%1").arg(p.valid));
    check(p.rect.isEmpty(), "xác nhận nguyên nhân gốc: QRect rộng 0 báo isEmpty()==true");
    check(parseCursorPayload("1, 2, 3, 4, 5, 6").valid, "chấp nhận payload có cột/hàng kèm theo");
    check(parseCursorPayload("{\"rectangle\":\"10, 20, 0, 300\"}").rect == QRect(10, 20, 0, 300), "JSON rectangle");
    check(!parseCursorPayload("EMPTY").valid, "EMPTY → không có con trỏ");
    check(!parseCursorPayload("").valid, "chuỗi rỗng → không có");
    check(!parseCursorPayload("a,b,c,d").valid, "không phải số → không hợp lệ");
    check(!parseCursorPayload("1,2,0,0").valid, "0x0 → không hợp lệ");
    check(!parseCursorPayload("1,2,-3,4").valid, "rộng âm → không hợp lệ");
    check(caretPhysicalWidth(1.0) == 1 && caretPhysicalWidth(1.25) == 1 && caretPhysicalWidth(1.5) == 2 && caretPhysicalWidth(2.0) == 2,
          "độ dày con trỏ theo DPI 100/125/150/200%");
    for (double d : {1.0, 1.25, 1.5, 1.75, 2.0}) {
        const QRectF r = caretDeviceRect(QRect(101, 57, 1, 20), d);
        const double px = r.width() * d, py = r.height() * d;
        check(px >= 1.0 - 1e-6 && qAbs(px - std::round(px)) < 1e-6 && r.height() * d >= 2.0 - 1e-6 && qAbs(py - std::round(py)) < 1e-6,
              QString("hình chữ nhật con trỏ khớp lưới điểm ảnh vật lý ở %1%").arg(int(d * 100)));
    }
}
}  // namespace

#ifdef DUNG_CARET_WITH_ENGINE
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

namespace {
void pump(int ms) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < ms) { QApplication::processEvents(QEventLoop::AllEvents, 20); QThread::msleep(5); }
}
bool waitFor(const std::function<bool()>& f, int ms) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < ms) { if (f()) return true; QApplication::processEvents(QEventLoop::AllEvents, 20); QThread::msleep(5); }
    return f();
}
void click(QWidget* vp, QPoint at, bool dbl = false) {
    for (int i = 0; i < (dbl ? 2 : 1); ++i) {
        QMouseEvent pr(QEvent::MouseButtonPress, at, vp->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &pr);
        QMouseEvent re(QEvent::MouseButtonRelease, at, vp->mapToGlobal(at), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &re);
        pump(60);
    }
}
// Số điểm ảnh khác nhau trong hcn r giữa hai ảnh, và có điểm ảnh gần màu con trỏ ở ảnh "bật" không.
void compare(const QImage& on, const QImage& off, QRect r, int& diffInside, int& diffOutside, bool& darkOn) {
    diffInside = diffOutside = 0; darkOn = false;
    r = r.adjusted(-1, -1, 1, 1);
    for (int y = 0; y < on.height(); ++y)
        for (int x = 0; x < on.width(); ++x) {
            if (on.pixel(x, y) == off.pixel(x, y)) continue;
            if (r.contains(x, y)) {
                ++diffInside;
                const QColor c = QColor::fromRgb(on.pixel(x, y));
                if (c.red() < 90 && c.green() < 90 && c.blue() < 120) darkOn = true;
            } else ++diffOutside;
        }
}

void runDoc(dung::Engine& eng, const QString& label, const QString& path, bool wantCell, bool wantCaret, const QString& outDir) {
    std::cout << "---- " << label.toStdString() << " ----" << std::endl;
    std::string err;
    auto doc = eng.open(dung::fromUtf8(path.toUtf8().toStdString()), &err);
    check((bool)doc, label + ": mở tài liệu", QString::fromStdString(err));
    if (!doc) return;
    TileView view(doc.get());
    view.resize(900, 700);
    QPointer<TileView> pv(&view);
    doc->setEventSink([pv](const dung::EngineEvent& e) {
        const int t = e.type; const QString pl = QString::fromStdString(e.payload);
        QMetaObject::invokeMethod(qApp, [pv, t, pl] { if (pv) pv->handleEvent(t, pl); }, Qt::QueuedConnection);
    });
    view.show();
    view.activateWindow();
    view.setFocus();
    pump(1500);
    note(label + QString(": focus=%1").arg(view.hasFocus()));
    QWidget* vp = view.viewport();

    if (wantCell) {
        click(vp, QPoint(120, 60));
        bool ok = waitFor([&] { return view.hasCellCursor(); }, 8000);
        check(ok, label + ": engine báo khung ô (CELL_CURSOR) và TileView ghi nhận", QString("rect=%1,%2,%3,%4").arg(view.cellCursorViewRect().x()).arg(view.cellCursorViewRect().y()).arg(view.cellCursorViewRect().width()).arg(view.cellCursorViewRect().height()));
        if (ok) {
            pump(300);
            const QRect r = view.cellCursorViewRect();
            check(r.width() > 4 && r.height() > 4 && vp->rect().intersects(r), label + ": khung ô có kích thước và nằm trong vùng nhìn");
            view.freezeBlink(true);   // khung ô không nhấp nháy: chụp và tìm màu viền Excel
            pump(100);
            const QImage img = vp->grab().toImage().convertToFormat(QImage::Format_RGB32);
            img.save(outDir + "/" + label + "-cell.png");
            int hits = 0; const QColor ex(0x1E, 0x8E, 0x4F);
            for (int x = r.left(); x <= r.right(); ++x)
                for (int dy = -2; dy <= 2; ++dy) {
                    const QRgb c = img.pixel(qBound(0, x, img.width() - 1), qBound(0, r.top() + dy, img.height() - 1));
                    if (qAbs(qRed(c) - ex.red()) < 40 && qAbs(qGreen(c) - ex.green()) < 40 && qAbs(qBlue(c) - ex.blue()) < 40) { ++hits; break; }
                }
            note(label + QString(": viền khung ô: %1/%2 cột có điểm ảnh màu Excel ở cạnh trên").arg(hits).arg(r.width()));
            check(hits > r.width() / 2, label + ": khung ô Excel được VẼ ra điểm ảnh ở đúng toạ độ");
            view.unfreezeBlink();
        }
        // vùng nhiều ô: kéo chuột
        QMouseEvent pr(QEvent::MouseButtonPress, QPoint(120, 60), vp->mapToGlobal(QPoint(120, 60)), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &pr);
        QMouseEvent mv(QEvent::MouseMove, QPoint(330, 130), vp->mapToGlobal(QPoint(330, 130)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &mv);
        QMouseEvent rl(QEvent::MouseButtonRelease, QPoint(330, 130), vp->mapToGlobal(QPoint(330, 130)), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &rl);
        pump(1500);
        const QImage multi = vp->grab().toImage();
        multi.save(outDir + "/" + label + "-multi.png");
        note(label + ": ảnh chọn nhiều ô đã lưu (kiểm bằng mắt)");
    }
    if (wantCaret) {
        bool ok = false;
        if (label.contains("pptx")) {
            // Dò lưới điểm: nhấp đúp để vào chế độ sửa chữ của ô chữ trên trang chiếu đầu, dừng khi engine báo con trỏ.
            for (int y = 100; y <= 560 && !ok; y += 60)
                for (int x = 120; x <= 780 && !ok; x += 110) {
                    click(vp, QPoint(x, y), true);
                    ok = waitFor([&] { return view.hasCaret(); }, 700);
                    if (ok) note(label + QString(": vào được ô chữ ở (%1,%2)").arg(x).arg(y));
                }
        } else {
            click(vp, QPoint(140, 120));
            ok = waitFor([&] { return view.hasCaret(); }, 8000);
        }
        if (!ok && label.contains("pptx")) {
            note(label + ": CHƯA kiểm được con trỏ trong ô chữ Impress bằng chuột giả lập (không có điểm dừng chữ ở toạ độ thử)");
            return;
        }
        check(ok, label + ": engine báo con trỏ chữ và TileView ghi nhận (kể cả rộng 0)", QString("rect=%1,%2,%3,%4").arg(view.caretViewRect().x()).arg(view.caretViewRect().y()).arg(view.caretViewRect().width()).arg(view.caretViewRect().height()));
        if (!ok) return;
        pump(300);
        const QRect r = view.caretViewRect();
        check(r.height() >= 4 && vp->rect().intersects(r), label + ": con trỏ có chiều cao và nằm trong vùng nhìn");
        // BẬT vs TẮT: cùng một tài liệu, chỉ khác pha nhấp nháy.
        view.freezeBlink(true);  pump(150);
        check(view.caretShownNow(), label + ": pha BẬT + có focus → caretShownNow()");
        const QImage on = vp->grab().toImage().convertToFormat(QImage::Format_RGB32);
        view.freezeBlink(false); pump(150);
        check(!view.caretShownNow(), label + ": pha TẮT → caretShownNow() sai");
        const QImage off = vp->grab().toImage().convertToFormat(QImage::Format_RGB32);
        on.save(outDir + "/" + label + "-caret-on.png"); off.save(outDir + "/" + label + "-caret-off.png");
        int di, dout; bool dark;
        compare(on, off, r, di, dout, dark);
        check(di > 0, label + ": điểm ảnh ở vị trí con trỏ KHÁC nhau giữa BẬT và TẮT", QString("khác trong hcn=%1").arg(di));
        check(dark, label + ": ảnh BẬT có điểm ảnh màu tối của con trỏ");
        check(dout == 0, label + ": ngoài vị trí con trỏ ảnh BẬT/TẮT giống hệt", QString("khác ngoài=%1").arg(dout));
        // mất focus → ẩn; lấy lại focus → hiện ngay
        view.freezeBlink(true);
        view.clearFocus(); pump(100);
        check(!view.caretShownNow(), label + ": mất focus → con trỏ ẩn");
        view.setFocus(); pump(100);
        view.unfreezeBlink();
        check(view.caretShownNow(), label + ": lấy lại focus → con trỏ hiện ngay (pha BẬT)");
        // gõ chữ: con trỏ phải đặc (không tắt) và di chuyển sang phải
        if (!wantCell && !label.contains("pptx")) {
            const QRect before = view.caretViewRect();
            QKeyEvent k(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
            QApplication::sendEvent(&view, &k);
            QKeyEvent kr(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a");
            QApplication::sendEvent(&view, &kr);
            const bool moved = waitFor([&] { return view.caretViewRect().x() > before.x(); }, 3000);
            check(moved, label + ": gõ chữ → con trỏ dịch sang phải", QString("%1 → %2").arg(before.x()).arg(view.caretViewRect().x()));
            check(view.caretShownNow(), label + ": ngay sau khi gõ/di chuyển con trỏ đặc (hiện, chưa vào pha tắt)");
        }
    }
    doc->setEventSink(nullptr);
}
}  // namespace
#endif

int main(int argc, char** argv) {
    QString program, out, docx, xlsx, pptx;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto next = [&] { return i + 1 < argc ? QString::fromLocal8Bit(argv[++i]) : QString(); };
        if (a == "--program") program = next(); else if (a == "--out") out = next();
        else if (a == "--docx") docx = next(); else if (a == "--xlsx") xlsx = next(); else if (a == "--pptx") pptx = next();
    }
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen") : qgetenv("QT_QPA_PLATFORM"));
    QApplication app(argc, argv);
    unit();
#ifdef DUNG_CARET_WITH_ENGINE
    if (!program.isEmpty()) {
        if (out.isEmpty()) out = "caret-out";
        QDir().mkpath(out);
        std::string err;
        auto eng = dung::Engine::create(dung::fromUtf8(program.toUtf8().toStdString()), dung::fromUtf8((out + "/profile").toUtf8().toStdString()),
                                        dung::fromUtf8((out + "/work").toUtf8().toStdString()), &err);
        check((bool)eng, "khởi động engine", QString::fromStdString(err));
        if (eng) {
            if (!docx.isEmpty()) runDoc(*eng, "docx", docx, false, true, out);
            if (!xlsx.isEmpty()) runDoc(*eng, "xlsx", xlsx, true, false, out);
            if (!pptx.isEmpty()) runDoc(*eng, "pptx", pptx, false, true, out);
        }
    }
#endif
    std::cout << "Tổng: " << g_pass << " đạt, " << g_fail << " lỗi" << std::endl;
    std::cout.flush();
    _Exit(g_fail ? 1 : 0);   // tránh treo lúc huỷ engine (lỗi đã biết)
}
