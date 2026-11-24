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
};

// Đường dẫn qpdf: DUNG_QPDF, rồi <thư mục app>/qpdf/qpdf(.exe), rồi PATH. Rỗng nếu không thấy.
QString findQpdf();

// "1-3,5,8-" -> chỉ số 0-based (có kiểm tra theo `total`). Khi lỗi trả danh sách rỗng và đặt *error.
QList<int> parsePages(const QString& spec, int total, QString* error = nullptr);

Info info(const QString& path, const QString& password = QString());

Result merge(const QList<Input>& inputs, const QString& dest, bool allowReplace = false);
Result extract(const Input& in, const QString& pagesSpec, const QString& dest, bool allowReplace = false);
// Mỗi trang một tệp: <outDir>/<tên>-trangNNN.pdf. Không ghi đè: nếu có tệp trùng tên thì từ chối toàn bộ.
Result splitEach(const Input& in, const QString& outDir);
Result rotate(const Input& in, const QString& pagesSpec, int degrees, const QString& dest, bool allowReplace = false);
Result compress(const Input& in, const QString& dest, bool allowReplace = false);

// Tên đề xuất cho tệp kết quả: "<tên> - Dũng Office.pdf" cạnh tệp nguồn; thêm số nếu đã tồn tại.
QString suggestOutputPath(const QString& sourcePath, const QString& label = QString());

}  // namespace dungpdf
