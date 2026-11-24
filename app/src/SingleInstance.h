// Dũng Office — một phiên duy nhất: nhấp đúp tệp khi app đang chạy thì chuyển tệp sang cửa sổ cũ.
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QObject>
#include <QStringList>

class QLocalServer;

class SingleInstance : public QObject {
    Q_OBJECT
public:
    // key: tên kênh (mặc định theo người dùng Windows hiện tại).
    explicit SingleInstance(const QString& key = defaultKey(), QObject* parent = nullptr);
    ~SingleInstance() override;

    static QString defaultKey();

    // Thử gửi danh sách tệp (có thể rỗng = chỉ yêu cầu đưa cửa sổ lên) tới phiên đang chạy.
    // true = phiên cũ đã nhận và xác nhận; khi đó tiến trình mới nên thoát.
    // false = không có phiên nào đang chạy (hoặc không phản hồi) -> tiến trình này tự chạy.
    bool sendToRunning(const QStringList& files, int timeoutMs = 1500) const;

    // Trở thành phiên chính (mở kênh lắng nghe). false nếu không mở được kênh.
    bool listen();

    // Kiểm tra thông điệp nhận được: chỉ giữ đường dẫn tuyệt đối trỏ tới TỆP có thật, tối đa 50, thông điệp tối đa 1 MiB.
    static QStringList parseMessage(const QByteArray& msg, bool* wantsRaise = nullptr);
    static QByteArray buildMessage(const QStringList& files);

signals:
    void filesReceived(const QStringList& files);  // đã lọc
    void raiseRequested();

private:
    QString m_key;
    QLocalServer* m_server = nullptr;
};
