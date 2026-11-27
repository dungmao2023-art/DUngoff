// SPDX-License-Identifier: MPL-2.0
// Cầu nối giữa tiện ích dữ liệu và một tài liệu Calc đang mở (qua engine). Không phụ thuộc Qt để dung-smoketest dùng chung.
// Quy tắc: không ghi đè ô đang có dữ liệu nếu chưa được xác nhận; mọi ghi đều là một thao tác của engine nên .uno:Undo hoàn tác được.
#pragma once
#include "dung/Engine.h"

#include <chrono>
#include <functional>
#include <string>
#include <thread>

namespace dataassist {

inline std::string bridgeTrim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// Vùng đang chọn dạng văn bản (TSV). Rỗng nếu không chọn gì.
inline std::string bridgeSelectedText(dung::Document& d) { return d.selectionAs("text/plain;charset=utf-8"); }

struct InsertOutcome {
    bool ok = false;
    bool declined = false;      // người dùng không đồng ý ghi đè
    bool hadData = false;       // ô đích đang có dữ liệu
    std::string previous;       // nội dung cũ (để báo/hoàn tác)
    std::string resultText;     // kết quả engine tính được sau khi chèn (đọc lại từ ô)
    std::string message;
};

// Đưa `formula` vào ô đang chọn. Nếu ô có dữ liệu: gọi confirmOverwrite(nộiDungCũ); false => không ghi.
inline InsertOutcome bridgeInsertFormula(dung::Document& d, const std::string& formula,
                                         const std::function<bool(const std::string&)>& confirmOverwrite,
                                         int settleMs = 300) {
    InsertOutcome o;
    if (d.kind() != dung::DocKind::Spreadsheet) { o.message = "Tab đang mở không phải bảng tính"; return o; }
    if (d.readOnly()) { o.message = "Tài liệu đang ở chế độ chỉ đọc"; return o; }
    if (formula.empty() || formula[0] != '=') { o.message = "Công thức phải bắt đầu bằng ="; return o; }
    o.previous = bridgeTrim(bridgeSelectedText(d));
    o.hadData = !o.previous.empty();
    if (o.hadData && !(confirmOverwrite && confirmOverwrite(o.previous))) { o.declined = true; o.message = "Đã huỷ: ô đang có dữ liệu"; return o; }
    d.command(".uno:EnterString", dung::Document::stringArg("StringName", formula));
    std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));
    o.resultText = bridgeTrim(bridgeSelectedText(d));
    o.ok = true;
    return o;
}

// Chỉ nhận địa chỉ A1 một ô thuộc sheet hiện tại; không nhận tên sheet, vùng hoặc tên định nghĩa.
inline std::string bridgeCellAddress(std::string s) {
    s = bridgeTrim(s);
    std::string col, row;
    size_t i = 0;
    if (i < s.size() && s[i] == '$') ++i;
    while (i < s.size() && ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z'))) {
        char c = s[i++];
        col += (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
    }
    if (i < s.size() && s[i] == '$') ++i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') row += s[i++];
    if (i != s.size() || col.empty() || col.size() > 3 || row.empty() || row.size() > 7 || row[0] == '0') return {};
    int n = 0, rn = 0;
    for (char c : col) n = n * 26 + c - 'A' + 1;
    for (char c : row) rn = rn * 10 + c - '0';
    if (n > 16384 || rn > 1048576) return {};
    return col + row;
}

// Kiểm địa chỉ tham chiếu vùng (không dùng cho ô ghi kết quả).
inline std::string bridgeRangeAddress(const std::string& s) {
    const auto colon = s.find(':');
    if (colon == std::string::npos) return bridgeCellAddress(s);
    const auto a = bridgeCellAddress(s.substr(0, colon));
    const auto b = bridgeCellAddress(s.substr(colon + 1));
    if (a.empty() || b.empty()) return {};
    return a == b ? a : a + ":" + b;
}

// Formula bar may write only if the selected single cell and the engine
// address callback agree; an old cursor rectangle alone is insufficient.
inline std::string bridgeConfirmedCell(const std::string& selected, const std::string& reported) {
    const auto a=bridgeCellAddress(selected), b=bridgeCellAddress(reported);
    return !a.empty() && a==b ? a : std::string();
}

// Chờ callback mới để xác minh địa chỉ đích; không suy đoán từ vùng chọn cũ.
// Luôn xác nhận trước khi ghi, kể cả khi giá trị hiển thị rỗng (ô có thể chứa công thức).
inline InsertOutcome bridgeInsertFormulaAt(dung::Document& d, const std::string& target, const std::string& formula,
                                           int addressEvent,
                                           const std::function<bool(const std::string&)>& confirm,
                                           int timeoutMs = 2500) {
    InsertOutcome o;
    const auto addr = bridgeCellAddress(target);
    if (addr.empty()) { o.message = "Ô kết quả phải là một địa chỉ A1 hợp lệ (vd. C6) trong sheet hiện tại."; return o; }
    if (d.kind() != dung::DocKind::Spreadsheet || d.readOnly()) { o.message = "Cần bảng tính có thể chỉnh sửa."; return o; }
    if (formula.empty() || formula[0] != '=') { o.message = "Công thức phải bắt đầu bằng ="; return o; }
    d.waitInputIdle();
    const int part = d.currentPart();
    d.clearEvents();
    d.command(".uno:GoToCell", dung::Document::stringArg("ToPoint", addr));
    const bool matched = d.waitFor(addressEvent, [&](const std::string& s) { return bridgeCellAddress(s) == addr; }, timeoutMs);
    d.waitInputIdle();
    if (!matched || d.currentPart() != part || bridgeCellAddress(d.lastPayload(addressEvent)) != addr) {
        o.message = "Chưa xác minh được ô kết quả. Chọn ô khác rồi thử lại; chưa ghi dữ liệu.";
        return o;
    }
    const auto sheet = d.partName(part);
    o.previous = bridgeSelectedText(d);
    if (!confirm || !confirm(o.previous)) { o.declined = true; o.message = "Đã huỷ; chưa ghi dữ liệu."; return o; }
    // Dialog xác nhận có thể chạy event loop: kiểm lại địa chỉ, sheet và dữ liệu đã hiện.
    d.waitInputIdle();
    if (d.currentPart() != part || d.partName(part) != sheet ||
        bridgeCellAddress(d.lastPayload(addressEvent)) != addr ||
        bridgeSelectedText(d) != o.previous) {
        o.message = "Ô kết quả hoặc dữ liệu đã thay đổi trong lúc xác nhận; chưa ghi công thức.";
        return o;
    }
    return bridgeInsertFormula(d, formula, [](const std::string&) { return true; });
}

inline bool bridgeLooksLikeError(const std::string& r) { return r.rfind("#", 0) == 0 || r.rfind("Err:", 0) == 0; }

}  // namespace dataassist
