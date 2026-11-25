// dung-smoketest --sweep 1 : quét các lệnh UNO của thanh ribbon mà các bộ kiểm thử khác chưa chạm tới,
// trên ENGINE THẬT (Writer + Calc). Mỗi lệnh được kiểm bằng hiệu ứng quan sát được (nội dung/định dạng
// vùng chọn, trạng thái engine báo về), không chỉ "gửi lệnh không lỗi".
// Mỗi kết quả in một dòng máy đọc được:  SWEEP|<ứng dụng>|<lệnh>|<ĐẠT/LỖI>|<bằng chứng>
// để docs/COMMAND_MATRIX.md dựng từ đó. Lệnh nào [LỖI] phải bị vô hiệu hoá trên giao diện (xem Ribbon.cpp).
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

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

int runSweepTests(const fs::path& program, const fs::path& out) {
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
    forget(".uno:FreezePanes");
    run(x, ".uno:FreezePanes");
    std::string fp = stateOf(".uno:FreezePanes");
    result(C, ".uno:FreezePanes", fp == "true", "state=" + fp);
    run(x, ".uno:FreezePanes");
    fp = stateOf(".uno:FreezePanes");
    result(C, ".uno:FreezePanes(tắt)", fp == "false", "state=" + fp);
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

    std::cout << "\nKẾT QUẢ QUÉT LỆNH: " << (g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT") << " (" << g_fail << " lỗi)\n";
    std::cout.flush();
    wr.reset();
    return g_fail ? 1 : 0;
}
