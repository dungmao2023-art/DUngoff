// SPDX-License-Identifier: MPL-2.0
// Kiểm thử đăng ký từ điển Hunspell tiếng Việt cho engine: sinh khối registrymodifications.xcu từ thư mục từ điển.
// Không cần chạy LibreOffice. KHÔNG kiểm engine có thật sự dùng từ điển để kiểm chính tả hay không (xem dung-spellprobe).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include "dung/Engine.h"
namespace fs = std::filesystem;
static int g_fail = 0, g_pass = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; return; }
    ++g_fail;
    std::printf("[LỖI]  %s\n", what);
}
static void touch(const fs::path& p, const char* c = "x") { std::ofstream o(p, std::ios::binary); o << c; }
static bool has(const std::string& s, const std::string& n) { return s.find(n) != std::string::npos; }

int main() {
    const fs::path root = fs::temp_directory_path() / "dung-spell-dict-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "ok", ec);
    fs::create_directories(root / "lonely", ec);
    fs::create_directories(root / "space dir" / "tiếng Việt", ec);

    check(dung::Engine::viDictionaryRegistryXml(root / "khong-co").empty(), "thư mục không tồn tại -> khối rỗng");
    check(dung::Engine::viDictionaryRegistryXml(fs::path()).empty(), "đường dẫn rỗng -> khối rỗng");

    touch(root / "lonely" / "a.dic");
    touch(root / "lonely" / "README-en.txt");
    check(dung::Engine::viDictionaryRegistryXml(root / "lonely").empty(), ".dic thiếu .aff -> không đăng ký");

    touch(root / "ok" / "vi-DauCu.dic"); touch(root / "ok" / "vi-DauCu.aff");
    touch(root / "ok" / "vi-DauMoi.dic"); touch(root / "ok" / "vi-DauMoi.aff");
    touch(root / "ok" / "README-vi.txt");
    const std::string x = dung::Engine::viDictionaryRegistryXml(root / "ok");
    check(!x.empty(), "thư mục có 2 cặp -> có khối");
    check(has(x, "HunSpellDic_dung_vi_DauCu") && has(x, "HunSpellDic_dung_vi_DauMoi"), "mỗi cặp một nút, tên chỉ gồm chữ/số/_");
    check(has(x, "<value>vi-VN</value>"), "gắn locale vi-VN");
    check(has(x, "DICT_SPELL"), "định dạng DICT_SPELL");
    check(has(x, "vi-DauCu.dic file") == false && has(x, "file://"), "đường dẫn dạng URL file://");
    check(x.find("vi-DauCu") < x.find("vi-DauMoi"), "thứ tự xác định (DauCu trước)");
    check(!has(x, "README"), "không đăng ký tệp không phải từ điển");

    touch(root / "space dir" / "tiếng Việt" / "vi_x&y.dic"); touch(root / "space dir" / "tiếng Việt" / "vi_x&y.aff");
    const std::string y = dung::Engine::viDictionaryRegistryXml(root / "space dir" / "tiếng Việt");
    check(!y.empty(), "đường dẫn có dấu cách và tiếng Việt vẫn đăng ký");
    check(has(y, "&amp;") && !has(y, "x&y"), "ký tự & được escape trong XML");
    check(has(y, "%20") || has(y, " "), "dấu cách trong URL được xử lý");

    fs::remove_all(root, ec);
    std::printf("\nĐạt: %d; lỗi: %d\nKẾT QUẢ: %s\n", g_pass, g_fail, g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT");
    return g_fail ? 1 : 0;
}
