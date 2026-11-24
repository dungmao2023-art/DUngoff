// Kiểm thử lõi cập nhật + một phiên duy nhất. Không dùng mạng. SPDX-License-Identifier: MPL-2.0
#include "SingleInstance.h"
#include "UpdateCore.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* name, const QString& detail = QString()) {
    if (ok) { ++g_pass; std::printf("[ĐẠT]  %s\n", name); }
    else { ++g_fail; std::printf("[LỖI]  %s\n", name); if (!detail.isEmpty()) std::printf("       %s\n", qPrintable(detail)); }
    std::fflush(stdout);
}

static QByteArray hexOf(const QByteArray& d) { return QCryptographicHash::hash(d, QCryptographicHash::Sha256).toHex(); }

static void testVersions() {
    using namespace update;
    auto newer = [](const char* a, const char* b) { return isNewer(QString::fromLatin1(a), QString::fromLatin1(b)); };
    check(newer("0.1.0", "0.2.0"), "0.2.0 mới hơn 0.1.0");
    check(newer("0.9.0", "0.10.0"), "so sánh theo số: 0.10.0 mới hơn 0.9.0 (không theo chuỗi)");
    check(!newer("1.0.0", "v1.0.0"), "v1.0.0 bằng 1.0.0, không phải bản mới");
    check(!newer("1.0.0", "1.0"), "1.0 bằng 1.0.0");
    check(!newer("1.0.0", "0.9.9"), "bản cũ hơn không được coi là mới");
    check(!newer("1.0.0", "1.0.0-beta.1"), "bản thử nghiệm nhỏ hơn bản chính thức cùng số");
    check(newer("1.0.0-beta.1", "1.0.0"), "bản chính thức mới hơn bản thử nghiệm của nó");
    check(newer("1.0.0-beta.2", "1.0.0-beta.10"), "beta.10 mới hơn beta.2 (so sánh số trong hậu tố)");
    check(newer("1.0.0-alpha", "1.0.0-beta"), "alpha < beta");
    check(newer("1.0.0", "1.0.1+build7"), "bỏ phần +build khi so sánh");
    QString err;
    check(!isNewer(QStringLiteral("1.0.0"), QStringLiteral("abc"), &err) && !err.isEmpty(), "phiên bản sai định dạng -> không kết luận 'mới' và có lý do", err);
    check(!isNewer(QStringLiteral("1.0.0"), QString(), &err) && !err.isEmpty(), "phiên bản rỗng bị từ chối");
    check(!parseVersion(QStringLiteral("1.2.x")).valid, "1.2.x không hợp lệ");
    check(!parseVersion(QStringLiteral("1..2")).valid, "1..2 không hợp lệ");
    check(!parseVersion(QStringLiteral("1.2.3-")).valid, "hậu tố rỗng không hợp lệ");
    check(!parseVersion(QStringLiteral("1.2.3.4.5")).valid, "quá nhiều thành phần bị từ chối");
    check(!parseVersion(QStringLiteral("99999999999.0")).valid, "số quá lớn bị từ chối");
}

static void testUrls() {
    using update::isTrustedUrl;
    check(isTrustedUrl(QUrl(QStringLiteral("https://github.com/dungmao2023-art/DUngoff/releases/download/v1/a.exe"))), "github.com https tin cậy");
    check(isTrustedUrl(QUrl(QStringLiteral("https://objects.githubusercontent.com/x"))), "objects.githubusercontent.com tin cậy");
    check(isTrustedUrl(QUrl(QStringLiteral("https://api.github.com/repos/a/b"))), "api.github.com tin cậy");
    check(!isTrustedUrl(QUrl(QStringLiteral("http://github.com/x"))), "http (không mã hoá) bị từ chối");
    check(!isTrustedUrl(QUrl(QStringLiteral("https://evilgithub.com/x"))), "evilgithub.com bị từ chối");
    check(!isTrustedUrl(QUrl(QStringLiteral("https://github.com.evil.com/x"))), "github.com.evil.com bị từ chối");
    check(!isTrustedUrl(QUrl(QStringLiteral("https://github.com@evil.com/x"))), "user@host lừa đảo bị từ chối");
    check(!isTrustedUrl(QUrl(QStringLiteral("https://user:pw@github.com/x"))), "URL có thông tin đăng nhập bị từ chối");
    check(!isTrustedUrl(QUrl(QStringLiteral("file:///C:/a.exe"))), "file:// bị từ chối");
    check(!isTrustedUrl(QUrl(QStringLiteral("ftp://github.com/a"))), "ftp bị từ chối");
    check(!isTrustedUrl(QUrl()), "URL rỗng bị từ chối");
}

