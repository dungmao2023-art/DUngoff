// dung-inventory — kiểm kê hiệu năng và an toàn dữ liệu của engine (không sửa engine).
//   perf <program> <tệp> <out>          : đo khởi động/mở/vẽ/lưu/RAM đỉnh
//   safety <program> <tệp> <out>        : tệp gốc không đổi, đích không ghi được, tệp tạm
//   victim <program> <tệp> <đích> <out> : lưu lặp lại (để script khác giết tiến trình giữa chừng)
//   longpath <program> <tệp> <out>      : đường dẫn dài + tên tiếng Việt
// Mỗi kết quả in một dòng "INV <khoá>=<giá trị>" để tập hợp báo cáo.
// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace dung;

static long long ms(std::chrono::steady_clock::time_point a) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - a).count();
}
static void inv(const std::string& k, const std::string& v) { std::cout << "INV " << k << "=" << v << std::endl; }
static long long peakKb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return (long long)(pmc.PeakWorkingSetSize / 1024);
    return -1;
#else
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("VmHWM:", 0) == 0) return std::atoll(line.c_str() + 6);
    return -1;
#endif
}
static std::vector<char> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(f), {});
}
static int countTemp(const fs::path& dir) {
    int n = 0;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir, ec))
        if (e.path().filename().u8string().rfind("~dung-", 0) == 0) ++n;
    return n;
}
static std::string ext(const fs::path& p) {
    std::string e = p.extension().u8string();
    return e.empty() ? e : e.substr(1);
}
static fs::path outName(const fs::path& src, const fs::path& dir, const std::string& tag) {
    return dir / fs::u8path(src.stem().u8string() + " - " + tag + "." + ext(src));
}

static int perf(const fs::path& program, const fs::path& file, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    const std::string name = file.filename().u8string();
    inv("tep", name);
    inv("kich_thuoc_tep_kb", std::to_string((long long)(fs::file_size(file, ec) / 1024)));
    inv("ram_dinh_truoc_kb", std::to_string(peakKb()));
    std::string err;
    auto t0 = std::chrono::steady_clock::now();
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) { inv("loi", "khoi dong engine: " + err); return 1; }
    inv("khoi_dong_engine_ms", std::to_string(ms(t0)));
    inv("ram_dinh_sau_khoi_dong_kb", std::to_string(peakKb()));
    t0 = std::chrono::steady_clock::now();
    auto d = engine->open(file, &err);
    if (!d) { inv("loi", "mo tep: " + err); return 1; }
    inv("mo_tep_ms", std::to_string(ms(t0)));
    inv("ram_dinh_sau_mo_kb", std::to_string(peakKb()));
    inv("so_phan", std::to_string(d->partCount()));
    {
        const int n = 512;
        std::vector<uint8_t> b(n * n * 4, 0);
        t0 = std::chrono::steady_clock::now();
        d->setClientView(n, n, n * 15, n * 15, 0, 0, n * 15, n * 15);
        d->paintTile(b.data(), n, n, 0, 0, n * 15, n * 15);
        inv("ve_man_hinh_dau_ms", std::to_string(ms(t0)));
    }
    inv("ram_dinh_sau_ve_kb", std::to_string(peakKb()));
    fs::path target = outName(file, out, "ban sao");
    fs::remove(target, ec);
    const auto before = readAll(file);
    t0 = std::chrono::steady_clock::now();
    auto r = d->saveAs(target);
    inv("luu_ban_sao_ms", std::to_string(ms(t0)));
    inv("luu_ban_sao_ok", r.ok ? "1" : "0");
    if (!r.ok) inv("luu_ban_sao_loi", r.message);
    inv("kich_thuoc_ban_sao_kb", std::to_string((long long)(fs::file_size(target, ec) / 1024)));
    inv("ram_dinh_sau_luu_kb", std::to_string(peakKb()));
    inv("tep_goc_nguyen_byte", readAll(file) == before ? "1" : "0");
    inv("tep_tam_con_lai", std::to_string(countTemp(out)));
    std::cout.flush();
    std::_Exit(0);
}

