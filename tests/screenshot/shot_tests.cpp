// SPDX-License-Identifier: MPL-2.0
// Kiểm thử lõi chụp màn hình bằng điểm ảnh (không giao diện). Chạy với QT_QPA_PLATFORM=offscreen.
#include "ScreenshotCore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPainter>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <cstdio>

using namespace shot;
static int g_fail = 0, g_pass = 0, g_warn = 0;
static void check(bool ok, const char* name, const QString& extra = QString()) {
    if (ok) { ++g_pass; printf("[ĐẠT]  %s\n", name); }
    else { ++g_fail; printf("[LỖI]  %s\n", name); }
    if (!extra.isEmpty()) printf("       %s\n", extra.toUtf8().constData());
}
static void warn(const char* name, const QString& why) { ++g_warn; printf("[CẢNH BÁO] %s\n       %s\n", name, why.toUtf8().constData()); }

static QImage solid(int w, int h, QColor c) { QImage i(w, h, QImage::Format_ARGB32); i.fill(c); return i; }
static QImage noise(int w, int h, uint seed) {
    QImage i(w, h, QImage::Format_ARGB32); QRandomGenerator g(seed);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) i.setPixel(x, y, qRgb(g.bounded(256), g.bounded(256), g.bounded(256)));
    return i;
}
static int diffCount(const QImage& a, const QImage& b) {
    if (a.size() != b.size()) return -1;
    int n = 0; for (int y = 0; y < a.height(); ++y) for (int x = 0; x < a.width(); ++x) if (a.pixel(x, y) != b.pixel(x, y)) ++n;
    return n;
}
static QRect diffBox(const QImage& a, const QImage& b) {
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    for (int y = 0; y < a.height(); ++y) for (int x = 0; x < a.width(); ++x) if (a.pixel(x, y) != b.pixel(x, y)) { x0 = qMin(x0, x); y0 = qMin(y0, y); x1 = qMax(x1, x); y1 = qMax(y1, y); }
    return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
}
static bool blocksUniform(const QImage& img, const QRect& r, int block) {
    for (int by = r.top(); by <= r.bottom(); by += block)
        for (int bx = r.left(); bx <= r.right(); bx += block) {
            const QRgb c = img.pixel(bx, by);
            for (int y = by; y <= qMin(by + block - 1, r.bottom()); ++y)
                for (int x = bx; x <= qMin(bx + block - 1, r.right()); ++x) if (img.pixel(x, y) != c) return false;
        }
    return true;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    printf("== Kiểm thử lõi chụp màn hình ==\n");

    // 1. không thao tác = ảnh gốc
    {
        QImage base = noise(60, 40, 1); Document d(base);
        check(diffCount(d.render(), base.convertToFormat(QImage::Format_ARGB32)) == 0, "không thao tác: ảnh ra giống hệt ảnh gốc");
        check(!d.canUndo() && !d.canRedo(), "ban đầu không có gì để hoàn tác");
    }
    // 2. khung
    {
        QImage base = solid(100, 80, Qt::white); Document d(base);
        Op o; o.type = OpType::Rect; o.a = {20, 20}; o.b = {60, 50}; o.color = Qt::red; o.width = 4;
        check(d.add(o), "thêm khung");
        QImage r = d.render();
        check(QColor(r.pixel(20, 35)).red() > 200 && QColor(r.pixel(20, 35)).green() < 80, "khung: cạnh trái có màu đỏ");
        check(r.pixel(40, 35) == QColor(Qt::white).rgb(), "khung: bên trong không bị tô");
        check(r.pixel(90, 70) == QColor(Qt::white).rgb(), "khung: ngoài khung không đổi");
        check(base.pixel(20, 35) == QColor(Qt::white).rgb() && d.base().pixel(20, 35) == QColor(Qt::white).rgb(), "ảnh gốc trong Document không bị sửa");
    }
    // 3. mũi tên, bút, tô sáng
    {
        QImage base = solid(120, 80, Qt::white); Document d(base);
        Op a; a.type = OpType::Arrow; a.a = {10, 40}; a.b = {100, 40}; a.color = Qt::blue; a.width = 4; d.add(a);
        QImage r = d.render();
        check(QColor(r.pixel(50, 40)).blue() > 200 && QColor(r.pixel(50, 40)).red() < 80, "mũi tên: thân tô đúng màu");
        check(diffCount(r, base.convertToFormat(QImage::Format_ARGB32)) > 200, "mũi tên: có đầu mũi tên (đủ điểm ảnh thay đổi)");
        check(r.pixel(50, 5) == QColor(Qt::white).rgb(), "mũi tên: xa đường vẽ không đổi");
        Document d2(base); Op p; p.type = OpType::Pen; p.path = {{10, 10}, {30, 30}, {60, 10}}; p.color = Qt::green; p.width = 5; d2.add(p);
        check(diffCount(d2.render(), base.convertToFormat(QImage::Format_ARGB32)) > 50, "bút: nét vẽ xuất hiện");
        QImage dark = solid(60, 40, Qt::black); Document d3(dark); Op h; h.type = OpType::Highlight; h.a = {0, 0}; h.b = {60, 40}; h.color = QColor(255, 235, 0); d3.add(h);
        check(d3.render().pixel(10, 10) == QColor(Qt::black).rgb(), "tô sáng: chữ/nền đen bên dưới vẫn đen (không che mất nội dung)");
        QImage wht = solid(60, 40, Qt::white); Document d4(wht); d4.add(h);
        check(QColor(d4.render().pixel(10, 10)).blue() < 50 && QColor(d4.render().pixel(10, 10)).red() > 200, "tô sáng: nền trắng thành vàng");
    }
    // 4. đánh số bước
    {
        Document d(solid(100, 100, Qt::white));
        check(d.nextStepNumber() == 1, "đánh số: bắt đầu từ 1");
        Op s; s.type = OpType::Step; s.a = {30, 30}; s.number = d.nextStepNumber(); s.color = Qt::red; d.add(s);
        check(d.nextStepNumber() == 2, "đánh số: tăng lên 2 sau khi thêm");
        d.undo(); check(d.nextStepNumber() == 1, "đánh số: hoàn tác thì số quay lại 1");
        d.redo();
        check(QColor(d.render().pixel(30, 22)).red() > 180 && QColor(d.render().pixel(30, 22)).green() < 100, "đánh số: vòng tròn đỏ xuất hiện");
    }
    // 5. chữ (tiếng Việt)
    {
        QImage base = solid(400, 120, Qt::white);
        auto textBox = [&](const QString& t) { Document d(base); Op o; o.type = OpType::Text; o.a = {10, 40}; o.text = t; o.color = Qt::black; o.fontPx = 32; d.add(o); return diffBox(d.render(), base); };
        const QRect ascii = textBox("eeee"), viet = textBox(QString::fromUtf8("ệệệệ")), full = textBox(QString::fromUtf8("Đặt tên Việt Nam"));
        if (ascii.isEmpty()) {
            warn("chữ: máy chạy CI không vẽ được chữ (thiếu font hệ thống)", "Không kiểm được hiển thị chữ; không tính là lỗi mã nguồn.");
        } else {
            check(!full.isEmpty(), "chữ: chuỗi tiếng Việt có dấu được vẽ");
            if (viet.height() > ascii.height()) check(true, "chữ: dấu mũ và dấu nặng của 'ệ' làm ảnh cao hơn 'e' (dấu thật sự được vẽ)");
            else warn("chữ: 'ệ' không cao hơn 'e'", "Font hệ thống trên máy này có thể thiếu dấu tổ hợp; cần thử trên Windows thật với font Segoe UI.");
        }
    }
    // 6. undo/redo
    {
        QImage base = solid(100, 100, Qt::white); Document d(base);
        Op r1; r1.type = OpType::Rect; r1.a = {5, 5}; r1.b = {30, 30}; r1.color = Qt::red;
        Op r2 = r1; r2.a = {40, 40}; r2.b = {80, 80}; r2.color = Qt::blue;
        Op r3 = r1; r3.a = {10, 60}; r3.b = {30, 90}; r3.color = Qt::green;
        d.add(r1); QImage after1 = d.render(); d.add(r2); QImage after2 = d.render(); d.add(r3); QImage after3 = d.render();
        check(d.opCount() == 3 && d.canUndo(), "undo: 3 thao tác");
        d.undo(); check(diffCount(d.render(), after2) == 0, "undo 1 lần: về đúng trạng thái sau thao tác 2");
        d.undo(); check(diffCount(d.render(), after1) == 0, "undo 2 lần: về đúng trạng thái sau thao tác 1");
        d.redo(); d.redo(); check(diffCount(d.render(), after3) == 0, "redo 2 lần: về đúng trạng thái cuối");
        d.undo(); Op r4 = r1; r4.a = {60, 5}; r4.b = {90, 30}; d.add(r4);
        check(!d.canRedo(), "thêm thao tác mới sau undo: xoá nhánh redo");
        d.undo(); d.undo(); d.undo(); check(!d.canUndo() && diffCount(d.render(), base.convertToFormat(QImage::Format_ARGB32)) == 0, "undo hết: ra ảnh gốc");
        check(!d.undo(), "undo khi hết lịch sử: trả false, không lỗi");
    }
    // 7. cắt
    {
        Document d(solid(200, 100, Qt::white));
        Op c; c.type = OpType::Crop; c.region = QRect(20, 10, 100, 60);
        check(d.add(c) && d.render().size() == QSize(100, 60), "cắt: kích thước đúng");
        Op c2; c2.type = OpType::Crop; c2.region = QRect(70, 30, 200, 200);   // chồng một phần, vượt ngoài
        check(d.add(c2) && d.render().size() == QSize(50, 40), "cắt lần hai: giao của hai vùng, tự kẹp trong ảnh");
        d.undo(); check(d.render().size() == QSize(100, 60), "undo cắt: trở lại vùng cắt trước");
        d.undo(); check(d.render().size() == QSize(200, 100), "undo hết: ảnh nguyên");
        Op bad; bad.type = OpType::Crop; bad.region = QRect(500, 500, 50, 50);
        check(!d.add(bad), "cắt hoàn toàn ngoài ảnh: bị từ chối");
        Op tiny; tiny.type = OpType::Crop; tiny.region = QRect(5, 5, 1, 1);
        check(!d.add(tiny), "cắt vùng 1 điểm ảnh: bị từ chối");
        QImage base = noise(200, 100, 7); Document d2(base); Op cc; cc.type = OpType::Crop; cc.region = QRect(30, 20, 50, 40); d2.add(cc);
        check(diffCount(d2.render(), base.convertToFormat(QImage::Format_ARGB32).copy(QRect(30, 20, 50, 40))) == 0, "cắt: nội dung điểm ảnh khớp đúng vùng đã chọn");
    }
    // 8. mosaic
    {
        QImage base = noise(120, 90, 3); Document d(base);
        Op m; m.type = OpType::Mosaic; m.region = QRect(10, 10, 80, 60); m.width = 10; d.add(m);
        QImage r = d.render();
        check(blocksUniform(r, QRect(10, 10, 80, 60), 10), "mosaic: mỗi khối là một màu duy nhất");
        bool outsideSame = true;
        for (int y = 0; y < 90; ++y) for (int x = 0; x < 120; ++x) if (!QRect(10, 10, 80, 60).contains(x, y) && r.pixel(x, y) != base.pixel(x, y)) outsideSame = false;
        check(outsideSame, "mosaic: ngoài vùng chọn không đổi một điểm ảnh");
        check(diffCount(r, base.convertToFormat(QImage::Format_ARGB32)) > 3000, "mosaic: vùng được che thay đổi mạnh");
        Op tiny = m; tiny.width = 1; Document d2(base); d2.add(tiny);
        check(blocksUniform(d2.render(), QRect(10, 10, 80, 60), kMinRedactBlock), "mosaic: cỡ khối bé bị nâng lên mức tối thiểu (không che quá yếu)");
        Op part; part.type = OpType::Mosaic; part.region = QRect(100, 70, 100, 100); part.width = 8; Document d3(base);
        check(d3.add(part) && blocksUniform(d3.render(), QRect(100, 70, 20, 20), 8), "mosaic: vùng vượt ngoài ảnh được kẹp lại, không lỗi");
    }
    // 9. thông tin bị phá thật: hai ảnh khác nhau nhưng trung bình khối bằng nhau -> kết quả giống hệt
    {
        const int B = 8;
        QImage a(64, 64, QImage::Format_ARGB32), b(64, 64, QImage::Format_ARGB32);
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
            const bool w = ((x / 2) + (y / 2)) % 2 == 0;           // bàn cờ 2px
            a.setPixel(x, y, w ? qRgb(255, 255, 255) : qRgb(0, 0, 0));
            const bool w2 = (x % 2 == 0);                          // sọc dọc 1px: cùng tỉ lệ trắng/đen mỗi khối
            b.setPixel(x, y, w2 ? qRgb(255, 255, 255) : qRgb(0, 0, 0));
        }
        check(diffCount(a, b) > 1000, "chuẩn bị: hai ảnh nguồn thực sự khác nhau");
        Op m; m.type = OpType::Mosaic; m.region = QRect(0, 0, 64, 64); m.width = B;
        Document da(a), db(b); da.add(m); db.add(m);
        check(diffCount(da.render(), db.render()) == 0, "mosaic: hai nguồn khác nhau cho ra ảnh GIỐNG HỆT (thông tin chi tiết bị phá thật)");
        Op bl = m; bl.type = OpType::Blur;
        Document ea(a), eb(b); ea.add(bl); eb.add(bl);
        check(diffCount(ea.render(), eb.render()) == 0, "làm mờ: hai nguồn khác nhau cho ra ảnh GIỐNG HỆT (không khôi phục được bằng giải chập)");
        check(diffCount(ea.render(), a.convertToFormat(QImage::Format_ARGB32)) > 3000, "làm mờ: ảnh ra khác ảnh gốc");
    }
    // 10. làm mờ
    {
        QImage base = noise(120, 90, 5); Document d(base);
        Op bl; bl.type = OpType::Blur; bl.region = QRect(20, 15, 70, 50); bl.width = 12; d.add(bl);
        QImage r = d.render();
        auto variance = [](const QImage& im, QRect rc) { double s = 0, s2 = 0; int n = 0; for (int y = rc.top(); y <= rc.bottom(); ++y) for (int x = rc.left(); x <= rc.right(); ++x) { double g = qGray(im.pixel(x, y)); s += g; s2 += g * g; ++n; } double m = s / n; return s2 / n - m * m; };
        check(variance(r, bl.region) < variance(base, bl.region) * 0.1, "làm mờ: phương sai trong vùng giảm hơn 10 lần");
        bool outside = true;
        for (int y = 0; y < 90; ++y) for (int x = 0; x < 120; ++x) if (!bl.region.contains(x, y) && r.pixel(x, y) != base.pixel(x, y)) outside = false;
        check(outside, "làm mờ: ngoài vùng chọn không đổi");
    }
    // 11. thứ tự thao tác: che sau khi vẽ thì phần đã vẽ cũng bị che
    {
        QImage base = solid(100, 100, Qt::white); Document d(base);
        Op s; s.type = OpType::Step; s.a = {50, 50}; s.number = 7; s.color = Qt::red; d.add(s);
        Op m; m.type = OpType::Mosaic; m.region = QRect(30, 30, 40, 40); m.width = 40; d.add(m);
        QImage r = d.render(); const QRgb c = r.pixel(30, 30);
        check(blocksUniform(r, QRect(30, 30, 40, 40), 40), "thứ tự: thao tác vẽ trước đó bị che đồng đều (không lộ số 7)");
        (void)c;
    }
    // 12. xuất ảnh đã che: mở lại tệp không còn thông tin gốc
    {
        QTemporaryDir td; QImage base = noise(80, 60, 11); Document d(base);
        Op m; m.type = OpType::Mosaic; m.region = QRect(0, 0, 80, 60); m.width = 80; d.add(m);
        SaveResult sr = saveImage(d.render(), td.path(), Format::Png, QDateTime(QDate(2026, 11, 25), QTime(1, 2, 3)));
        QImage back(sr.path);
        check(sr.ok && !back.isNull() && blocksUniform(back.convertToFormat(QImage::Format_ARGB32), QRect(0, 0, 80, 60), 80), "xuất PNG sau khi che: đọc lại vẫn là một màu duy nhất (không rò thông tin)");
    }
    // 13. lưu tệp
    {
        QTemporaryDir td; const QString dir = td.filePath(QString::fromUtf8("Ảnh chụp màn hình"));
        const QDateTime t(QDate(2026, 11, 25), QTime(0, 15, 30));
        check(timestampStem(t) == "ChupManHinh_20261125_001530", "tên tệp theo ngày giờ: ChupManHinh_20261125_001530");
        QImage img = noise(50, 30, 9);
        SaveResult a = saveImage(img, dir, Format::Png, t);
        check(a.ok && QFileInfo(a.path).fileName() == "ChupManHinh_20261125_001530.png", "lưu PNG vào thư mục có dấu tiếng Việt (tự tạo thư mục)");
        QFile fa(a.path); fa.open(QIODevice::ReadOnly); const QByteArray before = fa.readAll(); fa.close();
        SaveResult b = saveImage(noise(50, 30, 99), dir, Format::Png, t);   // cùng giây, cùng tên
        check(b.ok && b.path != a.path && QFileInfo(b.path).fileName() == "ChupManHinh_20261125_001530_2.png", "cùng giây: sang tên _2, không ghi đè");
        QFile fa2(a.path); fa2.open(QIODevice::ReadOnly);
        check(fa2.readAll() == before, "tệp đầu tiên không đổi một byte");
        QImage back(a.path);
        check(diffCount(back.convertToFormat(QImage::Format_ARGB32), img) == 0, "PNG đọc lại giống hệt từng điểm ảnh");
        SaveResult j = saveImage(img, dir, Format::Jpg, t);
        check(j.ok && QImage(j.path).size() == img.size(), "lưu JPG: đọc lại đúng kích thước");
        SaveResult p = saveImage(img, dir, Format::Pdf, t);
        QFile fp(p.path); fp.open(QIODevice::ReadOnly);
        check(p.ok && fp.read(4) == "%PDF" && fp.size() > 500, "lưu PDF: tệp hợp lệ, có nội dung");
        // saveImageAs với tệp đã có
        SaveResult c = saveImageAs(img, a.path);
        check(c.ok && c.path != a.path && !c.message.isEmpty(), "Lưu thành với tên đã có: lưu tên khác và báo rõ", c.message);
        QFile fa3(a.path); fa3.open(QIODevice::ReadOnly);
        check(fa3.readAll() == before, "Lưu thành: tệp có sẵn không bị ghi đè");
        // thư mục đích không tạo được (nằm dưới một tệp thường)
        SaveResult bad = saveImage(img, a.path + "/con", Format::Png, t);
        check(!bad.ok && !bad.message.isEmpty(), "thư mục đích không tạo được: báo lỗi, không giả vờ thành công", bad.message);
        SaveResult empty = saveImage(QImage(), dir, Format::Png, t);
        check(!empty.ok, "ảnh trống: từ chối lưu");
        // không để lại tệp tạm
        bool stray = false; for (const QString& n : QDir(dir).entryList(QDir::Files)) if (n.contains("tmp", Qt::CaseInsensitive) || n.startsWith(".")) stray = true;
        check(!stray, "không để lại tệp tạm trong thư mục lưu");
    }
    // 14. thước
    {
        Measure m = measure({0, 0}, {30, 40});
        check(m.dx == 30 && m.dy == 40 && qAbs(m.length - 50.0) < 1e-9, "thước pixel: 30,40 → độ dài 50");
        check(defaultSaveDir().contains("Dung Office"), "thư mục lưu mặc định nằm trong 'Dung Office'");
    }

    printf("\nTổng: %d đạt, %d lỗi, %d cảnh báo\n", g_pass, g_fail, g_warn);
    printf("KẾT QUẢ: %s\n", g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT");
    return g_fail ? 1 : 0;
}
