// SPDX-License-Identifier: MPL-2.0
// Kiểm thử các lệnh Excel (UNO) trên engine thật: dung-smoketest --excel 1 --xlsx <excel-co-ban.xlsx>
// [--stress <stress-5000-dong.xlsx>] --program <dir> --out <dir>. Ghi tệp kết quả để
// tests/excel/verify_excel.py kiểm tra nội dung (công thức, ghi chú, liên kết, định dạng không mất).
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"
#include "ExcelCommands.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using namespace dung;
namespace ex = dung::excel;

namespace {
int g_fail = 0;
std::atomic<long long> g_t0{0};
std::mutex g_m;
std::string g_step = "khởi động";
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void step(const std::string& s) {
    { std::lock_guard<std::mutex> g(g_m); g_step = s; }
    g_t0 = nowMs();
    std::cout << "  -> " << s << std::endl;
}
void check(bool ok, const std::string& what) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what << std::endl;
    if (!ok) ++g_fail;
}
void info(const std::string& what) { std::cout << "[TIN]  " << what << std::endl; }
void watchdog(int seconds) {
    g_t0 = nowMs();
    std::thread([seconds] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t0 > seconds * 1000LL) {
                std::lock_guard<std::mutex> g(g_m);
                std::cout << "[LỖI]  TREO quá " << seconds << " giây ở bước: " << g_step << "\nKẾT QUẢ EXCEL: CÓ LỖI (treo)\n";
                std::cout.flush();
                std::_Exit(3);
            }
        }
    }).detach();
}
void gotoCell(Document& d, const std::string& ref) {
    d.command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"" + ref + "\"}}");
    d.waitFor(LOK_CALLBACK_CELL_ADDRESS, [&](const std::string& p) { return p == ref; }, 15000);
}
void settle(int ms = 400) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
std::string names(Document& d) {
    std::string s;
    for (int i = 0; i < d.partCount(); ++i) s += (i ? "|" : "") + d.partName(i);
    return s;
}
} // namespace

