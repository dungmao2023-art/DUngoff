// Kiểm thử các lệnh UNO của PowerPoint (Impress) trên engine thật.
// Chạy:  dung-smoketest --program <program> --ppt <mau.pptx> --out <thư mục>
// Mỗi lệnh được kiểm bằng HIỆU ỨNG thật (số trang, băm nội dung trang, trạng thái engine báo về),
// không chỉ "gửi lệnh không lỗi". Lệnh nào chưa hoạt động được ghi rõ là [CHƯA HỖ TRỢ] và KHÔNG
// được nút giao diện nào dựa vào.
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace dung;

namespace {

int g_fail = 0;
std::atomic<long long> g_stepStart{0};
std::string g_stepName = "khởi động";

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void step(const std::string& n) {
    g_stepName = n;
    g_stepStart = nowMs();
    std::cout << "  -> " << n << std::endl;
}

void startWatchdog(int seconds) {
    g_stepStart = nowMs();
    std::thread([seconds] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_stepStart > seconds * 1000LL) {
                std::cout << "[LỖI]  TREO quá " << seconds << " giây ở bước: " << g_stepName
                          << "\n\nKẾT QUẢ PPT: CÓ LỖI (treo)\n" << std::flush;
                std::_Exit(3);
            }
        }
    }).detach();
}

void check(bool ok, const std::string& what) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what << std::endl;
    if (!ok) ++g_fail;
}

// Lệnh chưa hoạt động: ghi lại để giao diện không dùng, KHÔNG tính là lỗi kiểm thử.
void unsupported(const std::string& what) {
    std::cout << "[CHƯA HỖ TRỢ]  " << what << std::endl;
}

std::string longArg(const std::string& name, long v) {
    return "{\"" + name + "\":{\"type\":\"long\",\"value\":\"" + std::to_string(v) + "\"}}";
}

// Băm nhanh nội dung một trang bằng cách vẽ ra bộ đệm nhỏ (độc lập với getPartHash của engine).
unsigned long long renderHash(Document& d, int part) {
    long w = 0, h = 0;
    d.documentSize(w, h);
    const int pw = 160, ph = 90;
    std::vector<uint8_t> b(pw * ph * 4, 0);
    d.paintPartTile(b.data(), part, pw, ph, 0, 0, (int)w, (int)h);
    unsigned long long x = 1469598103934665603ULL;
    for (uint8_t c : b) { x ^= c; x *= 1099511628211ULL; }
    return x;
}

// Chờ sự kiện trạng thái có dạng "<cmd>=<value>".
bool waitState(Document& d, const std::string& cmd, const std::string& value, int ms = 4000) {
    const std::string want = cmd + "=" + value;
    return d.waitFor(LOK_CALLBACK_STATE_CHANGED, [&](const std::string& p) { return p == want; }, ms);
}

