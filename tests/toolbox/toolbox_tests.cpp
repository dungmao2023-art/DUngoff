// SPDX-License-Identifier: MPL-2.0
// Kiểm thử Hộp công cụ (không giao diện): sổ đăng ký, tìm kiếm không dấu, tool.json (hợp lệ/hỏng/thoát thư mục),
// xác nhận lần đầu gắn với dấu vân tay, ghép đối số an toàn, chạy/hết giờ/dừng, đồng hồ.
#include "ExternalTool.h"
#include "ToolRegistry.h"
#include "tools/TimerFormat.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

using namespace toolbox;

static int g_fail = 0;
#define CHECK(c, what) do { const bool ok_ = (c); std::printf("[%s]  %s\n", ok_ ? "ĐẠT" : "LỖI", what); if (!ok_) ++g_fail; } while (0)
#define CHECKQ(c, what) CHECK((c), what.toUtf8().constData())

static void writeFile(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(data);
}

static QWidget* dummyCreate(QWidget*) { return nullptr; }

static QByteArray manifest(const QString& extra = QString(), const QString& command = QStringLiteral("python"),
                           const QString& id = QStringLiteral("thu-nghiem")) {
    QString j = QStringLiteral("{\"schema\":1,\"id\":\"%1\",\"name\":\"Tool thử\",\"command\":\"%2\"").arg(id, command);
    if (!extra.isEmpty()) j += QLatin1Char(',') + extra;
    j += QLatin1Char('}');
    return j.toUtf8();
}

