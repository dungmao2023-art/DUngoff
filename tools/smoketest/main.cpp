// dung-smoketest — kiểm thử engine Dũng Office trên runtime LibreOffice thật.
// Không dùng giao diện. Chạy được trên Windows và Linux.
//
//   dung-smoketest --program <thư mục program của LibreOffice> --xlsx mau.xlsx [--csv mau.csv] --out <thư mục>
//
// Thoát mã 0 nếu mọi bước đạt.
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW
#endif

using namespace dung;

static int g_fail = 0;
static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what << std::endl;
    if (!ok) ++g_fail;
}

static std::vector<char> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(f), {});
}

static void writeBmp(const fs::path& p, const std::vector<uint8_t>& bgra, int w, int h) {
    std::ofstream f(p, std::ios::binary);
    uint32_t dataSize = (uint32_t)(w * h * 4), fileSize = 54 + dataSize;
    uint8_t hdr[54] = {'B', 'M'};
    std::memcpy(hdr + 2, &fileSize, 4);
    uint32_t off = 54, dib = 40, planesBpp = 1 | (32 << 16);
    int32_t hh = -h; // top-down
    std::memcpy(hdr + 10, &off, 4);
    std::memcpy(hdr + 14, &dib, 4);
    std::memcpy(hdr + 18, &w, 4);
    std::memcpy(hdr + 22, &hh, 4);
    std::memcpy(hdr + 26, &planesBpp, 4);
    std::memcpy(hdr + 34, &dataSize, 4);
    f.write((const char*)hdr, 54);
    f.write((const char*)bgra.data(), dataSize);
}

static void gotoCell(Document& d, const std::string& ref) {
    d.command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"" + ref + "\"}}");
}

