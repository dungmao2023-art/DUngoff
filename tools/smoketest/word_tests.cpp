// dung-smoketest --word 1 : kiểm thử các lệnh Word (Writer) trên engine thật.
// Mỗi lệnh được kiểm chứng bằng chứng cứ cụ thể (trạng thái engine báo về, HTML của vùng chọn,
// kích thước tài liệu...). Không đoán tên lệnh: lệnh nào lỗi sẽ hiện [LỖI] kèm dữ liệu quan sát.
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"
#include "../../app/src/word/WordCommands.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

using namespace dung;
namespace w = dung::word;

namespace {
int g_fail = 0;
std::mutex g_mu;
std::map<std::string, std::string> g_states;   // trạng thái lệnh do engine báo (STATE_CHANGED)
std::set<std::string> g_seen;                   // mọi tên lệnh engine đã từng báo trạng thái
std::atomic<long long> g_stepStart{0};
std::string g_step = "khởi động";

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void step(const std::string& s) {
    { std::lock_guard<std::mutex> g(g_mu); g_step = s; }
    g_stepStart = nowMs();
    std::cout << "  -> " << s << std::endl;
}
void check(bool ok, const std::string& what, const std::string& evidence = "") {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what;
    if (!evidence.empty()) std::cout << "  {" << evidence << "}";
    std::cout << std::endl;
    if (!ok) ++g_fail;
}
void watchdog(int seconds) {
    g_stepStart = nowMs();
    std::thread([seconds] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_stepStart > seconds * 1000LL) {
                std::lock_guard<std::mutex> g(g_mu);
                std::cout << "[LỖI]  TREO quá " << seconds << " giây ở bước: " << g_step << "\n\nKẾT QUẢ WORD: CÓ LỖI (treo)\n";
                std::cout.flush();
                std::_Exit(3);
            }
        }
    }).detach();
}

// ---- theo dõi trạng thái lệnh ----
void onEvent(const EngineEvent& ev) {
    if (ev.type != LOK_CALLBACK_STATE_CHANGED) return;
    std::string cmd, val;
    const auto eq = ev.payload.find('=');
    if (!ev.payload.empty() && ev.payload[0] == '{') {
        // JSON: tìm "commandName":"..." và "state":...
        auto grab = [&](const char* key) {
            const auto p = ev.payload.find(std::string("\"") + key + "\"");
            if (p == std::string::npos) return std::string();
            auto c = ev.payload.find(':', p);
            if (c == std::string::npos) return std::string();
            ++c;
            while (c < ev.payload.size() && ev.payload[c] == ' ') ++c;
            if (c < ev.payload.size() && ev.payload[c] == '"') {
                auto e = ev.payload.find('"', c + 1);
                return ev.payload.substr(c + 1, e == std::string::npos ? std::string::npos : e - c - 1);
            }
            auto e = ev.payload.find_first_of(",}", c);
            return ev.payload.substr(c, e == std::string::npos ? std::string::npos : e - c);
        };
        cmd = grab("commandName");
        val = grab("state");
    } else if (eq != std::string::npos) {
        cmd = ev.payload.substr(0, eq);
        val = ev.payload.substr(eq + 1);
    }
    if (cmd.empty()) return;
    std::lock_guard<std::mutex> g(g_mu);
    g_states[cmd] = val;
    g_seen.insert(cmd);
}
void forgetState(const std::string& cmd) { std::lock_guard<std::mutex> g(g_mu); g_states.erase(cmd); }
bool waitState(const std::string& cmd, const std::function<bool(const std::string&)>& pred, int ms, std::string* got = nullptr) {
    const auto deadline = nowMs() + ms;
    for (;;) {
        {
            std::lock_guard<std::mutex> g(g_mu);
            auto it = g_states.find(cmd);
            if (it != g_states.end()) {
                if (got) *got = it->second;
                if (pred(it->second)) return true;
            }
        }
        if (nowMs() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
}
std::string stateOf(const std::string& cmd) {
    std::lock_guard<std::mutex> g(g_mu);
    auto it = g_states.find(cmd);
    return it == g_states.end() ? "(chưa báo)" : it->second;
}

std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); }); return s; }
bool has(const std::string& hay, const std::string& needle) { return lower(hay).find(lower(needle)) != std::string::npos; }
std::string snippet(const std::string& s, size_t n = 160) {
    std::string t = s.substr(0, n);
    std::replace(t.begin(), t.end(), '\n', ' ');
    return t;
}

