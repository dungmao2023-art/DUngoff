// SPDX-License-Identifier: MPL-2.0
// "Xuất theo mẫu" (MISA, FAST, KiotViet, Sapo, mẫu XLSX khác).
// Người dùng TỰ cung cấp tệp mẫu .xlsx chính thức của phần mềm đích (giống thiết kế gốc); ứng dụng KHÔNG có sẵn
// bố cục cột của các phần mềm đó và KHÔNG hứa phần mềm đích sẽ nhận tệp. Quy trình:
//   1) loadTemplate: đọc mẫu, tìm dòng tiêu đề.  2) autoMap: gợi ý ghép cột (người dùng sửa được).
//   3) validate: báo cột mẫu chưa có dữ liệu, cột nguồn không dùng, ô số bị bảo vệ...  4) exportToTemplate: ghi BẢN MỚI.
// Mẫu gốc không bao giờ bị sửa; không ghi đè tệp có sẵn. Chỉ cần QtCore + QZip (Gui private).
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace dataassist {

struct TemplateInfo {
    bool ok = false;
    QString error;
    QStringList sheetNames;
    int sheetIndex = 0;
    int headerRow = 1;            // dòng tiêu đề (tính từ 1)
    QStringList headers;          // tiêu đề các cột mẫu (giữ ô trống ở giữa)
    int existingDataRows = 0;     // số dòng dữ liệu mẫu có sẵn dưới tiêu đề (sẽ bị thay)
    QStringList warnings;
};
TemplateInfo loadTemplate(const QString &xlsxPath, int sheetIndex = 0, int headerRowOverride = 0);

// Ghép cột: kết quả[i] = chỉ số cột nguồn cho cột mẫu i, hoặc -1. Mỗi cột nguồn chỉ dùng một lần.
// So khớp: giống hệt (bỏ dấu/hoa thường/khoảng trắng) > chứa nhau. Không đoán khi có nhiều ứng viên ngang nhau (để -1).
QVector<int> autoMap(const QStringList &templateHeaders, const QStringList &sourceHeaders);

enum class ColType { Text, Number };   // Number: ghi số thật nếu đọc được nghiêm ngặt; ngược lại ghi chữ và cảnh báo

struct ExportOptions {
    QVector<int> mapping;               // như autoMap
    QVector<ColType> colTypes;          // theo cột mẫu; thiếu => Text
    bool vietnameseNumbers = true;      // 1.234,5 là một nghìn hai trăm ba mươi bốn phẩy năm
};
struct ValidateResult { QStringList errors; QStringList warnings; };
ValidateResult validate(const TemplateInfo &t, const QStringList &sourceHeaders, const QVector<QStringList> &sourceRows, const ExportOptions &o);

struct ExportResult {
    bool ok = false;
    QString error;
    QString outPath;
    int rowsWritten = 0;
    QStringList warnings;
    QStringList unmappedTemplateColumns;
};
// Ghi tệp MỚI = bản sao mẫu + dữ liệu. Từ chối nếu outPath đã tồn tại hoặc trùng tệp mẫu.
ExportResult exportToTemplate(const QString &templatePath, const QString &outPath, const TemplateInfo &t,
                              const QStringList &sourceHeaders, const QVector<QStringList> &sourceRows, const ExportOptions &o);

}  // namespace dataassist
