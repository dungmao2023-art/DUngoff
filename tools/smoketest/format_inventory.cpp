// SPDX-License-Identifier: MPL-2.0
// Kiểm kê định dạng Excel trên engine thật: dung-smoketest --formats <thư mục tệp mẫu> --program <dir> --out <dir>
// Với mỗi tệp base.{xlsx,xls,xlsm,xlsb,csv}: mở, vẽ thử, thử sửa 1 ô, thử lưu cùng đuôi, thử "Lưu thành" .xlsx
// (không xác nhận / có xác nhận mất mát). Ghi dòng "INV <đuôi> <khoá>=<giá trị>" để tests/excel/verify_formats.py đối chiếu.
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using namespace dung;

namespace {
std::atomic<long long> g_t0{0};
std::mutex g_m;
std::string g_step = "khởi động";
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void step(const std::string& s) { { std::lock_guard<std::mutex> g(g_m); g_step = s; } g_t0 = nowMs(); std::cout << "  -> " << s << std::endl; }
void inv(const std::string& ext, const std::string& key, const std::string& val) {
    std::cout << "INV " << ext << " " << key << "=" << val << std::endl;
}
std::string oneLine(std::string s) { for (auto& c : s) if (c == '\n' || c == '\r') c = ' '; return s; }
const char* kindStr(DocKind k) {
    switch (k) { case DocKind::Text: return "text"; case DocKind::Spreadsheet: return "sheet"; case DocKind::Presentation: return "slide"; case DocKind::Drawing: return "drawing"; default: return "other"; }
}
void watchdog(int seconds) {
    g_t0 = nowMs();
    std::thread([seconds] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t0 > seconds * 1000LL) {
                std::lock_guard<std::mutex> g(g_m);
                std::cout << "INV ? treo=quá " << seconds << " giây ở bước: " << g_step << "\nKẾT QUẢ KIỂM KÊ: TREO\n";
                std::cout.flush();
                std::_Exit(3);
            }
        }
    }).detach();
}
} // namespace


namespace {
std::string fnv(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::uint64_t h = 1469598103934665603ull; std::uint64_t n = 0;
    char c;
    while (f.get(c)) { h ^= (unsigned char)c; h *= 1099511628211ull; ++n; }
    char buf[64]; std::snprintf(buf, sizeof buf, "%016llx:%llu", (unsigned long long)h, (unsigned long long)n);
    return buf;
}
} // namespace

// Mẫu .xlsm THẬT do Excel tạo (có macro VBA + nút form control): chính sách app, mất mát khi lưu .xlsx, và đo engine ghi .xlsm.
static void realXlsm(Engine& engine, const fs::path& src, const fs::path& out) {
    const std::string K = "thuchi";
    std::error_code ec;
    const std::string before = fnv(src);
    const fs::path work = out / "goc-thuchi.xlsm";
    fs::copy_file(src, work, fs::copy_options::overwrite_existing, ec);
    std::string err;
    step("mở Mau_Thu_Chi.xlsm thật");
    auto doc = engine.open(work, &err);
    if (!doc) { inv(K, "mở", "LỖI " + oneLine(err)); return; }
    inv(K, "mở", "ok");
    inv(K, "chỉ_đọc", doc->readOnly() ? "có" : "không");
    inv(K, "có_macro", doc->hasMacro() ? "có" : "không");
    inv(K, "số_nút", std::to_string(doc->formControlCount()));
    inv(K, "số_sheet", std::to_string(doc->partCount()));
    std::string names;
    for (int i = 0; i < doc->partCount(); ++i) names += (i ? "|" : "") + doc->partName(i);
    inv(K, "tên_sheet", names);
    for (const auto& w : doc->warnings()) inv(K, "cảnh_báo", oneLine(w).substr(0, 500));
    const int n = 512;
    std::vector<uint8_t> b(n * n * 4, 0);
    doc->setClientView(n, n, n * 15, n * 15, 0, 0, n * 15, n * 15);
    doc->paintTile(b.data(), n, n, 0, 0, n * 15, n * 15);
    size_t dark = 0;
    for (size_t k = 0; k < b.size(); k += 4) if (b[k] < 200 || b[k + 1] < 200 || b[k + 2] < 200) ++dark;
    inv(K, "điểm_ảnh_tối", std::to_string(dark));
    step("sửa ô trên .xlsm thật");
    doc->command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"H2\"}}");
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    doc->typeText(u"SUA");
    doc->postKey(LOK_KEYEVENT_KEYINPUT, 13, 1280);
    doc->postKey(LOK_KEYEVENT_KEYUP, 13, 1280);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    inv(K, "đã_sửa", doc->modified() ? "có" : "không");
    step("lưu .xlsm thật theo chính sách app");
    {
        auto r = doc->saveAs(out / "chinh-sach-cung-duoi.xlsm");
        inv(K, "lưu_xlsm_theo_app", std::string(r.ok ? "ok" : "bị_chặn") + ": " + oneLine(r.message).substr(0, 300));
        auto r1 = doc->saveAs(out / "thuchi-khong-xac-nhan.xlsx");
        inv(K, "xlsx_không_xác_nhận", std::string(r1.ok ? "ok" : "bị_chặn") + ": " + oneLine(r1.message).substr(0, 400));
        SaveOptions so; so.allowLossyExport = true;
        auto r2 = doc->saveAs(out / "thuchi-sang-xlsx.xlsx", so);
        inv(K, "xlsx_có_xác_nhận", std::string(r2.ok ? "ok" : "lỗi") + ": " + oneLine(r2.message).substr(0, 200));
    }
    // Đo thẳng engine (BỎ QUA chính sách của app): ghi .xlsm có giữ macro/nút không?
    step("đo engine ghi .xlsm (bỏ qua chính sách, chỉ để kiểm kê)");
    struct Try { const char* tag; const char* format; } tries[] = {
        {"a", "xlsm"}, {"b", "Calc MS Excel 2007 VBA XML"}, {"c", "xlsx"}};
    const char* skip = std::getenv("DUNG_INV_SKIP_RAW"); // chẩn đoán lỗi thoát: bỏ ghi thô (a,b,c)
    for (const auto& t : tries) {
        if (skip && std::string(skip).find(t.tag) != std::string::npos) continue;
        const fs::path o = out / (std::string("raw-") + t.tag + (std::string(t.tag) == "c" ? ".xlsx" : ".xlsm"));
        const bool ok = doc->saveRawForDiagnostics(o, t.format);
        inv(K, std::string("engine_ghi_") + t.tag, std::string(ok ? "ok" : "thất_bại") + " định_dạng=" + t.format);
    }
    const std::string afterSrc = fnv(src), afterWork = fnv(work);
    inv(K, "gốc_không_đổi_byte", (before == afterSrc) ? "có" : "KHÔNG");
    (void)afterWork;
}

