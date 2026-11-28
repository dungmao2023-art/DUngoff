// dung-smoketest --sweep 1 : quét các lệnh UNO của thanh ribbon mà các bộ kiểm thử khác chưa chạm tới,
// trên ENGINE THẬT (Writer + Calc). Mỗi lệnh được kiểm bằng hiệu ứng quan sát được (nội dung/định dạng
// vùng chọn, trạng thái engine báo về), không chỉ "gửi lệnh không lỗi".
// Mỗi kết quả in một dòng máy đọc được:  SWEEP|<ứng dụng>|<lệnh>|<ĐẠT/LỖI>|<bằng chứng>
// để docs/COMMAND_MATRIX.md dựng từ đó. Lệnh nào [LỖI] phải bị vô hiệu hoá trên giao diện (xem Ribbon.cpp).
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"
#include "../../app/src/tools/dataassist/ExcelBridge.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

using namespace dung;

namespace {
int g_fail = 0;
std::mutex g_mu;
std::map<std::string, std::string> g_states;
std::atomic<long long> g_t0{0};
std::string g_step = "khởi động";

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void step(const std::string& s) {
    { std::lock_guard<std::mutex> g(g_mu); g_step = s; }
    g_t0 = nowMs();
    std::cout << "  -> " << s << std::endl;
}
std::string flat(std::string s, size_t n = 140) {
    for (auto& c : s) if (c == '\n' || c == '\r' || c == '\t' || c == '|') c = ' ';
    if (s.size() > n) s.resize(n);
    return s;
}
void result(const std::string& app, const std::string& cmd, bool ok, const std::string& evidence) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << app << " " << cmd << "  {" << flat(evidence) << "}\n";
    std::cout << "SWEEP|" << app << "|" << cmd << "|" << (ok ? "ĐẠT" : "LỖI") << "|" << flat(evidence) << std::endl;
    if (!ok) ++g_fail;
}
void unchecked(const std::string& app, const std::string& cmd, const std::string& why) {
    std::cout << "[CHƯA KIỂM]  " << app << " " << cmd << "  {" << flat(why) << "}\n";
    std::cout << "SWEEP|" << app << "|" << cmd << "|CHƯA KIỂM|" << flat(why) << std::endl;
}
// Thăm dò: không làm hỏng lượt chạy; ghi nhận để chọn bật/ẩn nút trên giao diện.
void probe(const std::string& app, const std::string& cmd, bool ok, const std::string& evidence) {
    std::cout << (ok ? "[ĐẠT]  " : "[THĂM DÒ: KHÔNG CHẠY]  ") << app << " " << cmd << "  {" << flat(evidence) << "}\n";
    std::cout << "SWEEP|" << app << "|" << cmd << "|" << (ok ? "ĐẠT" : "KHÔNG CHẠY") << "|" << flat(evidence) << std::endl;
}
void watchdog(int seconds) {
    g_t0 = nowMs();
    std::thread([seconds] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t0 > seconds * 1000LL) {
                std::lock_guard<std::mutex> g(g_mu);
                std::cout << "[LỖI]  TREO quá " << seconds << " giây ở bước: " << g_step << "\n\nKẾT QUẢ QUÉT LỆNH: CÓ LỖI (treo)\n";
                std::cout.flush();
                std::_Exit(3);
            }
        }
    }).detach();
}

void onEvent(const EngineEvent& ev) {
    if (ev.type != LOK_CALLBACK_STATE_CHANGED) return;
    const auto eq = ev.payload.find('=');
    if (eq == std::string::npos) return;
    std::lock_guard<std::mutex> g(g_mu);
    g_states[ev.payload.substr(0, eq)] = ev.payload.substr(eq + 1);
}
std::string stateOf(const std::string& cmd) {
    std::lock_guard<std::mutex> g(g_mu);
    auto it = g_states.find(cmd);
    return it == g_states.end() ? "(chưa báo)" : it->second;
}
void forget(const std::string& cmd) { std::lock_guard<std::mutex> g(g_mu); g_states.erase(cmd); }

void settle(int ms = 350) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void run(Document& d, const std::string& uno, const std::string& args = "") { d.command(uno, args); settle(); }
std::string text(Document& d) { return d.selectionAs("text/plain;charset=utf-8"); }
std::string html(Document& d) { return d.selectionAs("text/html"); }
bool has(const std::string& h, const std::string& n) {
    std::string a = h, b = n;
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return a.find(b) != std::string::npos;
}
std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r')) ++i;
    return s.substr(i);
}