static QByteArray sampleRelease(const QString& assetsJson, const QString& tag = QStringLiteral("v0.2.0")) {
    return QStringLiteral("{\"tag_name\":\"%1\",\"name\":\"Bản %1\",\"body\":\"Ghi chú <b>x</b>\",\"html_url\":\"https://github.com/dungmao2023-art/DUngoff/releases/tag/%1\","
                          "\"prerelease\":false,\"draft\":false,\"assets\":%2}").arg(tag, assetsJson).toUtf8();
}

static void testRelease() {
    using namespace update;
    const QString good =
        "[{\"name\":\"DungOffice-Setup-0.2.0-x64.exe\",\"browser_download_url\":\"https://github.com/dungmao2023-art/DUngoff/releases/download/v0.2.0/DungOffice-Setup-0.2.0-x64.exe\",\"size\":193000000},"
        "{\"name\":\"SHA256SUMS.txt\",\"browser_download_url\":\"https://github.com/dungmao2023-art/DUngoff/releases/download/v0.2.0/SHA256SUMS.txt\",\"size\":120}]";
    Release r = parseRelease(sampleRelease(good));
    check(r.ok && r.version == QLatin1String("0.2.0") && r.assets.size() == 2, "phân tích release hợp lệ");
    check(findInstaller(r).name == QLatin1String("DungOffice-Setup-0.2.0-x64.exe"), "tìm đúng bộ cài x64");
    check(findChecksumAsset(r, findInstaller(r).name).name == QLatin1String("SHA256SUMS.txt"), "tìm đúng tệp checksum");
    check(r.notes.contains(QLatin1String("<b>")), "ghi chú giữ nguyên chữ (hiển thị dạng văn bản thuần ở giao diện)");

    const QString perFile =
        "[{\"name\":\"DungOffice-Setup-0.2.0-x64.exe\",\"browser_download_url\":\"https://github.com/a/b/x.exe\"},"
        "{\"name\":\"DungOffice-Setup-0.2.0-x64.exe.sha256\",\"browser_download_url\":\"https://github.com/a/b/x.exe.sha256\"}]";
    r = parseRelease(sampleRelease(perFile));
    check(findChecksumAsset(r, QStringLiteral("DungOffice-Setup-0.2.0-x64.exe")).name.endsWith(QLatin1String(".sha256")), "ưu tiên tệp .sha256 riêng của bộ cài");

    const QString evil =
        "[{\"name\":\"DungOffice-Setup-0.2.0-x64.exe\",\"browser_download_url\":\"https://evil.example.com/x.exe\"},"
        "{\"name\":\"../../boot.exe\",\"browser_download_url\":\"https://github.com/a/b/x\"},"
        "{\"name\":\"a\\\\b.exe\",\"browser_download_url\":\"https://github.com/a/b/y\"},"
        "{\"name\":\"plain.exe\",\"browser_download_url\":\"http://github.com/a/b/z\"}]";
    r = parseRelease(sampleRelease(evil));
    check(r.ok && r.assets.isEmpty() && r.rejected.size() == 4, "tệp có URL lạ/tên chứa đường dẫn/http đều bị loại", QString::number(r.assets.size()));
    check(findInstaller(r).name.isEmpty(), "không có bộ cài tin cậy -> không chọn gì");

    check(!parseRelease("not json").ok, "JSON hỏng bị từ chối");
    check(!parseRelease("[]").ok, "JSON không phải đối tượng bị từ chối");
    check(!parseRelease("{\"assets\":[]}").ok, "thiếu tag_name bị từ chối");
    check(!parseRelease(sampleRelease("[]", QStringLiteral("không-phải-số"))).ok, "tag sai định dạng bị từ chối");
    check(parseRelease(sampleRelease("[]")).assets.isEmpty(), "release không có tệp đính kèm vẫn phân tích được");
    r = parseRelease("{\"tag_name\":\"v1.0.0\",\"draft\":true,\"prerelease\":true,\"assets\":[]}");
    check(r.ok && r.draft && r.prerelease, "nhận biết bản nháp và bản thử nghiệm");
}

