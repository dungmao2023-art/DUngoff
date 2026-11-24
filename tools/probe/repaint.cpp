// dung-repaintprobe: kiểm tra engine có VẼ LẠI tile sau khi gõ chữ hay không.
// Cách dùng: dung-repaintprobe <thư mục program> <thư mục làm việc> <tệp> <writer|calc|impress>
// In dòng "REPAINT <chế độ>: ..." và thoát mã 1 nếu gõ xong mà không có điểm ảnh nào đổi.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif
#include "dung/Engine.h"
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
namespace fs = std::filesystem;
using namespace dung;

struct Box { int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1; long n = 0; };
static Box diff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int w) {
    Box r;
    for (size_t i = 0; i + 3 < a.size(); i += 4) {
        if (memcmp(&a[i], &b[i], 4) == 0) continue;
        int p = int(i / 4), x = p % w, y = p / w;
        r.x0 = std::min(r.x0, x); r.y0 = std::min(r.y0, y); r.x1 = std::max(r.x1, x); r.y1 = std::max(r.y1, y); ++r.n;
    }
    return r;
}
static std::vector<uint8_t> tile(Document& d, int tw, int th) {
    std::vector<uint8_t> buf(512 * 512 * 4, 255);
    d.paintTile(buf.data(), 512, 512, 0, 0, tw, th);
    return buf;
}
static void report(const char* label, const Box& b) {
    printf("REPAINT %s: điểm ảnh đổi=%ld vùng=[%d,%d]-[%d,%d]\n", label, b.n, b.x0, b.y0, b.x1, b.y1);
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 5) { printf("thiếu tham số\n"); return 2; }
    fs::path prog = fs::u8path(argv[1]), out = fs::u8path(argv[2]), file = fs::u8path(argv[3]);
    const std::string mode = argv[4];
    fs::create_directories(out);
    std::string err;
    auto eng = Engine::create(prog, out / "profile", out / "work", &err);
    if (!eng) { printf("REPAINT %s: LỖI khởi động engine: %s\n", mode.c_str(), err.c_str()); return 2; }
    auto doc = eng->open(file, &err);
    if (!doc) { printf("REPAINT %s: LỖI mở tệp: %s\n", mode.c_str(), err.c_str()); return 2; }

    const int tw = mode == "calc" ? 20000 : 12000, th = mode == "calc" ? 6000 : 12000;
    doc->setClientView(512, 512, tw, th, 0, 0, tw, th);
    auto before = tile(*doc, tw, th);
    doc->clearEvents();
    if (mode == "calc") {
        doc->command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"F2\"}}");
    } else if (mode == "impress") {
        // Tab chọn khung đầu tiên của trang chiếu, Enter vào chế độ soạn chữ (không phụ thuộc toạ độ).
        doc->postKey(0, 0, 1282); doc->postKey(1, 0, 1282);
        doc->postKey(0, 0, 1280); doc->postKey(1, 0, 1280);
    }
    const bool gotBefore = doc->waitFor(0, [](const std::string&) { return true; }, 500);
    (void)gotBefore;
    doc->clearEvents();
    doc->typeText(u"Dung Office xin chao");
    {   // Con trỏ nhập liệu: engine có gửi vị trí/hiện-ẩn con trỏ không?
        std::string cr, cv;
        const bool gotRect = doc->waitFor(1 /*INVALIDATE_VISIBLE_CURSOR*/, [&](const std::string& pl) { cr = pl; return true; }, 1500);
        const bool gotVis = doc->waitFor(5 /*CURSOR_VISIBLE*/, [&](const std::string& pl) { cv = pl; return true; }, 500);
        printf("REPAINT %s: sự kiện vị trí con trỏ: %s %s\n", mode.c_str(), gotRect ? "có" : "KHÔNG", cr.substr(0, 60).c_str());
        printf("REPAINT %s: sự kiện hiện/ẩn con trỏ: %s %s\n", mode.c_str(), gotVis ? "có" : "KHÔNG", cv.substr(0, 40).c_str());
    }
    // Phân biệt: sự kiện làm mới do engine LibreOffice tự báo, và sự kiện "SYNTH" do tầng engine của Dũng Office bù thêm.
    const bool nativeInval = doc->waitFor(0, [](const std::string& pl) { return pl != "SYNTH"; }, 3000);
    const bool synthInval = doc->waitFor(0, [](const std::string& pl) { return pl == "SYNTH"; }, 100)
                         || doc->lastPayload(0) == "SYNTH";
    printf("REPAINT %s: engine tự báo làm mới tile sau khi gõ: %s\n", mode.c_str(), nativeInval ? "có" : "KHÔNG");
    printf("REPAINT %s: tầng engine bù sự kiện làm mới (SYNTH): %s\n", mode.c_str(), synthInval ? "có" : "KHÔNG");
    auto typed = tile(*doc, tw, th);
    Box b1 = diff(before, typed, 512);
    report((mode + " (đang gõ)").c_str(), b1);
    long total = b1.n;
    if (mode == "calc") {
        doc->postKey(0, 0, 1280); doc->postKey(1, 0, 1280); // Enter
        auto committed = tile(*doc, tw, th);
        Box b2 = diff(before, committed, 512);
        report("calc (sau Enter)", b2);
        total += b2.n;
    }
    const bool ok = total > 0 && synthInval;
    printf("REPAINT %s: %s\n", mode.c_str(), ok ? "ĐẠT (giao diện thay đổi sau khi gõ)" : "LỖI (gõ xong mà hình không đổi)");
    fflush(stdout);
    std::_Exit(ok ? 0 : 1);
}
