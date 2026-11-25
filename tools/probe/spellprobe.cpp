// dung-spellprobe: thăm dò engine có gạch chân đỏ (kiểm chính tả tự động) cho tiếng Việt khi đăng ký từ điển Hunspell hay không.
// Cách dùng: dung-spellprobe <thư mục program> <thư mục làm việc> <tệp.docx> [thư mục từ điển vi]
// In: "SPELL <nhãn>: đỏ=<số điểm ảnh đỏ lớn nhất> ..."  (điểm ảnh đỏ ~ nét gạch chân lượn sóng của kiểm chính tả).
// Đây là THĂM DÒ: LibreOfficeKit headless có thể không bật kiểm chính tả tự động; kết quả 0 không chứng minh gì về từ điển.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include "dung/Engine.h"
namespace fs = std::filesystem;
using namespace dung;

static long redCount(const std::vector<uint8_t>& b) {
    long n = 0;
    for (size_t i = 0; i + 3 < b.size(); i += 4) {
        const int B = b[i], G = b[i + 1], R = b[i + 2];
        if (R > 170 && G < 110 && B < 110) ++n;
    }
    return n;
}
static long maxRed(Document& d, int tw, int th, int rounds) {
    long best = 0;
    for (int i = 0; i < rounds; ++i) {
        std::vector<uint8_t> buf(512 * 512 * 4, 255);
        d.paintTile(buf.data(), 512, 512, 0, 0, tw, th);
        best = std::max(best, redCount(buf));
        d.waitInputIdle(100, 600);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    return best;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 4) { printf("SPELL: thiếu tham số\n"); return 2; }
    fs::path prog = fs::u8path(argv[1]), out = fs::u8path(argv[2]), file = fs::u8path(argv[3]);
    const std::string label = file.stem().string() + (argc > 4 ? "+từ-điển" : "+không-từ-điển");
    if (argc > 4) {
        const fs::path dict = fs::u8path(argv[4]);
        Engine::setViDictionaryDir(dict);
        const std::string xml = Engine::viDictionaryRegistryXml(dict);
        printf("SPELL %s: khối đăng ký %zu byte, %s\n", label.c_str(), xml.size(), xml.empty() ? "RỖNG (thiếu cặp .dic/.aff)" : "có");
    }
    fs::create_directories(out);
    std::string err;
    auto eng = Engine::create(prog, out / "profile", out / "work", &err);
    if (!eng) { printf("SPELL %s: LỖI khởi động engine: %s\n", label.c_str(), err.c_str()); return 2; }
    auto doc = eng->open(file, &err);
    if (!doc) { printf("SPELL %s: LỖI mở tệp: %s\n", label.c_str(), err.c_str()); return 2; }
    const int tw = 12000, th = 12000;
    doc->setClientView(512, 512, tw, th, 0, 0, tw, th);
    const long a = maxRed(*doc, tw, th, 4);
    printf("SPELL %s: đỏ(mặc định)=%ld\n", label.c_str(), a);
    // Bật/tắt kiểm chính tả tự động (nếu mặc định đang tắt, lệnh này bật; nếu đang bật thì tắt).
    doc->command(".uno:SpellOnline");
    doc->waitInputIdle(300, 3000);
    const long b = maxRed(*doc, tw, th, 5);
    printf("SPELL %s: đỏ(sau .uno:SpellOnline)=%ld\n", label.c_str(), b);
    doc->command(".uno:SpellOnline");
    doc->waitInputIdle(300, 3000);
    const long c = maxRed(*doc, tw, th, 5);
    printf("SPELL %s: đỏ(sau lần bật/tắt thứ hai)=%ld\n", label.c_str(), c);
    printf("SPELL %s: TỔNG KẾT max=%ld\n", label.c_str(), std::max(a, std::max(b, c)));
    fflush(stdout);
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 0);   // tránh sập khi dọn DLL lúc thoát (đã gặp ở các thăm dò khác)
#endif
    _Exit(0);
}
