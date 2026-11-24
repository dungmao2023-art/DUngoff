// SPDX-License-Identifier: MPL-2.0
// Lõi chụp màn hình/chỉnh ảnh: chỉ cần QtGui (không widget) để kiểm thử không giao diện trên CI.
//  - Document: ảnh gốc + danh sách thao tác (mũi tên, khung, bút, tô sáng, chữ, mosaic/làm mờ, đánh số, cắt) có undo/redo.
//  - Làm mờ/mosaic là che THẬT: render() ghi đè điểm ảnh bằng giá trị trung bình khối nên ảnh xuất ra không còn thông tin gốc.
//  - Lưu: PNG/JPG/PDF, tên theo ngày giờ, KHÔNG BAO GIỜ ghi đè tệp có sẵn.
#pragma once
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVector>

namespace shot {

enum class OpType { Arrow, Rect, Pen, Highlight, Text, Blur, Mosaic, Step, Crop };

struct Op {
    OpType type = OpType::Rect;
    QPointF a, b;               // Arrow: a→b; Rect/Highlight: hai góc
    QVector<QPointF> path;      // Pen
    QRect region;               // Blur/Mosaic/Crop (toạ độ ảnh gốc)
    QColor color = QColor(220, 38, 38);
    int width = 4;              // nét vẽ; Blur/Mosaic: cỡ khối (px)
    QString text;               // Text
    int fontPx = 28;            // Text
    int number = 0;             // Step
};

constexpr int kMinRedactBlock = 6;   // cỡ khối tối thiểu: không cho che quá yếu

// Che thật (hàm thuần, kiểm thử bằng điểm ảnh). Vùng được cắt theo kích thước ảnh.
void mosaicRegion(QImage& img, const QRect& region, int block);
void blurRegion(QImage& img, const QRect& region, int block);   // gộp khối rồi làm mịn: không khôi phục được

class Document {
public:
    explicit Document(const QImage& base);
    const QImage& base() const { return m_base; }
    // Thêm thao tác; xoá nhánh redo. Trả false (không thêm) nếu thao tác không hợp lệ (vùng rỗng, cắt ngoài ảnh...).
    bool add(const Op& op);
    bool undo();
    bool redo();
    bool canUndo() const { return m_cursor > 0; }
    bool canRedo() const { return m_cursor < m_ops.size(); }
    int  opCount() const { return m_cursor; }
    int  nextStepNumber() const;
    // Ảnh kết quả: gốc + thao tác đang hiệu lực, rồi cắt. Ảnh gốc trong Document không bị sửa.
    QImage render() const;
    QRect  currentCrop() const;   // vùng cắt hiệu lực (toàn ảnh nếu chưa cắt)
private:
    QImage m_base;
    QVector<Op> m_ops;
    int m_cursor = 0;
};

struct Measure { double dx, dy, length; };
Measure measure(const QPointF& a, const QPointF& b);   // thước pixel

// ---- lưu tệp ----
enum class Format { Png, Jpg, Pdf };
struct SaveResult { bool ok = false; QString path; QString message; };
QString defaultSaveDir();                                    // Hình ảnh/Dung Office/Chụp màn hình
QString timestampStem(const QDateTime& t);                   // ChupManHinh_YYYYMMDD_HHMMSS
QString uniquePath(const QString& dir, const QString& stem, const QString& ext);   // thêm _2, _3... nếu đã có
SaveResult saveImage(const QImage& img, const QString& dir, Format fmt, const QDateTime& now, int jpegQuality = 92);
SaveResult saveImageAs(const QImage& img, const QString& wantedPath, int jpegQuality = 92);   // tệp đã có → lưu tên khác, báo rõ
QString extensionFor(Format f);

} // namespace shot
