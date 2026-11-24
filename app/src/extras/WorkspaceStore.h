// SPDX-License-Identifier: MPL-2.0
// Lưu Note, Lịch, Danh bạ và danh sách Dự án khác trong thư mục dữ liệu của người dùng (JSON, ghi nguyên tử).
// Không có kết nối mạng: dữ liệu chỉ nằm trên máy.
#pragma once
#include <QJsonObject>
#include <QString>

namespace workspace {
// Thư mục dữ liệu, ví dụ %APPDATA%/DungOffice. Tự tạo nếu chưa có.
QString dataDir();
// Đọc <tên>.json; trả object rỗng nếu chưa có hoặc hỏng (tệp hỏng được đổi tên thành .hong để không mất).
QJsonObject load(const QString& name);
// Ghi nguyên tử (QSaveFile). Trả false nếu lỗi.
bool save(const QString& name, const QJsonObject& obj);
}
