// SPDX-License-Identifier: MPL-2.0
// Kiểm thử trên engine thật: (1) bản phục hồi (tự lưu) không đổi cờ "đã sửa", mở lại được, chữ còn nguyên;
// (2) đường dẫn dài (quanh MAX_PATH của Windows): mở, lưu bản sao, tệp gốc không đổi byte.
// Chạy:  dung-smoketest --program <program> --recover 1 --out <thư mục>
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using namespace dung;
namespace fs = std::filesystem;

namespace {
int g_bad = 0;
std::atomic<long long> g_t{0};
std::string g_cur = "khởi động";
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void step(const std::string& s) { g_cur = s; g_t = nowMs(); std::cout << "  -> " << s << std::endl; }
void check(bool ok, const std::string& what) {
    std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what << std::endl;
    if (!ok) ++g_bad;
}
std::vector<char> readAll(const fs::path& p) {
    std::ifstream f(longPath(p), std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(f), {});
}
bool isZip(const fs::path& p) {
    const auto b = readAll(p);
    return b.size() > 100 && b[0] == 'P' && b[1] == 'K';
}
size_t chars(const fs::path& p) { return fs::absolute(p).native().size(); }

// Dựng thư mục + tên tệp sao cho đường dẫn tuyệt đối tới tệp dài đúng `want` ký tự (nếu có thể).
bool buildLongPath(const fs::path& base, size_t want, const std::string& fileStem, const std::string& ext, fs::path& dirOut,
                   fs::path& fileOut, size_t& real) {
    const fs::path root = fs::absolute(base);
    const std::string name = fileStem + "." + ext;
    long rem = (long)want - (long)root.native().size() - 1 - (long)fs::u8path(name).native().size();
    if (rem < 2) return false;
    fs::path dir = root;
    while (rem > 0) {
        long t = rem > 201 ? 150 : rem;
        if (rem - t == 1) t = rem - 2;
        dir /= std::string(size_t(t - 1), 'a');
        rem -= t;
    }
    dirOut = dir;
    fileOut = dir / fs::u8path(name);
    real = chars(fileOut);
    return true;
}
}  // namespace

