#pragma once
// Quét tệp gốc (zip OOXML) để biết trước tính năng nào SẼ KHÔNG được giữ khi engine lưu bản sao.
// Không phụ thuộc Qt/LibreOffice; chỉ đọc danh sách tên trong zip (không giải nén), nên nhanh và an toàn.
// Danh sách này THẬN TRỌNG: chỉ những thứ thí nghiệm CI (exp/feature-fidelity) hoặc LibreOffice đã biết là mất/biến đổi.
// Không phát hiện được không có nghĩa là không mất (vd. sparkline nằm trong XML của sheet, chưa quét).
#include <filesystem>
#include <string>
#include <vector>

namespace dung {

struct FeatureLoss {
    std::string id;       // mã ổn định, dùng trong kiểm thử: "font-nhung", "macro", "smartart", ...
    std::string title;    // tên tính năng bằng tiếng Việt
    std::string effect;   // điều gì xảy ra khi lưu bằng Dũng Office
    int count = 0;        // số thành phần tìm thấy trong tệp
    bool certain = true;  // true: đã có bằng chứng mất; false: chỉ "có thể"
    bool blocking = true; // false: chỉ báo ngắn, KHÔNG bắt xác nhận trước khi lưu (vd. master-tach)
};

struct FeatureScanResult {
    bool scanned = false;               // false: không đọc được cấu trúc zip (tệp lạ/hỏng/zip64) -> không kết luận
    std::string note;                   // lý do khi scanned=false
    std::vector<FeatureLoss> items;
};

// Liệt kê tên thành phần trong zip (đọc central directory). Trả false và đặt err nếu không đọc được.
bool listZipNames(const std::filesystem::path& zipFile, std::vector<std::string>& names, std::string* err);

// Phân loại theo tên thành phần (hàm thuần, dùng cho kiểm thử với danh sách tên giả).
std::vector<FeatureLoss> classifyEntries(const std::vector<std::string>& names);

FeatureScanResult scanFeatureLoss(const std::filesystem::path& zipFile);

// Văn bản tiếng Việt liệt kê chính xác các mục, dùng cho thông báo của engine/giao diện.
std::string describeFeatureLoss(const std::vector<FeatureLoss>& items);

} // namespace dung
