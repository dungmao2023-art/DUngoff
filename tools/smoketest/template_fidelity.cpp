// SPDX-License-Identifier: MPL-2.0
// Kiểm thử toàn vẹn "hiển thị ↔ in/trình chiếu" trên thư viện mẫu thật.
// Với mỗi DOCX/PPTX trong thư mục: mở bằng engine, ghi số trang/slide mà engine hiển thị, xuất PDF (đường in),
// rồi ghi fidelity.csv. tests/templates/verify_fidelity.py đối chiếu số trang và nội dung chữ giữa tệp gốc và PDF.
// Chạy:  dung-smoketest --program <program> --templates <thư mục mẫu> --out <thư mục kết quả>
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

using namespace dung;
namespace fs = std::filesystem;

namespace {
std::atomic<long long> g_t{0};
std::string g_cur = "khởi động";
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::string csvq(const std::string& s) {
    std::string o = "\"";
    for (char c : s) { if (c == '"') o += '"'; o += c; }
    return o + "\"";
}
}  // namespace

int dungRunTemplateFidelity(const fs::path& program, const fs::path& dir, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out / "pdf", ec);
    g_t = nowMs();
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t > 90 * 1000LL) {
                std::cout << "[LỖI]  TREO quá 90 giây ở: " << g_cur << "\nKẾT QUẢ MẪU: CÓ LỖI (treo)\n" << std::flush;
                std::_Exit(3);
            }
        }
    }).detach();

    std::string err;
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) { std::cout << "[LỖI]  Khởi động engine " << err << "\n"; return 1; }

    std::vector<fs::path> files;
    for (auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (ext == ".docx" || ext == ".pptx") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    std::ofstream csv(out / "fidelity.csv", std::ios::binary);
    csv << "index,relpath,kind,parts,open_ok,pdf_ok,pdf_bytes\n";
    int bad = 0, n = 0;
    for (const auto& f : files) {
        ++n;
        g_t = nowMs();
        const std::string rel = fs::relative(f, dir, ec).generic_u8string();
        g_cur = rel;
        std::string e2;
        auto d = engine->open(f, &e2);
        int parts = 0;
        bool openOk = (bool)d, pdfOk = false;
        long long bytes = 0;
        std::string kind = "?";
        if (d) {
            kind = kindName(d->kind());
            parts = d->partCount();
            const fs::path pdf = out / "pdf" / (std::to_string(n) + ".pdf");
            SaveOptions so;
            so.allowLossyExport = true; // xuất PDF = đường in, đã được xác nhận
            auto r = d->saveAs(pdf, so);
            pdfOk = r.ok;
            if (pdfOk) { bytes = (long long)fs::file_size(pdf, ec); }
            else std::cout << "[LỖI]  xuất PDF " << rel << ": " << r.message << "\n";
        } else {
            std::cout << "[LỖI]  mở " << rel << ": " << e2 << "\n";
        }
        d.reset();
        if (!openOk || !pdfOk || parts <= 0 || bytes <= 0) ++bad;
        csv << n << "," << csvq(rel) << "," << kind << "," << parts << "," << openOk << "," << pdfOk << "," << bytes << "\n";
        csv.flush();
    }
    std::cout << "Đã thử " << n << " mẫu; lỗi mở/xuất PDF/số trang: " << bad << "\n";
    std::cout << (bad ? "KẾT QUẢ MẪU: CÓ LỖI\n" : "KẾT QUẢ MẪU: TẤT CẢ MỞ VÀ XUẤT PDF ĐƯỢC\n") << std::flush;
    std::_Exit(bad ? 1 : 0);
}
