// SPDX-License-Identifier: MPL-2.0
// Thư viện biểu mẫu: gói mẫu cài vào thư mục dữ liệu người dùng. Cài gói mới THAY HẲN gói cũ cùng mã
// (mẫu bị cơ quan nhà nước thay thế thì bản cũ phải biến mất để không bị dùng nhầm).
// Chỉ phụ thuộc QtCore. Không bao giờ ghi vào tệp tài liệu của người dùng.
#pragma once
#include <QList>
#include <QString>

namespace tpl {

struct LibInfo {
    bool valid = false;
    QString id;       // ví dụ "thu-vien-bieu-mau"
    QString name;
    QString version;  // ví dụ "2026.1"
    QString root;     // thư mục gốc của gói
    int count = 0;    // số mẫu có trong danh mục
};

struct Entry {
    QString relPath;  // đường dẫn tương đối, dấu /
    QString status;   // "Mẫu nội bộ tham khảo", ...
    QString origin;
    QString url;
    QString format;   // DOCX / PPTX
};

struct Source {
    QString group, document, url, guide;
};

struct InstallResult {
    bool ok = false;
    bool needConfirm = false; // bản đang cài MỚI HƠN gói được chọn
    bool replaced = false;
    QString message;
    LibInfo info;
};

// Thư mục chứa các gói đã cài: <dữ liệu người dùng>/templates
QString installedRoot();
// Tìm thư mục gốc gói trong `dir` (chính nó hoặc thư mục con cấp 1-2 có DANH_MUC_TAP_TIN.csv hoặc manifest.json).
bool findLibraryRoot(const QString& dir, QString* root);
LibInfo readInfo(const QString& root);
// Gói đang cài (mã mặc định "thu-vien-bieu-mau"); valid=false nếu chưa có.
LibInfo installed(const QString& id = QStringLiteral("thu-vien-bieu-mau"));
// So sánh phiên bản dạng "2026.1": <0 nếu a cũ hơn b.
int compareVersion(const QString& a, const QString& b);

// Cài từ thư mục. Gói cũ cùng mã bị xoá sau khi gói mới đã chép xong (đổi tên nguyên tử, hoàn lại nếu lỗi).
InstallResult installFromDir(const QString& dir, bool allowOlder = false);
// Cài từ tệp .zip (giải nén vào thư mục tạm bằng tar/unzip của hệ điều hành).
InstallResult installFromZip(const QString& zipPath, bool allowOlder = false);
// Gói đi kèm bộ cài (<thư mục app>/templates/*): chỉ cài khi chưa có hoặc cũ hơn. Trả true nếu có thay đổi.
bool seedFromBundled(const QString& appDir);

QList<Entry> entries(const LibInfo& lib);
QList<Source> sources(const LibInfo& lib);

// Tạo bản làm việc từ mẫu: sao chép sang `destFile` (không ghi đè nếu đã có). Mẫu gốc không bị đổi.
bool makeWorkingCopy(const QString& templatePath, const QString& destFile, QString* error);

}  // namespace tpl
