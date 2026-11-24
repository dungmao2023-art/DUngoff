// Dũng Office — kết nối mạng cho kiểm tra cập nhật. CHỈ chạy khi người dùng bấm (hoặc bật tự kiểm tra).
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "UpdateCore.h"

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    static QString latestApiUrl();
    void checkLatest();
    // Tải bộ cài + tệp checksum, kiểm SHA-256. Không tự cài.
    void downloadAndVerify(const update::Release& r);
    void cancel();

signals:
    void checked(const update::Release& r, bool newer, const QString& message);  // message: lý do khi lỗi
    void progress(qint64 done, qint64 total);
    void verified(const QString& installerPath);          // đã tải và SHA-256 khớp
    void failed(const QString& message);                  // mọi lỗi, tệp đã tải (nếu có) đã bị xoá

private:
    QNetworkReply* get(const QUrl& u);
    void startDownload(const update::Release& r, const update::Asset& inst, const QString& expectedHex);

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply = nullptr;
};
