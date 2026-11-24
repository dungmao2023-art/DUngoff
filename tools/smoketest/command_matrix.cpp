// dung-smoketest --cmdcase <id> : chạy MỘT ca lệnh UNO trên engine thật rồi lưu bản sao để kiểm tra.
// dung-smoketest --cmdlist 1     : in danh sách ca (id|ứng dụng|tệp mẫu|đuôi|mô tả).
// Mỗi ca chạy trong MỘT TIẾN TRÌNH RIÊNG (do tests/cmdmatrix/run_matrix.py gọi, có timeout) để lệnh treo
// vì hộp thoại không làm hỏng các ca khác. Việc kiểm chứng hiệu ứng nằm ở Python (đọc tệp đã lưu).
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace dung;

namespace {
std::string S(const std::string& n, const std::string& v) { return "\"" + n + "\":{\"type\":\"string\",\"value\":\"" + v + "\"}"; }
std::string L(const std::string& n, long long v) { return "\"" + n + "\":{\"type\":\"long\",\"value\":\"" + std::to_string(v) + "\"}"; }
std::string B(const std::string& n, bool v) { return "\"" + n + "\":{\"type\":\"boolean\",\"value\":\"" + (v ? "true" : "false") + "\"}"; }
std::string J(std::initializer_list<std::string> a) {
    std::string r = "{";
    bool first = true;
    for (const auto& x : a) { if (!first) r += ","; r += x; first = false; }
    return r + "}";
}

// Bước: "C:<lệnh>" hoặc "C:<lệnh>|<json>", "T:<chữ ASCII>", "G:<ô/vùng Calc>", "P:<số phần>", "W:<ms>".
struct Case {
    const char* id;
    const char* app;      // writer | calc | impress
    const char* fixture;  // fx.docx | fx.xlsx | fx.pptx
    const char* ext;
    const char* desc;
    std::vector<std::string> steps;
};

const std::string END = "C:.uno:GoToEndOfDoc";
const std::string START = "C:.uno:GoToStartOfDoc";

const std::vector<Case>& cases() {
    static const std::vector<Case> v = {
        // ===================== WRITER =====================
        {"w_table", "writer", "fx.docx", "docx", "Chèn bảng 3x2", {END, "C:.uno:InsertTable|" + J({L("Columns", 3), L("Rows", 2)})}},
        {"w_table_row", "writer", "fx.docx", "docx", "Chèn bảng rồi thêm hàng", {END, "C:.uno:InsertTable|" + J({L("Columns", 3), L("Rows", 2)}), "C:.uno:InsertRowsAfter"}},
        {"w_image", "writer", "fx.docx", "docx", "Chèn ảnh PNG", {END, "C:.uno:InsertGraphic|" + J({S("FileName", "{IMGURL}"), B("AsLink", false)})}},
        {"w_pagebreak", "writer", "fx.docx", "docx", "Ngắt trang", {END, "C:.uno:InsertPagebreak", "T:Sau ngat trang"}},
        {"w_header", "writer", "fx.docx", "docx", "Bật đầu trang", {"C:.uno:InsertPageHeader|" + J({S("PageStyle", "Default Page Style"), B("On", true)})}},
        {"w_footer", "writer", "fx.docx", "docx", "Bật chân trang", {"C:.uno:InsertPageFooter|" + J({S("PageStyle", "Default Page Style"), B("On", true)})}},
        {"w_toc", "writer", "fx.docx", "docx", "Mục lục (InsertMultiIndex, thường là hộp thoại)", {START, "C:.uno:InsertMultiIndex"}},
        {"w_numbering", "writer", "fx.docx", "docx", "Đánh số đoạn", {START, "C:.uno:GoDown", "C:.uno:DefaultNumbering"}},
        {"w_list_level", "writer", "fx.docx", "docx", "Danh sách đa cấp: tăng cấp (IncrementLevel)", {START, "C:.uno:GoDown", "C:.uno:DefaultNumbering", "C:.uno:IncrementLevel"}},
        {"w_list_outline", "writer", "fx.docx", "docx", "Danh sách đa cấp: OutlineRight", {START, "C:.uno:GoDown", "C:.uno:DefaultNumbering", "C:.uno:OutlineRight"}},
        {"w_footnote", "writer", "fx.docx", "docx", "Chú thích cuối trang", {START, "C:.uno:GoDown", "C:.uno:InsertFootnote", "T:Chu thich chan trang"}},
        {"w_endnote", "writer", "fx.docx", "docx", "Chú thích cuối văn bản", {START, "C:.uno:GoDown", "C:.uno:InsertEndnote", "T:Chu thich cuoi van ban"}},
        {"w_replace_all", "writer", "fx.docx", "docx", "Tìm và thay thế tất cả", {"C:.uno:ExecuteSearch|" + J({S("SearchItem.SearchString", "ABC"), S("SearchItem.ReplaceString", "XYZ"), L("SearchItem.Command", 3), B("SearchItem.Backward", false)})}},
        {"w_track", "writer", "fx.docx", "docx", "Theo dõi thay đổi", {"C:.uno:TrackChanges", END, "T:Them moi co theo doi"}},
        {"w_accept_all", "writer", "fx.docx", "docx", "Theo dõi rồi chấp nhận tất cả", {"C:.uno:TrackChanges", END, "T:Them moi", "C:.uno:AcceptAllTrackedChanges"}},
        {"w_comment", "writer", "fx.docx", "docx", "Chèn bình luận", {START, "C:.uno:InsertAnnotation|" + J({S("Text", "Binh luan thu"), S("Author", "Dung")})}},
        {"w_heading", "writer", "fx.docx", "docx", "Kiểu Heading 1 (đối chứng)", {START, "C:.uno:StyleApply|" + J({S("Style", "Heading 1"), S("FamilyName", "ParagraphStyles")})}},
        {"w_hyperlink", "writer", "fx.docx", "docx", "Chèn liên kết (đối chứng)", {END, "C:.uno:SetHyperlink|" + J({S("Hyperlink.Text", "Lien ket"), S("Hyperlink.URL", "https://example.com/")})}},
        {"w_bookmark", "writer", "fx.docx", "docx", "Chèn dấu trang", {START, "C:.uno:InsertBookmark|" + J({S("Bookmark", "BM1")})}},
        {"w_docprops", "writer", "fx.docx", "docx", "Đặt thuộc tính tài liệu (Title)", {"C:.uno:SetDocumentProperties|" + J({S("Title", "Tieu de thu nghiem")})}},
        {"w_date_field", "writer", "fx.docx", "docx", "Chèn trường ngày", {END, "C:.uno:InsertDateField"}},
        {"w_page_number", "writer", "fx.docx", "docx", "Chèn trường số trang (đối chứng)", {END, "C:.uno:InsertPageNumberField"}},
        {"w_pagesize_a5", "writer", "fx.docx", "docx", "Khổ giấy A5 (đối chứng)", {"C:.uno:AttributePageSize|" + J({L("AttributePageSize.Width", 14800), L("AttributePageSize.Height", 21000)})}},
        {"w_margin", "writer", "fx.docx", "docx", "Lề 30/15/20/20 mm (PageLRMargin/PageULMargin)", {"C:.uno:PageLRMargin|" + J({L("PageLRMargin.Left", 3000), L("PageLRMargin.Right", 1500)}), "C:.uno:PageULMargin|" + J({L("PageULMargin.Upper", 2000), L("PageULMargin.Lower", 2000)})}},
        {"w_wordcount", "writer", "fx.docx", "docx", "Hộp thoại đếm từ", {"C:.uno:WordCountDialog"}},
        {"w_spell_online", "writer", "fx.docx", "docx", "Bật/tắt kiểm tra chính tả khi gõ", {"C:.uno:SpellOnline"}},
        {"w_spell_dialog", "writer", "fx.docx", "docx", "Hộp thoại chính tả", {"C:.uno:SpellingAndGrammarDialog"}},
        {"w_columns", "writer", "fx.docx", "docx", "Hộp thoại cột", {"C:.uno:FormatColumns"}},
        {"w_symbol", "writer", "fx.docx", "docx", "Hộp thoại chèn ký tự", {"C:.uno:InsertSymbol"}},
        {"w_print", "writer", "fx.docx", "docx", "In (.uno:Print)", {"C:.uno:Print"}},
        {"w_print_preview", "writer", "fx.docx", "docx", "Xem trước in", {"C:.uno:PrintPreview"}},
        // --- hộp thoại: engine có phát JSON (JSDIALOG=46)/cửa sổ (WINDOW=36) không? ---
        {"w_page_dialog", "writer", "fx.docx", "docx", "Hộp thoại định dạng trang (có lề)", {"C:.uno:PageDialog"}},
        {"w_para_dialog", "writer", "fx.docx", "docx", "Hộp thoại đoạn văn", {START, "C:.uno:ParagraphDialog"}},
        {"w_font_dialog", "writer", "fx.docx", "docx", "Hộp thoại ký tự", {START, "C:.uno:FontDialog"}},
        {"w_table_dialog", "writer", "fx.docx", "docx", "Hộp thoại thuộc tính bảng", {END, "C:.uno:InsertTable|" + J({L("Columns", 3), L("Rows", 2)}), "C:.uno:TableDialog"}},
        {"w_lang_vi", "writer", "fx.docx", "docx", "Đặt ngôn ngữ view = vi rồi mở hộp thoại đoạn văn", {"L:vi", START, "C:.uno:ParagraphDialog"}},
        // --- chữ đang soạn của bộ gõ (LOK_EXT_TEXTINPUT) ---
        {"x_ime_commit", "writer", "fx.docx", "docx", "IME: soạn d→du→dư rồi chốt (END)", {START, "X:0:d", "X:0:du", "X:0:dư", "X:2:", "R:"}},
        {"x_ime_nocommit", "writer", "fx.docx", "docx", "IME: soạn dư, KHÔNG chốt, lưu luôn", {START, "X:0:d", "X:0:dư"}},
        {"x_ime_two_words", "writer", "fx.docx", "docx", "IME: soạn 'a'→'á' chốt, rồi 'n'→'ng' chốt", {START, "X:0:a", "X:0:á", "X:2:", "X:0:n", "X:0:ng", "X:2:", "R:"}},
        {"x_ime_calc", "calc", "fx.xlsx", "xlsx", "IME trong ô Calc: soạn 'Xin chào' rồi chốt", {"G:E2", "X:0:Xin chà", "X:0:Xin chào", "X:2:", "R:"}},
        // ===================== CALC =====================
        {"c_sort_asc", "calc", "fx.xlsx", "xlsx", "Sắp xếp tăng dần theo cột A", {"G:A2:C11", "C:.uno:SortAscending"}},
        {"c_sort_desc", "calc", "fx.xlsx", "xlsx", "Sắp xếp giảm dần theo cột A", {"G:A2:C11", "C:.uno:SortDescending"}},
        {"c_sort_dialog", "calc", "fx.xlsx", "xlsx", "Hộp thoại sắp xếp", {"G:A1:C11", "C:.uno:DataSort"}},
        {"c_autofilter", "calc", "fx.xlsx", "xlsx", "Bộ lọc tự động", {"G:A1:C11", "C:.uno:DataFilterAutoFilter"}},
        {"c_freeze", "calc", "fx.xlsx", "xlsx", "Đóng băng tại B2", {"G:B2", "C:.uno:FreezePanes"}},
        {"c_cond_dialog", "calc", "fx.xlsx", "xlsx", "Hộp thoại định dạng có điều kiện", {"G:B2:B11", "C:.uno:ConditionalFormatDialog"}},
        {"c_cond_manager", "calc", "fx.xlsx", "xlsx", "Quản lý định dạng có điều kiện", {"G:B2:B11", "C:.uno:ConditionalFormatManagerDialog"}},
        {"c_validation", "calc", "fx.xlsx", "xlsx", "Hộp thoại kiểm tra dữ liệu", {"G:B2:B11", "C:.uno:Validation"}},
        {"c_name_define", "calc", "fx.xlsx", "xlsx", "Hộp thoại đặt tên vùng", {"G:A1:C11", "C:.uno:DefineName"}},
        {"c_chart", "calc", "fx.xlsx", "xlsx", "Chèn biểu đồ", {"G:A1:C11", "C:.uno:InsertObjectChart"}},
        {"c_pivot", "calc", "fx.xlsx", "xlsx", "Pivot (DataDataPilotRun)", {"G:A1:C11", "C:.uno:DataDataPilotRun"}},
        {"c_merge", "calc", "fx.xlsx", "xlsx", "Gộp ô E1:F2", {"G:E1:F2", "C:.uno:ToggleMergeCells"}},
        {"c_print_area", "calc", "fx.xlsx", "xlsx", "Đặt vùng in", {"G:A1:C11", "C:.uno:DefinePrintArea"}},
        {"c_protect", "calc", "fx.xlsx", "xlsx", "Bảo vệ sheet", {"C:.uno:Protect"}},
        {"c_insert_row", "calc", "fx.xlsx", "xlsx", "Chèn hàng", {"G:A3", "C:.uno:InsertRows"}},
        {"c_insert_col", "calc", "fx.xlsx", "xlsx", "Chèn cột", {"G:B1", "C:.uno:InsertColumns"}},
        {"c_delete_row", "calc", "fx.xlsx", "xlsx", "Xoá hàng", {"G:A3", "C:.uno:DeleteRows"}},
        {"c_comment", "calc", "fx.xlsx", "xlsx", "Chèn ghi chú ô", {"G:C3", "C:.uno:InsertAnnotation|" + J({S("Text", "Ghi chu o"), S("Author", "Dung")})}},
        {"c_hyperlink", "calc", "fx.xlsx", "xlsx", "Chèn liên kết vào ô", {"G:E3", "C:.uno:SetHyperlink|" + J({S("Hyperlink.Text", "Lien ket"), S("Hyperlink.URL", "https://example.com/")})}},
        {"c_autosum", "calc", "fx.xlsx", "xlsx", "Tự tính tổng cột B", {"G:B2:B11", "C:.uno:AutoSum"}},
        {"c_group", "calc", "fx.xlsx", "xlsx", "Nhóm hàng (outline)", {"G:A2:A4", "C:.uno:Group"}},
        {"c_text_to_cols", "calc", "fx.xlsx", "xlsx", "Hộp thoại tách văn bản thành cột", {"G:A2:A11", "C:.uno:TextToColumns"}},
        {"c_remove_dups", "calc", "fx.xlsx", "xlsx", "Xoá trùng (DataRemoveDuplicates)", {"G:A1:C11", "C:.uno:DataRemoveDuplicates"}},
        {"c_rename_sheet", "calc", "fx.xlsx", "xlsx", "Đổi tên sheet (đối chứng)", {"C:.uno:RenameTable|" + J({S("Name", "DoiTen")})}},
        {"c_percent", "calc", "fx.xlsx", "xlsx", "Định dạng phần trăm (đối chứng)", {"G:B2:B11", "C:.uno:NumberFormatPercent"}},
        {"c_wrap", "calc", "fx.xlsx", "xlsx", "Xuống dòng trong ô", {"G:A2:A11", "C:.uno:WrapText"}},
        {"c_formula", "calc", "fx.xlsx", "xlsx", "Gõ công thức (đối chứng)", {"G:D2", "T:=B2*2\n"}},
        {"c_bgcolor", "calc", "fx.xlsx", "xlsx", "Tô nền ô A1:C1", {"G:A1:C1", "C:.uno:BackgroundColor|" + J({L("BackgroundColor", 16776960)})}},
        {"c_cell_dialog", "calc", "fx.xlsx", "xlsx", "Hộp thoại định dạng ô", {"G:B2:B11", "C:.uno:FormatCellDialog"}},
        {"c_page_dialog", "calc", "fx.xlsx", "xlsx", "Hộp thoại kiểu trang Calc (lề)", {"C:.uno:PageFormatDialog"}},
        {"c_print", "calc", "fx.xlsx", "xlsx", "In (.uno:Print)", {"C:.uno:Print"}},
        // ===================== IMPRESS =====================
        {"i_new_slide", "impress", "fx.pptx", "pptx", "Slide mới", {"C:.uno:InsertPage"}},
        {"i_dup_slide", "impress", "fx.pptx", "pptx", "Nhân đôi slide", {"C:.uno:DuplicatePage"}},
        {"i_delete_slide", "impress", "fx.pptx", "pptx", "Xoá slide", {"C:.uno:DeletePage"}},
        {"i_layout", "impress", "fx.pptx", "pptx", "Đổi bố cục slide 1 (AssignLayout)", {"P:0", "C:.uno:AssignLayout|" + J({L("WhatPage", 1), L("WhatLayout", 4)})}},
        {"i_transition", "impress", "fx.pptx", "pptx", "Hiệu ứng chuyển slide (SlideChange)", {"P:0", "C:.uno:SlideChange"}},
        {"i_image", "impress", "fx.pptx", "pptx", "Chèn ảnh", {"P:0", "C:.uno:InsertGraphic|" + J({S("FileName", "{IMGURL}"), B("AsLink", false)})}},
        {"i_table", "impress", "fx.pptx", "pptx", "Chèn bảng 3x2", {"P:0", "C:.uno:InsertTable|" + J({L("Columns", 3), L("Rows", 2)})}},
        {"i_chart", "impress", "fx.pptx", "pptx", "Chèn biểu đồ", {"P:0", "C:.uno:InsertObjectChart"}},
        {"i_hide", "impress", "fx.pptx", "pptx", "Ẩn slide", {"P:0", "C:.uno:HideSlide"}},
        {"i_slideshow", "impress", "fx.pptx", "pptx", "Trình chiếu (Presentation)", {"P:0", "C:.uno:Presentation"}},
        {"i_notes", "impress", "fx.pptx", "pptx", "Ghi chú diễn giả", {"P:0", "C:.uno:NotesMode", "T:Ghi chu dien gia"}},
        {"i_slide_dialog", "impress", "fx.pptx", "pptx", "Hộp thoại thiết lập slide", {"P:0", "C:.uno:PageSetup"}},
        {"i_print", "impress", "fx.pptx", "pptx", "In (.uno:Print)", {"C:.uno:Print"}},
    };
    return v;
}

std::mutex g_mu;
std::string g_step = "khởi động";
std::atomic<long long> g_start{0};
std::map<int, int> g_types;
std::vector<std::string> g_states;
std::string g_dialogHint;
struct EvRec { int type; std::string payload; };
std::vector<EvRec> g_evt;   // mọi sự kiện loại 16 (UNO_COMMAND_RESULT), 36 (WINDOW), 46 (JSDIALOG)

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void setStep(const std::string& s) {
    { std::lock_guard<std::mutex> g(g_mu); g_step = s; }
    g_start = nowMs();
}
void startWatchdog(int sec, const std::string& id) {
    g_start = nowMs();
    std::thread([sec, id] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_start > sec * 1000LL) {
                std::lock_guard<std::mutex> g(g_mu);
                std::cout << "CASE " << id << " TREO bước=" << g_step << " sau=" << sec << "s" << std::endl;
                std::_Exit(3);
            }
        }
    }).detach();
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}
std::u16string toU16(const std::string& s) {  // chỉ ASCII
    std::u16string r;
    for (unsigned char c : s) r.push_back(c);
    return r;
}
void replaceAll(std::string& s, const std::string& a, const std::string& b) {
    for (size_t p = 0; (p = s.find(a, p)) != std::string::npos; p += b.size()) s.replace(p, a.size(), b);
}
}  // namespace

