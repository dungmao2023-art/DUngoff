// dung-smoketest --vninput 1 : kiểm kê nhập tiếng Việt qua đường phím → engine (Writer/Calc/Impress).
// Mô phỏng đúng những gì app gửi cho engine: postKey(KEYINPUT, ký tự, 0) + KEYUP cho từng ký tự đã chốt;
// kiểu Unikey "backspace" (gõ rồi xoá lùi để thay); dán NFC/NFD; ký tự tổ hợp gõ rời.
// Mỗi bước lưu ra tệp thật; tests/vninput/verify_vn.py đọc lại tệp và so với đáp án (không đoán).
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <chrono>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace dung;
namespace fs = std::filesystem;

namespace {
int g_fail = 0;
void check(bool ok, const std::string& what, const std::string& ev = "") {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what;
    if (!ev.empty()) std::cout << "  {" << ev << "}";
    std::cout << std::endl;
    if (!ok) ++g_fail;
}
void settle(int ms = 250) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

constexpr int KEY_RETURN = 1280, KEY_TAB = 1282, KEY_BACKSPACE = 1283;

void key(Document& d, int charCode, int keyCode = 0) {
    d.postKey(LOK_KEYEVENT_KEYINPUT, charCode, keyCode);
    d.postKey(LOK_KEYEVENT_KEYUP, charCode, keyCode);
}
// Đường "đã chốt chuỗi" của TileView::inputMethodEvent: từng ký tự UTF-16 → KEYINPUT/KEYUP, keyCode 0.
void commit(Document& d, const std::u16string& s) { for (char16_t c : s) { key(d, c); d.waitInputIdle(120, 2000); } }
void enter(Document& d) { key(d, 0, KEY_RETURN); d.waitInputIdle(); }
void backspace(Document& d, int n) { for (int i = 0; i < n; ++i) { key(d, 0, KEY_BACKSPACE); d.waitInputIdle(120, 2000); } }

std::string u8(const std::u16string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t c = s[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size()) { c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00); ++i; }
        if (c < 0x80) r += (char)c;
        else if (c < 0x800) { r += (char)(0xC0 | (c >> 6)); r += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { r += (char)(0xE0 | (c >> 12)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
        else { r += (char)(0xF0 | (c >> 18)); r += (char)(0x80 | ((c >> 12) & 0x3F)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
    }
    return r;
}

// Các dòng thử (đáp án NFC). Tên ca để ghi log và để verify_vn.py in kết quả theo ca.
struct Line { const char* id; std::u16string nfc; };
const std::u16string kCong = u"Cộng hòa xã hội chủ nghĩa Việt Nam";
const std::u16string kUpper = u"ĐẠI HỌC BÁCH KHOA – Ơ Ư Ă Â Ê Ô";
const std::u16string kName = u"Trần Văn Dũng";
const std::u16string kNameNfd = u"Trần Văn Dũng";   // NFD: a + ̂ + ̀ ; a + ̆ ; u + ̃
const std::u16string kUniDeco = u"Nguyễn";                          // ê + ̃ (kết hợp lai: dựng sẵn + dấu rời)

void writeExpected(const fs::path& f, const std::vector<std::pair<std::string, std::u16string>>& rows) {
    std::ofstream o(f, std::ios::binary);
    for (auto& r : rows) o << r.first << "\t" << u8(r.second) << "\n";
}

bool save(Document& d, const fs::path& p) {
    SaveOptions so;
    auto r = d.saveAs(p, so);
    check(r.ok, "Lưu " + p.filename().string(), r.message);
    return r.ok;
}
}  // namespace

int dungRunVnInput(const fs::path& program, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    std::string err;
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine " + err);
    if (!engine) return 1;

    // ---------------- Writer ----------------
    {
        auto doc = engine->createNew(DocKind::Text, &err);
        check((bool)doc, "Tạo Word mới " + err);
        if (!doc) return 1;
        Document& d = *doc;
        d.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
        settle();
        std::vector<std::pair<std::string, std::u16string>> exp;

        commit(d, kCong);                         enter(d); exp.push_back({"commit-NFC", kCong});
        commit(d, kUpper);                        enter(d); exp.push_back({"commit-HOA", kUpper});
        // Unikey kiểu backspace: gõ "Nguyen", xoá 3 ký tự, chốt "yễn" (dựng sẵn)
        commit(d, u"Nguyen"); backspace(d, 3); commit(d, u"yễn");
        enter(d); exp.push_back({"backspace-thay", u"Nguyễn"});
        // Dán (đường Ctrl+V / dán chuỗi): NFC và NFD
        d.paste("text/plain;charset=utf-8", u8(kName));    settle(); enter(d); exp.push_back({"dan-NFC", kName});
        d.paste("text/plain;charset=utf-8", u8(kNameNfd)); settle(); enter(d); exp.push_back({"dan-NFD", kName});
        // Ký tự tổ hợp gõ rời từng ký tự (bộ gõ chốt dấu rời)
        commit(d, kNameNfd); enter(d); exp.push_back({"commit-NFD", kName});
        commit(d, kUniDeco); enter(d); exp.push_back({"commit-lai", u"Nguyễn"});
        // Thông tin: ký tự ngoài BMP (surrogate) — chỉ ghi nhận, không tính lỗi
        commit(d, u"x\U0001F600y"); exp.push_back({"info-ngoai-BMP", u"x\U0001F600y"});
        settle(400);
        save(d, out / "vn-writer.docx");
        writeExpected(out / "expected-writer.txt", exp);
    }

    // ---------------- Calc ----------------
    {
        auto doc = engine->createNew(DocKind::Spreadsheet, &err);
        check((bool)doc, "Tạo Excel mới " + err);
        if (doc) {
            Document& d = *doc;
            d.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
            settle();
            std::vector<std::pair<std::string, std::u16string>> exp;
            auto cell = [&](const char* ref, const std::function<void()>& typeIt) {
                d.command(".uno:GoToCell", std::string("{\"ToPoint\":{\"type\":\"string\",\"value\":\"") + ref + "\"}}");
                settle();
                typeIt();
                enter(d);
                settle();
            };
            cell("A1", [&] { commit(d, kCong); });                                 exp.push_back({"A1 commit-NFC", kCong});
            cell("A2", [&] { commit(d, u"Nguyen"); backspace(d, 3); commit(d, u"yễn"); }); exp.push_back({"A2 backspace-thay", u"Nguyễn"});
            cell("A3", [&] { commit(d, kUpper); });                                exp.push_back({"A3 commit-HOA", kUpper});
            cell("A4", [&] { commit(d, kNameNfd); });                              exp.push_back({"A4 commit-NFD", kName});
            cell("A5", [&] { commit(d, u"0012345"); });                            exp.push_back({"A5 so-0-dau-giu-nguyen-dang-nhap", u"0012345"});
            save(d, out / "vn-calc.xlsx");
            writeExpected(out / "expected-calc.txt", exp);
        }
    }

    // ---------------- Impress ----------------
    {
        auto doc = engine->createNew(DocKind::Presentation, &err);
        check((bool)doc, "Tạo PowerPoint mới " + err);
        if (doc) {
            Document& d = *doc;
            d.setClientView(512, 512, 15874, 8929, 0, 0, 15874, 8929);
            settle(500);
            std::vector<std::pair<std::string, std::u16string>> exp;
            // Vào chế độ sửa chữ của khung tiêu đề: nhấp đúp vào giữa vùng tiêu đề (tọa độ twip).
            d.postMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, 7900, 1500, 2, 1, 0);
            d.postMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, 7900, 1500, 1, 1, 0);
            settle(500);
            commit(d, kCong); exp.push_back({"tieu-de commit-NFC", kCong});
            commit(d, u" "); commit(d, u"Nguyen"); backspace(d, 3); commit(d, u"yễn");
            exp.back().second = kCong + u" Nguyễn";
            settle(400);
            d.command(".uno:Escape"); settle();
            save(d, out / "vn-impress.pptx");
            writeExpected(out / "expected-impress.txt", exp);
        }
    }

    std::cout << "\nNHẬP TIẾNG VIỆT (tầng engine): đã chạy xong các ca; so khớp với tệp lưu do verify_vn.py báo. Lỗi bước: "
              << g_fail << std::endl;
    return g_fail ? 1 : 0;
}
