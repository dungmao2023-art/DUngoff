// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "dung/Engine.h"

#include <QObject>
#include <QString>
#include <memory>

// Giữ engine duy nhất của tiến trình, tìm runtime LibreOffice, quản lý thư mục tạm.
class EngineHost : public QObject {
    Q_OBJECT
public:
    static EngineHost& instance();

    bool isReady() const { return (bool)m_engine; }
    // Khởi động engine (chặn ~1–3 giây lần đầu). Trả về false và đặt lastError() nếu lỗi.
    bool start();
    QString lastError() const { return m_error; }
    QString runtimeDir() const { return m_programDir; }
    QString versionText() const;

    std::unique_ptr<dung::Document> open(const QString& path, QString* error);
    std::unique_ptr<dung::Document> createNew(dung::DocKind kind, QString* error);

    // Tìm thư mục "program" của runtime LibreOffice đi kèm hoặc trên máy (khi phát triển).
    static QString findRuntime();

signals:
    void started();

private:
    EngineHost() = default;
    std::unique_ptr<dung::Engine> m_engine;
    QString m_error;
    QString m_programDir;
};

inline QString qs(const std::string& s) { return QString::fromUtf8(s.data(), (int)s.size()); }
inline std::string ss(const QString& s) { const QByteArray b = s.toUtf8(); return std::string(b.constData(), (size_t)b.size()); }
inline dung::fs::path toPath(const QString& s) { return dung::fromUtf8(ss(s)); }
inline QString fromPath(const dung::fs::path& p) { return qs(dung::toUtf8(p)); }
