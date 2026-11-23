// SPDX-License-Identifier: MPL-2.0
#include "dung/FormatPolicy.h"

#include <algorithm>
#include <cctype>

namespace dung {
namespace {

// Lưu ý: chỉ khai báo "Native" cho những định dạng mà LibreOffice ghi được đầy đủ VÀ
// đã có (hoặc sẽ có) kiểm thử round-trip trong tests/. Mọi định dạng khác mặc định
// ReadOnly để không bao giờ làm mất dữ liệu một cách âm thầm.
const std::vector<FormatInfo>& table() {
    static const std::vector<FormatInfo> t = {
        // ---- Bảng tính ----
        {"xlsx", DocKind::Spreadsheet, Fidelity::Native, "Calc Office Open XML", "Sổ làm việc Excel (.xlsx)", ""},
        {"ods",  DocKind::Spreadsheet, Fidelity::Native, "calc8", "Bảng tính OpenDocument (.ods)", ""},
        {"xls",  DocKind::Spreadsheet, Fidelity::ReadOnly, "", "Excel 97-2003 (.xls)",
         "Định dạng .xls cũ: mở ở chế độ chỉ đọc. Hãy Lưu thành .xlsx để chỉnh sửa an toàn."},
        {"xlsm", DocKind::Spreadsheet, Fidelity::ReadOnly, "", "Excel có macro (.xlsm)",
         "Tệp có macro VBA: Dũng Office không chạy macro và chưa bảo đảm giữ nguyên macro khi ghi. Mở ở chế độ chỉ đọc."},
        {"xlsb", DocKind::Spreadsheet, Fidelity::ReadOnly, "", "Excel nhị phân (.xlsb)",
         "Không ghi được định dạng .xlsb. Mở ở chế độ chỉ đọc; có thể Lưu thành .xlsx."},
        {"csv",  DocKind::Spreadsheet, Fidelity::ReadOnly, "", "CSV (.csv)",
         "CSV chưa được hỗ trợ ghi: Dũng Office chưa bảo đảm giữ nguyên kiểu dữ liệu (vd. số 0 ở đầu). Mở ở chế độ chỉ đọc."},
        // ---- Văn bản ----
        {"docx", DocKind::Text, Fidelity::Native, "MS Word 2007 XML", "Tài liệu Word (.docx)", ""},
        {"odt",  DocKind::Text, Fidelity::Native, "writer8", "Văn bản OpenDocument (.odt)", ""},
        {"doc",  DocKind::Text, Fidelity::ReadOnly, "", "Word 97-2003 (.doc)",
         "Định dạng .doc cũ: mở ở chế độ chỉ đọc. Hãy Lưu thành .docx để chỉnh sửa an toàn."},
        {"rtf",  DocKind::Text, Fidelity::ReadOnly, "", "Rich Text (.rtf)", "Mở ở chế độ chỉ đọc."},
        {"txt",  DocKind::Text, Fidelity::ReadOnly, "", "Văn bản thuần (.txt)", "Mở ở chế độ chỉ đọc."},
        // ---- Trình chiếu ----
        {"pptx", DocKind::Presentation, Fidelity::Native, "Impress Office Open XML", "Bản trình chiếu PowerPoint (.pptx)", ""},
        {"odp",  DocKind::Presentation, Fidelity::Native, "impress8", "Trình chiếu OpenDocument (.odp)", ""},
        {"ppt",  DocKind::Presentation, Fidelity::ReadOnly, "", "PowerPoint 97-2003 (.ppt)",
         "Định dạng .ppt cũ: mở ở chế độ chỉ đọc. Hãy Lưu thành .pptx để chỉnh sửa an toàn."},
        // ---- PDF ----
        {"pdf",  DocKind::Drawing, Fidelity::ReadOnly, "", "PDF (.pdf)",
         "PDF được mở để xem. Chỉnh sửa PDF trực tiếp chưa được hỗ trợ vì có thể làm sai bố cục; tệp gốc không bị thay đổi."},
    };
    return t;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

} // namespace

const FormatInfo* formatForExt(std::string ext) {
    ext = lower(ext);
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (const auto& f : table())
        if (f.ext == ext) return &f;
    return nullptr;
}

const FormatInfo* formatForPath(const std::filesystem::path& p) {
    return formatForExt(p.extension().u8string()); // C++17: u8string() trả về std::string UTF-8
}

std::vector<const FormatInfo*> nativeSaveFormats(DocKind kind) {
    std::vector<const FormatInfo*> out;
    for (const auto& f : table())
        if (f.kind == kind && f.fidelity == Fidelity::Native) out.push_back(&f);
    return out;
}

std::vector<const FormatInfo*> exportFormats(DocKind kind) {
    static const FormatInfo pdfCalc{"pdf", DocKind::Spreadsheet, Fidelity::ExportOnly, "calc_pdf_Export", "PDF (xuất)", "Xuất PDF chỉ là bản in, không chứa công thức."};
    static const FormatInfo pdfText{"pdf", DocKind::Text, Fidelity::ExportOnly, "writer_pdf_Export", "PDF (xuất)", "Xuất PDF chỉ là bản in, không chỉnh sửa lại như tài liệu Word được."};
    static const FormatInfo pdfImp{"pdf", DocKind::Presentation, Fidelity::ExportOnly, "impress_pdf_Export", "PDF (xuất)", "Xuất PDF không giữ hiệu ứng trình chiếu."};
    static const FormatInfo pdfDraw{"pdf", DocKind::Drawing, Fidelity::ExportOnly, "draw_pdf_Export", "PDF (xuất)", "Bản PDF mới được tạo lại từ nội dung hiển thị."};
    switch (kind) {
    case DocKind::Spreadsheet: return {&pdfCalc};
    case DocKind::Text: return {&pdfText};
    case DocKind::Presentation: return {&pdfImp};
    case DocKind::Drawing: return {&pdfDraw};
    default: return {};
    }
}

std::string pdfExportFilter(DocKind kind) {
    auto v = exportFormats(kind);
    return v.empty() ? std::string() : v.front()->saveFilter;
}

const char* kindName(DocKind kind) {
    switch (kind) {
    case DocKind::Text: return "Word";
    case DocKind::Spreadsheet: return "Excel";
    case DocKind::Presentation: return "PowerPoint";
    case DocKind::Drawing: return "PDF";
    default: return "Khác";
    }
}

} // namespace dung
