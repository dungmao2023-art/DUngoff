// SPDX-License-Identifier: MPL-2.0
// Kiểm thử kho bản phục hồi. Tự chạy lại chính nó làm "tiến trình con" để mô phỏng sập (thoát không dọn).
#include "RecoveryStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s (dòng %d)\n", msg, __LINE__); ++g_fail; } else std::printf("ok: %s\n", msg); } while (0)

static bool writeFile(const QString& p, const QByteArray& b) { QFile f(p); if (!f.open(QIODevice::WriteOnly)) return false; f.write(b); return true; }

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList a = app.arguments();
    if (a.size() >= 4 && a[1] == QLatin1String("--child")) {
        // a[2]=thư mục, a[3]=crash|live
        recovery::setDirForTest(a[2]);
        recovery::Session& s = recovery::Session::instance();
        if (!s.begin()) return 3;
        const QString id = s.newDocId();
        writeFile(s.dataPath(id, "docx"), "DU-LIEU-CON");
        if (!s.commit(id, "docx", "Bao cao.docx", "C:/tai-lieu/Bao cao.docx", false)) return 4;
        const QString id2 = s.newDocId();
        writeFile(s.dataPath(id2, "xlsx"), "MOI");
        s.commit(id2, "xlsx", "Bảng 1", "", true);
        std::printf("READY\n"); std::fflush(stdout);
        if (a[3] == QLatin1String("live")) { char c; if (std::fread(&c, 1, 1, stdin) < 0) {} s.end(); return 0; }
        std::_Exit(0);  // mô phỏng sập: không gọi end(), khoá để lại
    }

    QTemporaryDir td;
    CHECK(td.isValid(), "tạo thư mục tạm");
    const QString dir = td.path() + "/recovery-" + QString(40, QChar('x'));  // đường dẫn dài một chút
    recovery::setDirForTest(dir);

    CHECK(recovery::snapshotOrigin("new.xlsx",true,"original.xlsx","temporary.xlsx")=="new.xlsx","Save As recovery points to current saved file");
    CHECK(recovery::snapshotOrigin("",true,"original.xlsx","temporary.xlsx")=="original.xlsx","recovered origin retained before Save As");
    CHECK(recovery::snapshotOrigin("",true,"","temporary.xlsx").isEmpty(),"new recovered document not linked to temporary file");
    CHECK(recovery::snapshotOrigin("",false,"","initial.docx")=="initial.docx","ordinary document keeps source");
    // 1. phiên này: begin / commit / discard / end
    auto& s = recovery::Session::instance();
    CHECK(s.begin(), "begin phiên");
    CHECK(s.begin(), "begin lần hai vẫn ổn");
    QString id = s.newDocId();
    CHECK(!s.commit(id, "docx", "x", "", true), "commit khi chưa có dữ liệu bị từ chối");
    CHECK(writeFile(s.dataPath(id, "docx"), "abc"), "ghi dữ liệu");
    CHECK(s.commit(id, "docx", "x", "", true), "commit sau khi có dữ liệu");
    CHECK(recovery::findOrphans().isEmpty(), "bản của phiên đang chạy không bị coi là mồ côi");
    s.discard(id);
    CHECK(!QFile::exists(s.dataPath(id, "docx")), "discard xoá dữ liệu");
    CHECK(s.hasRoom(), "còn chỗ");
    CHECK(!s.hasRoom(1), "giới hạn dung lượng được tôn trọng");
    QString id3 = s.newDocId();
    writeFile(s.dataPath(id3, "pptx"), "z"); s.commit(id3, "pptx", "z", "", true);
    s.end();
    CHECK(QDir(dir).entryList(QDir::Files).filter(".pptx").isEmpty(), "thoát bình thường xoá bản phục hồi");

    // 2. tiến trình con sập -> mồ côi
    {
        QProcess p; p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(a[0], {"--child", dir, "crash"});
        CHECK(p.waitForFinished(20000) && p.exitCode() == 0, "tiến trình con chạy xong");
        CHECK(p.readAll().contains("READY"), "con báo READY");
    }
    auto orph = recovery::findOrphans();
    CHECK(orph.size() == 2, "thấy 2 bản mồ côi sau sập");
    bool gotDoc = false, gotNew = false;
    for (const auto& e : orph) {
        if (e.originalPath == "C:/tai-lieu/Bao cao.docx" && e.ext == "docx" && !e.isNew) {
            QFile f(e.dataPath); gotDoc = f.open(QIODevice::ReadOnly) && f.readAll() == "DU-LIEU-CON"; }
        if (e.isNew && e.displayName == QString::fromUtf8("Bảng 1") && e.originalPath.isEmpty()) gotNew = true;
    }
    CHECK(gotDoc, "bản của tệp có sẵn đủ thông tin và dữ liệu");
    CHECK(gotNew, "bản của tài liệu mới (tên tiếng Việt)");

    // 3. mồ côi chỉ liệt kê, chưa xoá, đến khi người dùng chọn
    CHECK(recovery::findOrphans().size() == 2, "tìm lại vẫn còn (không tự xoá)");

    // 4. tệp mô tả hỏng không làm sập; dữ liệu mồ côi không mô tả không hiện
    writeFile(dir + "/aaaaaaaaaaaa-1.json", "{hong");
    writeFile(dir + "/bbbbbbbbbbbb-1.docx", "ghi do");
    CHECK(recovery::findOrphans().size() == 2, "mô tả hỏng bị bỏ qua");
    CHECK(QFile::exists(dir + "/aaaaaaaaaaaa-1.json.hong"), "mô tả hỏng được giữ lại (đổi tên)");

    // 5. phiên khác còn sống: không bị coi là mồ côi
    {
        QProcess live; live.setProcessChannelMode(QProcess::MergedChannels);
        live.start(a[0], {"--child", dir, "live"});
        CHECK(live.waitForStarted(10000), "mở phiên con còn sống");
        bool ready = false;
        for (int i = 0; i < 100 && !ready; ++i) { live.waitForReadyRead(200); ready = live.peek(256).contains("READY"); }
        CHECK(ready, "phiên con sẵn sàng");
        CHECK(recovery::findOrphans().size() == 2, "bản của phiên còn sống không bị liệt kê");
        live.write("x"); live.closeWriteChannel();
        CHECK(live.waitForFinished(20000), "phiên con kết thúc");
        CHECK(QDir(dir).entryList(QStringList{"*-*.json"}, QDir::Files).size() <= 3, "phiên con thoát bình thường đã dọn bản của nó");
    }

    // 6. discardEntry
    orph = recovery::findOrphans();
    CHECK(!orph.isEmpty() && recovery::discardEntry(orph.first()), "bỏ một bản");
    CHECK(recovery::findOrphans().size() == 1, "còn 1 bản");

    // 7. prune theo dung lượng và tuổi
    CHECK(recovery::pruneOld(30, 1) >= 1, "dọn theo giới hạn dung lượng");
    CHECK(recovery::findOrphans().isEmpty(), "sau dọn không còn bản");

    std::printf(g_fail ? "RECOVERY: %d LỖI\n" : "RECOVERY: ĐẠT\n", g_fail);
    return g_fail ? 1 : 0;
}
