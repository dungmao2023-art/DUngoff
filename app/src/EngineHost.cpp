// SPDX-License-Identifier: MPL-2.0
#include "EngineHost.h"
#include "ExitTrace.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

EngineHost& EngineHost::instance() {
    static EngineHost h;
    return h;
}

EngineHost::~EngineHost() {
    exittrace::log("EngineHost: bắt đầu huỷ engine (lok destroy)");
    m_engine.reset();
    exittrace::log("EngineHost: huỷ engine xong");
}

static bool hasLokLibrary(const QString& dir) {
    QDir d(dir);
#ifdef Q_OS_WIN
    return d.exists(QStringLiteral("sofficeapp.dll")) || d.exists(QStringLiteral("mergedlo.dll"));
#else
    return d.exists(QStringLiteral("libsofficeapp.so")) || d.exists(QStringLiteral("libmergedlo.so"));
#endif
}

QString EngineHost::findRuntime() {
    QStringList candidates;
    const QString env = qEnvironmentVariable("DUNG_LO_PROGRAM");
    if (!env.isEmpty()) candidates << env;
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << appDir + QStringLiteral("/engine/program") << appDir + QStringLiteral("/../engine/program");
#ifdef Q_OS_WIN
    candidates << QStringLiteral("C:/Program Files/LibreOffice/program");
#else
    candidates << QStringLiteral("/usr/lib/libreoffice/program") << QStringLiteral("/usr/lib64/libreoffice/program");
    for (const QString& o : QDir(QStringLiteral("/opt")).entryList({QStringLiteral("libreoffice*")}, QDir::Dirs))
        candidates << QStringLiteral("/opt/") + o + QStringLiteral("/program");
#endif
    for (const QString& c : candidates)
        if (hasLokLibrary(c)) return QDir::cleanPath(QFileInfo(c).absoluteFilePath());
    return {};
}

bool EngineHost::start() {
    if (m_engine) return true;
    m_programDir = findRuntime();
    if (m_programDir.isEmpty()) {
        m_error = tr("Không tìm thấy bộ engine tài liệu (thư mục engine/program). "
                     "Hãy cài lại Dũng Office.");
        return false;
    }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    // Thư mục tạm riêng cho tiến trình này; dọn các thư mục tạm cũ hơn 2 ngày của phiên trước.
    QDir workRoot(base + QStringLiteral("/work"));
    workRoot.mkpath(QStringLiteral("."));
    for (const QFileInfo& fi : workRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (fi.lastModified().daysTo(QDateTime::currentDateTime()) >= 2) QDir(fi.absoluteFilePath()).removeRecursively();
    const QString work = workRoot.filePath(QStringLiteral("%1-%2").arg(QCoreApplication::applicationPid())
                                               .arg(QDateTime::currentMSecsSinceEpoch()));
    std::string err;
    m_engine = dung::Engine::create(toPath(m_programDir), toPath(base + QStringLiteral("/profile")), toPath(work), &err);
    if (!m_engine) {
        m_error = qs(err);
        return false;
    }
    emit started();
    return true;
}

QString EngineHost::versionText() const {
    if (!m_engine) return {};
    const QJsonObject o = QJsonDocument::fromJson(QByteArray::fromStdString(m_engine->versionInfo())).object();
    return tr("Engine LibreOffice %1%2").arg(o.value(QStringLiteral("ProductVersion")).toString(),
                                             o.value(QStringLiteral("ProductExtension")).toString());
}

std::unique_ptr<dung::Document> EngineHost::open(const QString& path, QString* error) {
    if (!start()) { if (error) *error = m_error; return nullptr; }
    std::string err;
    auto d = m_engine->open(toPath(path), &err);
    if (!d && error) *error = qs(err);
    return d;
}

std::unique_ptr<dung::Document> EngineHost::createNew(dung::DocKind kind, QString* error) {
    if (!start()) { if (error) *error = m_error; return nullptr; }
    std::string err;
    auto d = m_engine->createNew(kind, &err);
    if (!d && error) *error = qs(err);
    return d;
}
