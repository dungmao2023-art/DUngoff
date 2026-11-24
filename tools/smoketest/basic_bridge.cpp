// dung-smoketest --basic 1 : THÍ NGHIỆM — engine có chạy được macro Basic do chính ứng dụng đóng gói
// (thư viện trong hồ sơ engine, location=application) để gọi API UNO đầy đủ (lề trang, header/footer, thuộc tính tài liệu)?
// Kết quả quyết định có cần build lại engine để mở API hay không. Không phải mã sản xuất.
// Bảo mật: chỉ macro trong user/basic của hồ sơ riêng của ứng dụng; macro "application" không bị chặn bởi mức bảo mật
// macro (mức này chỉ áp cho macro NẰM TRONG tài liệu). Ứng dụng không bao giờ chạy macro từ tệp người dùng.
// SPDX-License-Identifier: MPL-2.0
#include "dung/Engine.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <stdlib.h>
#endif

using namespace dung;
namespace fs = std::filesystem;

namespace {
const char* kBasic = R"BAS(
Sub Mark(msg As String)
  Dim n As Integer, p As String
  p = Environ("DUNG_MARK")
  If p = "" Then Exit Sub
  n = FreeFile()
  Open p For Append As #n
  Print #n, msg
  Close #n
End Sub

Function FindWriter() As Object
  Dim e As Object, c As Object
  On Error Resume Next
  c = StarDesktop.CurrentComponent
  If Not IsNull(c) Then
    If Not IsEmpty(c) Then
      If c.supportsService("com.sun.star.text.TextDocument") Then
        FindWriter = c
        Exit Function
      End If
    End If
  End If
  e = StarDesktop.Components.createEnumeration()
  Do While e.hasMoreElements()
    c = e.nextElement()
    If c.supportsService("com.sun.star.text.TextDocument") Then
      FindWriter = c
      Exit Function
    End If
  Loop
End Function

Sub Ping()
  Mark "PING ok"
End Sub

Sub SetMargins()
  Dim d As Object, st As Object, i As Integer, ps As Object, names
  On Error GoTo EH
  Mark "SetMargins start"
  d = FindWriter()
  If IsNull(d) Or IsEmpty(d) Then
    Mark "NO_WRITER"
    Exit Sub
  End If
  st = d.StyleFamilies.getByName("PageStyles")
  names = st.ElementNames
  For i = LBound(names) To UBound(names)
    ps = st.getByName(names(i))
    ps.LeftMargin = 3000
    ps.RightMargin = 1500
    ps.TopMargin = 2000
    ps.BottomMargin = 2000
    Mark "style " & names(i) & " L=" & ps.LeftMargin & " R=" & ps.RightMargin & " T=" & ps.TopMargin & " B=" & ps.BottomMargin
  Next
  Mark "SetMargins done"
  Exit Sub
EH:
  Mark "ERR SetMargins " & Err & " " & Error$
End Sub

Sub SetHeader()
  Dim d As Object, st As Object, i As Integer, ps As Object, names
  On Error GoTo EH
  d = FindWriter()
  If IsNull(d) Or IsEmpty(d) Then
    Mark "NO_WRITER"
    Exit Sub
  End If
  st = d.StyleFamilies.getByName("PageStyles")
  names = st.ElementNames
  For i = LBound(names) To UBound(names)
    ps = st.getByName(names(i))
    ps.HeaderIsOn = True
    ps.HeaderText.String = "DUNG-HEADER-THU"
  Next
  Mark "SetHeader done"
  Exit Sub
EH:
  Mark "ERR SetHeader " & Err & " " & Error$
End Sub

Sub SetTitle()
  Dim d As Object
  On Error GoTo EH
  d = FindWriter()
  If IsNull(d) Or IsEmpty(d) Then
    Mark "NO_WRITER"
    Exit Sub
  End If
  d.DocumentProperties.Title = "DUNG-TITLE-THU"
  d.DocumentProperties.Author = "DUNG-AUTHOR-THU"
  Mark "SetTitle done"
  Exit Sub
EH:
  Mark "ERR SetTitle " & Err & " " & Error$
End Sub
)BAS";

std::string xmlEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

void writeFile(const fs::path& p, const std::string& content) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << content;
}