// ---- chế độ tiến trình con: chính tệp thử này đóng vai "chương trình ngoài" ----
// Dùng QCoreApplication::arguments() (UTF-16 trên Windows) để đối số tiếng Việt không bị hỏng bởi bảng mã ANSI của argv.
static int childMain(const QStringList& args) {
    const QString mode = args.value(2);
    if (mode == QLatin1String("echo")) {
        const QByteArray out = "ARGS:" + args.mid(3).join(QLatin1Char('|')).toUtf8() + "\n" + QStringLiteral("Tiếng Việt").toUtf8() + "\n";
        std::fwrite(out.constData(), 1, size_t(out.size()), stdout);
        std::fflush(stdout);
        return 3;
    }
    if (mode == QLatin1String("sleep")) {
        std::fputs("ngu\n", stdout);
        std::fflush(stdout);
        for (int i = 0; i < 600; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    }
    return 9;
}

struct RunResult { QString out; int code = -99; bool timedOut = false, stopped = false, crashed = false, got = false; qint64 ms = 0; };

static RunResult runAndWait(const Command& c, int stopAfterMs = -1, int hardLimitMs = 20000) {
    RunResult r;
    ToolRunner runner;
    runner.setToolId(QStringLiteral("thu-nghiem"));
    QEventLoop loop;
    QObject::connect(&runner, &ToolRunner::output, [&](const QString& t, bool) { r.out += t; });
    QObject::connect(&runner, &ToolRunner::finished, [&](int code, bool to, bool st, bool cr) {
        r.code = code; r.timedOut = to; r.stopped = st; r.crashed = cr; r.got = true;
        loop.quit();
    });
    QTimer hard;
    hard.setSingleShot(true);
    QObject::connect(&hard, &QTimer::timeout, &loop, &QEventLoop::quit);
    hard.start(hardLimitMs);
    if (stopAfterMs >= 0) QTimer::singleShot(stopAfterMs, &runner, [&runner] { runner.stop(); });
    QString err;
    QElapsedTimer t;
    t.start();
    if (!runner.start(c, &err)) { r.out = err; return r; }
    if (!r.got) loop.exec(); // nếu đã kết thúc ngay trong start() (khởi động hỏng) thì không chờ nữa
    r.ms = t.elapsed();
    return r;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (QCoreApplication::arguments().size() >= 3 && QCoreApplication::arguments().at(1) == QLatin1String("--child"))
        return childMain(QCoreApplication::arguments());
    QCoreApplication::setApplicationName(QStringLiteral("DungOfficeToolboxTest"));
    QStandardPaths::setTestModeEnabled(true);
    QDir(toolsRoot()).removeRecursively();
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tools.json"));

    // ===== Sổ đăng ký =====
    clearForTest();
    CHECK(registerTool({QStringLiteral("dong_ho"), QStringLiteral("Đồng hồ"), QStringLiteral("Bấm giờ và đếm ngược"), QStringLiteral("Tiện ích"), QStringLiteral("T"), dummyCreate, false}),
          "đăng ký công cụ hợp lệ");
    CHECK(!registerTool({QStringLiteral("dong_ho"), QStringLiteral("Trùng mã"), QString(), QStringLiteral("Tiện ích"), QString(), dummyCreate, false}),
          "từ chối trùng mã (không ghi đè âm thầm)");
    CHECK(tools().size() == 1 && tools().first().name == QStringLiteral("Đồng hồ"), "công cụ gốc vẫn còn nguyên sau khi bị trùng mã");
    for (const QString& bad : {QStringLiteral("A"), QStringLiteral("Ab"), QStringLiteral("a b"), QStringLiteral("../x"), QStringLiteral("-ab"), QString(), QStringLiteral("x")})
        CHECKQ(!registerTool({bad, QStringLiteral("X"), QString(), QStringLiteral("K"), QString(), dummyCreate, false}), (QStringLiteral("từ chối mã sai: \"%1\"").arg(bad)));
    CHECK(!registerTool({QStringLiteral("khong_ham"), QStringLiteral("X"), QString(), QStringLiteral("K"), QString(), nullptr, false}), "từ chối thiếu hàm tạo giao diện");
    CHECK(!registerTool({QStringLiteral("khong_ten"), QStringLiteral("  "), QString(), QStringLiteral("K"), QString(), dummyCreate, false}), "từ chối tên rỗng");
    registerTool({QStringLiteral("chup_man_hinh"), QStringLiteral("Chụp màn hình"), QStringLiteral("Chụp và chú thích ảnh"), QStringLiteral("Ảnh"), QString(), dummyCreate, false});
    registerTool({QStringLiteral("tim_tep"), QStringLiteral("Tìm trong thư mục"), QStringLiteral("Tìm từ khoá nhanh"), QStringLiteral("Tệp"), QString(), dummyCreate, false});
    registerTool({QStringLiteral("khong_nhom"), QStringLiteral("Lẻ loi"), QString(), QString(), QString(), dummyCreate, false});
    CHECK(categories() == QStringList({QStringLiteral("Ảnh"), QStringLiteral("Khác"), QStringLiteral("Tệp"), QStringLiteral("Tiện ích")}) || categories().size() == 4,
          "gom công cụ theo 4 nhóm (nhóm trống thành \"Khác\")");
    CHECK(byCategory(QStringLiteral("Khác")).size() == 1, "nhóm \"Khác\" chứa công cụ không khai báo nhóm");
    CHECK(find(QStringLiteral("tim_tep")) && !find(QStringLiteral("khong_co")), "tìm công cụ theo mã");
    CHECK(search(QStringLiteral("chup man hinh")).size() == 1, "tìm không dấu: \"chup man hinh\" ra Chụp màn hình");
    CHECK(search(QStringLiteral("DONG HO")).size() == 1, "tìm không phân biệt hoa-thường");
    CHECK(search(QStringLiteral("từ khoá")).size() == 1 && search(QStringLiteral("tu khoa")).size() == 1, "tìm theo mô tả, có dấu hay không đều ra");
    CHECK(search(QStringLiteral("abc xyz khong co")).isEmpty(), "không khớp thì trả danh sách rỗng");
    CHECK(search(QString()).size() == tools().size(), "ô tìm rỗng trả tất cả");
    CHECK(foldVn(QStringLiteral("Đồng hồ đếm giờ")) == QStringLiteral("dong ho dem gio"), "foldVn bỏ dấu và chữ đ");
    clearForTest();
    CHECK(tools().isEmpty(), "xoá sổ để thử lại");

    // ===== Đồng hồ =====
    CHECK(formatDuration(3725000) == QStringLiteral("01:02:05"), "định dạng 3725 giây = 01:02:05");
    CHECK(formatDuration(3725500, true) == QStringLiteral("01:02:05.5"), "định dạng có phần mười giây");
    CHECK(formatDuration(-5) == QStringLiteral("00:00:00"), "thời lượng âm về 0");
    CHECK(parseDuration(QStringLiteral("90")) == 90LL * 60000, "\"90\" = 90 phút");
    CHECK(parseDuration(QStringLiteral("5:30")) == 330000, "\"5:30\" = 5 phút 30 giây");
    CHECK(parseDuration(QStringLiteral("1:02:03")) == 3723000, "\"1:02:03\" = 1 giờ 2 phút 3 giây");
    for (const QString& bad : {QStringLiteral(""), QStringLiteral("abc"), QStringLiteral("0"), QStringLiteral("1:75"), QStringLiteral("-5"), QStringLiteral("1:2:3:4"), QStringLiteral("5:"), QStringLiteral("100000")})
        CHECKQ(parseDuration(bad) == -1, (QStringLiteral("từ chối thời lượng sai: \"%1\"").arg(bad)));

    // ===== Đường dẫn tương đối an toàn =====
    CHECK(isSafeRelativePath(QStringLiteral("chay.py")) && isSafeRelativePath(QStringLiteral("./bin/chay.exe")), "đường dẫn tương đối trong thư mục được chấp nhận");
    for (const QString& bad : {QStringLiteral("../x"), QStringLiteral("a/../../x"), QStringLiteral("/etc/passwd"), QStringLiteral("C:/x"), QStringLiteral("a\\b"), QStringLiteral("~/x"), QString()})
        CHECKQ(!isSafeRelativePath(bad), (QStringLiteral("từ chối đường dẫn: \"%1\"").arg(bad)));

    // ===== tool.json =====
    CHECK(parseManifest(manifest()).ok, "tool.json tối thiểu hợp lệ");
    {
        auto r = parseManifest(manifest(QStringLiteral("\"args\":[\"a.py\",\"{ten}\",\"--n={so}\",\"{tooldir}\"],\"timeoutSec\":30,\"workingDir\":\"sub\",\"fields\":["
                                                       "{\"id\":\"ten\",\"label\":\"Tên\",\"type\":\"text\",\"required\":true},"
                                                       "{\"id\":\"so\",\"label\":\"Số\",\"type\":\"number\",\"min\":1,\"max\":9},"
                                                       "{\"id\":\"c\",\"label\":\"Chọn\",\"type\":\"choice\",\"choices\":[\"a\",\"b\"],\"default\":\"a\"},"
                                                       "{\"id\":\"f\",\"label\":\"Tệp\",\"type\":\"file\"},{\"id\":\"d\",\"label\":\"Thư mục\",\"type\":\"folder\"},"
                                                       "{\"id\":\"m\",\"label\":\"Nhiều dòng\",\"type\":\"multiline\"}]")));
        CHECK(r.ok && r.spec.fields.size() == 6 && r.spec.args.size() == 4 && r.spec.timeoutSec == 30, "tool.json đủ 6 kiểu trường hợp lệ");
    }
    struct Bad { const char* what; QByteArray json; };
    const QList<Bad> bads = {
        {"không phải JSON", QByteArray("{ khong phai json")},
        {"gốc là danh sách", QByteArray("[1,2]")},
        {"thiếu id", QByteArray("{\"name\":\"x\",\"command\":\"python\"}")},
        {"thiếu command", QByteArray("{\"id\":\"ab\",\"name\":\"x\"}")},
        {"mã chứa ../", manifest(QString(), QStringLiteral("python"), QStringLiteral("../x"))},
        {"mã chữ hoa", manifest(QString(), QStringLiteral("python"), QStringLiteral("ABC"))},
        {"lệnh tuyệt đối", manifest(QString(), QStringLiteral("/usr/bin/python3"))},
        {"lệnh có ổ đĩa", manifest(QString(), QStringLiteral("C:\\\\Python\\\\python.exe"))},
        {"lệnh thoát thư mục", manifest(QString(), QStringLiteral("../evil.exe"))},
        {"lệnh thoát thư mục sâu", manifest(QString(), QStringLiteral("sub/../../evil"))},
        {"lệnh powershell", manifest(QString(), QStringLiteral("powershell"))},
        {"lệnh cmd.exe", manifest(QString(), QStringLiteral("CMD.EXE"))},
        {"lệnh bash", manifest(QString(), QStringLiteral("bash"))},
        {"lệnh .bat", manifest(QString(), QStringLiteral("./chay.bat"))},
        {"lệnh .ps1", manifest(QString(), QStringLiteral("./chay.ps1"))},
        {"lệnh .vbs", manifest(QString(), QStringLiteral("./chay.vbs"))},
        {"workingDir thoát ra ngoài", manifest(QStringLiteral("\"workingDir\":\"../..\""))},
        {"workingDir tuyệt đối", manifest(QStringLiteral("\"workingDir\":\"/etc\""))},
        {"timeoutSec = 0", manifest(QStringLiteral("\"timeoutSec\":0"))},
        {"timeoutSec quá lớn", manifest(QStringLiteral("\"timeoutSec\":99999"))},
        {"schema khác 1", manifest(QStringLiteral("\"schema\":2")).replace("\"schema\":1,", "")},
        {"đối số dùng trường không tồn tại", manifest(QStringLiteral("\"args\":[\"{khong_co}\"]"))},
        {"trường trùng mã", manifest(QStringLiteral("\"fields\":[{\"id\":\"a\",\"label\":\"A\"},{\"id\":\"a\",\"label\":\"B\"}]"))},
        {"trường tên tooldir", manifest(QStringLiteral("\"fields\":[{\"id\":\"tooldir\",\"label\":\"A\"}]"))},
        {"kiểu trường lạ", manifest(QStringLiteral("\"fields\":[{\"id\":\"a\",\"label\":\"A\",\"type\":\"script\"}]"))},
        {"choice không có danh sách", manifest(QStringLiteral("\"fields\":[{\"id\":\"a\",\"label\":\"A\",\"type\":\"choice\"}]"))},
        {"choice mặc định ngoài danh sách", manifest(QStringLiteral("\"fields\":[{\"id\":\"a\",\"label\":\"A\",\"type\":\"choice\",\"choices\":[\"x\"],\"default\":\"y\"}]"))},
        {"min lớn hơn max", manifest(QStringLiteral("\"fields\":[{\"id\":\"a\",\"label\":\"A\",\"type\":\"number\",\"min\":5,\"max\":1}]"))},
        {"args không phải danh sách", manifest(QStringLiteral("\"args\":\"a b\""))},
    };
    for (const Bad& b : bads) {
        const auto r = parseManifest(b.json);
        CHECKQ(!r.ok && !r.error.isEmpty(), (QStringLiteral("từ chối tool.json: %1 — %2").arg(QString::fromUtf8(b.what), r.error.left(70))));
    }
    CHECK(!parseManifest(QByteArray(300 * 1024, ' ')).ok, "từ chối tool.json quá lớn");

    // ===== Thư mục tool =====
    QTemporaryDir tmp;
    const QString root = tmp.path() + QStringLiteral("/tools");
    const QString good = root + QStringLiteral("/chao");
    const QByteArray goodJson =
        "{\"schema\":1,\"id\":\"chao\",\"name\":\"Chào\",\"description\":\"Thử\",\"command\":\"./run.bin\",\"args\":[\"--ten\",\"{ten}\",\"--so\",\"{so}\",\"{ghichu}\"],"
        "\"fields\":[{\"id\":\"ten\",\"label\":\"Tên\",\"required\":true},"
        "{\"id\":\"so\",\"label\":\"Số\",\"type\":\"number\",\"min\":1,\"max\":9,\"default\":\"2\"},"
        "{\"id\":\"mau\",\"label\":\"Màu\",\"type\":\"choice\",\"choices\":[\"đỏ\",\"xanh\"]},"
        "{\"id\":\"tep\",\"label\":\"Tệp\",\"type\":\"file\"},"
        "{\"id\":\"ghichu\",\"label\":\"Ghi chú\"}]}";
    writeFile(good + QStringLiteral("/tool.json"), goodJson);
    writeFile(good + QStringLiteral("/run.bin"), "#!/bin/sh\necho hi\n");
    QFile::setPermissions(good + QStringLiteral("/run.bin"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    writeFile(root + QStringLiteral("/hong/tool.json"), "{ khong dung");
    writeFile(root + QStringLiteral("/thieu_lenh/tool.json"), manifest(QString(), QStringLiteral("./khong_co.bin"), QStringLiteral("thieu-lenh")));
    writeFile(root + QStringLiteral("/trung/tool.json"), goodJson); // trùng mã "chao" với thư mục chao
    writeFile(root + QStringLiteral("/trung/run.bin"), "#!/bin/sh\necho trung\n");
    writeFile(root + QStringLiteral("/khong_co_json/ghi_chu.txt"), "x");
#ifndef Q_OS_WIN
    writeFile(tmp.path() + QStringLiteral("/ngoai/tool.json"), manifest(QString(), QStringLiteral("python"), QStringLiteral("ngoai")));
    QFile::link(tmp.path() + QStringLiteral("/ngoai"), root + QStringLiteral("/lk_thumuc")); // thư mục tool là liên kết ra ngoài
    writeFile(root + QStringLiteral("/lk_tep/tool.json"), manifest(QString(), QStringLiteral("python"), QStringLiteral("lk-tep")));
    writeFile(tmp.path() + QStringLiteral("/bimat.txt"), "bi mat");
    QFile::link(tmp.path() + QStringLiteral("/bimat.txt"), root + QStringLiteral("/lk_tep/doc_trom.txt")); // tệp là liên kết ra ngoài
#endif

    const auto lg = loadToolDir(root, QStringLiteral("chao"));
    CHECK(lg.ok && lg.spec.id == QStringLiteral("chao") && lg.spec.fingerprint.size() == 64, "nạp thư mục tool hợp lệ, có dấu vân tay SHA-256");
    CHECK(!loadToolDir(root, QStringLiteral("../ngoai")).ok, "từ chối tên thư mục có ../");
    CHECK(!loadToolDir(root, QStringLiteral("hong")).ok, "từ chối tool.json hỏng");
    CHECK(!loadToolDir(root, QStringLiteral("thieu_lenh")).ok, "từ chối khi tệp lệnh tương đối không có thật");
    CHECK(!loadToolDir(root, QStringLiteral("khong_co_json")).ok, "từ chối thư mục thiếu tool.json");
#ifndef Q_OS_WIN
    CHECK(!loadToolDir(root, QStringLiteral("lk_thumuc")).ok, "từ chối thư mục tool là liên kết tượng trưng trỏ ra ngoài");
    CHECK(!loadToolDir(root, QStringLiteral("lk_tep")).ok, "từ chối thư mục tool chứa liên kết tượng trưng (không đọc trộm tệp ngoài)");
#endif
    const auto sc = scan(root);
    CHECK(sc.tools.size() == 1 && sc.tools.first().id == QStringLiteral("chao"), "quét: chỉ nhận đúng 1 tool hợp lệ");
    CHECK(sc.errors.size() >= 4, "quét: các tool lỗi được liệt kê kèm lý do (không im lặng)");
    bool dupMsg = false;
    for (const auto& e : sc.errors) dupMsg = dupMsg || e.contains(QStringLiteral("trùng mã"));
    CHECK(dupMsg, "quét: báo trùng mã tool");

    // ===== Xác nhận lần đầu + dấu vân tay =====
    ToolSpec spec = lg.spec;
    CHECK(!isConfirmed(spec), "tool mới chưa được xác nhận");
    QMap<QString, QString> v{{QStringLiteral("ten"), QStringLiteral("Dũng")}};
    auto p0 = prepare(spec, v);
    CHECK(!p0.ok && p0.error.contains(QStringLiteral("xác nhận")), "chưa xác nhận thì KHÔNG chuẩn bị lệnh chạy");
    setConfirmed(spec);
    CHECK(isConfirmed(spec), "sau khi người dùng xác nhận thì được nhớ");
    auto p1 = prepare(spec, v);
    CHECK(p1.ok, "đã xác nhận: chuẩn bị lệnh thành công");
    CHECKQ(p1.ok && QFileInfo(p1.cmd.program).fileName() == QStringLiteral("run.bin"), (QStringLiteral("chương trình là tệp trong thư mục tool: %1").arg(p1.cmd.program)));
    CHECK(p1.ok && p1.cmd.args == QStringList({QStringLiteral("--ten"), QStringLiteral("Dũng"), QStringLiteral("--so"), QStringLiteral("2")}),
          "đối số: giá trị mặc định điền vào, trường tuỳ chọn để trống bị bỏ khỏi dòng lệnh");
    CHECK(p1.ok && QFileInfo(p1.cmd.workDir) == QFileInfo(lg.spec.dir), "thư mục chạy là thư mục tool");

    v[QStringLiteral("ghichu")] = QStringLiteral("x; rm -rf / && echo \"hack\" | cat");
    auto pinj = prepare(spec, v);
    CHECK(pinj.ok && pinj.cmd.args.size() == 5 && pinj.cmd.args.last() == v.value(QStringLiteral("ghichu")), "giá trị lạ nằm gọn trong MỘT đối số (không có shell để chèn lệnh)");
    v.remove(QStringLiteral("ghichu"));

    CHECK(!prepare(spec, {}).ok, "thiếu trường bắt buộc bị từ chối");
    { auto x = v; x[QStringLiteral("so")] = QStringLiteral("abc"); CHECK(!prepare(spec, x).ok, "số không hợp lệ bị từ chối"); }
    { auto x = v; x[QStringLiteral("so")] = QStringLiteral("10"); CHECK(!prepare(spec, x).ok, "số vượt max bị từ chối"); }
    { auto x = v; x[QStringLiteral("so")] = QStringLiteral("0"); CHECK(!prepare(spec, x).ok, "số dưới min bị từ chối"); }
    { auto x = v; x[QStringLiteral("so")] = QStringLiteral("1,5"); CHECK(!prepare(spec, x).ok, "dấu phẩy thập phân bị từ chối (tránh hiểu nhầm)"); }
    { auto x = v; x[QStringLiteral("mau")] = QStringLiteral("tím"); CHECK(!prepare(spec, x).ok, "giá trị ngoài danh sách choice bị từ chối"); }
    { auto x = v; x[QStringLiteral("mau")] = QStringLiteral("đỏ"); CHECK(prepare(spec, x).ok, "giá trị trong danh sách choice được nhận"); }
    { auto x = v; x[QStringLiteral("tep")] = tmp.path() + QStringLiteral("/khong_co.txt"); CHECK(!prepare(spec, x).ok, "tệp không tồn tại bị từ chối"); }
    { auto x = v; x[QStringLiteral("tep")] = good + QStringLiteral("/tool.json"); CHECK(prepare(spec, x).ok, "tệp có thật được nhận"); }
    { auto x = v; x[QStringLiteral("ten")] = QStringLiteral("a\nb"); CHECK(!prepare(spec, x).ok, "ô một dòng chứa xuống dòng bị từ chối"); }
    { auto x = v; x[QStringLiteral("ten")] = QString(5000, QLatin1Char('a')); CHECK(!prepare(spec, x).ok, "giá trị quá dài bị từ chối"); }

    // Sửa script SAU khi xác nhận => phải xác nhận lại
    writeFile(good + QStringLiteral("/run.bin"), "#!/bin/sh\necho hacked\n");
    auto p2 = prepare(spec, v);
    CHECK(!p2.ok && (p2.error.contains(QStringLiteral("thay đổi")) || p2.error.contains(QStringLiteral("xác nhận"))), "script bị sửa sau lúc quét: từ chối chạy");
    const auto lg2 = loadToolDir(root, QStringLiteral("chao"));
    CHECK(lg2.ok && lg2.spec.fingerprint != spec.fingerprint, "dấu vân tay đổi khi script đổi");
    CHECK(!isConfirmed(lg2.spec), "xác nhận cũ không còn hiệu lực với bản đã sửa");
    setConfirmed(lg2.spec);
    CHECK(prepare(lg2.spec, v).ok, "xác nhận lại bản mới thì chạy được");
    revokeConfirmation(QStringLiteral("chao"));
    CHECK(!isConfirmed(lg2.spec), "thu hồi xác nhận");

    // Chương trình bare chưa cài
    writeFile(root + QStringLiteral("/bare/tool.json"), manifest(QString(), QStringLiteral("chuong-trinh-khong-ton-tai-xyz"), QStringLiteral("bare")));
    const auto lb = loadToolDir(root, QStringLiteral("bare"));
    CHECK(lb.ok, "tên chương trình trần hợp lệ về cú pháp");
    setConfirmed(lb.spec);
    auto pb = prepare(lb.spec, {});
    CHECK(!pb.ok && pb.error.contains(QStringLiteral("chưa cài")), "chương trình chưa cài: báo rõ, không kèm theo Python/AutoHotkey");

    // ===== Tool mẫu trong repo =====
    {
        const QString samples = QStringLiteral(DUNG_SAMPLES_DIR);
        const auto ls = loadToolDir(samples, QStringLiteral("xin-chao"));
        CHECKQ(ls.ok, (QStringLiteral("tool mẫu xin-chao hợp lệ %1").arg(ls.error)));
        if (ls.ok) {
            setConfirmed(ls.spec);
            auto ps = prepare(ls.spec, {{QStringLiteral("ten"), QStringLiteral("Dũng")}, {QStringLiteral("kieu"), QStringLiteral("Thân mật")}});
            if (!ps.ok && ps.error.contains(QStringLiteral("chưa cài"))) {
                std::printf("[BỎ QUA]  chạy tool mẫu: máy thử chưa cài python (đúng thiết kế — không đóng gói Python)\n");
            } else {
                CHECKQ(ps.ok, (QStringLiteral("tool mẫu chuẩn bị được lệnh %1").arg(ps.error)));
                if (ps.ok) {
                    const auto rr = runAndWait(ps.cmd);
                    CHECK(rr.got && rr.code == 0 && rr.out.contains(QStringLiteral("Chào Dũng")), "tool mẫu Python chạy và in tiếng Việt đúng");
                }
            }
            revokeConfirmation(ls.spec.id);
        }
    }

    // ===== Chạy tiến trình =====
    const QString self = QCoreApplication::applicationFilePath();
    {
        Command c;
        c.program = self;
        c.args = {QStringLiteral("--child"), QStringLiteral("echo"), QStringLiteral("một"), QStringLiteral("hai ba")};
        c.workDir = tmp.path();
        c.timeoutSec = 20;
        const auto r = runAndWait(c);
        CHECK(r.got && r.code == 3 && !r.timedOut && !r.stopped && !r.crashed, "chạy: lấy đúng mã thoát 3");
        CHECK(r.out.contains(QStringLiteral("ARGS:một|hai ba")), "đối số có dấu và dấu cách đến nguyên vẹn");
        CHECK(r.out.contains(QStringLiteral("Tiếng Việt")), "đầu ra UTF-8 tiếng Việt giải mã đúng");
    }
    {
        Command c;
        c.program = self;
        c.args = {QStringLiteral("--child"), QStringLiteral("sleep")};
        c.workDir = tmp.path();
        c.timeoutSec = 1;
        const auto r = runAndWait(c);
        CHECK(r.got && r.timedOut, "hết giờ: tự dừng và báo timedOut");
        CHECK(r.ms < 12000, "hết giờ: dừng trong thời gian hợp lý");
    }
    {
        Command c;
        c.program = self;
        c.args = {QStringLiteral("--child"), QStringLiteral("sleep")};
        c.workDir = tmp.path();
        c.timeoutSec = 120;
        const auto r = runAndWait(c, 400);
        CHECK(r.got && r.stopped && !r.timedOut, "nút Dừng: dừng tiến trình theo yêu cầu");
        CHECK(r.ms < 12000, "nút Dừng: dừng nhanh");
    }
    {
        Command c;
        c.program = tmp.path() + QStringLiteral("/khong_co_chuong_trinh");
        c.workDir = tmp.path();
        c.timeoutSec = 5;
        const auto r = runAndWait(c);
        CHECK(r.got && r.crashed, "chương trình không khởi động được: báo lỗi, không treo");
    }
    {
        QFile lf(ToolRunner::logPath());
        const bool opened = lf.open(QIODevice::ReadOnly);
        const QString log = QString::fromUtf8(lf.readAll());
        CHECK(opened && log.contains(QStringLiteral("BẮT ĐẦU")) && log.contains(QStringLiteral("KẾT THÚC")), "mỗi lần chạy được ghi log (bắt đầu/kết thúc)");
        CHECK(log.contains(QStringLiteral("hết-giờ")) && log.contains(QStringLiteral("đã-dừng")), "log ghi rõ hết giờ và dừng tay");
    }

    std::printf("\n%s\n", g_fail ? "KẾT QUẢ: CÓ LỖI" : "KẾT QUẢ: TẤT CẢ ĐẠT");
    return g_fail ? 1 : 0;
}