void settle() { std::this_thread::sleep_for(std::chrono::milliseconds(250)); }
void run(Document& d, const w::Call& c) { d.command(c.first, c.second); settle(); }
void run(Document& d, const std::string& uno, const std::string& args = "") { d.command(uno, args); settle(); }
std::string selHtml(Document& d) { return d.selectionAs("text/html"); }
std::string selText(Document& d) { return d.selectionAs("text/plain;charset=utf-8"); }
void selectAll(Document& d) { run(d, ".uno:SelectAll"); }

bool truthy(const std::string& v) { return v == "true" || v == "1"; }
}  // namespace

int runWordTests(const fs::path& program, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    watchdog(60);

    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine " + err);
    if (!engine) return 1;

    step("tạo tài liệu Word mới");
    auto doc = engine->createNew(DocKind::Text, &err);
    check((bool)doc && doc->kind() == DocKind::Text, "Tạo tài liệu Word mới " + err);
    if (!doc) return 1;
    Document& d = *doc;
    d.setEventSink(onEvent);
    d.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
    settle();

    // ---------------- danh sách phông và kiểu từ engine ----------------
    step("danh sách phông/kiểu từ engine");
    const std::string fontJson = d.commandValues(".uno:CharFontName");
    const auto fonts = w::parseFontList(fontJson);
    check(fonts.size() >= 3, "getCommandValues(CharFontName) trả về danh sách phông (" + std::to_string(fonts.size()) + ")",
          fonts.empty() ? snippet(fontJson) : fonts.front());
    const std::string styleJson = d.commandValues(".uno:StyleApply");
    check(has(styleJson, "Heading 1") || has(styleJson, "Heading"), "getCommandValues(StyleApply) có kiểu Heading", snippet(styleJson, 120));

    // ---------------- gõ văn bản ----------------
    step("gõ văn bản");
    d.typeText(u"Xin chao Dũng Office");
    settle();
    selectAll(d);
    check(has(selText(d), "Xin chao"), "Gõ văn bản và chọn tất cả", snippet(selText(d)));
    check(w::countWords(selText(d)) == 4, "Đếm từ (countWords) = 4 trên văn bản vừa gõ", std::to_string(w::countWords(selText(d))));

    // ---------------- bật/tắt: đậm, nghiêng, gạch chân, gạch ngang ----------------
    struct Toggle { const char* uno; const char* html; };
    const Toggle toggles[] = {{".uno:Bold", "<b>"}, {".uno:Italic", "<i>"}, {".uno:Underline", "<u>"}, {".uno:Strikeout", "<s>"}};
    for (const auto& t : toggles) {
        step(std::string("bật/tắt ") + t.uno);
        forgetState(t.uno);
        run(d, t.uno);
        std::string got;
        const bool st = waitState(t.uno, truthy, 3000, &got);
        const std::string html = selHtml(d);
        const bool htmlOn = has(html, t.html) || (std::string(t.uno) == ".uno:Bold" && has(html, "font-weight: bold")) ||
                            (std::string(t.uno) == ".uno:Strikeout" && (has(html, "line-through") || has(html, "<strike")));
        check(st || htmlOn, std::string("Bật ") + t.uno, "trạng thái=" + got + ", html=" + (htmlOn ? "có" : "không"));
        forgetState(t.uno);
        run(d, t.uno);
        const bool off = waitState(t.uno, [](const std::string& v) { return !truthy(v); }, 3000, &got);
        check(off || !has(selHtml(d), t.html), std::string("Tắt ") + t.uno, "trạng thái=" + got);
    }

    // ---------------- phông, cỡ chữ ----------------
    step("đổi phông");
    std::string pick;
    for (const auto& f : fonts) if (f != stateOf(".uno:CharFontName")) { pick = f; break; }
    if (pick.empty() && !fonts.empty()) pick = fonts.front();
    forgetState(".uno:CharFontName");
    run(d, w::fontName(pick));
    std::string got;
    const bool fontState = waitState(".uno:CharFontName", [&](const std::string& v) { return v == pick; }, 3000, &got);
    check(fontState || has(selHtml(d), pick), "Đổi phông sang '" + pick + "'", "trạng thái=" + got);

    step("đổi cỡ chữ");
    forgetState(".uno:FontHeight");
    run(d, w::fontSize(20));
    const bool sizeState = waitState(".uno:FontHeight", [](const std::string& v) { return std::atof(v.c_str()) == 20.0; }, 3000, &got);
    check(sizeState || has(selHtml(d), "20pt"), "Đổi cỡ chữ 20", "trạng thái=" + got);

    // ---------------- màu chữ, tô nền ----------------
    step("màu chữ");
    run(d, w::fontColor(0xFF0000));
    std::string html = selHtml(d);
    check(has(html, "ff0000") || has(html, "color=\"#ff0000\""), "Đổi màu chữ đỏ (.uno:Color)", snippet(html, 200));
    step("tô nền");
    run(d, w::backColor(0xFFFF00));
    html = selHtml(d);
    check(has(html, "ffff00"), "Tô nền vàng (.uno:BackColor)", snippet(html, 200));

    // ---------------- chỉ số trên/dưới, xoá định dạng ----------------
    step("chỉ số trên");
    run(d, ".uno:SuperScript");
    html = selHtml(d);
    check(has(html, "<sup") || has(html, "vertical-align: super"), "Chỉ số trên (.uno:SuperScript)", snippet(html, 200));
    run(d, ".uno:SuperScript"); // tắt
    step("chỉ số dưới");
    run(d, ".uno:SubScript");
    html = selHtml(d);
    check(has(html, "<sub") || has(html, "vertical-align: sub"), "Chỉ số dưới (.uno:SubScript)", snippet(html, 200));
    run(d, ".uno:SubScript"); // tắt
    step("xoá định dạng");
    run(d, ".uno:ResetAttributes");
    html = selHtml(d);
    check(!has(html, "ffff00") && !has(html, "ff0000"), "Xoá định dạng bỏ màu chữ và màu nền", snippet(html, 200));

    // ---------------- căn lề ----------------
    struct Align { const char* uno; const char* html1; const char* html2; };
    const Align aligns[] = {{".uno:CenterPara", "text-align: center", "align=\"center\""},
                            {".uno:RightPara", "text-align: right", "align=\"right\""},
                            {".uno:JustifyPara", "text-align: justify", "align=\"justify\""},
                            {".uno:LeftPara", "text-align: left", "align=\"left\""}};
    for (const auto& a : aligns) {
        step(std::string("căn lề ") + a.uno);
        forgetState(a.uno);
        run(d, a.uno);
        const bool ok = waitState(a.uno, truthy, 3000, &got);
        html = selHtml(d);
        check(ok || has(html, a.html1) || has(html, a.html2), std::string("Căn lề ") + a.uno, "trạng thái=" + got);
    }

    // ---------------- giãn dòng ----------------
    step("giãn dòng");
    forgetState(".uno:SpacePara15");
    run(d, ".uno:SpacePara15");
    check(waitState(".uno:SpacePara15", truthy, 3000, &got) || has(selHtml(d), "150%"), "Giãn dòng 1,5 (.uno:SpacePara15)", "trạng thái=" + got);
    run(d, ".uno:SpacePara1");

    forgetState(".uno:SpacePara2");
    run(d, ".uno:SpacePara2");
    check(waitState(".uno:SpacePara2", truthy, 3000, &got) || has(selHtml(d), "200%"), "Giãn dòng đôi (.uno:SpacePara2)", "trạng thái=" + got);
    run(d, ".uno:SpacePara1");

    // ---------------- kiểu đoạn ----------------
    step("kiểu đoạn");
    forgetState(".uno:StyleApply");
    run(d, w::paragraphStyle("Heading 1"));
    const bool h1State = waitState(".uno:StyleApply", [](const std::string& v) { return has(v, "Heading 1"); }, 3000, &got);
    check(h1State || has(selHtml(d), "<h1"), "Áp kiểu Tiêu đề 1", "trạng thái=" + got);
    run(d, w::paragraphStyle("Default Paragraph Style"));
    forgetState(".uno:StyleApply");
    run(d, w::paragraphStyle("Heading 2"));
    check(waitState(".uno:StyleApply", [](const std::string& v) { return has(v, "Heading 2"); }, 3000, &got) || has(selHtml(d), "<h2"),
          "Áp kiểu Tiêu đề 2", "trạng thái=" + got);
    run(d, w::paragraphStyle("Default Paragraph Style"));

    // ---------------- danh sách, thụt lề ----------------
    step("danh sách");
    forgetState(".uno:DefaultBullet");
    run(d, ".uno:DefaultBullet");
    check(waitState(".uno:DefaultBullet", truthy, 3000, &got) || has(selHtml(d), "<ul"), "Gạch đầu dòng (.uno:DefaultBullet)", "trạng thái=" + got);
    run(d, ".uno:DefaultBullet");
    forgetState(".uno:DefaultNumbering");
    run(d, ".uno:DefaultNumbering");
    check(waitState(".uno:DefaultNumbering", truthy, 3000, &got) || has(selHtml(d), "<ol"), "Đánh số (.uno:DefaultNumbering)", "trạng thái=" + got);
    run(d, ".uno:DefaultNumbering");
    step("thụt lề");
    const std::string before = selHtml(d);
    run(d, ".uno:IncrementIndent");
    const std::string afterInc = selHtml(d);
    check(afterInc != before, "Tăng thụt lề làm đổi định dạng đoạn", snippet(afterInc, 120));
    run(d, ".uno:DecrementIndent");

    // ---------------- hoàn tác / làm lại ----------------
    step("hoàn tác / làm lại");
    d.typeText(u" them");
    settle();
    selectAll(d);
    const bool typed = has(selText(d), "them");
    run(d, ".uno:Undo");
    selectAll(d);
    const bool undone = !has(selText(d), "them");
    run(d, ".uno:Redo");
    selectAll(d);
    check(typed && undone && has(selText(d), "them"), "Hoàn tác rồi làm lại văn bản vừa gõ", "đã gõ=" + std::to_string(typed) + ", đã hoàn tác=" + std::to_string(undone));

    // ---------------- tìm / thay thế ----------------
    step("tìm và thay thế");
    run(d, ".uno:GoToEndOfDoc");
    d.typeText(u"\nfoo bar foo");
    settle();
    run(d, w::findNext("bar", false));
    check(selText(d) == "bar", "Tìm 'bar' chọn đúng từ", "vùng chọn='" + snippet(selText(d), 30) + "'");
    run(d, w::replaceAll("foo", "baz"));
    selectAll(d);
    const std::string afterReplace = selText(d);
    check(has(afterReplace, "baz bar baz") && !has(afterReplace, "foo"), "Thay tất cả 'foo' bằng 'baz'", snippet(afterReplace, 80));

    step("tìm ngược, thay một lần và hoàn tác");
    {
        auto searchDoc = engine->createNew(DocKind::Text, &err);
        check(bool(searchDoc), "Tạo tài liệu độc lập thử thay từng lần");
        if (searchDoc) {
            searchDoc->setClientView(512,512,7680,7680,0,0,7680,7680);
            searchDoc->typeText(u"REPL_MARK left REPL_MARK right");
            settle();
            run(*searchDoc, ".uno:GoToEndOfDoc");
            run(*searchDoc, w::findNext("REPL_MARK", true));
            check(selText(*searchDoc) == "REPL_MARK", "Tìm ngược chọn đúng từ");
            run(*searchDoc, w::replaceOne("REPL_MARK", "NEW_MARK"));
            selectAll(*searchDoc);
            const auto changed=selText(*searchDoc);
            check(has(changed,"REPL_MARK left NEW_MARK right"),
                  "Chỉ thay lần xuất hiện thứ hai",snippet(changed));
            run(*searchDoc, ".uno:Undo");
            selectAll(*searchDoc);
            check(has(selText(*searchDoc),"REPL_MARK left REPL_MARK right"),
                  "Undo khôi phục cả hai từ gốc");
        }
    }

    // ---------------- chèn chữ Việt qua paste (cơ chế dùng cho nhóm "Tiện ích Việt") ----------------
    step("dán chữ Việt");
    run(d, ".uno:GoToEndOfDoc");
    const std::string vn = "Một triệu đồng chẵn ngày 05 tháng 02 năm 2026";
    const bool pasted = d.paste("text/plain;charset=utf-8", vn);
    settle();
    selectAll(d);
    check(pasted && has(selText(d), vn), "Dán chuỗi tiếng Việt có dấu vào vị trí con trỏ", snippet(selText(d), 80));

    // ---------------- chèn: bảng, ảnh, ngắt trang, liên kết ----------------
    step("chèn bảng");
    run(d, ".uno:GoToEndOfDoc");
    run(d, w::insertTable(3, 2));
    selectAll(d);
    html = selHtml(d);
    check(has(html, "<table"), "Chèn bảng 3x2", snippet(html, 120));

    step("chèn ảnh");
    run(d, ".uno:GoToEndOfDoc");
    static const unsigned char kPng[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x06,0x00,0x00,0x00,0x1F,0x15,0xC4,0x89,0x00,0x00,0x00,0x0D,0x49,0x44,0x41,0x54,0x78,0x9C,0x63,0xF8,0xFF,0xFF,0x3F,0x00,0x05,0xFE,0x02,0xFE,0xA7,
        0x35,0x81,0x84,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82};
    const fs::path png = fs::absolute(out) / "anh-thu.png";
    { std::ofstream f(png, std::ios::binary); f.write((const char*)kPng, sizeof kPng); }
    run(d, w::insertImage(toFileUrl(png)));
    selectAll(d);
    html = selHtml(d);
    check(has(html, "<img"), "Chèn ảnh từ tệp (.uno:InsertGraphic)", snippet(html, 120));

    step("ngắt trang");
    long wTw = 0, hTw = 0;
    d.documentSize(wTw, hTw);
    run(d, ".uno:GoToEndOfDoc");
    run(d, ".uno:InsertPagebreak");
    long w2 = 0, h2 = 0;
    d.documentSize(w2, h2);
    check(h2 > hTw, "Ngắt trang làm tăng chiều cao tài liệu", std::to_string(hTw) + " -> " + std::to_string(h2) + " twip");

    step("chèn liên kết");
    run(d, ".uno:GoToEndOfDoc");
    run(d, w::hyperlink("Trang web", "https://example.com/"));
    selectAll(d);
    html = selHtml(d);
    check(has(html, "href=\"https://example.com/\""), "Chèn liên kết (.uno:SetHyperlink)", snippet(html, 160));

    step("ký hiệu Unicode qua engine");
    run(d, ".uno:GoToEndOfDoc");
    check(d.paste("text/plain;charset=utf-8", "Ω ≤ ≥ ₫"), "Engine nhận ký hiệu Unicode");
    d.waitInputIdle();
    selectAll(d);
    check(has(selText(d), "Ω ≤ ≥ ₫"), "Ký hiệu Unicode xuất hiện trong tài liệu", snippet(selText(d), 180));

    step("khối hành chính Việt Nam");
    w::AdminBlock admin;
    admin.agency = "CƠ QUAN A & B"; admin.title = "THÔNG BÁO"; admin.signerName = "Nguyễn Văn A";
    const auto block = w::adminBlockHtml(admin, true, true);
    check(has(block, "A &amp; B") && !has(block, "A & B"), "Khối hành chính escape HTML đầu vào");
    check(!has(w::adminBlockHtml(admin, true, false), "Nơi nhận:") && !has(w::adminBlockHtml(admin, false, true), "CỘNG HÒA"), "Chèn riêng đầu/cuối");
    run(d, ".uno:GoToEndOfDoc");
    check(d.paste("text/html", block), "Engine nhận khối hành chính HTML");
    d.waitInputIdle(); selectAll(d);
    check(has(selText(d), "CỘNG HÒA") && has(selText(d), "Nguyễn Văn A") && has(selText(d), "CƠ QUAN A & B"), "Khối hành chính là văn bản thật trong Writer", snippet(selText(d), 100));
    check(has(selHtml(d), "<table"), "Khối hành chính xuất dưới dạng bảng có thể chỉnh sửa");

    // ---------------- bố cục trang: khổ giấy, hướng ----------------
    step("khổ giấy A4");
    long bw = 0, bh = 0;
    d.documentSize(bw, bh);
    // Thử đơn vị 1/100 mm (UNO), rồi twip nếu chưa đổi.
    run(d, ".uno:AttributePageSize", w::join({w::argLong("AttributePageSize.Width", 21000), w::argLong("AttributePageSize.Height", 29700)}));
    long aw = 0, ah = 0;
    d.documentSize(aw, ah);
    std::cout << "       kích thước tài liệu: " << bw << "x" << bh << " -> " << aw << "x" << ah << " twip" << std::endl;
    // Chiều rộng tài liệu engine báo = rộng trang + phần đệm hai bên khi xem (~568 twip).
    check(aw >= 11906 && aw <= 11906 + 700, "Đặt khổ A4 (trang rộng 11906 twip + đệm xem)", std::to_string(aw) + " twip");

    step("hướng ngang");
    run(d, ".uno:AttributePageSize", w::join({w::argLong("AttributePageSize.Width", 29700), w::argLong("AttributePageSize.Height", 21000)}));
    long lw = 0, lh = 0;
    d.documentSize(lw, lh);
    check(lw >= 16838 && lw <= 16838 + 700, "Đặt khổ ngang (trang rộng 16838 twip + đệm xem)", std::to_string(lw) + " twip");
    run(d, ".uno:AttributePageSize", w::join({w::argLong("AttributePageSize.Width", 21000), w::argLong("AttributePageSize.Height", 29700)}));

    // ---------------- dán/cắt/sao chép ----------------
    step("sao chép");
    selectAll(d);
    check(!selText(d).empty() && !selHtml(d).empty(), "Sao chép vùng chọn ra văn bản và HTML");

    // ---------------- tài liệu mẫu để kiểm tra định dạng sau khi lưu DOCX ----------------
    step("dựng tài liệu mẫu và lưu DOCX");
    auto d2p = engine->createNew(DocKind::Text, &err);
    check((bool)d2p, "Tạo tài liệu mẫu " + err);
    if (d2p) {
        Document& e = *d2p;
        e.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
        settle();
        run(e, w::paragraphStyle("Heading 1"));
        e.typeText(u"Tiêu đề thử\n");
        settle();
        run(e, ".uno:Bold");
        e.typeText(u"Chữ đậm");
        run(e, ".uno:Bold");
        e.typeText(u" thường\n");
        settle();
        run(e, ".uno:DefaultBullet");
        e.typeText(u"Mục một\nMục hai\n");
        settle();
        run(e, ".uno:DefaultBullet");
        run(e, w::insertTable(3, 2));
        run(e, ".uno:GoToEndOfDoc");
        run(e, w::hyperlink("Liên kết", "https://example.com/"));
        // Lề chuẩn VN: trái 30, phải 15, trên 20, dưới 20 mm (kiểm trong verify_docx.py qua w:pgMar)
        run(e, ".uno:PageLRMargin", w::join({w::argLong("PageLRMargin.Left", 3000), w::argLong("PageLRMargin.Right", 1500)}));
        run(e, ".uno:PageULMargin", w::join({w::argLong("PageULMargin.Upper", 2000), w::argLong("PageULMargin.Lower", 2000)}));
        const fs::path docx = out / "word-dinh-dang.docx";
        fs::remove(docx, ec);
        auto r = e.saveAs(docx);
        check(r.ok, "Lưu tài liệu mẫu thành DOCX: " + r.message);
    }

    // ---------------- thống kê lệnh engine đã báo trạng thái ----------------
    {
        std::lock_guard<std::mutex> g(g_mu);
        std::cout << "       lệnh engine đã báo trạng thái (" << g_seen.size() << "):";
        for (const auto& s : g_seen) std::cout << " " << s;
        std::cout << std::endl;
    }
    std::cout << (g_fail ? "\nKẾT QUẢ WORD: CÓ LỖI\n" : "\nKẾT QUẢ WORD: TẤT CẢ ĐẠT\n");
    std::cout.flush();
    std::_Exit(g_fail ? 1 : 0);
}