static void testChecksums() {
    using namespace update;
    const QByteArray h1 = hexOf("a"), h2 = hexOf("b");
    const QString txt = QString::fromLatin1(h1 + "  DungOffice-Setup-0.2.0-x64.exe\r\n# ghi chú\r\n\r\n" + h2.toUpper() + " *Other.zip\r\nrác không phải mã\r\n"
                                           "1234  tooshort.bin\r\n");
    auto m = parseChecksums(txt);
    check(m.size() == 2 && m.value(QStringLiteral("dungoffice-setup-0.2.0-x64.exe")) == QString::fromLatin1(h1), "đọc định dạng sha256sum, bỏ dòng rác/mã ngắn");
    check(m.value(QStringLiteral("other.zip")) == QString::fromLatin1(h2), "mã chữ hoa được chuẩn hoá, dấu * (nhị phân) được chấp nhận");
    QString err;
    check(expectedHash(txt, QStringLiteral("DungOffice-Setup-0.2.0-x64.exe"), &err) == QString::fromLatin1(h1), "lấy đúng mã cho bộ cài (không phân biệt hoa/thường tên)");
    check(expectedHash(txt, QStringLiteral("khong-co.exe"), &err).isEmpty() && !err.isEmpty(), "tên không có trong checksum -> từ chối", err);
    const QString conflict = QString::fromLatin1(h1 + "  a.exe\n" + h2 + "  a.exe\n");
    err.clear();
    check(expectedHash(conflict, QStringLiteral("a.exe"), &err).isEmpty() && !err.isEmpty(), "hai mã khác nhau cho cùng một tên -> từ chối", err);
    check(expectedHash(QString::fromLatin1(h1 + "\n"), QStringLiteral("whatever.exe"), &err) == QString::fromLatin1(h1), "tệp .sha256 chỉ có một mã không kèm tên được chấp nhận");
    check(expectedHash(QString::fromLatin1(h1 + "\n" + h2 + "\n"), QStringLiteral("whatever.exe"), &err).isEmpty(), "hai mã không kèm tên -> từ chối (mơ hồ)");
    check(expectedHash(QString(), QStringLiteral("a.exe"), &err).isEmpty(), "tệp checksum rỗng -> từ chối");
}

static void testVerify() {
    QTemporaryDir td;
    const QString path = td.path() + QStringLiteral("/setup.exe");
    const QByteArray data(300000, 'x');
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(data);
    f.close();
    QString err;
    check(update::verifyFile(path, QString::fromLatin1(hexOf(data)), &err), "SHA-256 khớp -> chấp nhận");
    check(update::verifyFile(path, QString::fromLatin1(hexOf(data).toUpper()), &err), "mã chữ hoa vẫn khớp");
    err.clear();
    check(!update::verifyFile(path, QString::fromLatin1(hexOf("khác")), &err) && err.contains(QStringLiteral("KHÔNG khớp")), "SHA-256 sai -> từ chối", err);
    check(!update::verifyFile(path, QString(), &err), "mã rỗng -> từ chối");
    check(!update::verifyFile(path, QStringLiteral("abc"), &err), "mã sai độ dài -> từ chối");
    check(!update::verifyFile(path, QString(64, QLatin1Char('z')), &err), "mã không phải hex -> từ chối");
    check(!update::verifyFile(td.path() + QStringLiteral("/khong-co.exe"), QString::fromLatin1(hexOf(data)), &err), "thiếu tệp -> từ chối");
    // Một byte bị đổi
    QFile g(path);
    g.open(QIODevice::ReadWrite);
    g.seek(1000);
    g.write("Y");
    g.close();
    check(!update::verifyFile(path, QString::fromLatin1(hexOf(data)), &err), "tệp bị sửa 1 byte -> từ chối");
}

