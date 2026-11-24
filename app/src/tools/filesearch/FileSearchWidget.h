// Giao diện công cụ "Tìm tệp" (nhanh, nhẹ, không dịch vụ nền).
// Hợp đồng tích hợp: chỉ cần gọi createFileSearchWidget(parent) rồi đặt widget vào tab/dock bất kỳ.
// Công cụ này KHÔNG tự đăng ký vào menu — việc đăng ký do nơi tích hợp quyết định.
#pragma once
class QWidget;
QWidget *createFileSearchWidget(QWidget *parent);
