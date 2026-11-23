// Dũng Office — chính sách định dạng: định dạng nào mở được, ghi được mà không mất dữ liệu,
// định dạng nào chỉ được mở ở chế độ chỉ đọc hoặc chỉ được "xuất" (export, có mất mát).
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dung {

enum class DocKind { Text, Spreadsheet, Presentation, Drawing, Other };

enum class Fidelity {
    Native,     // định dạng chính, ghi lại được (vd. XLSX, DOCX, PPTX, ODF)
    ReadOnly,   // mở để xem; không cho ghi đè cùng định dạng (vd. XLSM có macro, XLSB, PDF)
    ExportOnly  // chỉ xuất ra, có mất mát (vd. PDF xuất từ bảng tính, CSV)
};

struct FormatInfo {
    std::string ext;          // không có dấu chấm, chữ thường: "xlsx"
    DocKind kind;
    Fidelity fidelity;
    std::string saveFilter;   // tên filter LibreOffice khi ghi; rỗng = không ghi được
    std::string label;        // tên hiển thị tiếng Việt
    std::string warning;      // cảnh báo hiển thị khi mở / khi ghi (rỗng nếu không có)
};

// Tra cứu theo phần mở rộng của đường dẫn. Trả về nullptr nếu không hỗ trợ.
const FormatInfo* formatForPath(const std::filesystem::path& p);
const FormatInfo* formatForExt(std::string ext);

// Các định dạng có thể Save As (không mất dữ liệu) cho một loại tài liệu.
std::vector<const FormatInfo*> nativeSaveFormats(DocKind kind);
// Các định dạng chỉ xuất (có mất mát) cho một loại tài liệu.
std::vector<const FormatInfo*> exportFormats(DocKind kind);

// Tên filter xuất PDF theo loại tài liệu.
std::string pdfExportFilter(DocKind kind);

const char* kindName(DocKind kind);

} // namespace dung