void seedBasicLibrary(const fs::path& profile) {
    const std::string hdr = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    const fs::path b = profile / "user" / "basic";
    writeFile(b / "script.xlc", hdr +
        "<!DOCTYPE library:libraries PUBLIC \"-//OpenOffice.org//DTD OfficeDocument 1.0//EN\" \"libraries.dtd\">\n"
        "<library:libraries xmlns:library=\"http://openoffice.org/2000/library\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
        " <library:library library:name=\"Standard\" xlink:href=\"$(USER)/basic/Standard/script.xlb/\" xlink:type=\"simple\" library:link=\"false\"/>\n"
        "</library:libraries>\n");
    writeFile(b / "dialog.xlc", hdr +
        "<!DOCTYPE library:libraries PUBLIC \"-//OpenOffice.org//DTD OfficeDocument 1.0//EN\" \"libraries.dtd\">\n"
        "<library:libraries xmlns:library=\"http://openoffice.org/2000/library\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
        " <library:library library:name=\"Standard\" xlink:href=\"$(USER)/basic/Standard/dialog.xlb/\" xlink:type=\"simple\" library:link=\"false\"/>\n"
        "</library:libraries>\n");
    writeFile(b / "Standard" / "script.xlb", hdr +
        "<!DOCTYPE library:library PUBLIC \"-//OpenOffice.org//DTD OfficeDocument 1.0//EN\" \"library.dtd\">\n"
        "<library:library xmlns:library=\"http://openoffice.org/2000/library\" library:name=\"Standard\" library:readonly=\"false\" library:passwordprotected=\"false\">\n"
        " <library:element library:name=\"DungMod\"/>\n"
        "</library:library>\n");
    writeFile(b / "Standard" / "dialog.xlb", hdr +
        "<!DOCTYPE library:library PUBLIC \"-//OpenOffice.org//DTD OfficeDocument 1.0//EN\" \"library.dtd\">\n"
        "<library:library xmlns:library=\"http://openoffice.org/2000/library\" library:name=\"Standard\" library:readonly=\"false\" library:passwordprotected=\"false\"/>\n");
    writeFile(b / "Standard" / "DungMod.xba", hdr +
        "<!DOCTYPE script:module PUBLIC \"-//OpenOffice.org//DTD OfficeDocument 1.0//EN\" \"module.dtd\">\n"
        "<script:module xmlns:script=\"http://openoffice.org/2000/script\" script:name=\"DungMod\" script:language=\"StarBasic\">" +
        xmlEscape(kBasic) + "</script:module>\n");
}

std::string readAll(const fs::path& p) {
    std::ifstream i(p, std::ios::binary);
    std::stringstream ss;
    ss << i.rdbuf();
    return ss.str();
}

void setEnv(const char* k, const std::string& v) {
#ifdef _WIN32
    _putenv_s(k, v.c_str());
#else
    setenv(k, v.c_str(), 1);
#endif
}

