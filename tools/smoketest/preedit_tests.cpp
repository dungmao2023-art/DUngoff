// SPDX-License-Identifier: MPL-2.0
// Kiểm thử chữ đang soạn (preedit) của bộ gõ trên engine thật, qua Document::postExtTextInput
// (LOK_EXT_TEXTINPUT=0 đặt chữ đang soạn, LOK_EXT_TEXTINPUT_END=2 chốt). Không thay được việc thử bằng Unikey/EVKey thật.
// Chạy:  dung-smoketest --program <program> --preedit 1 --out <thư mục>
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using namespace dung;
namespace fs = std::filesystem;

namespace {
int g_bad = 0;
std::atomic<long long> g_t{0};
std::string g_cur = "khởi động";
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void step(const std::string& s) { g_cur = s; g_t = nowMs(); std::cout << "  -> " << s << std::endl; }
void check(bool ok, const std::string& what) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what << std::endl;
    if (!ok) ++g_bad;
}
void info(const std::string& s) { std::cout << "[GHI NHẬN] " << s << std::endl; }

void pre(Document& d, const std::string& t) { d.postExtTextInput(0, t); d.waitInputIdle(150, 2000); }
void end(Document& d) { d.postExtTextInput(2, ""); d.waitInputIdle(150, 2000); }
std::string all(Document& d) {
    d.command(".uno:SelectAll");
    d.waitInputIdle(150, 2000);
    return d.selectionAs("text/plain;charset=utf-8");
}
std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}
// Về cuối tài liệu, bỏ chọn (sau SelectAll) để gõ tiếp ở cuối.
void toEnd(Document& d) { d.command(".uno:GoToEndOfDoc"); d.waitInputIdle(100, 1500); }
}  // namespace

int dungRunPreeditTests(const fs::path& program, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    g_t = nowMs();
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t > 120000) {
                std::cout << "[LỖI]  TREO quá 120 giây ở bước: " << g_cur << "\n\nKẾT QUẢ: CÓ LỖI (treo)" << std::endl;
                std::_Exit(3);
            }
        }
    }).detach();

    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine " + err);
    if (!engine) return 1;

    // ---------- Writer ----------
    {
        step("Writer: S1 soạn từng bước rồi chốt");
        auto d = engine->createNew(DocKind::Text, &err);
        check((bool)d, "Tạo tài liệu Word " + err);
        if (!d) return 1;
        d->typeText(u"A ");
        d->waitInputIdle(200, 2000);
        pre(*d, "d");
        pre(*d, "d\xC6\xB0");                // "dư"
        pre(*d, "d\xC6\xB0\xE1\xBB\x9D");    // "dườ" -> gần giống thật (đổi từng bước)
        pre(*d, "d\xC6\xB0\xE1\xBB\x9D" "n"); // "dườn"
        pre(*d, "d\xC6\xB0\xE1\xBB\x9D" "ng");
        end(*d);
        std::string t = trim(all(*d));
        check(t == "A d\xC6\xB0\xE1\xBB\x9D" "ng", "S1: sau khi chốt, tài liệu là 'A dường' đúng một lần [" + t + "]");

        step("Writer: S2 huỷ chữ đang soạn");
        toEnd(*d);
        d->typeText(u" B");
        d->waitInputIdle(150, 2000);
        pre(*d, "xyz");
        pre(*d, "");      // huỷ: preedit rỗng
        end(*d);
        t = trim(all(*d));
        check(t.find("xyz") == std::string::npos, "S2: chữ đang soạn đã bị huỷ, không còn 'xyz' [" + t + "]");
        check(t.size() >= 3 && t.compare(t.size() - 2, 2, " B") == 0, "S2: phần đã gõ trước đó ' B' còn nguyên [" + t + "]");

        step("Writer: S3 chốt rồi soạn tiếp cụm mới");
        toEnd(*d);
        pre(*d, "vi");
        pre(*d, "vi\xE1\xBB\x87");           // "việ"
        pre(*d, "vi\xE1\xBB\x87t");          // "việt"
        end(*d);
        d->typeText(u" ");
        pre(*d, "nam");
        end(*d);
        t = trim(all(*d));
        check(t.find("vi\xE1\xBB\x87t nam") != std::string::npos, "S3: 'việt nam' đúng, không nhân đôi [" + t + "]");
        size_t c = 0, pos = 0;
        while ((pos = t.find("vi\xE1\xBB\x87t", pos)) != std::string::npos) { ++c; pos += 1; }
        check(c == 1, "S3: 'việt' chỉ xuất hiện một lần");

        step("Writer: S4 chốt rỗng (END khi không soạn) không làm hỏng");
        toEnd(*d);
        const std::string before = trim(all(*d));
        toEnd(*d);
        end(*d);
        check(trim(all(*d)) == before, "S4: END khi không có chữ đang soạn không đổi tài liệu");
    }

    // ---------- Calc ----------
    {
        step("Calc: S5 soạn trong ô");
        auto d = engine->createNew(DocKind::Spreadsheet, &err);
        check((bool)d, "Tạo bảng tính " + err);
        if (d) {
            pre(*d, "T");
            pre(*d, "To");
            pre(*d, "To\xC3\xA1");   // "Toá"
            pre(*d, "To\xC3\xA1n");  // "Toán"
            end(*d);
            d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1280);   // Enter
            d->postKey(LOK_KEYEVENT_KEYUP, 0, 1280);
            d->waitInputIdle(200, 2000);
            d->command(".uno:GoToCell?ToPoint:string=A1");
            d->waitInputIdle(200, 2000);
            d->command(".uno:Copy");
            d->waitInputIdle(200, 2000);
            const std::string cell = trim(d->selectionAs("text/plain;charset=utf-8"));
            info("Calc: nội dung ô A1 sau khi soạn+chốt: [" + cell + "]");
            check(cell.find("To\xC3\xA1n") != std::string::npos, "S5: ô A1 chứa 'Toán' sau khi chốt");
        }
    }

    // ---------- Impress ----------
    {
        step("Impress: S6 soạn khi chưa có khung chữ (không được treo/sập)");
        auto d = engine->createNew(DocKind::Presentation, &err);
        check((bool)d, "Tạo bài trình chiếu " + err);
        if (d) {
            pre(*d, "T");
            pre(*d, "Th\xC3\xA1");
            end(*d);
            info("Impress: gửi preedit khi không ở chế độ sửa chữ — engine không sập, không treo (chỉ kiểm tra không sập)");
            check(true, "S6: engine vẫn sống sau preedit trong Impress");
        }
    }

    std::cout << (g_bad ? "\nKẾT QUẢ: CÓ LỖI\n" : "\nKẾT QUẢ: TẤT CẢ ĐẠT\n");
    std::cout.flush();
    std::_Exit(g_bad ? 1 : 0);
}