int runExcelTests(const fs::path& program, const fs::path& xlsx, const fs::path& stress, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    watchdog(150);
    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine " + err);
    if (!engine) return 1;

    // ======================= Tệp cơ bản =======================
    fs::path src = out / "excel-co-ban-goc.xlsx";
    fs::copy_file(xlsx, src, fs::copy_options::overwrite_existing, ec);
    step("mở tệp Excel cơ bản");
    auto doc = engine->open(src, &err);
    check((bool)doc, "Mở tệp " + err);
    if (!doc) return 1;
    doc->setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
    check(doc->partCount() == 2, "Ban đầu có 2 sheet: " + names(*doc));

    // --- Sheet ---
    step("thêm / đổi tên / nhân đôi / xoá sheet");
    doc->command(".uno:Insert", ex::sheetInsert("Mới", 3));
    settle();
    check(doc->partCount() == 3 && doc->partName(2) == "Mới", "Thêm sheet 'Mới' vào cuối: " + names(*doc));
    doc->setPart(2);
    doc->command(".uno:RenameTable", ex::sheetRename("Đổi tên"));
    settle();
    check(doc->partName(2) == "Đổi tên", "Đổi tên sheet: " + names(*doc));
    doc->setPart(1); // 'Phụ'
    doc->command(".uno:Move", ex::sheetMove(toUtf8(doc->workingCopy().stem()), 3, true));
    settle();
    check(doc->partCount() == 4, "Nhân đôi sheet 'Phụ' (số sheet = 4): " + names(*doc));
    info("Thứ tự sheet sau khi nhân đôi: " + names(*doc));
    // xoá sheet 'Đổi tên'
    int idx = -1;
    for (int i = 0; i < doc->partCount(); ++i) if (doc->partName(i) == "Đổi tên") idx = i;
    check(idx >= 0, "Tìm thấy sheet 'Đổi tên'");
    if (idx >= 0) {
        doc->setPart(idx);
        doc->command(".uno:Remove", ex::sheetRemove(idx + 1));
        settle();
        bool gone = true;
        for (int i = 0; i < doc->partCount(); ++i) if (doc->partName(i) == "Đổi tên") gone = false;
        check(doc->partCount() == 3 && gone, "Xoá sheet 'Đổi tên': " + names(*doc));
    }
    doc->setPart(0);
    check(doc->partName(0) == "Dữ liệu", "Sheet đầu vẫn là 'Dữ liệu'");

    // --- Định dạng ô (A1 là tiêu đề) ---
    step("định dạng ô A1");
    gotoCell(*doc, "A1");
    doc->command(".uno:Bold");
    doc->command(".uno:Italic");
    doc->command(".uno:FontHeight", ex::fontHeight(16));
    doc->command(".uno:CharFontName", ex::fontName("Arial"));
    doc->command(".uno:Color", ex::textColor(0xFF0000));
    doc->command(".uno:BackgroundColor", ex::fillColor(0xFFFF00));
    doc->command(".uno:AlignHorizontalCenter");
    doc->command(".uno:WrapText");
    settle();

    step("định dạng số B2 (%), B4 (#,##0) — không đổi kiểu/giá trị");
    gotoCell(*doc, "B2");
    doc->command(".uno:NumberFormatPercent");
    gotoCell(*doc, "B4");
    doc->command(".uno:NumberFormatThousands");
    settle();

    step("độ rộng cột C, chiều cao hàng 4");
    gotoCell(*doc, "C1");
    doc->command(".uno:ColumnWidth", ex::columnWidth(ex::cmToHundredthMm(4.0)));
    gotoCell(*doc, "A4");
    doc->command(".uno:RowHeight", ex::rowHeight(ex::cmToHundredthMm(1.5)));
    settle();

    step("tìm & thay thế chỉ trong giá trị (B -> Z), công thức giữ nguyên");
    doc->command(".uno:ExecuteSearch", ex::replaceAll("B", "Z"));
    settle();

    step("sắp xếp giảm dần theo cột Số");
    gotoCell(*doc, "B2");
    doc->command(".uno:SortDescending");
    settle(800);

    step("cố định hàng đầu + bộ lọc tự động");
    gotoCell(*doc, "A1");
    doc->command(".uno:FreezePanesRow");
    settle();
    doc->command(".uno:DataFilterAutoFilter");
    settle();

    step("chèn hàng 4 (công thức dịch theo)");
    gotoCell(*doc, "A4");
    doc->command(".uno:InsertRowsBefore");
    settle();

    step("AutoSum (thông tin, không chặn)");
    gotoCell(*doc, "B8");
    doc->command(".uno:AutoSum");
    settle();
    doc->postKey(LOK_KEYEVENT_KEYINPUT, 13, 1280); // Enter (KEY_RETURN)
    doc->postKey(LOK_KEYEVENT_KEYUP, 13, 1280);
    settle(600);

    step("lưu kết quả");
    auto r = doc->saveAs(out / "excel-ket-qua.xlsx");
    check(r.ok, "Lưu thành excel-ket-qua.xlsx: " + r.message);
    doc.reset();

    // ======================= Tệp stress 5000 dòng =======================
    if (!stress.empty() && fs::exists(stress)) {
        fs::path s2 = out / "stress-goc.xlsx";
        fs::copy_file(stress, s2, fs::copy_options::overwrite_existing, ec);
        step("mở tệp stress 5000 dòng");
        auto sd = engine->open(s2, &err);
        check((bool)sd, "Mở tệp stress " + err);
        if (sd) {
            sd->setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
            int dh = -1;
            for (int i = 0; i < sd->partCount(); ++i) if (sd->partName(i) == "DonHang") dh = i;
            check(dh >= 0, "Có sheet DonHang");
            if (dh >= 0) {
                sd->setPart(dh);
                step("stress: sắp xếp giảm dần theo cột F (5000 dòng, công thức)");
                gotoCell(*sd, "F2");
                auto t0 = nowMs();
                sd->clearEvents();
                sd->command(".uno:SortDescending", "", true);
                sd->waitFor(LOK_CALLBACK_UNO_COMMAND_RESULT, [](const std::string&) { return true; }, 120000);
                auto dt1 = nowMs() - t0;
                check(dt1 < 100000, "Sắp xếp 5000 dòng không treo (" + std::to_string(dt1) + " ms)");
                step("stress: bộ lọc tự động");
                gotoCell(*sd, "A1");
                t0 = nowMs();
                sd->clearEvents();
                sd->command(".uno:DataFilterAutoFilter", "", true);
                sd->waitFor(LOK_CALLBACK_UNO_COMMAND_RESULT, [](const std::string&) { return true; }, 120000);
                auto dt2 = nowMs() - t0;
                check(dt2 < 100000, "Bật bộ lọc 5000 dòng không treo (" + std::to_string(dt2) + " ms)");
                settle();
                step("stress: lưu");
                auto rs = sd->saveAs(out / "stress-ket-qua.xlsx");
                check(rs.ok, "Lưu kết quả stress: " + rs.message);
            }
        }
    } else {
        info("Không có tệp stress — bỏ qua phần 5000 dòng");
    }

    std::cout << (g_fail ? "\nKẾT QUẢ EXCEL: CÓ LỖI\n" : "\nKẾT QUẢ EXCEL: ĐẠT\n");
    std::cout.flush();
    std::_Exit(g_fail ? 1 : 0);
}