static void testParseMessage() {
    QTemporaryDir td;
    const QString real = td.path() + QStringLiteral("/Tài liệu có dấu.docx");
    QFile f(real);
    f.open(QIODevice::WriteOnly);
    f.write("x");
    f.close();
    bool raise = false;
    QStringList got = SingleInstance::parseMessage(SingleInstance::buildMessage({real}), &raise);
    check(raise && got.size() == 1 && got[0] == QFileInfo(real).absoluteFilePath(), "thông điệp hợp lệ giữ tệp thật (tên tiếng Việt)");
    got = SingleInstance::parseMessage(SingleInstance::buildMessage({real, td.path(), td.path() + QStringLiteral("/khong-co.docx"), QStringLiteral("tuong-doi.docx")}), &raise);
    check(raise && got.size() == 1, "thư mục, tệp không tồn tại, đường dẫn tương đối đều bị bỏ", QString::number(got.size()));
    got = SingleInstance::parseMessage(SingleInstance::buildMessage({}), &raise);
    check(raise && got.isEmpty(), "thông điệp không có tệp vẫn là yêu cầu đưa cửa sổ lên");
    got = SingleInstance::parseMessage("rác", &raise);
    check(!raise && got.isEmpty(), "thông điệp không phải JSON bị bỏ");
    got = SingleInstance::parseMessage("{\"v\":2,\"files\":[]}", &raise);
    check(!raise && got.isEmpty(), "phiên bản giao thức lạ bị bỏ");
    got = SingleInstance::parseMessage(QByteArray(2 * 1024 * 1024, 'a'), &raise);
    check(!raise && got.isEmpty(), "thông điệp quá lớn bị bỏ");
    QStringList many;
    for (int i = 0; i < 80; ++i) many << real;
    got = SingleInstance::parseMessage(SingleInstance::buildMessage(many), &raise);
    check(got.size() == 50, "tối đa 50 tệp một lần", QString::number(got.size()));
}

// Kiểm thử đa tiến trình: chính tiến trình này chạy lại với --send để đóng vai "lần nhấp đúp thứ hai".
static void testSingleInstance(const QString& self) {
    const QString key = QStringLiteral("DungOffice-test-%1").arg(QCoreApplication::applicationPid());
    QTemporaryDir td;
    const QString real = td.path() + QStringLiteral("/Hồ sơ.xlsx");
    QFile f(real);
    f.open(QIODevice::WriteOnly);
    f.write("x");
    f.close();

    {   // Không có phiên nào -> tiến trình mới phải tự chạy (trả false)
        SingleInstance lone(key);
        check(!lone.sendToRunning({real}, 300), "không có phiên đang chạy -> trả false (tiến trình mới tự chạy)");
    }

    SingleInstance primary(key);
    check(primary.listen(), "phiên chính mở được kênh lắng nghe");
    QStringList received;
    int raised = 0;
    QObject::connect(&primary, &SingleInstance::filesReceived, [&](const QStringList& l) { received += l; });
    QObject::connect(&primary, &SingleInstance::raiseRequested, [&] { ++raised; });

    auto runChild = [&](const QStringList& files) {
        QProcess p;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&p, &QProcess::finished, &loop, &QEventLoop::quit);
        p.start(self, QStringList{QStringLiteral("--send"), key} + files);
        timeout.start(15000);
        loop.exec();
        if (p.state() != QProcess::NotRunning) { p.kill(); p.waitForFinished(); return -99; }
        return p.exitCode();
    };

    int code = runChild({real});
    check(code == 0, "tiến trình thứ hai gửi tệp tới phiên chính và nhận xác nhận (mã thoát 0)", QString::number(code));
    check(received.size() == 1 && received[0] == QFileInfo(real).absoluteFilePath(), "phiên chính nhận đúng đường dẫn tệp tiếng Việt");
    check(raised == 1, "phiên chính được yêu cầu đưa cửa sổ lên");

    received.clear();
    code = runChild({});
    check(code == 0 && received.isEmpty() && raised == 2, "chạy lại không kèm tệp chỉ đưa cửa sổ lên");

    received.clear();
    code = runChild({td.path() + QStringLiteral("/khong-ton-tai.xlsx"), real});
    check(code == 0 && received.size() == 1, "tệp không tồn tại bị lọc, tệp thật vẫn được mở");
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();  // Unicode đúng trên Windows (argv là bảng mã ANSI)
    if (args.size() >= 3 && args[1] == QLatin1String("--send")) {
        SingleInstance s(args[2]);
        return s.sendToRunning(args.mid(3), 5000) ? 0 : 3;
    }
    testVersions();
    testUrls();
    testRelease();
    testChecksums();
    testVerify();
    testParseMessage();
    testSingleInstance(QCoreApplication::applicationFilePath());
    std::printf("\nKẾT QUẢ: %s (đạt %d, lỗi %d)\n", g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