int dungRunCommandMatrixList() {
    for (const auto& c : cases()) std::cout << c.id << "|" << c.app << "|" << c.fixture << "|" << c.ext << "|" << c.desc << "\n";
    return 0;
}

int dungRunCommandCase(const fs::path& program, const std::string& id, const fs::path& fixtures, const fs::path& out) {
    const Case* cs = nullptr;
    for (const auto& c : cases()) if (id == c.id) cs = &c;
    if (!cs) { std::cout << "CASE " << id << " LỖI không có ca này" << std::endl; return 2; }
    std::error_code ec;
    fs::create_directories(out, ec);
    fs::create_directories(out / "dialogs", ec);
    startWatchdog(40, id);

    std::string err;
    setStep("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / ("work-" + id), &err);
    if (!engine) { std::cout << "CASE " << id << " LỖI engine: " << err << std::endl; return 1; }
    setStep("mở tệp mẫu");
    // Mở bản sao của tệp mẫu (không đụng bản gốc).
    fs::path src = fixtures / cs->fixture;
    fs::path copy = out / (id + "-src." + cs->ext);
    fs::copy_file(src, copy, fs::copy_options::overwrite_existing, ec);
    auto doc = engine->open(copy, &err);
    if (!doc) { std::cout << "CASE " << id << " LỖI mở: " << err << std::endl; return 1; }
    Document& d = *doc;
    d.setEventSink([](const EngineEvent& ev) {
        std::lock_guard<std::mutex> g(g_mu);
        g_types[ev.type]++;
        const std::string lp = lower(ev.payload);
        if (ev.type == 16 || ev.type == 36 || ev.type == 46) g_evt.push_back({ev.type, ev.payload});
        if (lp.find("dialog") != std::string::npos && g_dialogHint.empty()) g_dialogHint = ev.payload.substr(0, 120);
        if (ev.type == LOK_CALLBACK_STATE_CHANGED && g_states.size() < 40) g_states.push_back(ev.payload.substr(0, 100));
    });
    d.setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::string imgUrl = "file://" + (fixtures / "img.png").generic_string();
    if (imgUrl.rfind("file://", 0) == 0 && imgUrl.size() > 8 && imgUrl[7] != '/') imgUrl = "file:///" + imgUrl.substr(7);  // Windows: file:///C:/...

    int idx = 0;
    for (const auto& st : cs->steps) {
        setStep("bước " + std::to_string(idx) + ": " + st.substr(0, 60));
        const char k = st.empty() ? '?' : st[0];
        const std::string rest = st.size() > 2 ? st.substr(2) : "";
        if (k == 'C') {
            std::string uno = rest, args;
            const auto bar = rest.find('|');
            if (bar != std::string::npos) { uno = rest.substr(0, bar); args = rest.substr(bar + 1); }
            replaceAll(args, "{IMGURL}", imgUrl);
            { std::lock_guard<std::mutex> g(g_mu); g_evt.clear(); }
            d.command(uno, args, true);
            d.waitInputIdle(500, 5000);
            std::this_thread::sleep_for(std::chrono::milliseconds(700));   // hộp thoại JSON có thể đến muộn
            std::vector<EvRec> snap;
            { std::lock_guard<std::mutex> g(g_mu); snap = g_evt; }
            int n = 0;
            for (const auto& e : snap) {
                if (e.type == 16) { std::cout << "CASE " << id << " RESULT " << uno << " " << e.payload.substr(0, 200) << std::endl; continue; }
                const std::string fn = id + "-" + std::to_string(e.type) + "-" + std::to_string(n++) + ".json";
                { std::ofstream f(out / "dialogs" / fn, std::ios::binary); f << e.payload; }
                std::cout << "CASE " << id << " EVT type=" << e.type << " bytes=" << e.payload.size() << " file=" << fn << std::endl;
            }
        } else if (k == 'T') {
            d.typeText(toU16(rest));
            d.waitInputIdle(300, 3000);
        } else if (k == 'G') {
            d.command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"" + rest + "\"}}");
            d.waitInputIdle(300, 3000);
        } else if (k == 'P') {
            d.setPart(std::atoi(rest.c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        } else if (k == 'L') {
            d.setViewLanguage(rest);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        } else if (k == 'X') {
            // "X:<kiểu>:<chữ UTF-8>"
            const auto c2 = rest.find(':');
            const int t = std::atoi(rest.substr(0, c2).c_str());
            d.postExtTextInput(t, c2 == std::string::npos ? "" : rest.substr(c2 + 1));
            d.waitInputIdle(300, 3000);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else if (k == 'R') {
            // đọc lại văn bản: chọn tất cả rồi lấy text
            d.command(".uno:SelectAll");
            d.waitInputIdle(300, 3000);
            std::cout << "CASE " << id << " TEXT [" << d.selectionAs("text/plain;charset=utf-8") << "]" << std::endl;
        } else if (k == 'W') {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(rest.c_str())));
        }
        ++idx;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    setStep("lưu bản sao");
    fs::path target = out / (id + "." + cs->ext);
    fs::remove(target, ec);
    SaveOptions so;
    auto r = d.saveAs(target, so);
    {
        std::lock_guard<std::mutex> g(g_mu);
        std::cout << "CASE " << id << " SAVED=" << (r.ok ? "yes" : "no") << " MSG=" << r.message << std::endl;
        std::cout << "CASE " << id << " EVENTS";
        for (auto& kv : g_types) std::cout << " " << kv.first << ":" << kv.second;
        std::cout << std::endl;
        if (!g_dialogHint.empty()) std::cout << "CASE " << id << " DIALOGHINT " << g_dialogHint << std::endl;
        for (auto& s : g_states) std::cout << "CASE " << id << " STATE " << s << std::endl;
        std::cout << "CASE " << id << " DONE" << std::endl;
    }
    std::cout.flush();
    std::_Exit(r.ok ? 0 : 1);
}