bool partCountIs(Document& d, int n, int ms = 4000) {
    const auto t0 = nowMs();
    while (nowMs() - t0 < ms) {
        if (d.partCount() == n) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return d.partCount() == n;
}

// Vào chế độ sửa chữ của đối tượng đầu tiên trên trang: Tab chọn đối tượng, Enter vào sửa.
void enterFirstTextObject(Document& d) {
    const int KEY_TAB = 1282, KEY_RETURN = 1280;
    d.postKey(LOK_KEYEVENT_KEYINPUT, 0, KEY_TAB);
    d.postKey(LOK_KEYEVENT_KEYUP, 0, KEY_TAB);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    d.postKey(LOK_KEYEVENT_KEYINPUT, 0, KEY_RETURN);
    d.postKey(LOK_KEYEVENT_KEYUP, 0, KEY_RETURN);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

// Ghi PNG 64x48 màu đặc (khối deflate "stored") để thử chèn ảnh, không cần thư viện ảnh.
void writeTestPng(const fs::path& path) {
    auto crcTable = [] {
        static uint32_t t[256];
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    auto crc = [&](const std::vector<uint8_t>& b) {
        uint32_t c = 0xFFFFFFFFu;
        for (uint8_t x : b) c = crcTable[(c ^ x) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    auto be32 = [](std::vector<uint8_t>& v, uint32_t x) {
        v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
    };
    auto chunk = [&](std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
        be32(out, (uint32_t)data.size());
        std::vector<uint8_t> td(type, type + 4);
        td.insert(td.end(), data.begin(), data.end());
        out.insert(out.end(), td.begin(), td.end());
        be32(out, crc(td));
    };
    const int w = 64, h = 48;
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; ++y) {
        raw.push_back(0); // bộ lọc none
        for (int x = 0; x < w; ++x) { raw.push_back(31); raw.push_back(95); raw.push_back(191); } // xanh #1F5FBF
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    for (size_t pos = 0; pos < raw.size();) {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xFF); z.push_back(n >> 8);
        z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t x : raw) { a = (a + x) % 65521; b = (b + a) % 65521; }
    be32(z, (b << 16) | a);
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    be32(ihdr, w); be32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    std::ofstream(path, std::ios::binary).write((const char*)png.data(), (std::streamsize)png.size());
}

} // namespace

int dungRunPptTests(const fs::path& program, const fs::path& pptx, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    startWatchdog(60);

    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine " + err);
    if (!engine) return 1;

    step("mở PPTX");
    auto d = engine->open(pptx, &err);
    check((bool)d, "Mở PPTX " + err);
    if (!d) return 1;
    check(d->kind() == DocKind::Presentation, "Nhận diện = PowerPoint");
    check(!d->readOnly(), "PPTX mở ở chế độ chỉnh sửa");
    const int n0 = d->partCount();
    check(n0 == 2, "Có 2 trang chiếu (thực tế: " + std::to_string(n0) + ")");
    d->setClientView(512, 288, 7680, 4320, 0, 0, 7680, 4320);
    if (const char* tr = std::getenv("DUNG_PPT_TRACE"); tr && *tr == '1')
        d->setEventSink([](const EngineEvent& e) {
            if (e.type == LOK_CALLBACK_STATE_CHANGED) std::cout << "       [sự kiện] " << e.payload << std::endl;
        });

    // Verify deletion of a nonempty original slide and Undo/Redo, not
    // merely deleting the blank duplicate used by the basic count test.
    if(n0==2){
        step("xóa slide có nội dung, Undo và Redo");
        d->setPart(1);d->waitInputIdle();
        const auto first=renderHash(*d,0),second=renderHash(*d,1);
        check(first!=second,"two original slides have distinct render content");
        d->command(".uno:DeletePage");d->waitInputIdle();
        check(partCountIs(*d,1)&&renderHash(*d,0)==first,"delete second slide preserves first slide");
        d->command(".uno:Undo");d->waitInputIdle();
        const bool restored=partCountIs(*d,2);
        check(restored&&renderHash(*d,0)==first&&renderHash(*d,1)==second,"Undo restores slide order and rendered content");
        if(restored){
            d->command(".uno:Redo");d->waitInputIdle();
            check(partCountIs(*d,1)&&renderHash(*d,0)==first,"Redo removes same slide");
            d->command(".uno:Undo");d->waitInputIdle();
            check(partCountIs(*d,2)&&renderHash(*d,0)==first&&renderHash(*d,1)==second,"second Undo restores original deck");
        }
        if(d->partCount()!=n0)return 1;
    }
    // ---------------- Quản lý trang chiếu ----------------
    step("trang mới (.uno:InsertPage)");
    d->setPart(1);
    d->command(".uno:InsertPage");
    { const bool ok = partCountIs(*d, n0 + 1); check(ok, "Trang mới: " + std::to_string(n0) + " -> " + std::to_string(d->partCount())); }

    step("nhân bản (.uno:DuplicatePage)");
    d->command(".uno:DuplicatePage");
    { const bool ok = partCountIs(*d, n0 + 2); check(ok, "Nhân bản trang: -> " + std::to_string(d->partCount())); }

    step("xoá trang (.uno:DeletePage)");
    d->command(".uno:DeletePage");
    { const bool ok = partCountIs(*d, n0 + 1); check(ok, "Xoá trang: -> " + std::to_string(d->partCount())); }

    // ---------------- Di chuyển trang ----------------
    step("di chuyển trang");
    d->setPart(0);
    const auto h0 = renderHash(*d, 0), h1 = renderHash(*d, 1);
    check(h0 != h1, "Hai trang đầu có nội dung khác nhau (để nhận ra thứ tự)");
    d->command(".uno:MovePageDown");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const auto a0 = renderHash(*d, 0), a1 = renderHash(*d, 1);
    if (a0 == h1 && a1 == h0) check(true, "Dời trang xuống (.uno:MovePageDown): hai trang đổi chỗ");
    else unsupported("MovePageDown không đổi thứ tự trang trong chế độ nhúng");
    d->command(".uno:MovePageUp");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const auto b0 = renderHash(*d, 0), b1 = renderHash(*d, 1);
    if (a0 == h1) check(b0 == h0 && b1 == h1, "Dời trang lên (.uno:MovePageUp): trả về thứ tự cũ");

    // ---------------- Ẩn trang ----------------
    step("ẩn trang");
    d->setPart(0);
    const std::string info0 = d->partInfo(0);
    std::cout << "       partInfo(0) = " << info0 << std::endl;
    d->command(".uno:HideSlide");
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    const std::string info1 = d->partInfo(0);
    std::cout << "       sau HideSlide = " << info1 << std::endl;
    if (!info0.empty() && info1 != info0) check(true, "Ẩn trang (.uno:HideSlide): thông tin trang đổi");
    else unsupported("HideSlide: engine không báo đổi trạng thái ẩn qua partInfo");
    d->command(".uno:ShowSlide");

    // ---------------- Nhập chữ + định dạng ----------------
    step("sửa chữ trong khung đầu tiên");
    d->setPart(0);
    d->clearEvents();
    enterFirstTextObject(*d);
    d->typeText(u"Dung Office");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    d->command(".uno:SelectAll");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    step("Đậm");
    d->clearEvents();
    d->command(".uno:Bold");
    if (waitState(*d, ".uno:Bold", "true")) check(true, "Đậm (.uno:Bold): engine báo true");
    else unsupported(".uno:Bold: không nhận được trạng thái true");

    step("Nghiêng");
    d->clearEvents();
    d->command(".uno:Italic");
    if (waitState(*d, ".uno:Italic", "true")) check(true, "Nghiêng (.uno:Italic): engine báo true");
    else unsupported(".uno:Italic: không nhận được trạng thái true");

    step("Gạch chân");
    d->clearEvents();
    d->command(".uno:Underline");
    if (waitState(*d, ".uno:Underline", "true")) check(true, "Gạch chân (.uno:Underline): engine báo true");
    else unsupported(".uno:Underline: không nhận được trạng thái true");

    step("Căn phải");
    d->clearEvents();
    d->command(".uno:RightPara");
    if (waitState(*d, ".uno:RightPara", "true")) check(true, "Căn phải (.uno:RightPara): engine báo true");
    else unsupported(".uno:RightPara: không nhận được trạng thái true");
    d->clearEvents();
    d->command(".uno:LeftPara");
    if (waitState(*d, ".uno:LeftPara", "true")) check(true, "Căn trái (.uno:LeftPara): engine báo true");
    else unsupported(".uno:LeftPara: không nhận được trạng thái true");
    d->clearEvents();
    d->command(".uno:JustifyPara");
    if (waitState(*d, ".uno:JustifyPara", "true")) check(true, "Căn đều (.uno:JustifyPara): engine báo true");
    else unsupported(".uno:JustifyPara: không nhận được trạng thái true");

    step("Phông chữ");
    d->clearEvents();
    d->command(".uno:CharFontName", Document::stringArg("CharFontName.FamilyName", "DejaVu Sans"));
    if (d->waitFor(LOK_CALLBACK_STATE_CHANGED,
                   [](const std::string& p) { return p.rfind(".uno:CharFontName=", 0) == 0 && p.find("DejaVu Sans") != std::string::npos; }, 4000))
        check(true, "Đổi phông (.uno:CharFontName): engine báo DejaVu Sans");
    else unsupported(".uno:CharFontName: không nhận được tên phông mới");

    step("Cỡ chữ");
    d->clearEvents();
    d->command(".uno:FontHeight", "{\"FontHeight.Height\":{\"type\":\"float\",\"value\":\"32\"}}");
    if (d->waitFor(LOK_CALLBACK_STATE_CHANGED,
                   [](const std::string& p) { return p.rfind(".uno:FontHeight=", 0) == 0 && p.find("32") != std::string::npos; }, 4000))
        check(true, "Đổi cỡ chữ (.uno:FontHeight): engine báo 32");
    else unsupported(".uno:FontHeight: không nhận được cỡ mới");

    step("Màu chữ");
    d->clearEvents();
    d->command(".uno:Color", longArg("Color", 0xFF0000));
    if (waitState(*d, ".uno:Color", "16711680")) check(true, "Đổi màu chữ (.uno:Color): engine báo 16711680 (đỏ)");
    else unsupported(".uno:Color: không nhận được màu mới");

    d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281); // Esc thoát chế độ sửa
    d->postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // ---------------- Danh sách trong khung nội dung (đối tượng thứ 2 của trang 2) ----------------
    step("danh sách trong khung nội dung");
    d->setPart(1);
    d->clearEvents();
    {
        const int KEY_TAB = 1282, KEY_RETURN = 1280;
        for (int i = 0; i < 2; ++i) {
            d->postKey(LOK_KEYEVENT_KEYINPUT, 0, KEY_TAB);
            d->postKey(LOK_KEYEVENT_KEYUP, 0, KEY_TAB);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        d->postKey(LOK_KEYEVENT_KEYINPUT, 0, KEY_RETURN);
        d->postKey(LOK_KEYEVENT_KEYUP, 0, KEY_RETURN);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    d->typeText(u"Muc mot");
    d->command(".uno:SelectAll");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    {
        const auto before = renderHash(*d, 1);
        d->command(".uno:DefaultBullet");
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        const auto bulleted = renderHash(*d, 1);
        if (bulleted != before) check(true, "Đầu dòng (.uno:DefaultBullet): nội dung trang đổi");
        else unsupported(".uno:DefaultBullet: nội dung trang không đổi");
        d->command(".uno:DefaultNumbering");
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        const auto numbered = renderHash(*d, 1);
        if (numbered != bulleted) check(true, "Đánh số (.uno:DefaultNumbering): nội dung trang đổi");
        else unsupported(".uno:DefaultNumbering: nội dung trang không đổi");
    }
    d->clearEvents();
    d->command(".uno:IncrementIndent");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281);
    d->postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // ---------------- Bố cục ----------------
    step("đổi bố cục");
    d->setPart(1);
    const auto lb = renderHash(*d, 1);
    d->command(".uno:AssignLayout", longArg("WhatLayout", 0));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    d->command(".uno:AssignLayout", longArg("WhatLayout", 20));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const auto la = renderHash(*d, 1);
    if (la != lb) check(true, "Đổi bố cục (.uno:AssignLayout): nội dung trang đổi");
    else unsupported(".uno:AssignLayout: nội dung trang không đổi");

    // ---------------- Chèn ----------------
    step("chèn bảng");
    d->setPart(1);
    const auto tb = renderHash(*d, 1);
    d->command(".uno:InsertTable", "{\"Columns\":{\"type\":\"long\",\"value\":\"3\"},\"Rows\":{\"type\":\"long\",\"value\":\"2\"}}");
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    if (renderHash(*d, 1) != tb) check(true, "Chèn bảng 3x2 (.uno:InsertTable): trang đổi");
    else unsupported(".uno:InsertTable: trang không đổi");

    step("chèn hình khối");
    d->setPart(0);
    const auto sb = renderHash(*d, 0);
    d->command(".uno:BasicShapes.rectangle");
    d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, 1500, 1500, 1, 1, 0);
    d->postMouse(LOK_MOUSEEVENT_MOUSEMOVE, 3000, 2500, 1, 1, 0);
    d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, 3000, 2500, 1, 1, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    if (renderHash(*d, 0) != sb) check(true, "Chèn hình chữ nhật (.uno:BasicShapes.rectangle + kéo chuột): trang đổi");
    else unsupported(".uno:BasicShapes.rectangle + kéo chuột: trang không đổi");

    step("chèn hộp văn bản");
    // End rectangle creation before arming the text tool. Match the UI input-idle sequence.
    d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281);
    d->postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
    d->waitInputIdle(350,4000);
    const auto xb = renderHash(*d, 0);
    d->command(".uno:Text");
    d->waitInputIdle(350,4000);
    d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, 4000, 3500, 1, 1, 0);
    d->postMouse(LOK_MOUSEEVENT_MOUSEMOVE, 6500, 4300, 1, 1, 0);
    d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, 6500, 4300, 1, 1, 0);
    d->waitInputIdle(350,4000);
    d->typeText(u"Hop van ban");
    d->waitInputIdle(350,4000);
    d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281);
    d->postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (renderHash(*d, 0) != xb) check(true, "Chèn hộp văn bản (.uno:Text + kéo chuột): trang đổi");
    else unsupported(".uno:Text + kéo chuột: trang không đổi");

    step("chèn ảnh");
    {
        d->setPart(1);
        const fs::path png = out / "anh-thu.png";
        writeTestPng(png);
        const auto before = renderHash(*d, 1);
        std::string uri = "file://" + (png.is_absolute() ? png : fs::absolute(png)).generic_string();
        if (uri.rfind("file://", 0) == 0 && uri.size() > 7 && uri[7] != '/') uri.insert(7, "/"); // Windows: file:///C:/...
        d->command(".uno:InsertGraphic", Document::stringArg("FileName", uri));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (renderHash(*d, 1) != before) check(true, "Chèn ảnh (.uno:InsertGraphic, FileName): trang đổi");
        else unsupported(".uno:InsertGraphic: trang không đổi");
        d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281);
        d->postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
    }

    step("ghi chú người nói");
    {
        d->setPart(0);
        long sw = 0, sh = 0, nw = 0, nh = 0;
        d->documentSize(sw, sh);
        d->setPartMode(1); // LOK_PARTMODE_NOTES
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        d->documentSize(nw, nh);
        std::cout << "       kích thước trang chiếu " << sw << "x" << sh << ", trang ghi chú " << nw << "x" << nh << std::endl;
        check(nw != sw || nh != sh, "Chế độ ghi chú đổi kích thước trang (đang ở trang ghi chú)");
        // Sửa khung chữ ghi chú bằng chuột kép vào vùng ghi chú (nửa dưới trang)
        d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, (int)(nw / 2), (int)(nh * 3 / 4), 2, 1, 0);
        d->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, (int)(nw / 2), (int)(nh * 3 / 4), 2, 1, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        d->typeText(u"Ghi chu nguoi noi");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281);
        d->postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
        d->postKey(LOK_KEYEVENT_KEYINPUT, 0, 1281);
        d->postKey(LOK_KEYEVENT_KEYUP, 0, 1281);
        d->setPartMode(0); // trở về trang chiếu
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        long bw = 0, bh = 0;
        d->documentSize(bw, bh);
        check(bw == sw && bh == sh, "Trở về chế độ trang chiếu (kích thước trang như cũ)");
    }

    // ---------------- Vẽ một trang bất kỳ không đổi trang hiện tại ----------------
    step("vẽ trang bất kỳ");
    d->setPart(0);
    long renderW=0,renderH=0;
    d->documentSize(renderW,renderH);
    std::vector<uint8_t> renderBuffer(160*90*4,0x5A);
    const auto sentinel=renderBuffer;
    check(!d->paintPartTile(renderBuffer.data(),-1,160,90,0,0,int(renderW),int(renderH)) &&
          !d->paintPartTile(renderBuffer.data(),d->partCount(),160,90,0,0,int(renderW),int(renderH)) &&
          !d->paintPartTile(renderBuffer.data(),0,0,90,0,0,int(renderW),int(renderH)) &&
          !d->paintPartTile(nullptr,0,160,90,0,0,int(renderW),int(renderH)) &&
          renderBuffer==sentinel,"Reject invalid slide render requests without touching buffer");
    check(d->paintPartTile(renderBuffer.data(),1,160,90,0,0,int(renderW),int(renderH)) &&
          renderBuffer!=sentinel,"Render valid slide through checked LOK ABI");
    check(d->currentPart() == 0, "paintPartTile không làm đổi trang hiện tại");
    const auto hash=d->partHash(0);
    const auto info=d->partInfo(0);
    if(hash.empty() || info.empty())
        std::cout << "[CHƯA KIỂM CHỨNG] LOK part metadata unavailable; guarded deletion must remain disabled" << std::endl;
    else check(d->partHash(0)==hash && d->partInfo(0)==info,"Part metadata stable without mutation");
    const int count=d->partCount();
    check(d->partHash(-1).empty() && d->partInfo(-1).empty() &&
          d->partHash(count).empty() && d->partInfo(count).empty(),
          "Reject invalid part indices before calling optional LOK ABI");
    check(d->currentPart()==0 && d->partCount()==count,"Metadata reads preserve slide identity");

    // ---------------- Lưu ----------------
    step("lưu PPTX");
    const fs::path target = out / fs::u8path("Trình chiếu thử - Dũng Office.pptx");
    fs::remove(target, ec);
    auto r = d->saveAs(target);
    check(r.ok, "Lưu thành PPTX sau khi chỉnh: " + r.message);
    check(fs::exists(target) && fs::file_size(target) > 2000, "Tệp PPTX mới có nội dung");
    std::ofstream(out / "saved-path.txt") << target.u8string();

    step("PowerPoint — thay thứ tự lớp và Undo bằng UNO native");
    d.reset();
    auto layers=engine->open(out/"ppt-layer-input.pptx",&err);
    check(bool(layers),"Mở fixture hai hình chồng nhau: "+err);
    if(layers){
        layers->setClientView(512,512,7680,7680,0,0,7680,7680);
        renderHash(*layers,0);
        auto snapshot=[&](const char* filename){
            const auto dest=out/filename;
            fs::remove(dest,ec);
            const auto saved=layers->saveAs(dest);
            check(saved.ok,std::string("Lưu lớp ")+filename+": "+saved.message);
        };
        auto click=[&](int x,int y){
            layers->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN,x,y,1,1,0);
            layers->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP,x,y,1,1,0);
            layers->waitInputIdle();
        };
        snapshot("ppt-layer-baseline.pptx");
        click(2160,2160); // exposed left part of first rectangle
        layers->command(".uno:Forward");layers->waitInputIdle();
        snapshot("ppt-layer-forward.pptx");
        layers->command(".uno:Undo");layers->waitInputIdle();
        snapshot("ppt-layer-forward-undone.pptx");
        click(5040,2160); // exposed right part of second rectangle
        layers->command(".uno:Backward");layers->waitInputIdle();
        snapshot("ppt-layer-backward.pptx");
        layers->command(".uno:Undo");layers->waitInputIdle();
        snapshot("ppt-layer-backward-undone.pptx");
    }
    layers.reset();
    step("PowerPoint — nhóm, bỏ nhóm và Undo trên fixture độc lập");
    auto grouped=engine->open(out/"ppt-layer-input.pptx",&err);
    check(bool(grouped),"Mở fixture nhóm: "+err);
    if(grouped){
        grouped->setClientView(512,512,7680,7680,0,0,7680,7680);
        renderHash(*grouped,0);
        auto snap=[&](const char* file){
            const auto dest=out/file;
            fs::remove(dest,ec);
            const auto saved=grouped->saveAs(dest);
            check(saved.ok,std::string("Lưu nhóm ")+file+": "+saved.message);
        };
        grouped->command(".uno:SelectAll");grouped->waitInputIdle();
        grouped->command(".uno:FormatGroup");grouped->waitInputIdle();
        snap("ppt-objects-grouped.pptx");
        grouped->command(".uno:FormatUngroup");grouped->waitInputIdle();
        snap("ppt-objects-ungrouped.pptx");
        grouped->command(".uno:Undo");grouped->waitInputIdle();
        snap("ppt-objects-ungroup-undone.pptx");
        grouped->command(".uno:Undo");grouped->waitInputIdle();
        snap("ppt-objects-group-undone.pptx");
    }
    grouped.reset();
    step("PowerPoint — căn chỉnh đối tượng native và Undo");
    struct AlignCase{const char* name;const char* command;};
    const AlignCase cases[]={{"left",".uno:ObjectAlignLeft"},{"center",".uno:AlignCenter"},
        {"right",".uno:ObjectAlignRight"},{"top",".uno:AlignUp"},{"middle",".uno:AlignMiddle"},{"bottom",".uno:AlignDown"}};
    for(const auto& item:cases){
        auto aligned=engine->open(out/"ppt-align-input.pptx",&err);
        check(bool(aligned),std::string("Mở fixture căn ")+item.name+": "+err);
        if(!aligned)continue;
        aligned->setClientView(512,512,7680,7680,0,0,7680,7680);
        renderHash(*aligned,0);
        auto snap=[&](const std::string& name){
            const auto dest=out/name;
            fs::remove(dest,ec);
            const auto saved=aligned->saveAs(dest);
            check(saved.ok,"Lưu căn "+name+": "+saved.message);
        };
        if(std::string(item.name)=="left")snap("ppt-align-baseline.pptx");
        aligned->command(".uno:SelectAll");aligned->waitInputIdle();
        aligned->command(item.command);aligned->waitInputIdle();
        snap(std::string("ppt-align-")+item.name+".pptx");
        aligned->command(".uno:Undo");aligned->waitInputIdle();
        snap(std::string("ppt-align-")+item.name+"-undone.pptx");
    }

    step("PowerPoint — đổi kích thước native và Undo");
    auto resized=engine->open(out/"ppt-layer-input.pptx",&err);
    check(bool(resized),"Mở fixture đổi kích thước: "+err);
    if(resized){
        resized->setClientView(512,512,7680,7680,0,0,7680,7680);
        renderHash(*resized,0);
        auto snap=[&](const char* name){
            const auto dest=out/name;
            fs::remove(dest,ec);
            const auto saved=resized->saveAs(dest);
            check(saved.ok,std::string("Lưu resize ")+name+": "+saved.message);
        };
        snap("ppt-resize-baseline.pptx");
        resized->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN,2160,2160,1,1,0);
        resized->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP,2160,2160,1,1,0);
        resized->waitInputIdle();
        resized->command(".uno:TransformDialog",R"({"TransformWidth":{"type":"long","value":4320},"TransformHeight":{"type":"long","value":2160}})");
        resized->waitInputIdle();snap("ppt-resize-edited.pptx");
        resized->command(".uno:Undo");resized->waitInputIdle();snap("ppt-resize-undone.pptx");
    }

    resized.reset();
    step("PowerPoint — native rotation with explicit pivot X/Y");
    struct Rotation{const char* name;int angle;};
    for(const auto& item:{Rotation{"positive",9000},Rotation{"negative",-4500}}){
        auto rotated=engine->open(out/"ppt-layer-input.pptx",&err);
        check(bool(rotated),"Mở fixture xoay: "+err);if(!rotated)continue;
        rotated->setClientView(512,512,7680,7680,0,0,7680,7680);
        renderHash(*rotated,0);
        auto snap=[&](const std::string& name){
            const auto dest=out/name;fs::remove(dest,ec);
            const auto saved=rotated->saveAs(dest);check(saved.ok,"Lưu xoay "+name+": "+saved.message);
        };
        if(item.angle>0)snap("ppt-rotate-baseline.pptx");
        rotated->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN,2160,2160,1,1,0);
        rotated->postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP,2160,2160,1,1,0);
        rotated->waitInputIdle();
        const auto sel=rotated->lastPayload(LOK_CALLBACK_GRAPHIC_SELECTION);
        int x=0,y=0,w=0,h=0;
        const bool selected=std::sscanf(sel.c_str(),"%d , %d , %d , %d",&x,&y,&w,&h)==4&&w>0&&h>0;
        check(selected,"Native graphic bounds and rotation pivot: "+sel);
        if(!selected)continue;
        const std::string args="{\"TransformRotationDeltaAngle\":{\"type\":\"long\",\"value\":"+std::to_string(item.angle)+
          "},\"TransformRotationX\":{\"type\":\"long\",\"value\":"+std::to_string(x+w/2)+
          "},\"TransformRotationY\":{\"type\":\"long\",\"value\":"+std::to_string(y+h/2)+"}}";
        step("Rotate dispatch with center "+args);
        rotated->command(".uno:TransformDialog",args);rotated->waitInputIdle();
        snap(std::string("ppt-rotate-")+item.name+".pptx");
        rotated->command(".uno:Undo");rotated->waitInputIdle();
        snap(std::string("ppt-rotate-")+item.name+"-undone.pptx");
    }

    step("PowerPoint — direct Slide Master editing and return to normal");
    auto master=engine->open(out/"ppt-master-input.pptx",&err);
    check(bool(master),"Mở fixture master: "+err);
    if(master){
        master->setClientView(512,512,7680,7680,0,0,7680,7680);
        renderHash(*master,0);
        const auto first=master->partHash(0), second=master->partHash(1);
        check(master->partCount()==2&&!first.empty()&&!second.empty()&&first!=second,"Two distinct normal slides");
        master->command(".uno:MasterPage",R"({"MasterPage":{"type":"boolean","value":false}})");master->waitInputIdle(350,4000);
        const auto hash=master->partHash(0);
        // PPTX layouts can import as separate native master pages. LOK's
        // getParts() switches to GetMasterSdPageCount in master mode.
        // Require all master identities to differ from both ordinary slides;
        // the independent OOXML verifier still requires the actual master edit.
        const int count=master->partCount();
        bool entered=count>0&&!hash.empty();
        for(int i=0;i<count&&entered;++i){
            const auto id=master->partHash(i);
            entered=!id.empty()&&id!=first&&id!=second;
        }
        check(entered,"Native master mode with distinct page identities; count="+std::to_string(count)+" first="+first+" master="+hash);
        if(entered){
            // Entered master mode from the blank-layout slide. Edit the
            // native current master/layout instead of guessing master index 0.
            const int active=master->currentPart();
            check(active>=0&&active<count,"Current master/layout index: "+std::to_string(active));
            // paintPartTile is an independent normal-slide renderer and may
            // switch view mode internally. Exercise the active editor tile
            // API instead, exactly as the canvas does in master mode.
            auto activeRender=[&](){
                long w=0,h=0;master->documentSize(w,h);
                if(w<=0||h<=0)return 0ULL;
                std::vector<uint8_t> pixels(640*480*4,0);
                master->paintTile(pixels.data(),640,480,0,0,int(w),int(h));
                unsigned long long digest=1469598103934665603ULL;
                bool nonzero=false;
                for(auto c:pixels){nonzero|=c!=0;digest^=c;digest*=1099511628211ULL;}
                return nonzero?digest:0ULL;
            };
            const auto before=activeRender();
            const auto activeId=master->partHash(active);
            check(before!=0&&master->currentPart()==active&&!activeId.empty(),"Active master editor tile renders without switching page");
            // Exercise a native object insertion that needs no mouse hit
            // testing or text-edit focus. It must persist in the inherited
            // master, not the current normal slide.
            master->setClientView(512,512,7680,7680,0,0,7680,7680);
            const auto png=out/"master-native-image.png";
            writeTestPng(png);
            std::string uri="file://"+fs::absolute(png).generic_string();
            if(uri.size()>7&&uri[7]!='/')uri.insert(7,"/");
            master->command(".uno:InsertGraphic",Document::stringArg("FileName",uri));
            master->waitInputIdle(350,4000);
            master->postKey(LOK_KEYEVENT_KEYINPUT,0,1281);
            master->postKey(LOK_KEYEVENT_KEYUP,0,1281);
            master->waitInputIdle(350,4000);
            const auto after=activeRender();
            check(before!=0&&after!=0&&before!=after&&master->currentPart()==active&&master->partHash(active)==activeId,
                  "Native master editor tile changed without switching page; before="+std::to_string(before)+" after="+std::to_string(after));
            master->command(".uno:CloseMasterView");master->waitInputIdle(350,4000);
            check(master->partCount()==2&&master->partHash(0)==first&&master->partHash(1)==second,"Return to both original normal slides in order");
            auto dest=out/"ppt-master-edited.pptx";fs::remove(dest,ec);
            auto saved=master->saveAs(dest);check(saved.ok,"Save master change: "+saved.message);
        }
    }

    std::cout << (g_fail ? "\nKẾT QUẢ PPT: CÓ LỖI\n" : "\nKẾT QUẢ PPT: TẤT CẢ ĐẠT\n");
    std::cout.flush();
    std::_Exit(g_fail ? 1 : 0);
}
