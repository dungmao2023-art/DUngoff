// SPDX-License-Identifier: MPL-2.0
// Nhật ký điểm thoát: chỉ ghi khi đặt biến môi trường DUNG_TRACE_EXIT=<đường dẫn tệp>.
// Mỗi dòng được ghi và đẩy xuống đĩa ngay, để biết tiến trình dừng ở bước nào nếu bị treo lúc thoát.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <chrono>

namespace exittrace {
inline void log(const char* step) {
    static const char* path = std::getenv("DUNG_TRACE_EXIT");
    if (!path || !*path) return;
    static const auto t0 = std::chrono::steady_clock::now();
    std::FILE* f = std::fopen(path, "ab");
    if (!f) return;
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(f, "[+%lldms] %s\n", ms, step);
    std::fflush(f);
    std::fclose(f);
}
} // namespace exittrace
