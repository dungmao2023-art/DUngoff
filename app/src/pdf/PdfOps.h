// SPDX-License-Identifier: MPL-2.0
// Thao tác PDF (gộp, tách, xoay, nén, thông tin) bằng qpdf (Apache-2.0), gọi qua QProcess.
// Chỉ phụ thuộc QtCore để kiểm thử được không cần giao diện.
//
// Quy tắc an toàn (docs/DATA_SAFETY.md, khớp tools/pdfcore/dung_pdf.py):
//  - Tệp nguồn chỉ được đọc, không bao giờ bị ghi; đích không được trùng tệp nguồn.
//  - Ghi vào tệp tạm "~dung-xxxx-tên.pdf" cùng thư mục đích, kiểm tra %PDF- và %%EOF
//    cùng `qpdf --check`, rồi mới đổi tên sang đích.
//  - Đích đã tồn tại: từ chối trừ khi allowReplace; khi thay thì sao lưu thành
//    "tên.dung-backup-YYYYMMDD-HHMMSS.pdf" trước.
//  - Lỗi trả về bằng tiếng Việt; tệp có mật khẩu được báo bằng needPassword.
#pragma once
#include <QList>
#include <QString>
#include <QStringList>

namespace dungpdf {

struct Input {
    QString path;
    QString password;  // rỗng nếu không có
};

struct Result {
    bool ok = false;
    QString error;             // thông báo tiếng Việt khi ok == false
    bool needPassword = false; // tệp cần mật khẩu (hoặc mật khẩu sai)
    QStringList files;         // các tệp đã ghi (đích, hoặc từng trang khi tách)
    QString backup;            // đường dẫn bản sao lưu nếu đã thay tệp cũ
    QStringList warnings;
};

struct Info {
    bool ok = false;
    QString error;
    bool needPassword = false;
    int pages = 0;
    bool encrypted = false;
    qint64 bytes = 0;
    QString warning;  // qpdf đọc được nhưng tệp hỏng/đã phải phục hồi
};

// Đường dẫn qpdf: DUNG_QPDF, rồi <thư mục app>/qpdf/qpdf(.exe), rồi PATH. Rỗng nếu không thấy.
QString findQpdf();

// "1-3,5,8-" -> chỉ số 0-based (có kiểm tra theo `total`). Khi lỗi trả danh sách rỗng và đặt *error.
QList<int> parsePages(const QString& spec, int total, QString* error = nullptr);

Info info(const QString& path, const QString& password = QString());

Result merge(const QList<Input>& inputs, const QString& dest, bool allowReplace = false);
Result extract(const Input& in, const QString& pagesSpec, const QString& dest, bool allowReplace = false);
// Sắp xếp: mọi trang phải xuất hiện đúng một lần; không xóa hoặc nhân bản trang.
Result reorder(const Input& in, const QString& pagesSpec, const QString& dest, bool allowReplace = false);
// Mỗi trang một tệp: <outDir>/<tên>-trangNNN.pdf. Không ghi đè: nếu có tệp trùng tên thì từ chối toàn bộ.
Result splitEach(const Input& in, const QString& outDir);
Result rotate(const Input& in, const QString& pagesSpec, int degrees, const QString& dest, bool allowReplace = false);
Result compress(const Input& in, const QString& dest, bool allowReplace = false);

// ---- Đánh số trang, đóng dấu, ảnh -> PDF, trang trắng (luôn ghi ra bản sao, không sửa tệp gốc) ----

enum class Position { TopLeft, TopCenter, TopRight, BottomLeft, BottomCenter, BottomRight, Center, Diagonal, Custom };

struct NumberOptions {
    // {n} = số trang hiện tại, {total} = số cuối cùng được đánh. Chỉ nhận ký tự ASCII (chữ không dấu): font
    // chuẩn của PDF không có chữ có dấu tiếng Việt và chưa nhúng font; gặp ký tự khác thì từ chối, không đánh sai.
    QString format = QStringLiteral("{n}");
    Position pos = Position::BottomCenter;
    double fontSize = 11;
    int start = 1;                       // số gán cho trang đầu tiên trong phần được chọn
    QString pages = QStringLiteral("1-"); // các trang được đánh số (cú pháp như parsePages)
    double gray = 0.0;                   // 0 = đen, 1 = trắng
};
Result addPageNumbers(const Input& in, const NumberOptions& opt, const QString& dest, bool allowReplace = false);

struct StampOptions {
    QString text;       // chữ Unicode một dòng, tối đa 200 ký tự; rỗng thì phải có imagePath
    QString imagePath;  // ảnh JPEG/PNG (PNG trong suốt được giữ độ trong)
    Position pos = Position::Diagonal;
    double fontSize = 48;
    double gray = 0.5;
    double opacity = 0.3;        // 0..1
    double widthPercent = 30;    // chỉ cho ảnh: bề rộng ảnh theo % bề rộng trang
    double xPercent = 50, yPercent = 50; // Custom: vị trí tương đối trong phần diện tích còn trống, tính từ trái/trên
    QString pages = QStringLiteral("1-");
};
Result stamp(const Input& in, const StampOptions& opt, const QString& dest, bool allowReplace = false);

struct ImageOptions {
    bool fitA4 = false;  // true: mỗi ảnh vào một trang A4 (giữ tỉ lệ); false: trang đúng cỡ ảnh theo `dpi`
    int dpi = 150;
};
// JPEG được nhúng nguyên bản (không nén lại, không mất chất lượng); PNG được nhúng không mất dữ liệu.
Result imagesToPdf(const QStringList& images, const ImageOptions& opt, const QString& dest, bool allowReplace = false);

struct BlankReport {
    bool ok = false;
    QString error;
    bool needPassword = false;
    int pages = 0;
    QList<int> blank;  // chỉ số 0-based của trang không có nội dung
    QStringList notes; // giới hạn của phép kiểm tra
};
// Trang "trắng" = không có lệnh vẽ nào và không có chú thích. KHÔNG phát hiện trang trắng nhìn bằng mắt nhưng
// có ảnh quét hoặc ô tô nền trắng.
BlankReport findBlankPages(const Input& in);
Result removeBlankPages(const Input& in, const QString& dest, bool allowReplace = false);

// Chuỗi chỉ gồm ký tự ASCII in được (32..126)? Dùng để báo trước cho người dùng khi nhập chữ đánh số/đóng dấu.
bool isAsciiText(const QString& s);

// Tên đề xuất cho tệp kết quả: "<tên> - Dũng Office.pdf" cạnh tệp nguồn; thêm số nếu đã tồn tại.
QString suggestOutputPath(const QString& sourcePath, const QString& label = QString());

}  // namespace dungpdf
