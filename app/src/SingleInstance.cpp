// SPDX-License-Identifier: MPL-2.0
#include "SingleInstance.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int kMaxMessage = 1 << 20;  // 1 MiB
constexpr int kMaxFiles = 50;
}  // namespace

QString SingleInstance::defaultKey() {
    const QString user = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER", QStringLiteral("user")));
    const QByteArray h = QCryptographicHash::hash(user.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("DungOffice-1-") + QString::fromLatin1(h);
}

SingleInstance::SingleInstance(const QString& key, QObject* parent) : QObject(parent), m_key(key) {}

SingleInstance::~SingleInstance() {
    if (m_server) m_server->close();
}

QByteArray SingleInstance::buildMessage(const QStringList& files) {
    QJsonArray a;
    for (const QString& f : files) a.append(f);
    QJsonObject o;
    o.insert(QStringLiteral("v"), 1);
    o.insert(QStringLiteral("files"), a);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QStringList SingleInstance::parseMessage(const QByteArray& msg, bool* wantsRaise) {
    if (wantsRaise) *wantsRaise = false;
    if (msg.isEmpty() || msg.size() > kMaxMessage) return {};
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(msg, &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) return {};
    const QJsonObject o = d.object();
    if (o.value(QStringLiteral("v")).toInt() != 1) return {};
    if (wantsRaise) *wantsRaise = true;
    QStringList out;
    for (const QJsonValue& v : o.value(QStringLiteral("files")).toArray()) {
        if (out.size() >= kMaxFiles) break;
        const QString p = v.toString();
        if (p.isEmpty() || p.contains(QChar(0))) continue;
        if (!QDir::isAbsolutePath(p)) continue;
        const QFileInfo fi(p);
        if (!fi.isFile()) continue;  // bỏ thư mục, thiết bị, đường dẫn không tồn tại
        out << fi.absoluteFilePath();
    }
    return out;
}

bool SingleInstance::sendToRunning(const QStringList& files, int timeoutMs) const {
    QLocalSocket s;
    s.connectToServer(m_key);
    if (!s.waitForConnected(timeoutMs)) return false;
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);  // cho phép cửa sổ cũ lên trước dù tiến trình mới đang giữ tiêu điểm
#endif
    QStringList abs;
    for (const QString& f : files) abs << QFileInfo(f).absoluteFilePath();
    s.write(buildMessage(abs));
    s.write("\n");
    if (!s.waitForBytesWritten(timeoutMs)) return false;
    // Chờ xác nhận "OK" để chắc chắn phiên cũ đã nhận trước khi tiến trình mới thoát.
    if (!s.waitForReadyRead(timeoutMs)) return false;
    return s.readAll().startsWith("OK");
}

bool SingleInstance::listen() {
    if (m_server) return true;
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);  // chỉ cùng người dùng mới kết nối được
    QLocalServer::removeServer(m_key);                           // xoá ổ cắm cũ sau crash (Linux/macOS)
    if (!m_server->listen(m_key)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* c = m_server->nextPendingConnection()) {
            auto* buf = new QByteArray;
            connect(c, &QLocalSocket::readyRead, c, [this, c, buf] {
                buf->append(c->readAll());
                if (buf->size() > kMaxMessage) { c->disconnectFromServer(); return; }
                if (!buf->contains('\n')) return;
                bool raise = false;
                const QStringList files = parseMessage(buf->left(buf->indexOf('\n')), &raise);
                buf->clear();
                c->write(raise ? "OK" : "NO");
                c->flush();
                if (raise) {
                    if (!files.isEmpty()) emit filesReceived(files);
                    emit raiseRequested();
                }
                c->disconnectFromServer();
            });
            connect(c, &QLocalSocket::disconnected, c, [c, buf] { delete buf; c->deleteLater(); });
        }
    });
    return true;
}