// Chờ tối đa ms cho tệp dấu xuất hiện chuỗi `needle`.
bool waitMark(const fs::path& mark, const std::string& needle, int ms) {
    for (int t = 0; t < ms; t += 100) {
        if (readAll(mark).find(needle) != std::string::npos) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

void report(const std::string& name, bool ran, const std::string& extra = "") {
    std::cout << (ran ? "[ĐẠT]  " : "[KHÔNG] ") << name << (extra.empty() ? "" : "  {" + extra + "}") << std::endl;
}
}  // namespace

int dungRunBasicBridge(const fs::path& program, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(180));
        std::cout << "[LỖI]  TREO quá 180 giây\nKẾT QUẢ CẦU NỐI BASIC: TREO\n";
        std::cout.flush();
        std::_Exit(3);
    }).detach();

    // Chẩn đoán: bản build engine có mô-đun Basic / scripting không?
    std::cout << "== Mô-đun trong thư mục program (basprov/sb/scriptframe/vba/dlgprov) ==" << std::endl;
    int found = 0;
    for (auto& e : fs::directory_iterator(program, ec)) {
        std::string n = e.path().filename().string();
        for (char& c : n) c = (char)tolower((unsigned char)c);
        if (n.find("basprov") != std::string::npos || n.find("scriptframe") != std::string::npos || n.find("vbaobj") != std::string::npos ||
            n.find("dlgprov") != std::string::npos || n.find("libsb") != std::string::npos || n.find("sblo") != std::string::npos ||
            n.find("stringresource") != std::string::npos) {
            std::cout << "   " << e.path().filename().string() << std::endl;
            ++found;
        }
    }
    std::cout << "   (tổng " << found << " tệp khớp)  share/basic tồn tại: "
              << (fs::exists(program.parent_path() / "share" / "basic") ? "có" : "KHÔNG") << std::endl;
    std::cout << "   share/registry: ";
    for (auto& e : fs::directory_iterator(program.parent_path() / "share" / "registry", ec)) {
        std::string n = e.path().filename().string();
        if (n.find("basic") != std::string::npos || n.find("script") != std::string::npos || n.find("main") != std::string::npos) std::cout << n << " ";
    }
    std::cout << std::endl;

    const fs::path profile = out / "profile";
    seedBasicLibrary(profile);
    const fs::path mark = out / "dung-mark.txt";
    fs::remove(mark, ec);
    setEnv("DUNG_MARK", fs::absolute(mark).string());

    std::string err;
    auto engine = Engine::create(program, profile, out / "work", &err);
    if (!engine) { std::cout << "[LỖI]  khởi động engine: " << err << "\nKẾT QUẢ CẦU NỐI BASIC: LỖI\n"; return 1; }
    auto doc = engine->createNew(DocKind::Text, &err);
    if (!doc) { std::cout << "[LỖI]  tạo tài liệu: " << err << "\nKẾT QUẢ CẦU NỐI BASIC: LỖI\n"; return 1; }
    doc->setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
    doc->typeText(u"Xin chao Dũng Office\n");
    doc->waitInputIdle(300, 3000);

    const std::string kScript = "vnd.sun.star.script:Standard.DungMod.";
    const std::string kScriptTail = "?language=Basic&location=application";
    auto tryPing = [&](const std::string& label, auto&& launch) -> bool {
        fs::remove(mark, ec);
        bool ret = false;
        try { ret = launch(); } catch (...) { std::cout << "   (ngoại lệ khi gọi)" << std::endl; }
        const bool ran = waitMark(mark, "PING ok", 4000);
        report(label, ran, std::string("giá trị trả về=") + (ret ? "true" : "false") + " dấu=" + (ran ? "có" : "không có"));
        return ran;
    };

    std::string method;
    if (tryPing("T1 Engine::runMacro(macro:///Standard.DungMod.Ping)", [&] { return engine->runMacro("macro:///Standard.DungMod.Ping"); })) method = "T1";
    if (tryPing("T2 Engine::runMacro(vnd.sun.star.script:...Ping)", [&] { return engine->runMacro(kScript + "Ping" + kScriptTail); }) && method.empty()) method = "T2";
    if (tryPing("T3 postUnoCommand(vnd.sun.star.script:...Ping)", [&] { doc->command(kScript + "Ping" + kScriptTail); return true; }) && method.empty()) method = "T3";
    if (tryPing("T4 postUnoCommand(macro:///Standard.DungMod.Ping)", [&] { doc->command("macro:///Standard.DungMod.Ping"); return true; }) && method.empty()) method = "T4";
    if (tryPing("T5 postUnoCommand(.uno:RunMacro)", [&] { doc->command(".uno:RunMacro", ""); return true; }) && method.empty()) method = "T5";

    std::cout << "   cách chạy được: " << (method.empty() ? "KHÔNG CÁCH NÀO" : method) << std::endl;
    int rc = 0;
    if (!method.empty()) {
        auto call = [&](const std::string& sub) {
            if (method == "T1") engine->runMacro("macro:///Standard.DungMod." + sub);
            else if (method == "T2") engine->runMacro(kScript + sub + kScriptTail);
            else if (method == "T3") doc->command(kScript + sub + kScriptTail);
            else if (method == "T4") doc->command("macro:///Standard.DungMod." + sub);
        };
        fs::remove(mark, ec);
        call("SetMargins");
        report("SetMargins chạy xong", waitMark(mark, "SetMargins done", 5000));
        call("SetHeader");
        report("SetHeader chạy xong", waitMark(mark, "SetHeader done", 5000));
        call("SetTitle");
        report("SetTitle chạy xong", waitMark(mark, "SetTitle done", 5000));
        doc->waitInputIdle(200, 1000);
        const fs::path docx = out / "basic-bridge.docx";
        fs::remove(docx, ec);
        auto r = doc->saveAs(docx);
        report("Lưu DOCX", r.ok, r.message);
        if (!r.ok) rc = 1;
    }
    std::cout << "== Nội dung tệp dấu của macro ==\n" << readAll(mark) << "== hết ==" << std::endl;
    std::cout << (method.empty() ? "\nKẾT QUẢ CẦU NỐI BASIC: KHÔNG CHẠY ĐƯỢC MACRO\n" : "\nKẾT QUẢ CẦU NỐI BASIC: MACRO CHẠY ĐƯỢC (xem verify_basic_bridge.py cho tệp DOCX)\n");
    std::cout.flush();
    std::_Exit(rc);
}
