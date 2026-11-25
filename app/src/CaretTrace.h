// SPDX-License-Identifier: MPL-2.0
// Nhật ký con trỏ nhập liệu: chỉ ghi khi đặt biến môi trường DUNG_TRACE_CARET=<đường dẫn tệp>.
// Ghi mọi sự kiện liên quan con trỏ/ô/vùng chọn từ engine, thay đổi focus và quyết định vẽ,
// để chẩn đoán "không thấy con trỏ" trên máy thật.
#pragma once
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace carettrace {
inline bool enabled() {
    static const bool on = [] { const char* p = std::getenv("DUNG_TRACE_CARET"); return p && *p; }();
    return on;
}
inline void log(const char* fmt, ...) {
    if (!enabled()) return;
    static const char* path = std::getenv("DUNG_TRACE_CARET");
    static const auto t0 = std::chrono::steady_clock::now();
    std::FILE* f = std::fopen(path, "ab");
    if (!f) return;
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(f, "[+%lldms] ", ms);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fflush(f);
    std::fclose(f);
}
}  // namespace carettrace