void gotoCell(Document& d, const std::string& ref) {
    d.command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"" + ref + "\"}}");
    d.waitFor(LOK_CALLBACK_CELL_ADDRESS, [&](const std::string&) { return true; }, 3000);
    settle(200);
}
void enter(Document& d, const std::string& ref, const std::u16string& v) {
    gotoCell(d, ref);
    d.typeText(v);
    d.postKey(LOK_KEYEVENT_KEYINPUT, 13, 1280);
    d.postKey(LOK_KEYEVENT_KEYUP, 13, 1280);
    settle(300);
}
std::string range(Document& d, const std::string& ref) { gotoCell(d, ref); return text(d); }
}  // namespace

int runSweepTests(const fs::path& program, const fs::path& out,const fs::path& pivotFixture) {
    std::error_code ec;
    fs::create_directories(out, ec);
    watchdog(120);
    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) { std::cout << "[LỖI]  Khởi động engine " << err << "\n"; return 1; }

    // ============================== CALC ==============================
    step("tạo bảng tính mới");
    auto xl = engine->createNew(DocKind::Spreadsheet, &err);
    if (!xl) { std::cout << "[LỖI]  Tạo bảng tính " << err << "\n"; return 1; }
    Document& x = *xl;
    x.setEventSink(onEvent);
    x.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
    settle();
    const std::string C = "Calc";

    step("sắp xếp");
    enter(x, "A1", u"3"); enter(x, "A2", u"1"); enter(x, "A3", u"4"); enter(x, "A4", u"2");
    gotoCell(x, "A1:A4");
    run(x, ".uno:SortAscending");
    std::string t = trim(range(x, "A1:A4"));
    result(C, ".uno:SortAscending", t.find('1') < t.find('2') && t.find('2') < t.find('3') && t.find('3') < t.find('4'), t);
    gotoCell(x, "A1:A4");
    run(x, ".uno:SortDescending");
    t = trim(range(x, "A1:A4"));
    result(C, ".uno:SortDescending", t.find('4') < t.find('3') && t.find('3') < t.find('2') && t.find('2') < t.find('1'), t);

    step("chèn/xoá cột và hàng");
    enter(x, "C2", u"X");
    gotoCell(x, "C2");
    run(x, ".uno:InsertColumnsBefore");
    result(C, ".uno:InsertColumnsBefore", trim(range(x, "D2")) == "X" && trim(range(x, "C2")).empty(), "D2=" + trim(range(x, "D2")));
    gotoCell(x, "C2");
    run(x, ".uno:DeleteColumns");
    result(C, ".uno:DeleteColumns", trim(range(x, "C2")) == "X", "C2=" + trim(range(x, "C2")));
    gotoCell(x, "C3");
    run(x, ".uno:InsertRowsBefore");
    result(C, ".uno:InsertRowsBefore(làm dịch hàng)", trim(range(x, "A5")) != "" , "A5=" + trim(range(x, "A5")));
    gotoCell(x, "C3");
    run(x, ".uno:DeleteRows");
    result(C, ".uno:DeleteRows", trim(range(x, "A4")) != "" && trim(range(x, "A5")).empty(), "A4=" + trim(range(x, "A4")) + " A5=" + trim(range(x, "A5")));

    step("căn lề ô");
    enter(x, "E1", u"canle");
    struct Al { const char* uno; const char* html1; const char* html2; };
    const Al als[] = {
        {".uno:AlignLeft", "align=\"left\"", "text-align: left"},
        {".uno:AlignRight", "align=\"right\"", "text-align: right"},
        {".uno:AlignHorizontalCenter", "align=\"center\"", "text-align: center"},
        {".uno:AlignTop", "valign=top", "valign=\"top\""},
        {".uno:AlignVCenter", "valign=middle", "valign=center"},
        {".uno:AlignBottom", "valign=bottom", "(mặc định)"},
    };
    for (const auto& a : als) {
        gotoCell(x, "E1");
        forget(a.uno);
        run(x, a.uno);
        const std::string h = html(x);
        if (getenv("DUNG_SWEEP_DEBUG")) std::cout << "HTML:" << h << "\n";
        const bool bottomDefault = std::string(a.uno) == ".uno:AlignBottom" && !has(h, "valign=top") && !has(h, "valign=middle") && !has(h, "valign=center");
        result(C, a.uno, has(h, a.html1) || has(h, a.html2) || bottomDefault, "state=" + stateOf(a.uno) + " html=" + flat(h, 80));
    }

    step("gộp ô");
    enter(x, "G1", u"gop");
    gotoCell(x, "G1:H1");
    run(x, ".uno:ToggleMergeCells");
    std::string h = html(x);
    result(C, ".uno:ToggleMergeCells", has(h, "colspan"), flat(h, 100));

    step("định dạng số");
    enter(x, "J1", u"0.5");
    gotoCell(x, "J1");
    run(x, ".uno:NumberFormatPercent");
    t = trim(text(x));
    result(C, ".uno:NumberFormatPercent", t.rfind("50", 0) == 0 && t.back() == '%', t);
    run(x, ".uno:NumberFormatStandard");
    t = trim(text(x));
    result(C, ".uno:NumberFormatStandard", t == "0.5" || t == "0,5", t);
    run(x, ".uno:NumberFormatIncDecimals");
    t = trim(text(x));
    result(C, ".uno:NumberFormatIncDecimals", t.find('.') != std::string::npos || t.find(',') != std::string::npos, t);
    const std::string afterInc = t;
    run(x, ".uno:NumberFormatDecDecimals");
    t = trim(text(x));
    result(C, ".uno:NumberFormatDecDecimals", t.size() < afterInc.size() || t != afterInc, afterInc + " -> " + t);
    enter(x, "J2", u"45000");
    gotoCell(x, "J2");
    run(x, ".uno:NumberFormatDate");
    t = trim(text(x));
    result(C, ".uno:NumberFormatDate", t != "45000" && (t.find('/') != std::string::npos || t.find('-') != std::string::npos || t.find('.') != std::string::npos), t);

    step("vừa cột");
    enter(x, "L1", u"mot-chuoi-rat-dai-de-cot-phai-gian-ra-nhieu");
    gotoCell(x, "L1");
    auto colW = [&]() {   // độ rộng cột L (px) lấy từ <col width=..> của HTML vùng chọn
        const std::string hh = html(x);
        const auto p = hh.find("width=");
        return p == std::string::npos ? 0L : std::atol(hh.c_str() + p + 6 + (hh[p + 6] == '"' ? 1 : 0));
    };
    const long w0 = colW();
    run(x, ".uno:SetOptimalColumnWidth", "{\"aExtraWidth\":{\"type\":\"unsigned short\",\"value\":\"0\"}}");
    const long w1 = colW();
    result(C, ".uno:SetOptimalColumnWidth", w1 > w0 && w0 > 0, "rộng " + std::to_string(w0) + " -> " + std::to_string(w1));

    step("cố định ô");
    gotoCell(x, "B2");
    // Trạng thái engine báo về BẤT ĐỒNG BỘ (trên Windows đến chậm hơn Linux): chờ tối đa 4 giây cho tới khi
    // đúng giá trị mong đợi thay vì đọc ngay sau 350 ms. Bằng chứng cuối cùng là TỆP xlsx lưu ra
    // (freeze-bat.xlsx / freeze-tat.xlsx) — tests/calc/check_freeze.py đọc <pane state="frozen"> trong XML.
    auto waitState = [&](const std::string& want) {
        for (int i = 0; i < 40 && stateOf(".uno:FreezePanes") != want; ++i) settle(100);
        return stateOf(".uno:FreezePanes");
    };
    forget(".uno:FreezePanes");
    run(x, ".uno:FreezePanes");
    std::string fp = waitState("true");
    result(C, ".uno:FreezePanes", fp == "true", "state=" + fp);
    {
        auto r = x.saveAs(out / "freeze-bat.xlsx");
        std::cout << "FREEZE-FILE|bat|" << (r.ok ? "đã lưu" : "LƯU LỖI: " + r.message) << std::endl;
    }
    run(x, ".uno:FreezePanes");
    fp = waitState("false");
    result(C, ".uno:FreezePanes(tắt)", fp == "false", "state=" + fp);
    {
        auto r = x.saveAs(out / "freeze-tat.xlsx");
        std::cout << "FREEZE-FILE|tat|" << (r.ok ? "đã lưu" : "LƯU LỖI: " + r.message) << std::endl;
    }
    forget(".uno:FreezePanesColumn");
    run(x, ".uno:FreezePanesColumn");
    fp = stateOf(".uno:FreezePanes");
    result(C, ".uno:FreezePanesColumn", fp == "true", "FreezePanes=" + fp);
    run(x, ".uno:FreezePanes");
    run(x, ".uno:FreezePanesRow");
    fp = stateOf(".uno:FreezePanes");
    result(C, ".uno:FreezePanesRow", fp == "true", "FreezePanes=" + fp);

    step("cắt");
    enter(x, "N1", u"catdi");
    gotoCell(x, "N1");
    run(x, ".uno:Cut");
    result(C, ".uno:Cut", trim(range(x, "N1")).empty(), "N1=" + trim(range(x, "N1")));

    step("chép định dạng");
    gotoCell(x, "E1");
    run(x, ".uno:FormatPaintbrush");
    forget(".uno:FormatPaintbrush");
    unchecked(C, ".uno:FormatPaintbrush", "hiệu ứng cần chọn ô đích bằng chuột; kiểm trên máy thật");
    x.postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281);   // ESC thoát chế độ chép định dạng
    x.postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
    settle();

    step("tiêu đề hàng/cột");
    const std::string hv = x.commandValues(".uno:ViewRowColumnHeaders?x=0&y=0&width=7680&height=7680");
    result(C, ".uno:ViewRowColumnHeaders", !hv.empty() && hv.find("rows") != std::string::npos, hv);

    // ---------- WIRE: cầu nối "Chèn vào ô" + hoàn tác ----------
    step("cầu nối chèn công thức");
    enter(x, "P2", u"1"); enter(x, "P3", u"2");
    gotoCell(x, "P1");
    auto o1 = dataassist::bridgeInsertFormula(x, "=SUM(P2:P3)", [](const std::string&) { return true; });
    result(C, "wire:chèn công thức vào ô trống", o1.ok && !o1.hadData && o1.resultText == "3", "kết quả=" + o1.resultText);
    gotoCell(x, "P2");
    bool asked = false;
    auto o2 = dataassist::bridgeInsertFormula(x, "=1+1", [&](const std::string&) { asked = true; return false; });
    result(C, "wire:ô có dữ liệu → hỏi và KHÔNG ghi khi từ chối", asked && o2.declined && !o2.ok && trim(range(x, "P2")) == "1", "P2=" + trim(range(x, "P2")));
    gotoCell(x, "P2");
    auto o3 = dataassist::bridgeInsertFormula(x, "=1+1", [&](const std::string&) { return true; });
    result(C, "wire:đồng ý ghi đè", o3.ok && o3.hadData && o3.previous == "1" && o3.resultText == "2", "cũ=" + o3.previous + " mới=" + o3.resultText);
    run(x, ".uno:Undo");
    result(C, "wire:.uno:Undo hoàn tác lần ghi đè", trim(range(x, "P2")) == "1", "P2=" + trim(range(x, "P2")));
    gotoCell(x, "P2:P3");
    t = trim(dataassist::bridgeSelectedText(x));
    result(C, "wire:lấy vùng đang chọn (TSV)", t.find('1') != std::string::npos && t.find('2') != std::string::npos, t);

    // Ô đích riêng: kiểm tra chuẩn hoá địa chỉ và không ghi khi hủy hoặc địa chỉ không hợp lệ.
    result(C, "wire:địa chỉ A1 hợp lệ", dataassist::bridgeCellAddress("$xFd$1048576") == "XFD1048576" &&
        dataassist::bridgeCellAddress("A1:B3").empty() && dataassist::bridgeCellAddress("XFE1").empty() &&
        dataassist::bridgeCellAddress("A1048577").empty() && dataassist::bridgeCellAddress("Sheet2.A1").empty(), "");
    gotoCell(x, "P2");
    result(C, "wire:địa chỉ vùng hợp lệ", dataassist::bridgeRangeAddress("$a$1:$b$3") == "A1:B3" &&
        dataassist::bridgeRangeAddress("A1:A1") == "A1" &&
        dataassist::bridgeRangeAddress("A1:XFE3").empty() && dataassist::bridgeRangeAddress("A1:B2:C3").empty(), "");
    result(C, "wire:formula bar requires matching single-cell address",
        dataassist::bridgeConfirmedCell("$p$2","P2")=="P2" &&
        dataassist::bridgeConfirmedCell("P2","P3").empty() &&
        dataassist::bridgeConfirmedCell("P2:P3","P2").empty() &&
        dataassist::bridgeConfirmedCell("P2","").empty() &&
        dataassist::bridgeConfirmedCell("XFE1","XFE1").empty(), "");
    result(C, "wire:name box normalizes safe local ranges",
        dataassist::bridgeRangeAddress(" $b$12 ")=="B12" &&
        dataassist::bridgeRangeAddress("A1:C5")=="A1:C5" &&
        dataassist::bridgeRangeAddress("Sheet2.A1").empty() &&
        dataassist::bridgeRangeAddress("A0").empty() &&
        dataassist::bridgeRangeAddress("A1;B2").empty(), "");
    auto o4 = dataassist::bridgeInsertFormulaAt(x, "P4", "=SUM(P2:P3)", LOK_CALLBACK_CELL_ADDRESS,
        [](const std::string&) { return true; });
    result(C, "wire:chèn vào ô đích riêng", o4.ok && trim(range(x, "P4")) == "3", o4.message);
    gotoCell(x, "P3");
    auto o5 = dataassist::bridgeInsertFormulaAt(x, "P2", "=999", LOK_CALLBACK_CELL_ADDRESS,
        [](const std::string&) { return false; });
    result(C, "wire:hủy ô đích không làm mất dữ liệu", o5.declined && !o5.ok && trim(range(x, "P2")) == "1", o5.message);
    auto o6 = dataassist::bridgeInsertFormulaAt(x, "P2:P3", "=999", LOK_CALLBACK_CELL_ADDRESS,
        [](const std::string&) { return true; });
    result(C, "wire:từ chối vùng làm ô đích", !o6.ok && trim(range(x, "P2")) == "1", o6.message);

    gotoCell(x, "P3");
    auto changed = dataassist::bridgeInsertFormulaAt(x, "P2", "=999", LOK_CALLBACK_CELL_ADDRESS,
        [&](const std::string&) { enter(x, "P2", u"17"); return true; });
    result(C, "wire:dữ liệu ô đích đổi trong lúc xác nhận → không ghi đè", !changed.ok && trim(range(x, "P2")) == "17", changed.message);
    enter(x, "P2", u"1");
    gotoCell(x, "P3");
    auto moved = dataassist::bridgeInsertFormulaAt(x, "P2", "=999", LOK_CALLBACK_CELL_ADDRESS,
        [&](const std::string&) { gotoCell(x, "P3"); return true; });
    result(C, "wire:đổi ô trong lúc xác nhận → không ghi vào ô khác", !moved.ok &&
        trim(range(x, "P2")) == "1" && trim(range(x, "P3")) == "2", moved.message);

    step("thăm dò hàm mới");
    {
        enter(x, "R1", u"a"); enter(x, "R2", u"b"); enter(x, "R3", u"a");
        enter(x, "S1", u"10"); enter(x, "S2", u"20"); enter(x, "S3", u"30");
        struct Fn { const char* name; const char* f; const char* cell; };
        const Fn fns[] = {
            {"XLOOKUP", "=XLOOKUP(\"b\",R1:R3,S1:S3)", "T1"},
            {"FILTER", "=FILTER(S1:S3,S1:S3>15)", "T2"},
            {"SORT", "=SORT(S1:S3,1,-1)", "T4"},
            {"UNIQUE", "=UNIQUE(R1:R3)", "T7"},
            {"INDEX/MATCH(thay XLOOKUP)", "=INDEX(S1:S3,MATCH(\"b\",R1:R3,0))", "T10"},
            {"VLOOKUP", "=VLOOKUP(\"b\",R1:S3,2,0)", "T11"},
        };
        for (const auto& fn : fns) {
            gotoCell(x, fn.cell);
            auto r = dataassist::bridgeInsertFormula(x, fn.f, [](const std::string&) { return true; });
            const bool good = r.ok && !r.resultText.empty() && !dataassist::bridgeLooksLikeError(r.resultText);
            const std::string nm = std::string("hàm ") + fn.name;
            if (std::string(fn.name).find("INDEX") == 0 || std::string(fn.name) == "VLOOKUP") result(C, nm, good, "kết quả=" + r.resultText);
            else probe(C, nm, good, "kết quả=" + r.resultText);
        }
    }

    step("điền (thay tay nắm điền)");
    enter(x, "V1", u"7");
    gotoCell(x, "V1:V4");
    run(x, ".uno:FillDown");
    t = trim(range(x, "V1:V4"));
    int sevens = 0; for (char ch : t) if (ch == '7') ++sevens;
    probe(C, ".uno:FillDown", sevens == 4, t);
    enter(x, "W1", u"5");
    gotoCell(x, "W1:Y1");
    run(x, ".uno:FillRight");
    t = trim(range(x, "W1:Y1"));
    sevens = 0; for (char ch : t) if (ch == '5') ++sevens;
    probe(C, ".uno:FillRight", sevens == 3, t);
    enter(x, "Z1", u"1"); enter(x, "Z2", u"2");
    gotoCell(x, "Z1:Z5");
    run(x, ".uno:FillSeries", "{\"FillDir\":{\"type\":\"unsigned short\",\"value\":\"0\"},\"FillCmd\":{\"type\":\"unsigned short\",\"value\":\"1\"},\"FillStep\":{\"type\":\"string\",\"value\":\"1\"}}");
    t = trim(range(x, "Z1:Z5"));
    probe(C, ".uno:FillSeries", t.find('5') != std::string::npos, t);

    xl.reset();

    // ============================== WRITER ==============================
    step("tạo văn bản mới");
    auto wr = engine->createNew(DocKind::Text, &err);
    if (!wr) { std::cout << "[LỖI]  Tạo văn bản " << err << "\n"; return 1; }
    Document& d = *wr;
    d.setEventSink(onEvent);
    d.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
    settle();
    const std::string W = "Writer";

    step("về đầu văn bản");
    d.typeText(u"dong dau");
    d.postKey(LOK_KEYEVENT_KEYINPUT, 13, 1280); d.postKey(LOK_KEYEVENT_KEYUP, 13, 1280);
    d.typeText(u"dong hai");
    settle();
    run(d, ".uno:GoToStartOfDoc");
    d.typeText(u"Z");   // gõ tại con trỏ: nếu đã về đầu văn bản, "Z" đứng đầu
    settle();
    run(d, ".uno:SelectAll");
    t = trim(text(d));
    result(W, ".uno:GoToStartOfDoc", t.rfind("Z", 0) == 0, t);

    step("số trang");
    run(d, ".uno:GoToEndOfDoc");
    d.postKey(LOK_KEYEVENT_KEYINPUT, 13, 1280); d.postKey(LOK_KEYEVENT_KEYUP, 13, 1280);
    settle();
    run(d, ".uno:InsertPageNumberField");
    run(d, ".uno:SelectAll");
    h = html(d);
    t = trim(text(d));
    result(W, ".uno:InsertPageNumberField", t.find("dong hai") != std::string::npos && (t.find('1') != std::string::npos || has(h, "page")),
           t + " | html=" + flat(h, 60));

    step("đổi hoa/thường");
    {
        struct Cs { const char* uno; const char* want; };
        const Cs css[] = {
            {".uno:ChangeCaseToUpper", "HELLO WORLD. XIN CHAO"},
            {".uno:ChangeCaseToLower", "hello world. xin chao"},
            {".uno:ChangeCaseToSentenceCase", "Hello world. Xin chao"},
            {".uno:ChangeCaseToTitleCase", "Hello World. Xin Chao"},
            {".uno:ChangeCaseToToggleCase", "hELLO WORLD. xIN CHAO"},   // "tOGGLE cASE" của engine: đảo theo từ (đã quan sát trên CI)
        };
        for (const auto& c : css) {
            run(d, ".uno:SelectAll");
            run(d, ".uno:Delete");
            d.typeText(u"hello world. xin chao");
            settle();
            run(d, ".uno:SelectAll");
            run(d, c.uno);
            run(d, ".uno:SelectAll");
            const std::string tt = trim(text(d));
            probe(W, c.uno, tt == c.want, tt);
        }
    }

    step("ký hiệu đặc biệt");
    run(d, ".uno:SelectAll"); run(d, ".uno:Delete");
    d.typeText(u"\u00A9");
    settle();
    run(d, ".uno:SelectAll");
    t = trim(text(d));
    probe(W, "typeText(©)", t == "\xC2\xA9", t);
    run(d, ".uno:SelectAll"); run(d, ".uno:Delete");
    run(d, ".uno:InsertSymbol", "{\"Symbols\":{\"type\":\"string\",\"value\":\"\xE2\x82\xAC\"},\"FontName\":{\"type\":\"string\",\"value\":\"Liberation Serif\"}}");
    run(d, ".uno:SelectAll");
    t = trim(text(d));
    probe(W, ".uno:InsertSymbol(€)", t.find("\xE2\x82\xAC") != std::string::npos, t);

    step("hướng giấy");
    {
        long w0 = 0, h0 = 0, w1 = 0, h1 = 0;
        d.documentSize(w0, h0);
        const char* tries[] = {
            "{\"AttributePageSize.Width\":{\"type\":\"long\",\"value\":\"16838\"},\"AttributePageSize.Height\":{\"type\":\"long\",\"value\":\"11906\"}}",
            "{\"Width\":{\"type\":\"long\",\"value\":\"16838\"},\"Height\":{\"type\":\"long\",\"value\":\"11906\"}}",
        };
        bool done = false; std::string which;
        for (const char* a : tries) {
            d.command(".uno:AttributePageSize", a); settle(600);
            d.documentSize(w1, h1);
            if (w1 != w0) { done = true; which = a; break; }
        }
        if (!done) { d.command(".uno:Orientation"); settle(600); d.documentSize(w1, h1); done = (w1 != w0); which = ".uno:Orientation"; }
        probe(W, "hướng giấy ngang (UNO)", done, "rộng " + std::to_string(w0) + " -> " + std::to_string(w1) + " bằng " + (done ? which : std::string("không lệnh nào")));
    }

    wr.reset();
    unchecked("Impress", "đổi chủ đề/bố cục chủ", "không có lệnh UNO nhận tham số trên engine này; cần kiểm bằng lưu .pptx rồi đọc XML");
    unchecked("Impress", "chuyển cảnh", "hiệu ứng chỉ quan sát được qua XML <p:transition> sau khi lưu; chưa kiểm");
    unchecked("Impress", "ghi chú diễn giả", "cần gõ ở chế độ trang ghi chú rồi đọc notesSlide trong .pptx; chưa kiểm");

    if(!pivotFixture.empty()){
        step("PivotTable native — đổi nguồn và cập nhật bảng có sẵn");
        auto pivot=engine->open(pivotFixture,&err);
        result("Calc","pivot-open",bool(pivot),err);
        if(pivot){
            pivot->setClientView(512,512,7680,7680,0,0,7680,7680);
            pivot->setPart(0);
            gotoCell(*pivot,"D2");
            const auto old=trim(text(*pivot));
            enter(*pivot,"D2",u"225000");
            gotoCell(*pivot,"D2");
            const auto changed=trim(text(*pivot));
            result("Calc","pivot-source-edit",old!=changed,old+" -> "+changed);
            gotoCell(*pivot,"G4");
            run(*pivot,".uno:RecalcPivotTable");
            settle(1000);
            const fs::path dest=out/"native-pivot.xlsx";
            fs::remove(dest,ec);
            const auto saved=pivot->saveAs(dest);
            result("Calc","pivot-save",saved.ok,saved.message);
            if(saved.ok){
                pivot.reset();
                auto reopened=engine->open(dest,&err);
                result("Calc","pivot-reopen",bool(reopened),err);
                if(reopened){
                    reopened->setClientView(512,512,7680,7680,0,0,7680,7680);
                    reopened->setPart(0);
                    enter(*reopened,"D2",u"325000");
                    gotoCell(*reopened,"D2");
                    result("Calc","pivot-second-source-edit",trim(text(*reopened))=="325000",trim(text(*reopened)));
                    gotoCell(*reopened,"G4");
                    run(*reopened,".uno:RecalcPivotTable");
                    settle(1000);
                    const fs::path roundtrip=out/"native-pivot-roundtrip.xlsx";
                    fs::remove(roundtrip,ec);
                    const auto again=reopened->saveAs(roundtrip);
                    result("Calc","pivot-roundtrip-save",again.ok,again.message);
                    if(again.ok){
                        // Move the first source row to another existing group.
                        // A stale grouping cache can retain the correct grand total
                        // while assigning the amount to the wrong category.
                        reopened.reset();
                        auto regroup=engine->open(roundtrip,&err);
                        result("Calc","pivot-regroup-reopen",bool(regroup),err);
                        if(regroup){
                            regroup->setClientView(512,512,7680,7680,0,0,7680,7680);
                            regroup->setPart(0);
                            enter(*regroup,"B2",u"Mặt hàng 2");
                            gotoCell(*regroup,"B2");
                            result("Calc","pivot-category-edit",trim(text(*regroup))=="Mặt hàng 2",trim(text(*regroup)));
                            gotoCell(*regroup,"G4");
                            run(*regroup,".uno:RecalcPivotTable");
                            settle(1000);
                            const auto target=out/"native-pivot-regroup.xlsx";
                            fs::remove(target,ec);
                            const auto finalSave=regroup->saveAs(target);
                            result("Calc","pivot-regroup-save",finalSave.ok,finalSave.message);
                        }
                    }
                }
            }
        }
    }
    step("chèn biểu đồ native vào sheet hiện tại");
    {
        auto xc=engine->createNew(DocKind::Spreadsheet,&err);
        if(!xc)result("Calc","chart-create-document",false,err);
        else {
            Document& c=*xc;
            c.setEventSink(onEvent);
            c.setClientView(512,512,7680,7680,0,0,7680,7680);
            settle();
            enter(c,"A1",u"Category");enter(c,"B1",u"Amount");
            enter(c,"A2",u"Alpha");enter(c,"B2",u"12");
            enter(c,"A3",u"Beta");enter(c,"B3",u"34");
            gotoCell(c,"A1:B3");
            const auto selected=text(c);
            const bool sourceOk=has(selected,"Alpha")&&has(selected,"Beta")&&has(selected,"12")&&has(selected,"34");
            result("Calc","chart-selection",sourceOk,selected);
            if(sourceOk){
                run(c,".uno:InsertObjectChart");
                settle(1500);
                // Exit chart object editing so the workbook can be saved.
                c.postKey(LOK_KEYEVENT_KEYINPUT,0,1281);
                c.postKey(LOK_KEYEVENT_KEYUP,0,1281);
                settle(500);
                const fs::path dest=out/"native-chart.xlsx";
                fs::remove(dest,ec);
                const auto saved=c.saveAs(dest);
                result("Calc","chart-save",saved.ok,saved.message);
                // Reopen through the same engine and change a linked value before saving
                // again. The independent OOXML checker verifies the actual data and chart.
                if(saved.ok){
                    xc.reset();
                    auto reopened=engine->open(dest,&err);
                    result("Calc","chart-reopen",bool(reopened),err);
                    if(reopened){
                        reopened->setPart(0);
                        reopened->setClientView(512,512,7680,7680,0,0,7680,7680);
                        enter(*reopened,"B3",u"56");
                        settle(1000);
                        const fs::path roundtrip=out/"native-chart-roundtrip.xlsx";
                        fs::remove(roundtrip,ec);
                        const auto again=reopened->saveAs(roundtrip);
                        result("Calc","chart-roundtrip-save",again.ok,again.message);
                    }
                }
                // Independent checker validates object/relationship/data references.
            }
        }
    }

    // This fixture is generated by the CI checker with no external data.
    // Exercise an allowed edit in an existing protected worksheet. Do not
    // invoke native password/validation dialogs until dialog transport works.
    const auto validationFixture=out/"validation-protection-input.xlsx";
    if(fs::exists(validationFixture)){
        step("bảo toàn dropdown và bảo vệ sheet khi sửa ô mở khóa");
        // Separate pristine open/save from keyboard editing. Both must retain
        // the input rule; a baseline failure identifies a filter/load issue.
        {
            auto pristine=engine->open(validationFixture,&err);
            result(C,"validation-baseline-open",bool(pristine),err);
            if(pristine){
                const auto dest=out/"validation-protection-baseline.xlsx";
                fs::remove(dest,ec);
                const auto saved=pristine->saveAs(dest);
                result(C,"validation-baseline-save",saved.ok,saved.message);
            }
        }
        auto vd=engine->open(validationFixture,&err);
        result(C,"validation-protection-open",bool(vd),err);
        if(vd){
            vd->setClientView(512,512,7680,7680,0,0,7680,7680);
            vd->setPart(0);vd->waitInputIdle();
            const bool editable=!vd->readOnly();
            result(C,"protected-workbook-editable",editable,"sheet protection is not document read-only");
            if(editable){
                enter(*vd,"B2",u"Chờ");
                vd->waitInputIdle();
                const auto value=trim(range(*vd,"B2"));
                result(C,"protected-unlocked-cell-edit",value=="Chờ",value);
                const auto dest=out/"validation-protection-output.xlsx";
                fs::remove(dest,ec);
                const auto saved=vd->saveAs(dest);
                result(C,"validation-protection-save",saved.ok,saved.message);
            }
        }
    }else unchecked(C,"validation-protection","fixture not supplied outside CI");
    std::cout << "\nKẾT QUẢ QUÉT LỆNH: " << (g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT") << " (" << g_fail << " lỗi)\n";
    std::cout.flush();
    std::_Exit(g_fail ? 1 : 0);   // bỏ qua huỷ engine (treo/abort khi thoát)
}