static int safety(const fs::path& program, const fs::path& file, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    std::string err;
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) { inv("loi", "khoi dong engine: " + err); return 1; }
    const auto before = readAll(file);
    auto d = engine->open(file, &err);
    if (!d) { inv("loi", "mo tep: " + err); return 1; }

    // 1) Lưu bản sao rồi so tệp gốc từng byte
    fs::path ok1 = outName(file, out, "an toan");
    fs::remove(ok1, ec);
    auto r1 = d->saveAs(ok1);
    inv("s1_luu_ban_sao_ok", r1.ok ? "1" : "0");
    inv("s1_tep_goc_nguyen_byte", readAll(file) == before ? "1" : "0");

    // 2) Từ chối ghi đè tệp gốc
    auto r2 = d->saveAs(file);
    inv("s2_tu_choi_ghi_de_goc", (!r2.ok && readAll(file) == before) ? "1" : "0");

    // 3) Thư mục đích không tồn tại
    auto r3 = d->saveAs(out / "khong-co-thu-muc" / fs::u8path("x." + ext(file)));
    inv("s3_thu_muc_khong_ton_tai_tu_choi", !r3.ok ? "1" : "0");
    inv("s3_thong_bao", r3.message);

    // 4) Đích là một THƯ MỤC trùng tên tệp (rename không thể thành công)
    fs::path dirAsFile = out / fs::u8path("dich-la-thu-muc." + ext(file));
    fs::create_directories(dirAsFile, ec);
    SaveOptions so; so.allowReplaceExisting = true;
    auto r4 = d->saveAs(dirAsFile, so);
    inv("s4_dich_la_thu_muc_tu_choi", !r4.ok ? "1" : "0");
    inv("s4_thong_bao", r4.message);
    inv("s4_tep_tam_con_lai", std::to_string(countTemp(out)));

    // 5) Thư mục đích KHÔNG GHI ĐƯỢC (giả lập hết quyền/đĩa đầy) + tệp đích đã có phải còn nguyên
    fs::path ro = out / fs::u8path("chi-doc");
    fs::create_directories(ro, ec);
    fs::path existing = ro / fs::u8path("da-co." + ext(file));
    { std::ofstream f(existing, std::ios::binary); f << "NOI DUNG CU - KHONG DUOC MAT"; }
    const auto existBefore = readAll(existing);
    bool locked = false;
#ifdef _WIN32
    {
        std::string cmd = "icacls \"" + ro.string() + "\" /deny \"*S-1-1-0:(OI)(CI)(WD,AD)\" >nul 2>&1";
        locked = std::system(cmd.c_str()) == 0;
    }
#else
    locked = ::chmod(ro.c_str(), 0555) == 0;
#endif
    inv("s5_khoa_thu_muc", locked ? "1" : "0");
    auto r5 = d->saveAs(existing, so);
    inv("s5_thu_muc_khoa_ghi_ok", r5.ok ? "1" : "0");
    inv("s5_thong_bao", r5.message);
    inv("s5_tep_dich_cu_nguyen_byte", readAll(existing) == existBefore ? "1" : "0");
    inv("s5_tep_tam_con_lai", std::to_string(countTemp(ro)));
    inv("s5_tep_goc_nguyen_byte", readAll(file) == before ? "1" : "0");
#ifdef _WIN32
    { std::string cmd = "icacls \"" + ro.string() + "\" /remove:d \"*S-1-1-0\" >nul 2>&1"; (void)std::system(cmd.c_str()); }
#else
    ::chmod(ro.c_str(), 0755);
#endif
    std::cout.flush();
    std::_Exit(0);
}

static int victim(const fs::path& program, const fs::path& file, const fs::path& target, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    fs::create_directories(target.parent_path(), ec);
    std::string err;
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) return 1;
    auto d = engine->open(file, &err);
    if (!d) return 1;
    std::cout << "VICTIM san-sang" << std::endl;
    SaveOptions so; so.allowReplaceExisting = true;
    for (int i = 0; i < 200; ++i) {
        auto r = d->saveAs(target, so);
        std::cout << "VICTIM luu " << i << " ok=" << r.ok << std::endl;
    }
    return 0;
}

static size_t pathLen(const fs::path& p) { return p.native().size(); } // đơn vị ký tự của hệ điều hành (UTF-16 trên Windows)