int dungRunRecoverTests(const fs::path& program, const fs::path& out) {
    std::error_code ec;
    fs::remove_all(longPath(out / "lp"), ec);
    fs::create_directories(out, ec);
    g_t = nowMs();
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (nowMs() - g_t > 120000) {
                std::cout << "[LỖI]  TREO quá 120 giây ở bước: " << g_cur << "\n\nKẾT QUẢ: CÓ LỖI (treo)" << std::endl;
                std::_Exit(3);
            }
        }
    }).detach();

    std::string err;
    step("khởi động engine");
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    check((bool)engine, "Khởi động engine " + err);
    if (!engine) return 1;

    // ---------- Phần 1: bản phục hồi ----------
    step("tạo tài liệu Word, gõ chữ");
    auto d = engine->createNew(DocKind::Text, &err);
    check((bool)d, "Tạo tài liệu mới " + err);
    if (!d) return 1;
    d->typeText(u"Xin chào Dũng Office - bản phục hồi");
    const bool gotMod = d->waitFor(LOK_CALLBACK_STATE_CHANGED,
                                   [](const std::string& p) { return p.find(".uno:ModifiedStatus=true") != std::string::npos; }, 10000);
    d->waitInputIdle(300, 4000);
    check(gotMod || d->modified(), "Engine báo tài liệu đã sửa sau khi gõ");
    check(d->modified(), "Document::modified() = true trước khi tạo bản phục hồi");

    step("ghi bản phục hồi");
    const fs::path rec = out / "rec" / "ban-phuc-hoi.docx";
    std::string why;
    const bool okRec = d->saveRecoveryCopy(rec, &why);
    check(okRec, "saveRecoveryCopy ghi được: " + why);
    check(okRec && isZip(rec), "Bản phục hồi là tệp DOCX hợp lệ (ZIP)");
    check(d->modified(), "Cờ 'đã sửa' vẫn là true sau khi tạo bản phục hồi (không bị coi là lần lưu)");
    check(d->suggestSaveAsPath().parent_path() != out / "rec", "Tài liệu mới: gợi ý Lưu thành không nằm trong thư mục phục hồi");

    step("tự lưu thất bại không làm mất bản phục hồi trước");
    const auto firstRecovery = readAll(rec);
    const fs::path blockedParent = out / "recovery-parent-is-file";
    { std::ofstream block(longPath(blockedParent), std::ios::binary); block << "BLOCK"; }
    why.clear();
    check(!d->saveRecoveryCopy(blockedParent / "invalid.docx", &why) &&
          why.find("Không tạo được thư mục phục hồi") != std::string::npos,
          "Báo đúng lỗi thư mục đích bị chặn: " + why);
    check(readAll(rec) == firstRecovery && d->modified(),
          "Bản phục hồi trước và cờ chưa lưu giữ nguyên");
    const auto blockedDest = out / "rec" / "directory.docx";
    fs::create_directories(longPath(blockedDest), ec);
    const auto sentinel = blockedDest / "keep.txt";
    { std::ofstream keep(longPath(sentinel), std::ios::binary); keep << "KEEP"; }
    why.clear();
    check(!d->saveRecoveryCopy(blockedDest, &why) && !why.empty(),
          "Không thay thế thư mục bằng tệp phục hồi: " + why);
    check(readAll(sentinel) == std::vector<char>({'K','E','E','P'}),
          "Dữ liệu trong thư mục đích giữ nguyên");
    int leftovers = 0;
    for (const auto& item : fs::directory_iterator(longPath(rec.parent_path())))
        if (item.path().filename().u8string().rfind("~rec-", 0) == 0) ++leftovers;
    check(leftovers == 0, "Không sót tệp tạm phục hồi cạnh đích");
    check(readAll(rec) == firstRecovery && d->modified(),
          "Bản phục hồi trước còn nguyên sau cả hai lỗi");

    step("mở lại bản phục hồi");
    auto d2 = engine->open(rec, &err);
    check((bool)d2, "Mở lại bản phục hồi " + err);
    if (d2) {
        d2->command(".uno:SelectAll");
        d2->waitInputIdle(300, 3000);
        const std::string text = d2->selectionAs("text/plain;charset=utf-8");
        check(text.find("D\xC5\xA9ng Office") != std::string::npos, "Chữ tiếng Việt còn nguyên trong bản phục hồi: [" + text.substr(0, 60) + "]");
        fs::create_directories(out / "goc", ec);
        d2->setRecoveredOrigin(out / "goc" / "Hop dong.docx", "Hop dong.docx");
        check(d2->isRecovered() && d2->modified(), "Bản khôi phục được đánh dấu chưa lưu");
        check(d2->displayName() == "Hop dong.docx", "Tên hiển thị là tên tệp gốc");
        const fs::path sg = d2->suggestSaveAsPath();
        check(sg.parent_path() == out / "goc", "Gợi ý Lưu thành nằm cạnh tệp gốc cũ, không phải thư mục phục hồi");
        check(fs::absolute(rec) != fs::absolute(sg), "Không gợi ý ghi đè chính bản phục hồi");
    }
    d2.reset();

    // ---------- Phần 2: đường dẫn dài ----------
    step("tạo tệp nguồn ngắn");
    const fs::path srcShort = out / "nguon-ngan.docx";
    {
        SaveOptions so;
        auto r = d->saveAs(srcShort, so);
        check(r.ok, "Lưu tệp nguồn ngắn: " + r.message);
    }
    d.reset();
    check(isZip(srcShort), "Tệp nguồn ngắn là DOCX hợp lệ");

    const std::vector<size_t> lens = {200, 230, 245, 250, 255, 259, 262, 270, 300};
    for (size_t want : lens) {
        const std::string tag = "dài " + std::to_string(want) + ": ";
        step("đường dẫn " + std::to_string(want));
        fs::path dir, srcLong, tgt;
        size_t real = 0, realT = 0;
        fs::path dummy;
        if (!buildLongPath(out / "lp" / ("d" + std::to_string(want)), want, "nguon-vi\xE1\xBB\x87t", "docx", dir, srcLong, real)) {
            std::cout << "[BỎ QUA] " << tag << "thư mục gốc quá dài để dựng đúng độ dài này\n";
            continue;
        }
        fs::create_directories(longPath(dir), ec);
        if (ec) { check(false, tag + "bộ kiểm thử không tạo được thư mục (" + ec.message() + ")"); continue; }
        fs::copy_file(longPath(srcShort), longPath(srcLong), fs::copy_options::overwrite_existing, ec);
        if (ec) { check(false, tag + "bộ kiểm thử không chép được tệp nguồn (" + ec.message() + ")"); continue; }
        const auto before = readAll(srcLong);
        std::cout << "       độ dài thật của đường dẫn tệp nguồn: " << real << " ký tự\n";

        std::string oerr;
        auto dl = engine->open(srcLong, &oerr);
        check((bool)dl, tag + "mở tệp gốc đường dẫn dài " + oerr);
        if (dl) {
            // đích: cùng độ dài tổng với nguồn
            tgt = dir / fs::u8path("kq-kkkkkkk.docx"); // 15 ký tự = cùng độ dài tên với tệp nguồn "nguon-việt.docx"
            realT = chars(tgt);
            auto r = dl->saveAs(tgt);
            check(r.ok, tag + "lưu bản sao (đích " + std::to_string(realT) + " ký tự): " + r.message);
            std::error_code e2;
            const bool exists = r.ok && fs::exists(longPath(tgt), e2) && isZip(tgt);
            check(!r.ok || exists, tag + "tệp đích có thật và là DOCX hợp lệ");
            // Không còn tệp tạm dở dang cạnh đích
            int leftovers = 0;
            for (auto& e : fs::directory_iterator(longPath(dir), e2))
                if (e.path().filename().u8string().rfind("~dung-", 0) == 0) ++leftovers;
            check(leftovers == 0, tag + "không sót tệp tạm cạnh đích");
        }
        dl.reset();
        check(readAll(srcLong) == before, tag + "tệp gốc không đổi byte");
    }
    fs::remove_all(longPath(out / "lp"), ec);

    std::cout << (g_bad ? "\nKẾT QUẢ: CÓ LỖI\n" : "\nKẾT QUẢ: TẤT CẢ ĐẠT\n");
    std::cout.flush();
    std::_Exit(g_bad ? 1 : 0);
}
