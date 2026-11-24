// SPDX-License-Identifier: MPL-2.0
// Sổ đăng ký công cụ của "Hộp công cụ". Thêm một công cụ mới = 1 dòng DUNG_REGISTER_TOOL
// trong tệp .cpp của công cụ + 1 dòng trong toolbox.cmake (xem docs/THEM_TOOL_MOI.md).
// Chỉ cần QtCore (QWidget chỉ khai báo trước) nên kiểm thử được không cần giao diện.
#pragma once
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

class QWidget;

namespace toolbox {

struct ToolInfo {
    QString id;          // chữ thường, số, '_' hoặc '-' ; duy nhất
    QString name;        // tên hiển thị
    QString description; // một dòng mô tả
    QString category;    // nhóm hiển thị
    QString iconText;    // ký tự/emoji trên thẻ
    std::function<QWidget*(QWidget*)> create; // tạo giao diện của công cụ (con của tham số)
    bool external = false;                    // true nếu là tool ngoài (tool.json)
};

// Hợp đồng cho mỗi công cụ: một hàm  QWidget* create<Ten>Widget(QWidget* parent).
using CreateFn = QWidget* (*)(QWidget*);

// Đăng ký. Trả false (và không đăng ký) nếu mã sai định dạng, trùng mã hoặc thiếu hàm tạo.
bool registerTool(const ToolInfo& info);
bool isValidId(const QString& id);

QList<ToolInfo> tools();                       // tất cả, sắp theo nhóm rồi tên
QStringList categories();                      // các nhóm, theo thứ tự chữ cái
QList<ToolInfo> byCategory(const QString& c);
bool find(const QString& id, ToolInfo* out = nullptr);
// Tìm theo tên/mô tả/nhóm; không phân biệt hoa-thường và dấu tiếng Việt ("chup man hinh" tìm được "Chụp màn hình").
QList<ToolInfo> search(const QString& query);
QString foldVn(const QString& s); // bỏ dấu + hạ chữ thường (dùng cho tìm kiếm)

void clearForTest();

} // namespace toolbox

// Đăng ký một công cụ ngay trong tệp .cpp của nó. ID là một định danh C++ (chữ thường/số/_).
// Ví dụ:  DUNG_REGISTER_TOOL(dem_gio, "Đồng hồ đếm giờ", "Bấm giờ và đếm ngược", "Tiện ích", "⏱", createTimerWidget)
#define DUNG_REGISTER_TOOL(ID, NAME, DESC, CATEGORY, ICON, CREATE_FN)                                     \
    namespace {                                                                                            \
    const bool dungToolRegistered_##ID = toolbox::registerTool(                                            \
        {QStringLiteral(#ID), QStringLiteral(NAME), QStringLiteral(DESC), QStringLiteral(CATEGORY),       \
         QStringLiteral(ICON), toolbox::CreateFn(CREATE_FN), false});                                      \
    }
