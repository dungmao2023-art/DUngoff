// SPDX-License-Identifier: MPL-2.0
#include "UpdateDialog.h"

#include "UpdateChecker.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

static const char* kReleasesPage = "https://github.com/dungmao2023-art/DUngoff/releases";

bool UpdateDialog::autoCheckEnabled() { return QSettings().value(QStringLiteral("update/autoCheck"), false).toBool(); }

bool UpdateDialog::autoCheckDue() {
    if (!autoCheckEnabled()) return false;
    const qint64 last = QSettings().value(QStringLiteral("update/lastCheck"), 0).toLongLong();
    return QDateTime::currentSecsSinceEpoch() - last > 24 * 3600;
}

void UpdateDialog::markChecked() { QSettings().setValue(QStringLiteral("update/lastCheck"), QDateTime::currentSecsSinceEpoch()); }

UpdateDialog::UpdateDialog(QWidget* parent) : QDialog(parent), m_checker(new UpdateChecker(this)) {
    setWindowTitle(tr("Cập nhật Dũng Office"));
    resize(560, 460);
    auto* v = new QVBoxLayout(this);
    v->addWidget(new QLabel(tr("<b>Phiên bản hiện tại:</b> %1<br><span style='color:#697992'>Ứng dụng chỉ kết nối mạng khi bạn bấm \"Kiểm tra\" (hoặc khi bạn tự bật kiểm tra hằng ngày bên dưới). "
                               "Không bao giờ tự cài: bộ cài chỉ chạy khi bạn bấm, sau khi đã kiểm tra SHA-256.</span>")
                               .arg(QCoreApplication::applicationVersion()), this));
    m_status = new QLabel(tr("Chưa kiểm tra."), this);
    m_status->setWordWrap(true);
    v->addWidget(m_status);
    m_notes = new QTextBrowser(this);
    m_notes->setOpenLinks(false);
    m_notes->setPlaceholderText(tr("Ghi chú phát hành sẽ hiện ở đây."));
    v->addWidget(m_notes, 1);
    m_bar = new QProgressBar(this);
    m_bar->setVisible(false);
    v->addWidget(m_bar);
    auto* row = new QHBoxLayout;
    m_check = new QPushButton(tr("Kiểm tra bản mới"), this);
    m_download = new QPushButton(tr("Tải về và kiểm tra SHA-256"), this);
    m_run = new QPushButton(tr("Chạy bộ cài…"), this);
    m_page = new QPushButton(tr("Mở trang phát hành"), this);
    m_download->setEnabled(false);
    m_run->setEnabled(false);
    for (auto* b : {m_check, m_download, m_run, m_page}) row->addWidget(b);
    v->addLayout(row);
    m_auto = new QCheckBox(tr("Tự kiểm tra khi mở ứng dụng (mặc định TẮT, tối đa mỗi ngày một lần)"), this);
    m_auto->setChecked(autoCheckEnabled());
    v->addWidget(m_auto);
    connect(m_auto, &QCheckBox::toggled, this, [](bool on) { QSettings().setValue(QStringLiteral("update/autoCheck"), on); });

    connect(m_check, &QPushButton::clicked, this, &UpdateDialog::startCheck);
    connect(m_page, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QString::fromLatin1(kReleasesPage))); });
    connect(m_checker, &UpdateChecker::checked, this, &UpdateDialog::onChecked);
    connect(m_checker, &UpdateChecker::progress, this, [this](qint64 d, qint64 t) {
        m_bar->setVisible(true);
        m_bar->setMaximum(t > 0 ? int(t / 1024) : 0);
        m_bar->setValue(int(d / 1024));
    });
    connect(m_checker, &UpdateChecker::failed, this, [this](const QString& m) {
        m_bar->setVisible(false);
        m_download->setEnabled(true);
        m_check->setEnabled(true);
        m_status->setText(tr("<span style='color:#B3261E'><b>Không thể cập nhật tự động:</b></span> %1<br>Tệp chưa được dùng. Bạn có thể tải thủ công ở trang phát hành.").arg(m.toHtmlEscaped()));
    });
    connect(m_checker, &UpdateChecker::verified, this, [this](const QString& path) {
        m_bar->setVisible(false);
        m_installer = path;
        m_run->setEnabled(true);
        m_check->setEnabled(true);
        m_status->setText(tr("<span style='color:#21866E'><b>Đã tải xong, SHA-256 khớp.</b></span> Đóng tài liệu đang mở rồi bấm \"Chạy bộ cài…\"."));
    });
    connect(m_download, &QPushButton::clicked, this, [this] {
        m_download->setEnabled(false);
        m_check->setEnabled(false);
        m_status->setText(tr("Đang tải bộ cài…"));
        m_checker->downloadAndVerify(m_release);
    });
    connect(m_run, &QPushButton::clicked, this, [this] {
        if (m_installer.isEmpty() || !QFileInfo::exists(m_installer)) return;
        if (QMessageBox::question(this, tr("Chạy bộ cài"),
                tr("Bộ cài sẽ chạy riêng. Hãy lưu và đóng mọi tài liệu của Dũng Office trước.\nChạy bộ cài bây giờ?")) != QMessageBox::Yes) return;
        QProcess::startDetached(m_installer, {});
    });
}

void UpdateDialog::startCheck() {
    m_check->setEnabled(false);
    m_download->setEnabled(false);
    m_run->setEnabled(false);
    m_status->setText(tr("Đang kiểm tra…"));
    m_checker->checkLatest();
}

void UpdateDialog::onChecked(const update::Release& r, bool newer, const QString& message) {
    m_check->setEnabled(true);
    markChecked();
    if (!r.ok) {
        m_status->setText(tr("<span style='color:#B3261E'><b>Lỗi:</b></span> %1").arg(message.toHtmlEscaped()));
        return;
    }
    m_release = r;
    m_notes->setPlainText(r.notes.isEmpty() ? tr("(Không có ghi chú)") : r.notes);  // văn bản thuần, không hiển thị HTML từ mạng
    if (!message.isEmpty()) {
        m_status->setText(message.toHtmlEscaped());
        return;
    }
    if (!newer) {
        m_status->setText(tr("Bạn đang dùng bản mới nhất (%1).").arg(r.version.toHtmlEscaped()));
        return;
    }
    QString txt = tr("<b>Có bản mới: %1</b>").arg(r.version.toHtmlEscaped());
    if (r.prerelease) txt += tr(" <span style='color:#AF791D'>(bản thử nghiệm)</span>");
    const bool hasInst = !update::findInstaller(r).name.isEmpty();
    const bool hasSum = hasInst && !update::findChecksumAsset(r, update::findInstaller(r).name).name.isEmpty();
    if (!hasInst) txt += tr("<br>Bản này chưa có bộ cài Windows x64 — hãy mở trang phát hành.");
    else if (!hasSum) txt += tr("<br>Bản này <b>không có tệp SHA-256</b> nên mình không tải để cài. Hãy tải thủ công và tự kiểm tra.");
    m_status->setText(txt);
    m_download->setEnabled(hasInst && hasSum);
}