static bool waitAddress(Document& d, const std::string& addr, int ms = 15000) {
    return d.waitFor(LOK_CALLBACK_CELL_ADDRESS, [&](const std::string& p) { return p == addr; }, ms);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    // Lấy đối số dạng Unicode để hỗ trợ đường dẫn tiếng Việt.
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    std::vector<std::string> args;
    for (int i = 0; i < wargc; ++i) args.push_back(fs::path(wargv[i]).u8string());
    LocalFree(wargv);
#else
    std::vector<std::string> args(argv, argv + argc);
#endif
    fs::path program, xlsx, csv, out, docx, pptx;
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (args[i] == "--program") program = fromUtf8(args[i + 1]);
        else if (args[i] == "--xlsx") xlsx = fromUtf8(args[i + 1]);
        else if (args[i] == "--csv") csv = fromUtf8(args[i + 1]);
        else if (args[i] == "--out") out = fromUtf8(args[i + 1]);
        else if (args[i] == "--docx") docx = fromUtf8(args[i + 1]);
        else if (args[i] == "--pptx") pptx = fromUtf8(args[i + 1]);
    }
    if (program.empty() || xlsx.empty() || out.empty()) {
        std::cerr << "Cách dùng: dung-smoketest --program <dir> --xlsx <tệp> [--csv <tệp>] --out <dir>\n";
        return 2;
    }
    std::error_code ec;
    fs::create_directories(out, ec);

    // Đặt tệp mẫu vào thư mục có tên tiếng Việt để kiểm tra đường dẫn Unicode.
    fs::path viDir = out / fs::u8path("Tài liệu thử");
    fs::create_directories(viDir, ec);
    if (ec || !fs::is_directory(viDir))
        std::cout << "       [chẩn đoán] tạo thư mục '" << viDir.u8string() << "' lỗi: " << ec.message() << std::endl;
    if (!fs::exists(xlsx))
        std::cout << "       [chẩn đoán] không thấy tệp mẫu '" << fs::absolute(xlsx).u8string() << "' (cwd="
                  << fs::current_path().u8string() << ")" << std::endl;
    fs::path original = viDir / fs::u8path("Báo cáo doanh thu.xlsx");
    fs::copy_file(xlsx, original, fs::copy_options::overwrite_existing, ec);
    check(!ec, "Chuẩn bị tệp gốc trong thư mục tiếng Việt" + (ec ? ": " + ec.message() + " (" + xlsx.u8string() +
                                                                 " -> " + original.u8string() + ")" : std::string()));
    const auto originalBytes = readAll(original);

    std::string err;
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine LibreOfficeKit " + err);
    if (!engine) return 1;
    std::cout << "       Phiên bản engine: " << engine->versionInfo() << std::endl;

    // ---------------- Mở XLSX ----------------
    auto doc = engine->open(original, &err);
    check((bool)doc, "Mở XLSX " + err);
    if (!doc) return 1;
    check(doc->kind() == DocKind::Spreadsheet, "Nhận diện loại tài liệu = Excel");
    check(!doc->readOnly(), "XLSX mở ở chế độ chỉnh sửa");
    check(doc->workingCopy() != original, "Engine nạp bản làm việc tạm, không nạp tệp gốc");
    check(doc->partCount() == 2, "Có 2 sheet (thực tế: " + std::to_string(doc->partCount()) + ")");
    check(doc->partName(0) == "Doanh thu" && doc->partName(1) == fs::u8path("Ghi chú").u8string(),
          "Tên sheet tiếng Việt: '" + doc->partName(0) + "', '" + doc->partName(1) + "'");

    // ---------------- Render ----------------
    const int px = 512;
    std::vector<uint8_t> buf(px * px * 4, 0);
    const int twips = px * 15; // 100% zoom: 1px = 15 twip
    doc->setClientView(px, px, twips, twips, 0, 0, twips, twips);
    doc->paintTile(buf.data(), px, px, 0, 0, twips, twips);
    size_t nonWhite = 0;
    for (size_t i = 0; i < buf.size(); i += 4)
        if (buf[i] < 200 || buf[i + 1] < 200 || buf[i + 2] < 200) ++nonWhite;
    writeBmp(out / "tile-sheet1.bmp", buf, px, px);
    check(nonWhite > 500, "Render tile có nội dung (" + std::to_string(nonWhite) + " điểm ảnh tối)");

    // ---------------- Chỉnh sửa ----------------
    doc->clearEvents();
    gotoCell(*doc, "F2");
    check(waitAddress(*doc, "F2"), "Di chuyển con trỏ ô tới F2");
    doc->typeText(u"Dũng\n");
    check(waitAddress(*doc, "F3"), "Nhập 'Dũng' vào F2 (con trỏ xuống F3)");
    doc->typeText(u"=D5*2\n");
    check(waitAddress(*doc, "F4"), "Nhập công thức '=D5*2' vào F3");
    gotoCell(*doc, "F3");
    bool formulaSeen = doc->waitFor(LOK_CALLBACK_CELL_FORMULA,
                                    [](const std::string& p) { return p == "=D5*2"; }, 15000);
    check(formulaSeen, "Thanh công thức báo đúng '=D5*2' (thực tế: '" +
                           doc->lastPayload(LOK_CALLBACK_CELL_FORMULA) + "')");
    // Thanh công thức: nhập qua .uno:EnterString (chuỗi có dấu ngoặc kép và tiếng Việt)
    doc->clearEvents();
    gotoCell(*doc, "G2");
    check(waitAddress(*doc, "G2"), "Di chuyển tới G2");
    doc->command(".uno:EnterString", Document::stringArg("StringName", "=\"Tổng: \"&D5"));
    gotoCell(*doc, "G3");
    waitAddress(*doc, "G3");
    gotoCell(*doc, "G2");
    check(doc->waitFor(LOK_CALLBACK_CELL_FORMULA,
                       [](const std::string& p) { return p == "=\"Tổng: \"&D5"; }, 15000),
          "Nhập công thức từ thanh công thức (EnterString): " + doc->lastPayload(LOK_CALLBACK_CELL_FORMULA));

    // Clipboard: lấy nội dung ô đang chọn dạng văn bản
    gotoCell(*doc, "A2");
    waitAddress(*doc, "A2");
    std::string sel = doc->selectionAs("text/plain;charset=utf-8");
    while (!sel.empty() && (sel.back() == '\n' || sel.back() == '\r')) sel.pop_back();
    check(sel == "Bút bi", "Sao chép ô A2 ra clipboard dạng văn bản: '" + sel + "'");

    for (int i = 0; i < 50 && !doc->modified(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(doc->modified(), "Engine báo tài liệu đã thay đổi");

    // ---------------- Quy tắc an toàn khi lưu ----------------
    auto r1 = doc->saveAs(original);
    check(!r1.ok, "Từ chối ghi đè tệp gốc khi chưa xác nhận: " + r1.message);
    auto r2 = doc->saveAs(viDir / "khac.xls");
    check(!r2.ok, "Từ chối ghi định dạng chưa bảo toàn (.xls): " + r2.message);
    auto r3 = doc->saveAs(viDir / "ban-in.pdf");
    check(!r3.ok, "Xuất PDF cần xác nhận: " + r3.message);
    {
        std::ofstream(viDir / "da-co.xlsx") << "x";
        auto r4 = doc->saveAs(viDir / "da-co.xlsx");
        check(!r4.ok, "Từ chối thay tệp đích đã tồn tại khi chưa xác nhận");
    }

    // ---------------- Lưu thành (mặc định) ----------------
    fs::path suggested = doc->suggestSaveAsPath();
    check(suggested != original && suggested.extension() == ".xlsx",
          "Gợi ý Lưu thành tệp mới: " + suggested.filename().u8string());
    auto saved = doc->saveAs(suggested);
    check(saved.ok, "Lưu thành XLSX: " + saved.message);
    check(readAll(original) == originalBytes, "Tệp gốc giữ nguyên từng byte sau khi lưu");
    {
        auto again = doc->saveAs(suggested); // Ctrl+S lần 2 vào chính tệp phiên này đã tạo
        check(again.ok && again.backup.empty(), "Lưu lại (Ctrl+S) vào tệp của phiên: không hỏi, không tạo sao lưu thừa");
    }
    {
        // Không để lại tệp tạm "~dung-*"
        bool leftover = false;
        for (auto& e : fs::directory_iterator(viDir))
            if (e.path().filename().u8string().rfind("~dung-", 0) == 0) leftover = true;
        check(!leftover, "Không để lại tệp tạm sau khi lưu");
    }
    SaveOptions exp;
    exp.allowLossyExport = true;
    auto pdf = doc->saveAs(viDir / "ban-in.pdf", exp);
    check(pdf.ok, "Xuất PDF khi đã xác nhận: " + pdf.message);

    // Ghi đè có xác nhận: phải tạo bản sao lưu trước
    {
        SaveOptions rep;
        rep.allowReplaceExisting = true;
        auto r5 = doc->saveAs(viDir / "da-co.xlsx", rep);
        check(r5.ok && fs::exists(r5.backup), "Thay tệp đã tồn tại có xác nhận -> có bản sao lưu: " +
                                                   r5.backup.filename().u8string());
    }

    // Ghi kết quả cho script Python kiểm chứng round-trip
    std::ofstream(out / "saved-path.txt") << suggested.u8string();
    std::cout << "       Đóng tài liệu..." << std::endl;
    doc.reset();
    std::cout << "       Đã đóng." << std::endl;

    // ---------------- CSV: chỉ đọc ----------------
    if (!csv.empty()) {
        auto c = engine->open(csv, &err);
        check((bool)c, "Mở CSV " + err);
        if (c) {
            check(c->readOnly(), "CSV mở ở chế độ chỉ đọc (chưa bảo toàn kiểu dữ liệu khi ghi)");
            check(!c->warnings().empty(), "Có cảnh báo cho người dùng khi mở CSV");
            auto r = c->saveAs(out / "csv-thanh.xlsx");
            check(!r.ok, "Tài liệu chỉ đọc không được lưu khi chưa xác nhận: " + r.message);
        }
    }

    // ---------------- Word / PowerPoint / PDF: mở, nhận loại, render ----------------
    auto renderCheck = [&](Document& d, const std::string& label) {
        const int n = 384;
        std::vector<uint8_t> b(n * n * 4, 0);
        long w = 0, h = 0;
        d.documentSize(w, h);
        const int tw = (int)std::min<long>(std::max<long>(w, 1), 12000);
        d.paintTile(b.data(), n, n, 0, 0, tw, tw);
        size_t dark = 0;
        for (size_t i = 0; i < b.size(); i += 4)
            if (b[i] < 160 && b[i + 1] < 160 && b[i + 2] < 160) ++dark;
        writeBmp(out / ("tile-" + label + ".bmp"), b, n, n);
        check(dark > 50, "Render " + label + " có nội dung (" + std::to_string(dark) + " điểm ảnh tối)");
    };
    if (!docx.empty()) {
        auto d = engine->open(docx, &err);
        check(d && d->kind() == DocKind::Text && !d->readOnly(), "Mở DOCX -> Word, chỉnh sửa được " + err);
        if (d) {
            renderCheck(*d, "docx");
            auto r = d->saveAs(out / fs::u8path("Văn bản - Dũng Office.docx"));
            check(r.ok, "Lưu thành DOCX: " + r.message);
        }
    }
    if (!pptx.empty()) {
        auto d = engine->open(pptx, &err);
        check(d && d->kind() == DocKind::Presentation && d->partCount() == 2,
              "Mở PPTX -> PowerPoint, 2 trang chiếu " + err);
        if (d) {
            renderCheck(*d, "pptx");
            auto r = d->saveAs(out / fs::u8path("Trình chiếu - Dũng Office.pptx"));
            check(r.ok, "Lưu thành PPTX: " + r.message);
        }
    }
    {
        fs::path pdfPath = viDir / "ban-in.pdf";
        auto d = engine->open(pdfPath, &err);
        check(d && d->kind() == DocKind::Drawing && d->readOnly(), "Mở PDF -> chế độ xem (chỉ đọc) " + err);
        if (d) {
            renderCheck(*d, "pdf");
            auto r = d->saveAs(out / "pdf-sua.pdf");
            check(!r.ok, "PDF: không ghi lại khi chưa xác nhận: " + r.message);
        }
    }

    // ---------------- Tạo mới ----------------
    auto nw = engine->createNew(DocKind::Spreadsheet, &err);
    check((bool)nw && nw->kind() == DocKind::Spreadsheet, "Tạo sổ làm việc mới " + err);
    if (nw) {
        auto r = nw->saveAs(out / fs::u8path("Sổ mới.xlsx"));
        check(r.ok, "Lưu sổ làm việc mới: " + r.message);
    }
    auto nt = engine->createNew(DocKind::Text, &err);
    check(nt && nt->kind() == DocKind::Text && nt->saveAs(out / fs::u8path("Tài liệu mới.docx")).ok, "Tạo + lưu tài liệu Word mới " + err);
    auto np = engine->createNew(DocKind::Presentation, &err);
    check(np && np->kind() == DocKind::Presentation && np->saveAs(out / fs::u8path("Trình chiếu mới.pptx")).ok,
          "Tạo + lưu bản trình chiếu mới " + err);

    std::cout << (g_fail ? "\nKẾT QUẢ: CÓ LỖI (" + std::to_string(g_fail) + ")\n" : "\nKẾT QUẢ: TẤT CẢ ĐẠT\n");
    std::cout.flush();
    // LibreOffice không hỗ trợ tắt rồi khởi tạo lại trong cùng tiến trình; thoát nhanh.
    std::_Exit(g_fail ? 1 : 0);
}