// Tạo thư mục (có thể vượt MAX_PATH) bằng tiền tố \\?\ trên Windows, để tách lỗi của bộ kiểm thử khỏi lỗi của engine/app.
static bool mkdirsLong(const fs::path& p) {
    std::error_code ec;
#ifdef _WIN32
    fs::create_directories(fs::path(L"\\\\?\\" + fs::absolute(p).wstring()), ec);
#else
    fs::create_directories(p, ec);
#endif
    return !ec && fs::is_directory(p, ec);
}
static bool copyLong(const fs::path& a, const fs::path& b, std::string* why) {
    std::error_code ec;
#ifdef _WIN32
    fs::copy_file(a, fs::path(L"\\\\?\\" + fs::absolute(b).wstring()), fs::copy_options::overwrite_existing, ec);
#else
    fs::copy_file(a, b, fs::copy_options::overwrite_existing, ec);
#endif
    if (ec && why) *why = ec.message();
    return !ec;
}

static int longpath(const fs::path& program, const fs::path& file, const fs::path& out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    std::string err;
    auto engine = Engine::create(program, out / "profile", out / "work", &err);
    if (!engine) { inv("loi", "khoi dong engine: " + err); return 1; }
    // Các độ dài đường dẫn tới tệp nguồn (ký tự): dưới, sát và vượt MAX_PATH (260 gồm ký tự kết thúc)
    for (size_t L : {200, 240, 250, 255, 258, 259, 262, 300, 400}) {
        const std::string tag = "lp" + std::to_string(L) + "_";
        fs::path base = fs::absolute(out) / fs::u8path("Hồ sơ " + std::to_string(L));
        const std::string fname = "tep.xlsx";
        // đệm thêm ký tự ASCII vào tên thư mục con cuối cho đủ độ dài mong muốn
        fs::path dir = base / fs::u8path("Tài liệu");
        size_t cur = pathLen(dir / fname);
        if (cur > L) { inv(tag + "bo_qua", "duong dan co ban da dai " + std::to_string(cur)); continue; }
        dir = base / fs::u8path("Tài liệu" + std::string(L - cur, 'a'));
        fs::path src = dir / fname;
        inv(tag + "do_dai_that", std::to_string(pathLen(src)));
        if (!mkdirsLong(dir)) { inv(tag + "tao_thu_muc_ok", "0"); continue; }
        std::string why;
        if (!copyLong(file, src, &why)) { inv(tag + "sao_chep_ok", "0"); inv(tag + "sao_chep_loi", why); continue; }
        auto d = engine->open(src, &err);
        inv(tag + "mo_ok", d ? "1" : "0");
        if (!d) { inv(tag + "mo_loi", err.substr(0, 160)); continue; }
        fs::path tgt = dir / fs::u8path("kq.xlsx");
        auto r = d->saveAs(tgt);
        inv(tag + "luu_ok", r.ok ? "1" : "0");
        if (!r.ok) inv(tag + "luu_loi", r.message.substr(0, 160));
    }
    // Tên tiếng Việt, ký tự đặc biệt
    fs::path vi = out / fs::u8path("Thư mục có dấu ‘ngoặc’ & ký hiệu");
    fs::create_directories(vi, ec);
    fs::path src2 = vi / fs::u8path("Đơn xin nghỉ học (bản 1).");
    src2 += ext(file);
    fs::copy_file(file, src2, fs::copy_options::overwrite_existing, ec);
    auto d2 = engine->open(src2, &err);
    inv("mo_ten_tieng_viet_ky_tu_dac_biet_ok", d2 ? "1" : "0");
    if (d2) {
        fs::path t2 = vi / fs::u8path("Kết quả (đã lưu) & thử.");
        t2 += ext(file);
        fs::remove(t2, ec);
        inv("luu_ten_tieng_viet_ky_tu_dac_biet_ok", d2->saveAs(t2).ok ? "1" : "0");
    }
    std::cout.flush();
    std::_Exit(0);
}

int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "dung-inventory perf|safety|victim|longpath ...\n"; return 2; }
    const std::string m = argv[1];
    auto P = [&](int i) { return fs::u8path(argv[i]); };
    if (m == "perf" && argc >= 5) return perf(P(2), P(3), P(4));
    if (m == "safety" && argc >= 5) return safety(P(2), P(3), P(4));
    if (m == "victim" && argc >= 6) return victim(P(2), P(3), P(4), P(5));
    if (m == "longpath" && argc >= 5) return longpath(P(2), P(3), P(4));
    std::cerr << "tham số không hợp lệ\n";
    return 2;
}
