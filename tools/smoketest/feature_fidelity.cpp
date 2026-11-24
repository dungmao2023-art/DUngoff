// SPDX-License-Identifier: MPL-2.0
// Thí nghiệm "giữ nguyên tính năng": với mỗi DOCX/XLSX/PPTX trong thư mục, mở bằng engine, SỬA MỘT CHỖ,
// rồi lưu bản sao cùng định dạng. tests/features/verify_features.py so sánh từng tính năng trước/sau.
// Chạy:  dung-smoketest --program <program> --features <thư mục tệp> --out <thư mục kết quả>
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace dung;
namespace fs = std::filesystem;

namespace {
std::atomic<long long> g_t{0};
std::string g_cur = "khởi động";
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void settle(int ms = 400) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
std::string lowerExt(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext;
}
}  // namespace

int dungRunFeatureFidelity(const fs::path& program, const fs::path& dir, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out / "saved", ec);
    g_t = nowMs();
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t > 120 * 1000LL) {
                std::cout << "[LỖI]  TREO quá 120 giây ở: " << g_cur << "\nKẾT QUẢ TÍNH NĂNG: CÓ LỖI (treo)\n" << std::flush;
                std::_Exit(3);
            }
        }
    }).detach();

    std::string err;
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) { std::cout << "[LỖI]  Khởi động engine " << err << "\n"; return 1; }
    std::cout << "Engine: " << engine->versionInfo() << "\n";

    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        const std::string ext = lowerExt(e.path());
        if (ext == ".docx" || ext == ".xlsx" || ext == ".pptx") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());

    int bad = 0;
    for (const auto& f : files) {
        g_t = nowMs();
        const std::string name = f.filename().u8string();
        g_cur = name;
        std::string e2;
        auto d = engine->open(f, &e2);
        if (!d) { std::cout << "[LỖI]  mở " << name << ": " << e2 << "\n"; ++bad; continue; }
        std::cout << "[ĐẠT]  mở " << name << " (loại " << (int)d->kind() << ", chỉ đọc: " << (d->readOnly() ? "có" : "không")
                  << ", số phần: " << d->partCount() << ")\n";
        d->setClientView(512, 512, 7680, 7680, 0, 0, 7680, 7680);
        settle();

        // SỬA MỘT CHỖ
        std::string edit;
        switch (d->kind()) {
        case DocKind::Text:
            d->command(".uno:GoToStartOfDoc"); settle();
            d->typeText(u"[SUA] "); settle();
            edit = "gõ '[SUA] ' ở đầu văn bản";
            break;
        case DocKind::Spreadsheet:
            d->command(".uno:GoToCell", "{\"ToPoint\":{\"type\":\"string\",\"value\":\"Z1\"}}"); settle();
            d->typeText(u"SUA\n"); settle(800);
            edit = "gõ 'SUA' vào ô Z1";
            break;
        case DocKind::Presentation:
            d->command(".uno:InsertPage"); settle(800);
            edit = "chèn thêm một slide";
            break;
        default: edit = "không sửa"; break;
        }
        std::cout << "       sửa: " << edit << "; engine báo đã sửa: " << (d->modified() ? "có" : "không") << "\n";

        const fs::path target = out / "saved" / fs::u8path(f.filename().u8string());
        fs::remove(target, ec);
        SaveOptions so; // lưu bản sao cùng định dạng; không ghi đè gốc
        auto r = d->saveAs(target, so);
        if (r.ok) std::cout << "[ĐẠT]  lưu bản sao " << name << " (" << fs::file_size(target, ec) << " byte)\n";
        else { std::cout << "[LỖI]  lưu bản sao " << name << ": " << r.message << "\n"; ++bad; }
    }
    std::cout << (bad ? "\nKẾT QUẢ TÍNH NĂNG: CÓ LỖI MỞ/LƯU\n" : "\nKẾT QUẢ TÍNH NĂNG: ĐÃ MỞ VÀ LƯU TẤT CẢ (đối chiếu tính năng bằng verify_features.py)\n");
    std::cout.flush();
    std::_Exit(bad ? 1 : 0);
}