int dungRunFormatInventory(const fs::path& program, const fs::path& dir, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    watchdog(240);
    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) { std::cout << "INV ? engine=lỗi " << oneLine(err) << "\nKẾT QUẢ KIỂM KÊ: LỖI\n"; return 1; }
    inv("*", "engine", oneLine(engine->versionInfo()).substr(0, 120));

    for (const char* ext : {"xlsx", "xls", "xlsm", "xlsb", "csv"}) {
        const fs::path src = dir / (std::string("base.") + ext);
        if (!fs::exists(src)) { inv(ext, "tệp", "không có"); continue; }
        // Mỗi tệp một bản sao riêng trong thư mục kết quả (tệp mẫu gốc không bị chạm).
        const fs::path work = out / (std::string("goc.") + ext);
        fs::copy_file(src, work, fs::copy_options::overwrite_existing, ec);
        step(std::string("mở .") + ext);
        auto doc = engine->open(work, &err);
        if (!doc) { inv(ext, "mở", "LỖI " + oneLine(err)); continue; }
        inv(ext, "mở", "ok");
        inv(ext, "loại", kindStr(doc->kind()));
        inv(ext, "chỉ_đọc", doc->readOnly() ? "có" : "không");
        inv(ext, "số_sheet", std::to_string(doc->partCount()));
        inv(ext, "tên_sheet_1", doc->partName(0));
        for (const auto& w : doc->warnings()) inv(ext, "cảnh_báo", oneLine(w).substr(0, 200));

        // Vẽ thử: có điểm ảnh không-trắng không?
        step(std::string("vẽ .") + ext);
        const int n = 512;
        std::vector<uint8_t> b(n * n * 4, 0);
        doc->setClientView(n, n, n * 15, n * 15, 0, 0, n * 15, n * 15);
        doc->paintTile(b.data(), n, n, 0, 0, n * 15, n * 15);
        size_t dark = 0;
        for (size_t k = 0; k < b.size(); k += 4) if (b[k] < 200 || b[k + 1] < 200 || b[k + 2] < 200) ++dark;
        inv(ext, "điểm_ảnh_tối", std::to_string(dark));

        // Thử sửa 1 ô (H2 = "SUA"). Tài liệu chỉ đọc phải chặn.
        step(std::string("sửa ô .") + ext);
        doc->command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"H2\"}}");
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        doc->typeText(u"SUA");
        doc->postKey(LOK_KEYEVENT_KEYINPUT, 13, 1280);
        doc->postKey(LOK_KEYEVENT_KEYUP, 13, 1280);
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        inv(ext, "đã_sửa", doc->modified() ? "có" : "không");

        // Lưu cùng đuôi (bản sao mới)
        step(std::string("lưu cùng đuôi .") + ext);
        {
            auto r = doc->saveAs(out / (std::string("cung-duoi.") + ext));
            inv(ext, "lưu_cùng_đuôi", std::string(r.ok ? "ok" : "bị_chặn") + ": " + oneLine(r.message).substr(0, 160));
        }
        // Lưu thành .xlsx: không xác nhận, rồi có xác nhận mất mát
        step(std::string("lưu thành xlsx từ .") + ext);
        {
            auto r = doc->saveAs(out / (std::string("sang-xlsx-khong-xac-nhan-") + ext + ".xlsx"));
            inv(ext, "xlsx_không_xác_nhận", std::string(r.ok ? "ok" : "bị_chặn") + ": " + oneLine(r.message).substr(0, 160));
            SaveOptions so; so.allowLossyExport = true;
            auto r2 = doc->saveAs(out / (std::string("sang-xlsx-") + ext + ".xlsx"), so);
            inv(ext, "xlsx_có_xác_nhận", std::string(r2.ok ? "ok" : "lỗi") + ": " + oneLine(r2.message).substr(0, 160));
        }
    }
    {
        const fs::path real = dir / "Mau_Thu_Chi.xlsm";
        if (fs::exists(real)) realXlsm(*engine, real, out); else inv("thuchi", "tệp", "không có");
    }
    std::cout << "KẾT QUẢ KIỂM KÊ: XONG\n";
    return 0;
}
