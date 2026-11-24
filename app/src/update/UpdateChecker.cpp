// SPDX-License-Identifier: MPL-2.0
#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <memory>

namespace {
constexpr qint64 kMaxApiBytes = 2 * 1024 * 1024;
constexpr qint64 kMaxChecksumBytes = 64 * 1024;
constexpr qint64 kMaxInstallerBytes = 1024LL * 1024 * 1024;  // 1 GiB trần an toàn
}  // namespace

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

QString UpdateChecker::latestApiUrl() {
    return QStringLiteral("https://api.github.com/repos/dungmao2023-art/DUngoff/releases/latest");
}

QNetworkReply* UpdateChecker::get(const QUrl& u) {
    QNetworkRequest rq(u);
    rq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("DungOffice/%1").arg(QCoreApplication::applicationVersion()));
    rq.setRawHeader("Accept", "application/vnd.github+json");
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    rq.setTransferTimeout(30000);
    return m_nam->get(rq);
}

void UpdateChecker::cancel() {
    if (m_reply) m_reply->abort();
}

void UpdateChecker::checkLatest() {
    m_reply = get(QUrl(latestApiUrl()));
    QNetworkReply* r = m_reply;
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        if (m_reply == r) m_reply = nullptr;
        if (r->error() != QNetworkReply::NoError) {
            emit checked({}, false, tr("Không kết nối được máy chủ phát hành: %1").arg(r->errorString()));
            return;
        }
        if (!update::isTrustedUrl(r->url())) {
            emit checked({}, false, tr("Phản hồi đến từ địa chỉ không tin cậy, đã bỏ qua."));
            return;
        }
        const QByteArray body = r->read(kMaxApiBytes + 1);
        if (body.size() > kMaxApiBytes) { emit checked({}, false, tr("Phản hồi quá lớn, đã bỏ qua.")); return; }
        const update::Release rel = update::parseRelease(body);
        if (!rel.ok) { emit checked(rel, false, rel.error); return; }
        if (rel.draft) { emit checked(rel, false, tr("Bản phát hành chưa công bố, bỏ qua.")); return; }
        QString err;
        const bool newer = update::isNewer(QCoreApplication::applicationVersion(), rel.version, &err);
        emit checked(rel, newer, err);
    });
}

void UpdateChecker::downloadAndVerify(const update::Release& rel) {
    const update::Asset inst = update::findInstaller(rel);
    if (inst.name.isEmpty()) { emit failed(tr("Bản phát hành không có bộ cài Windows x64 (hoặc địa chỉ tải không tin cậy).")); return; }
    const update::Asset sum = update::findChecksumAsset(rel, inst.name);
    if (sum.name.isEmpty()) { emit failed(tr("Bản phát hành KHÔNG có tệp checksum SHA-256 nên không tải để cài. Hãy tải thủ công từ trang phát hành.")); return; }
    m_reply = get(QUrl(sum.url));
    QNetworkReply* r = m_reply;
    connect(r, &QNetworkReply::finished, this, [this, r, rel, inst] {
        r->deleteLater();
        if (m_reply == r) m_reply = nullptr;
        if (r->error() != QNetworkReply::NoError) { emit failed(tr("Không tải được tệp checksum: %1").arg(r->errorString())); return; }
        if (!update::isTrustedUrl(r->url())) { emit failed(tr("Tệp checksum đến từ địa chỉ không tin cậy.")); return; }
        const QByteArray body = r->read(kMaxChecksumBytes + 1);
        if (body.size() > kMaxChecksumBytes) { emit failed(tr("Tệp checksum quá lớn.")); return; }
        QString err;
        const QString hex = update::expectedHash(QString::fromUtf8(body), inst.name, &err);
        if (hex.isEmpty()) { emit failed(err); return; }
        startDownload(rel, inst, hex);
    });
}

void UpdateChecker::startDownload(const update::Release& rel, const update::Asset& inst, const QString& expectedHex) {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/updates/") + rel.version;
    if (!QDir().mkpath(base)) { emit failed(tr("Không tạo được thư mục tải: %1").arg(base)); return; }
    const QString dest = base + QLatin1Char('/') + inst.name;  // tên đã được parseRelease loại ký tự đường dẫn
    m_reply = get(QUrl(inst.url));
    QNetworkReply* r = m_reply;
    auto* out = new QSaveFile(dest, r);
    if (!out->open(QIODevice::WriteOnly)) { r->abort(); emit failed(tr("Không ghi được tệp tải về.")); return; }
    auto total = std::make_shared<qint64>(0);
    connect(r, &QNetworkReply::downloadProgress, this, [this](qint64 d, qint64 t) { emit progress(d, t); });
    connect(r, &QNetworkReply::readyRead, r, [r, out, total, this] {
        const QByteArray chunk = r->readAll();
        *total += chunk.size();
        if (*total > kMaxInstallerBytes) { r->abort(); return; }
        out->write(chunk);
    });
    connect(r, &QNetworkReply::finished, this, [this, r, out, dest, expectedHex, total] {
        r->deleteLater();
        if (m_reply == r) m_reply = nullptr;
        if (r->error() != QNetworkReply::NoError || *total > kMaxInstallerBytes) {
            out->cancelWriting();
            emit failed(*total > kMaxInstallerBytes ? tr("Tệp tải về quá lớn, đã huỷ.") : tr("Tải bộ cài thất bại: %1").arg(r->errorString()));
            return;
        }
        if (!update::isTrustedUrl(r->url())) { out->cancelWriting(); emit failed(tr("Bộ cài đến từ địa chỉ không tin cậy.")); return; }
        if (!out->commit()) { emit failed(tr("Không lưu được bộ cài.")); return; }
        QString err;
        if (!update::verifyFile(dest, expectedHex, &err)) {
            QFile::remove(dest);  // không giữ tệp không khớp
            emit failed(err);
            return;
        }
        emit verified(dest);
    });
}
