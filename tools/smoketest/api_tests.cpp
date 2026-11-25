// SPDX-License-Identifier: MPL-2.0
// Kiểm thử cầu nối script dung_api.py trên engine thật (LOK runMacro):
//   dung-smoketest --program <program> --api 1 --out <thư mục>
// Phủ: ping, lề trang (đọc lại để chứng minh), từ chối hàm lạ / đối số lạ, tài liệu mở từ tệp (expect_url),
// tài liệu chỉ đọc, cờ "đã sửa", hàm Calc, và đối chiếu danh sách trắng C++ với Python.
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <atomic>
#include <cstdlib>
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
// Từ chối do chính script/ứng dụng (không phải do cầu nối hỏng): thông báo không được là lỗi runMacro/kết quả trống.
bool rejectedByRules(const ScriptResult& r) {
    return !r.ok && r.message.find("runMacro") == std::string::npos && r.message.find("không trả kết quả") == std::string::npos;
}
}  // namespace

int dungRunApiTests(const fs::path& program, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    g_t = nowMs();
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t > 180000) {
                std::cout << "[LỖI]  TREO quá 180 giây ở bước: " << g_cur << "\n\nKẾT QUẢ: CÓ LỖI (treo)" << std::endl;
                std::_Exit(3);
            }
        }
    }).detach();

    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine " + err);
    if (!engine) return 1;
    check(fs::exists(out / "profile" / "user" / "Scripts" / "python" / "dung_api.py"), "dung_api.py đã được cài vào hồ sơ engine");

    if (const char* pre = std::getenv("DUNG_API_PRELOAD"); pre && *pre) {
        // THÍ NGHIỆM: runMacro khi CHƯA có tài liệu nào
        std::cout << "[THÍ NGHIỆM] runMacro trước khi mở tài liệu: " << pre << " -> " << engine->diagRunMacro(pre) << std::endl;
    }
    step("Word mới: ping");
    auto d = engine->createNew(DocKind::Text, &err);
    check((bool)d, "Tạo tài liệu Word " + err);
    if (!d) return 1;
    d->typeText(u"Tieu de\nNoi dung thu nghiem");
    d->waitInputIdle(300, 4000);
    auto r = d->callScript("ping");
    check(r.ok, "ping: " + r.message + " | " + r.json);
    check(r.json.find("\"kind\": \"writer\"") != std::string::npos, "ping nhận ra tài liệu Writer");

    step("đặt và đọc lại lề");
    r = d->callScript("page_margins", R"({"left_mm":30,"right_mm":15,"top_mm":20,"bottom_mm":20})");
    check(r.ok && r.mutated, "page_margins: " + r.message);
    r = d->callScript("read_page_margins");
    check(r.ok && r.json.find("\"left_mm\": 30.0") != std::string::npos, "Đọc lại lề trái = 30 mm: " + r.json);

    step("từ chối đầu vào xấu");
    r = d->callScript("os.system", "{}");
    check(rejectedByRules(r), "Hàm ngoài danh sách bị từ chối: " + r.message);
    r = d->callScript("page_margins", R"({"left_mm":1,"right_mm":1,"top_mm":1,"bottom_mm":1,"evil":"x"})");
    check(rejectedByRules(r), "Đối số lạ bị từ chối: " + r.message);
    r = d->callScript("page_margins", R"({"left_mm":-5,"right_mm":1,"top_mm":1,"bottom_mm":1})");
    check(rejectedByRules(r), "Lề âm bị từ chối: " + r.message);
    r = d->callScript("sort_range", "{}");
    check(rejectedByRules(r), "Hàm Calc trên tài liệu Word bị từ chối: " + r.message);

    step("lưu docx, mở lại từ tệp (expect_url)");
    const fs::path docx = out / "api-mau.docx";
    SaveOptions so;
    auto sr = d->saveAs(docx, so);
    check(sr.ok, "Lưu docx: " + sr.message);
    auto d2 = engine->open(docx, &err);
    check((bool)d2, "Mở lại docx " + err);
    if (d2) {
        r = d2->callScript("page_margins", R"({"left_mm":25,"right_mm":25,"top_mm":25,"bottom_mm":25})");
        check(r.ok, "Gọi script trên tài liệu mở từ tệp (expect_url khớp): " + r.message);
        check(d2->modified(), "Cờ 'đã sửa' bật sau khi script đổi tài liệu");
        r = d2->callScript("header_footer", R"({"footer":true,"footer_page_number":true})");
        check(r.ok, "header_footer: " + r.message);
    }

    step("Calc mới");
    auto c = engine->createNew(DocKind::Spreadsheet, &err);
    check((bool)c, "Tạo bảng tính " + err);
    if (c) {
        r = c->callScript("cell_format", R"({"range":"A1:B2","bold":true,"number_format":"vnd","bg_color":"#FFFF00"})");
        check(r.ok, "cell_format: " + r.message);
        r = c->callScript("data_validation", R"({"range":"D1:D5","kind":"list","items":["Thu","Chi"]})");
        check(r.ok, "data_validation: " + r.message);
        r = c->callScript("page_margins", R"({"left_mm":1,"right_mm":1,"top_mm":1,"bottom_mm":1})");
        check(rejectedByRules(r), "Hàm Word trên bảng tính bị từ chối: " + r.message);
    }

    std::cout << (g_bad ? "\nKẾT QUẢ: CÓ LỖI" : "\nKẾT QUẢ: ĐẠT") << std::endl;
    std::cout.flush();
    std::_Exit(g_bad ? 1 : 0);   // LibreOffice không tắt sạch được trong cùng tiến trình
}