int runWordStructureTests(const fs::path& program, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    watchdog(60);
    std::string error;
    auto engine = Engine::create(program, out / "profile", out / "work", &error);
    if (!engine) { check(false, "Khởi động engine " + error); return 1; }
    auto doc = engine->createNew(DocKind::Text, &error);
    if (!doc) { check(false, "Tạo Word " + error); return 1; }
    auto& d = *doc;
    d.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
    step("đầu trang và chân trang");
    d.typeText(u"BODY_MARKER");
    d.waitInputIdle();
    run(d,w::enablePageBand(true,"Default Page Style"));
    d.waitInputIdle();
    d.typeText(u"HEADER_MARKER");
    d.waitInputIdle();
    run(d,w::enablePageBand(false,"Default Page Style"));
    d.waitInputIdle();
    d.typeText(u"FOOTER_MARKER");
    d.waitInputIdle();
    // GoToStart/EndOfDoc can move within the active header/footer or note.
    // Locate the unique body text and verify the selection before placing either anchor.
    auto placeBodyAnchor = [&]() {
        run(d,w::findNext("BODY_MARKER",false));
        d.waitInputIdle();
        const auto found = selText(d);
        const bool selected = has(found,"BODY_MARKER");
        check(selected,"Tìm đúng đoạn BODY_MARKER trước khi chèn chú thích",snippet(found));
        if (!selected) return false;
        // VCL KEY_RIGHT = 1027, same as app/src/KeyMap.cpp: collapse selection at its end.
        d.postKey(LOK_KEYEVENT_KEYINPUT,0,1027);
        d.postKey(LOK_KEYEVENT_KEYUP,0,1027);
        d.waitInputIdle();
        const bool collapsed = selText(d).empty();
        check(collapsed,"Đã bỏ chọn trước khi chèn neo chú thích");
        return collapsed;
    };
    step("chú thích cuối trang");
    if (!placeBodyAnchor()) return 1;
    run(d,w::insertFootnote(false));
    d.waitInputIdle();
    d.typeText(u"FOOTNOTE_MARKER");
    d.waitInputIdle();
    run(d,".uno:JumpToFootnoteOrAnchor");
    step("chú thích cuối văn bản");
    if (!placeBodyAnchor()) return 1;
    run(d,w::insertFootnote(true));
    d.waitInputIdle();
    d.typeText(u"ENDNOTE_MARKER");
    d.waitInputIdle();
    step("lưu DOCX để kiểm cấu trúc");
    const fs::path output = out / "word-structure.docx";
    fs::remove(output,ec);
    auto result=d.saveAs(output);
    check(result.ok, "Lưu cấu trúc Word: " + result.message);
    doc.reset();

    // Native Writer review: use the same UNO commands as the ribbon and inspect
    // the exported DOCX rather than inferring success from the modified flag.
    step("Word Review — ghi thay đổi và bình luận native");
    auto review=engine->createNew(DocKind::Text,&error);
    if(!review){check(false,"Tạo tài liệu review "+error);return 1;}
    review->setClientView(512,512,7680,7680,0,0,7680,7680);
    review->typeText(u"ORIGINAL_MARKER ");
    review->waitInputIdle();
    run(*review,".uno:TrackChanges");
    review->waitInputIdle();
    review->typeText(u"INSERTED_MARKER");
    review->waitInputIdle();
    run(*review,".uno:TrackChanges");
    review->waitInputIdle();
    run(*review,w::findNext("ORIGINAL_MARKER",false));
    review->waitInputIdle();
    check(has(selText(*review),"ORIGINAL_MARKER"),"Chọn đúng chữ để bình luận");
    review->command(".uno:InsertAnnotation",Document::stringArg("Text","COMMENT_MARKER · Ghi chú tiếng Việt: Đường dẫn & số liệu"));
    review->waitInputIdle();
    const fs::path pending=out/"word-review.docx";
    fs::remove(pending,ec);
    const auto saved=review->saveAs(pending);
    check(saved.ok,"Lưu DOCX có review thật: "+saved.message);
    review.reset();
    if(saved.ok){
        step("Mở lại DOCX có review");
        auto reopened=engine->open(pending,&error);
        check(bool(reopened),"Mở lại Word review: "+error);
        if(reopened){
            const fs::path roundtrip=out/"word-review-roundtrip.docx";
            fs::remove(roundtrip,ec);
            auto rt=reopened->saveAs(roundtrip);
            check(rt.ok,"Roundtrip review DOCX: "+rt.message);
        }
    }
    // Two independent opens of the same tracked revision, exercising the
    // actual commands exposed by the review ribbon.
    if(saved.ok){
        for(bool accept : {true,false}){
            step(accept?"Word Review — chấp nhận toàn bộ":"Word Review — từ chối toàn bộ");
            auto action=engine->open(pending,&error);
            check(bool(action),"Mở nguồn review: "+error);
            if(!action)continue;
            action->setClientView(512,512,7680,7680,0,0,7680,7680);
            run(*action,accept?".uno:AcceptAllTrackedChanges":".uno:RejectAllTrackedChanges");
            action->waitInputIdle();
            const fs::path target=out/(accept?"word-review-accepted.docx":"word-review-rejected.docx");
            fs::remove(target,ec);
            auto sr=action->saveAs(target);
            check(sr.ok,"Lưu kết quả review: "+sr.message);
        }
    }
    // Existing TOC is loaded from a standards-based DOCX fixture; update the
    // real Writer index after changing its source heading in the active document.
    step("Word mục lục native — cập nhật index hiện có");
    const fs::path tocFixture=out/"toc-input.docx";
    auto toc=engine->open(tocFixture,&error);
    check(bool(toc),"Mở fixture có mục lục: "+error);
    if(toc){
        toc->setClientView(512,512,7680,7680,0,0,7680,7680);
        run(*toc,w::findNext("HEADING_BEFORE",false));
        toc->waitInputIdle();
        check(has(selText(*toc),"HEADING_BEFORE"),"Tìm tiêu đề nguồn");
        if(has(selText(*toc),"HEADING_BEFORE")){
            toc->typeText(u"HEADING_AFTER");
            toc->waitInputIdle();
            run(*toc,".uno:UpdateAllIndexes");
            toc->waitInputIdle();
            const fs::path dest=out/"word-toc-updated.docx";
            fs::remove(dest,ec);
            // Only consent to the newly detected, uncertain custom XML warning
            // for this synthetic fixture, after proving the default still refuses.
            SaveOptions options;
            bool customXmlOnly=true, hasBlocking=false;
            for(const auto& item:toc->featureLoss())if(item.blocking){
                hasBlocking=true;customXmlOnly=customXmlOnly&&item.id=="xml-tuy-bien"&&!item.certain;
            }
            if(hasBlocking&&customXmlOnly){
                const auto refused=toc->saveAs(dest);
                check(!refused.ok&&!fs::exists(dest),"Mục lục: chặn lưu khi chưa xác nhận XML tùy biến");
                options.allowFeatureLoss=true;
            }
            const auto saved=toc->saveAs(dest,options);
            check(saved.ok,"Lưu mục lục đã cập nhật: "+saved.message);
        }
    }
    std::cout.flush();
    std::_Exit(g_fail ? 1 : 0);
}
