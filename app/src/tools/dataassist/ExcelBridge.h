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

inline bool bridgeLooksLikeError(const std::string& r) { return r.rfind("#", 0) == 0 || r.rfind("Err:", 0) == 0; }

}  // namespace dataassist
