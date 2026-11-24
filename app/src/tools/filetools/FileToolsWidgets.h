// SPDX-License-Identifier: MPL-2.0
// Năm widget công cụ tệp/thư mục/liên kết. KHÔNG tự đăng ký vào menu: nơi gọi tự đặt widget vào tab/hộp thoại.
// Mọi widget do người gọi sở hữu qua parent; không dùng Q_OBJECT nên không cần moc.
#pragma once
class QWidget;

QWidget* createBatchRenameWidget(QWidget* parent = nullptr);
QWidget* createDuplicateFinderWidget(QWidget* parent = nullptr);
QWidget* createFolderCompareWidget(QWidget* parent = nullptr);
QWidget* createDiskUsageWidget(QWidget* parent = nullptr);
QWidget* createLinkToolsWidget(QWidget* parent = nullptr);
QWidget* createOrganizeWidget(QWidget* parent = nullptr);      // sắp xếp tệp vào thư mục theo loại/đuôi/năm/tháng
QWidget* createPathToolsWidget(QWidget* parent = nullptr);     // sao chép đường dẫn các dạng + tìm đường dẫn > 260 ký tự
QWidget* createLinkScanWidget(QWidget* parent = nullptr);      // quét lối tắt .url/.lnk và liên kết trong docx/xlsx/pptx
