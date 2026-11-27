// SPDX-License-Identifier: MPL-2.0
// Kiểm thử lõi công cụ tệp/thư mục/liên kết: đổi tên (xung đột, hoán đổi, hoàn tác), tệp trùng (không xoá),
// so sánh thư mục, dung lượng, liên kết; tên tiếng Việt, đường dẫn dài, tệp bị khoá/không đọc được.
#include "FileOps.h"
#include "LinkScan.h"
#include "ZipLite.h"
#include "../../app/src/SavePathPolicy.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QDateTime>
#include <QRandomGenerator>
#include <iostream>

using namespace ft;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; std::cout << "[ĐẠT]  " << QString(msg).toStdString() << "\n"; } \
    else { ++g_fail; std::cout << "[LỖI]  " << QString(msg).toStdString() << "  (" #cond ")\n"; } } while (0)
#define NOTE(msg) std::cout << "[GHI CHÚ]  " << QString(msg).toStdString() << "\n"

static bool put(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(data);
    return true;
}
static QByteArray get(const QString& path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<không đọc được>"); }
static QStringList names(const QString& dir)
{
    QStringList l = QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    return l;
}
static bool hasTmp(const QString& dir)
{
    QDirIterator it(dir, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) if (it.next().contains(QStringLiteral(".dung-tmp-"))) return true;
    return false;
}


// ---- trợ giúp tạo ZIP / LNK để thử ----
static void le16(QByteArray& b, quint16 v) { b.append(char(v & 0xFF)); b.append(char(v >> 8)); }
static void le32(QByteArray& b, quint32 v) { for (int i = 0; i < 4; ++i) b.append(char((v >> (8 * i)) & 0xFF)); }
static QByteArray rawDeflate(const QByteArray& d, int level)
{
    const QByteArray c = qCompress(d, level);        // 4 byte kích thước + zlib(2 byte đầu + deflate + 4 byte adler)
    return c.mid(6, c.size() - 10);
}
struct ZEnt { QString name; QByteArray data; int method = 8; int level = 9; };
static QByteArray makeZip(const QVector<ZEnt>& es, quint32 forceCrc = 0, bool useForceCrc = false)
{
    QByteArray out, cd;
    for (const auto& e : es) {
        const QByteArray comp = e.method == 8 ? rawDeflate(e.data, e.level) : e.data;
        const QByteArray nm = e.name.toUtf8();
        const quint32 crc = useForceCrc ? forceCrc : zipCrc32(e.data);
        const quint32 off = quint32(out.size());
        le32(out, 0x04034b50); le16(out, 20); le16(out, 0x0800); le16(out, e.method); le16(out, 0); le16(out, 0x21);
        le32(out, crc); le32(out, comp.size()); le32(out, e.data.size()); le16(out, nm.size()); le16(out, 0);
        out += nm; out += comp;
        le32(cd, 0x02014b50); le16(cd, 20); le16(cd, 20); le16(cd, 0x0800); le16(cd, e.method); le16(cd, 0); le16(cd, 0x21);
        le32(cd, crc); le32(cd, comp.size()); le32(cd, e.data.size()); le16(cd, nm.size()); le16(cd, 0); le16(cd, 0); le16(cd, 0); le16(cd, 0);
        le32(cd, 0); le32(cd, off); cd += nm;
    }
    const quint32 cdOff = quint32(out.size());
    out += cd;
    le32(out, 0x06054b50); le16(out, 0); le16(out, 0); le16(out, es.size()); le16(out, es.size()); le32(out, cd.size()); le32(out, cdOff); le16(out, 0);
    return out;
}
static QByteArray relsXml(const QVector<QStringList>& rels)  // {id, type-suffix, target, "ext"|""}
{
    QByteArray x = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    for (const auto& r : rels)
        x += QString("<Relationship Id=\"%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/%2\" Target=\"%3\"%4/>")
                 .arg(r[0], r[1], r[2].toHtmlEscaped(), QString(r[3] == "ext" ? " TargetMode=\"External\"" : "")).toUtf8();
    return x + "</Relationships>";
}
static QByteArray makeLnk(const QString& basePath)
{
    QByteArray h(76, '\0');
    h[0] = 0x4C;
    const unsigned char clsid[16] = {0x01,0x14,0x02,0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46};
    for (int i = 0; i < 16; ++i) h[4 + i] = char(clsid[i]);
    h[20] = char(0x02 | 0x80);   // HasLinkInfo | IsUnicode
    QByteArray vol; le32(vol, 17); le32(vol, 3); le32(vol, 0x1234); le32(vol, 0x10); vol.append('\0');
    const QByteArray base = basePath.toLocal8Bit() + '\0';
    QByteArray li;
    const quint32 hdr = 0x1C, volOff = hdr, baseOff = hdr + quint32(vol.size()), sufOff = baseOff + quint32(base.size());
    le32(li, hdr + vol.size() + base.size() + 1); le32(li, hdr); le32(li, 1); le32(li, volOff); le32(li, baseOff); le32(li, 0); le32(li, sufOff);
    li += vol; li += base; li.append('\0');
    return h + li;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir td;
    const QString R = td.path();
    const QString J = R + "/nhat-ky";

    // Save As/Export must check the resolved filename, not the pre-suffix path.
    {
        const QString stem=R+QStringLiteral("/Báo cáo");
        const QString existing=stem+QStringLiteral(".xlsx");
        CHECK(put(existing,"KEEP"),"create existing save target");
        CHECK(savepath::resolve(stem,QStringLiteral("xlsx"))==existing &&
              !savepath::needsConfirmation(stem) && savepath::needsConfirmation(existing),
              "extension completion must require confirmation of existing workbook");
        CHECK(savepath::resolve(existing,QStringLiteral("docx"))==existing,
              "explicit extension preserved");
        const QString pdf=stem+QStringLiteral(".pdf");
        CHECK(put(pdf,"PDF"),"create existing export target");
        CHECK(savepath::resolve(stem,QStringLiteral("pdf"),true)==pdf &&
              savepath::needsConfirmation(pdf),"PDF extension completion checks existing export");
        CHECK(savepath::resolve(stem+QStringLiteral(".PDF"),QStringLiteral("pdf"),true)==stem+QStringLiteral(".PDF"),
              "case-insensitive PDF suffix");
        CHECK(savepath::resolve(QString(),QStringLiteral("pdf"),true).isEmpty() &&
              !savepath::needsConfirmation(QString()),"cancel remains empty");
        CHECK(get(existing)=="KEEP" && get(pdf)=="PDF","path policy never modifies existing data");
    }

    // ================= ĐỔI TÊN =================
    CHECK(removeVietnameseDiacritics(QStringLiteral("Đặng Thị Hương — Hợp đồng số 5.docx")) == QStringLiteral("Dang Thi Huong — Hop dong so 5.docx"), "bỏ dấu tiếng Việt (đ→d, Đ→D)");
    {
        const QString d = R + "/rn1";
        for (const char* n : {"Báo cáo tháng 1.docx", "Báo cáo tháng 2.docx", "Phiếu thu.xlsx"}) put(d + "/" + QString::fromUtf8(n), "x");
        QStringList ps; for (const auto& n : names(d)) ps << d + "/" + n;
        RenameOptions o; o.pattern = "HS_{n}_{name}"; o.start = 7; o.step = 2; o.pad = 3; o.removeDiacritics = true; o.normalizeSpaces = true;
        auto plan = planRename(ps, o);
        CHECK(!plan.hasProblems() && plan.changeCount() == 3, "kế hoạch đổi tên 3 tệp không có vấn đề");
        CHECK(plan.items[0].newName == "HS_007_Bao cao thang 1.docx" && plan.items[1].newName == "HS_009_Bao cao thang 2.docx" && plan.items[2].newName == "HS_011_Phieu thu.xlsx", "mẫu {n} bắt đầu/bước/độ rộng + bỏ dấu + giữ đuôi");
        CHECK(names(d).contains(QString::fromUtf8("Báo cáo tháng 1.docx")), "xem trước không đụng tới đĩa");
        auto r = applyRename(plan, J);
        CHECK(r.ok && r.done == 3 && QFileInfo::exists(d + "/HS_007_Bao cao thang 1.docx") && !hasTmp(d), "áp dụng đổi tên, không để tệp tạm");
        CHECK(QFileInfo::exists(r.journalPath), "đã ghi nhật ký hoàn tác");
        auto u = undoJournal(r.journalPath);
        CHECK(u.ok && u.done == 3 && QFileInfo::exists(d + QString::fromUtf8("/Báo cáo tháng 1.docx")) && QFileInfo::exists(d + QString::fromUtf8("/Phiếu thu.xlsx")), "hoàn tác trả đúng tên cũ (tên tiếng Việt)");
        CHECK(get(d + QString::fromUtf8("/Phiếu thu.xlsx")) == "x", "nội dung tệp không đổi");
    }
    {   // xung đột
        const QString d = R + "/rn2";
        put(d + "/a.txt", "A"); put(d + "/b.txt", "B"); put(d + "/c.txt", "C");
        RenameOptions o; o.pattern = "same";
        auto plan = planRename({d + "/a.txt", d + "/b.txt"}, o);
        CHECK(plan.hasProblems() && plan.items[0].status == RenameStatus::Conflict && plan.items[1].status == RenameStatus::Conflict, "hai tệp cùng tên mới → xung đột");
        auto r = applyRename(plan, J);
        CHECK(!r.ok && QFileInfo::exists(d + "/a.txt") && QFileInfo::exists(d + "/b.txt"), "kế hoạch có xung đột → không đổi gì");
        RenameOptions o2; o2.find = "a"; o2.replace = "c";
        auto p2 = planRename({d + "/a.txt"}, o2);
        CHECK(p2.items[0].status == RenameStatus::Conflict && p2.items[0].note.contains("đã có"), "tên mới trùng tệp đang có không nằm trong danh sách → xung đột");
        RenameOptions o3; o3.pattern = "x:y";
        CHECK(planRename({d + "/a.txt"}, o3).items[0].status == RenameStatus::Invalid, "ký tự không hợp lệ bị chặn");
        o3.pattern = "con"; CHECK(planRename({d + "/a.txt"}, o3).items[0].status == RenameStatus::Invalid, "tên dành riêng của Windows (con) bị chặn");
        o3.pattern = "ten."; o3.keepExtension = false; CHECK(planRename({d + "/a.txt"}, o3).items[0].status == RenameStatus::Invalid, "tên kết thúc bằng dấu chấm bị chặn");
        o3.pattern = QString(300, 'x'); CHECK(planRename({d + "/a.txt"}, o3).items[0].status == RenameStatus::Invalid, "tên quá dài bị chặn");
        o3.pattern = ""; o3.keepExtension = true; CHECK(planRename({d + "/a.txt"}, o3).items[0].status == RenameStatus::Unchanged, "mẫu rỗng = giữ tên (không đổi)");
    }
    {   // hoán đổi và chuỗi tên
        const QString d = R + "/rn3";
        put(d + "/1.txt", "một"); put(d + "/2.txt", "hai");
        RenamePlan plan;
        plan.items.push_back({d, "1.txt", "2.txt", QString(), RenameStatus::Ok});
        plan.items.push_back({d, "2.txt", "1.txt", QString(), RenameStatus::Ok});
        auto r = applyRename(plan, J);
        CHECK(r.ok && get(d + "/1.txt") == "hai" && get(d + "/2.txt") == "một" && !hasTmp(d), "hoán đổi 1.txt ↔ 2.txt an toàn (2 pha)");
        CHECK(undoJournal(r.journalPath).ok && get(d + "/1.txt") == "một", "hoàn tác hoán đổi");
        // lập kế hoạch thật: đánh số lại 1→2, 2→3 (tên mới trùng tên cũ của mục khác)
        RenameOptions o; o.pattern = "{n}"; o.start = 2; o.pad = 0;
        auto p2 = planRename({d + "/1.txt", d + "/2.txt"}, o);
        CHECK(!p2.hasProblems() && p2.changeCount() == 2, "đánh số 2,3 lên các tên 1,2 không bị coi là xung đột (chuỗi đổi tên)");
        auto r2 = applyRename(p2, J);
        CHECK(r2.ok && get(d + "/2.txt") == "một" && get(d + "/3.txt") == "hai", "chuỗi đổi tên áp đúng");
    }
    {   // thư mục + chỉ đổi hoa/thường
        const QString d = R + "/rn4";
        QDir().mkpath(d + "/Thư mục cũ"); put(d + "/Thư mục cũ/f.txt", "f"); put(d + "/abc.txt", "abc");
        RenameOptions o; o.pattern = "{name}_moi"; o.removeDiacritics = true;
        auto plan = planRename({d + QString::fromUtf8("/Thư mục cũ")}, o);
        auto r = applyRename(plan, J);
        CHECK(r.ok && QFileInfo(d + "/Thu muc cu_moi").isDir() && QFileInfo::exists(d + "/Thu muc cu_moi/f.txt"), "đổi tên thư mục (bỏ dấu), nội dung bên trong còn nguyên");
        RenameOptions oc; oc.caseMode = CaseMode::Upper; oc.keepExtension = true;
        auto pc = planRename({d + "/abc.txt"}, oc);
        CHECK(pc.items[0].newName == "ABC.txt" && !pc.hasProblems(), "đổi hoa/thường tên (giữ đuôi) không bị coi là xung đột với chính nó");
        auto rc = applyRename(pc, J);
        CHECK(rc.ok && names(d).contains("ABC.txt") && !names(d).contains("abc.txt") && get(d + "/ABC.txt") == "abc", "áp dụng đổi hoa/thường");
    }
    {   // tệp bị khoá (Windows) — kết quả phải nhất quán, không để lại tệp tạm
        const QString d = R + "/rn5";
        put(d + "/a.txt", "A"); put(d + "/b.txt", "B");
        QFile lock(d + "/b.txt");
        const bool opened = lock.open(QIODevice::ReadWrite);
        RenameOptions o; o.pattern = "m_{name}";
        auto plan = planRename({d + "/a.txt", d + "/b.txt"}, o);
        auto r = applyRename(plan, J);
        lock.close();
        const bool allOld = QFileInfo::exists(d + "/a.txt") && QFileInfo::exists(d + "/b.txt");
        const bool allNew = QFileInfo::exists(d + "/m_a.txt") && QFileInfo::exists(d + "/m_b.txt");
        CHECK(opened && ((!r.ok && allOld) || (r.ok && allNew)) && !hasTmp(d), "tệp đang mở: hoặc đổi hết hoặc khôi phục hết, không để tệp tạm");
#ifdef Q_OS_WIN
        CHECK(!r.ok && allOld && r.error.contains("khôi phục"), "Windows: tệp bị khoá → báo lỗi và khôi phục tên cũ");
#else
        NOTE("Linux cho phép đổi tên tệp đang mở nên không có ca 'bị khoá'; ca này được kiểm thật trên Windows.");
#endif
    }
    {   // hoàn tác không ghi đè khi chỗ cũ đã bị chiếm
        const QString d = R + "/rn6";
        put(d + "/a.txt", "A");
        RenameOptions o; o.pattern = "z"; auto r = applyRename(planRename({d + "/a.txt"}, o), J);
        put(d + "/a.txt", "MỚI-CỦA-NGƯỜI-DÙNG");
        auto u = undoJournal(r.journalPath);
        CHECK(!u.ok && !u.warnings.isEmpty() && get(d + "/a.txt") == QByteArray("MỚI-CỦA-NGƯỜI-DÙNG") && QFileInfo::exists(d + "/z.txt"), "hoàn tác bỏ qua mục bị chiếm chỗ, không ghi đè dữ liệu mới");
    }
    CHECK(!undoJournal(R + "/khong-co.json").ok, "nhật ký không tồn tại → báo lỗi, không sập");

    // ================= TỆP TRÙNG =================
    {
        const QString d = R + "/dup";
        const QByteArray big(300000, 'Q');
        put(d + "/A/bản sao.bin", big); put(d + "/B/bản sao.bin", big); put(d + "/C/khac.bin", QByteArray(300000, 'R'));
        put(d + "/A/nho.txt", "xin chào"); put(d + "/B/nho2.txt", "xin chào"); put(d + "/B/rong1.txt", ""); put(d + "/B/rong2.txt", "");
        put(d + "/A/duynhat.txt", "độc nhất");
        auto r = findDuplicates({d}, {});
        CHECK(r.groups.size() == 2 && !r.cancelled, "tìm đúng 2 nhóm trùng (cùng kích thước nhưng khác nội dung không bị gộp, tệp rỗng bị bỏ)");
        CHECK(r.groups[0].size == 300000 && r.groups[0].files.size() == 2 && r.wastedBytes == 300000 + QByteArray("xin chào").size(), "dung lượng giải phóng được tính đúng");
        auto rs = findDuplicates({d, d + "/A"}, {});
        CHECK(rs.groups.size() == 2 && rs.groups[0].files.size() == 2, "thư mục chồng nhau không đếm một tệp hai lần");
        QStringList all = r.groups[0].files;
        auto bad = moveDuplicates(r, all, d + "/_to_delete", J);
        CHECK(!bad.ok && bad.error.contains("giữ lại") && QFileInfo::exists(all[0]) && QFileInfo::exists(all[1]), "từ chối chuyển hết mọi bản của một nhóm");
        auto notInScan = moveDuplicates(r, {d + "/C/khac.bin"}, d + "/_to_delete", J);
        CHECK(!notInScan.ok && QFileInfo::exists(d + "/C/khac.bin"), "từ chối tệp không nằm trong kết quả quét");
        auto mv = moveDuplicates(r, {all[1]}, d + "/_to_delete", J);
        CHECK(mv.ok && mv.done == 1 && !QFileInfo::exists(all[1]) && QFileInfo::exists(all[0]), "chuyển một bản vào _to_delete, bản còn lại giữ nguyên");
        QStringList inTd = names(d + "/_to_delete");
        CHECK(inTd.size() == 1 && get(d + "/_to_delete/" + inTd[0]) == big, "tệp trong _to_delete còn nguyên nội dung (không xoá)");
        auto again = findDuplicates({d}, {});
        bool hasTdFile = false; for (const auto& g : again.groups) for (const auto& f : g.files) if (f.contains("_to_delete")) hasTdFile = true;
        CHECK(!hasTdFile, "quét lại không tính tệp trong _to_delete");
        auto u = undoJournal(mv.journalPath);
        CHECK(u.ok && QFileInfo::exists(all[1]) && names(d + "/_to_delete").isEmpty(), "hoàn tác đưa tệp về chỗ cũ");
        // sửa sau khi quét
        put(all[1], "đã sửa sau khi quét");
        auto stale = moveDuplicates(r, {all[1]}, d + "/_to_delete", J);
        CHECK(!stale.ok && QFileInfo::exists(all[1]), "tệp đổi sau khi quét → từ chối, yêu cầu quét lại");
        put(all[1], big);
        // trùng tên đích: hai tệp cùng tên khác thư mục
        put(d + "/X/same.dat", "S"); put(d + "/Y/same.dat", "S"); put(d + "/Z/same.dat", "S");
        auto r3 = findDuplicates({d + "/X", d + "/Y", d + "/Z"}, {});
        QStringList g3 = r3.groups[0].files;
        auto mv3 = moveDuplicates(r3, {g3[1], g3[2]}, d + "/td2", J);
        CHECK(mv3.ok && names(d + "/td2").size() == 2 && names(d + "/td2").contains("same (2).dat"), "tên trùng ở đích được đánh số, không ghi đè");
        // tệp không đọc được
        put(d + "/L/khoa.bin", QByteArray(4096, 'K')); put(d + "/M/khoa.bin", QByteArray(4096, 'K'));
        QFile::setPermissions(d + "/L/khoa.bin", QFileDevice::Permissions());
        QFile probe(d + "/L/khoa.bin");
        const bool unreadable = !probe.open(QIODevice::ReadOnly);
        if (probe.isOpen()) probe.close();
        auto r4 = findDuplicates({d + "/L", d + "/M"}, {});
        if (unreadable) CHECK(r4.groups.isEmpty() && r4.skipped.size() == 1 && r4.skipped[0].contains("khoa.bin"), "tệp không đọc được → vào danh sách bỏ qua kèm lý do, không sập");
        else NOTE("Tiến trình có quyền đọc mọi tệp (chạy bằng quản trị): bỏ qua ca tệp không đọc được.");
        QFile::setPermissions(d + "/L/khoa.bin", QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        bool cancelCalled = false;
        auto rc = findDuplicates({d}, {}, [&](qint64, qint64) { cancelCalled = true; return false; });
        CHECK(cancelCalled && rc.cancelled && rc.groups.isEmpty(), "huỷ giữa chừng: trả kết quả rỗng, đánh dấu cancelled");
    }

    // ================= ĐƯỜNG DẪN DÀI + TÊN VIỆT =================
    {
        QString d = R + "/dai";
        int depth = 0;
        while (d.size() < 300 && depth < 40) { d += QString::fromUtf8("/Thư mục rất dài số %1 để thử đường dẫn").arg(depth++); }
        const bool made = QDir().mkpath(d);
        CHECK(made, QString("tạo được đường dẫn dài %1 ký tự").arg(d.size()));
        if (made) {
            const QString f1 = d + QString::fromUtf8("/Tệp tiếng Việt số một.txt"), f2 = d + QString::fromUtf8("/Bản sao tệp.txt");
            CHECK(put(f1, "nội dung dài") && put(f2, "nội dung dài"), "ghi tệp trong đường dẫn dài");
            auto r = findDuplicates({R + "/dai"}, {});
            CHECK(r.groups.size() == 1 && r.groups[0].files.size() == 2, "tìm tệp trùng trong đường dẫn dài");
            RenameOptions o; o.pattern = "{n}_{name}"; o.removeDiacritics = true;
            auto plan = planRename({f1}, o);
            auto rr = applyRename(plan, J);
            CHECK(rr.ok && QFileInfo::exists(d + "/001_Tep tieng Viet so mot.txt"), "đổi tên trong đường dẫn dài");
            CHECK(undoJournal(rr.journalPath).ok && QFileInfo::exists(f1), "hoàn tác trong đường dẫn dài");
            auto cmp = compareFolders(R + "/dai", R + "/dai", {});
            CHECK(!cmp.entries.isEmpty() && cmp.entries[0].state == DiffState::Same, "so sánh/duyệt thư mục đường dẫn dài");
            auto du = diskUsage(R + "/dai", 5);
            CHECK(du.totalFiles == 2 && du.total == qint64(QByteArray("nội dung dài").size()) * 2, "đếm dung lượng trong đường dẫn dài");
        }
    }

    // ================= SO SÁNH THƯ MỤC =================
    {
        const QString a = R + "/ca", b = R + "/cb";
        put(a + "/giong.txt", "g"); put(b + "/giong.txt", "g");
        put(a + "/chiA.txt", "a"); put(b + "/chiB.txt", "b");
        put(a + QString::fromUtf8("/Báo cáo/khac.txt"), "ngắn"); put(b + QString::fromUtf8("/Báo cáo/khac.txt"), "dài hơn nhiều");
        put(a + "/cung-kich-thuoc.txt", "12345"); put(b + "/cung-kich-thuoc.txt", "54321");
        put(a + "/moi.txt", "m"); put(b + "/moi.txt", "m");
        {
            QFile fa(a + "/moi.txt"); fa.open(QIODevice::ReadWrite); fa.setFileTime(QDateTime::currentDateTime().addSecs(-3600), QFileDevice::FileModificationTime);
            QFile fb(b + "/moi.txt"); fb.open(QIODevice::ReadWrite); fb.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
            QFile fg1(a + "/giong.txt"); fg1.open(QIODevice::ReadWrite); fg1.setFileTime(QDateTime(QDate(2025, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime);
            QFile fg2(b + "/giong.txt"); fg2.open(QIODevice::ReadWrite); fg2.setFileTime(QDateTime(QDate(2025, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime);
            QFile k1(a + "/cung-kich-thuoc.txt"); k1.open(QIODevice::ReadWrite); k1.setFileTime(QDateTime(QDate(2025, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime);
            QFile k2(b + "/cung-kich-thuoc.txt"); k2.open(QIODevice::ReadWrite); k2.setFileTime(QDateTime(QDate(2025, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime);
        }
        auto st = [](const CompareResult& r, const QString& rel) { for (const auto& e : r.entries) if (e.rel == rel) return e.state; return DiffState(-1); };
        auto r = compareFolders(a, b, {});
        CHECK(st(r, "giong.txt") == DiffState::Same && st(r, "chiA.txt") == DiffState::OnlyA && st(r, "chiB.txt") == DiffState::OnlyB, "so sánh: giống / chỉ A / chỉ B");
        CHECK(st(r, QString::fromUtf8("Báo cáo/khac.txt")) == DiffState::Different || st(r, QString::fromUtf8("Báo cáo/khac.txt")) == DiffState::NewerA || st(r, QString::fromUtf8("Báo cáo/khac.txt")) == DiffState::NewerB, "so sánh: khác kích thước (tên tiếng Việt)");
        CHECK(st(r, "moi.txt") == DiffState::NewerB, "so sánh: B mới hơn theo giờ sửa");
        CHECK(st(r, "cung-kich-thuoc.txt") == DiffState::Same, "không so nội dung: cùng kích thước + giờ sửa → coi là giống (điểm mù đã biết)");
        CompareOptions oc; oc.compareContent = true;
        auto rc = compareFolders(a, b, oc);
        CHECK(st(rc, "cung-kich-thuoc.txt") == DiffState::Different, "so sánh nội dung bắt được khác biệt cùng kích thước");
        CHECK(st(rc, "giong.txt") == DiffState::Same, "so sánh nội dung: giống nhau vẫn giống");
        const QString csv = R + "/bao-cao.csv";
        put(a + "/=SUM(1+1).txt", "x");
        auto r2 = compareFolders(a, b, {});
        auto ex = exportCompareCsv(csv, r2, a, b);
        const QByteArray bytes = get(csv);
        CHECK(ex.ok && bytes.startsWith("\xEF\xBB\xBF") && bytes.contains("Chỉ có ở A"), "xuất báo cáo CSV UTF-8 có BOM");
        CHECK(!bytes.contains(",=SUM") && !bytes.contains("\n=SUM") && bytes.contains("'=SUM"), "CSV chống chèn công thức từ tên tệp bắt đầu bằng =");
        auto ex2 = exportCompareCsv(csv, r2, a, b);
        CHECK(!ex2.ok && get(csv) == bytes, "xuất báo cáo: từ chối ghi đè tệp có sẵn");
        bool called = false;
        auto rcn = compareFolders(a, b, {}, [&](qint64, qint64) { called = true; return false; });
        CHECK(called && rcn.cancelled && rcn.entries.isEmpty(), "huỷ so sánh giữa chừng");
    }

    // ================= DUNG LƯỢNG =================
    {
        const QString d = R + "/du";
        put(d + "/lon/a.bin", QByteArray(5000, 'a')); put(d + "/lon/sub/b.bin", QByteArray(3000, 'b'));
        put(d + "/nho/c.bin", QByteArray(100, 'c')); put(d + "/roi.bin", QByteArray(700, 'r'));
        auto r = diskUsage(d, 3);
        CHECK(r.total == 8800 && r.totalFiles == 4, "tổng dung lượng và số tệp đúng");
        CHECK(r.children.size() == 3 && r.children[0].name == "lon" && r.children[0].size == 8000 && r.children[0].files == 2 && r.children[1].name == "roi.bin" && r.children[2].name == "nho", "mục con sắp giảm dần (gồm tệp lẻ ở gốc)");
        CHECK(r.largestFiles.size() == 3 && r.largestFiles[0].name == "a.bin" && r.largestFiles[1].name == "b.bin" && r.largestFiles[2].name == "roi.bin", "N tệp lớn nhất toàn cây");
        CHECK(humanSize(512) == "512 B" && humanSize(1536) == "1.5 KB" && humanSize(5LL * 1024 * 1024 * 1024) == "5.0 GB", "định dạng dung lượng");
        CHECK(diskUsage(R + "/khong-co", 3).total == 0, "thư mục không tồn tại → kết quả rỗng, không sập");
    }

    // ================= LIÊN KẾT =================
    {
        const QString text = QStringLiteral("Xem https://Example.com/Bai-Viet?utm_source=fb&id=5&fbclid=XYZ. Hoặc (https://vi.wikipedia.org/wiki/Hà_Nội_(thành_phố)), "
                                            "và www.baomoi.com/tin?gclid=1; cuối cùng http://a.b/c#phan, hết.");
        const QStringList u = extractUrls(text);
        CHECK(u.size() == 4, QString("gom được 4 liên kết từ văn bản (được %1)").arg(u.size()));
        CHECK(u.value(0) == "https://Example.com/Bai-Viet?utm_source=fb&id=5&fbclid=XYZ", "bỏ dấu chấm cuối câu");
        CHECK(u.value(1).endsWith("(thành_phố)") && !u.value(1).endsWith("))"), "giữ ngoặc cân bằng trong URL, bỏ ngoặc đóng thừa");
        CHECK(u.value(2) == "https://www.baomoi.com/tin?gclid=1", "www. được bổ sung https://, bỏ dấu chấm phẩy");
        CHECK(u.value(3) == "http://a.b/c#phan", "bỏ dấu phẩy cuối, giữ #phan");
        LinkOptions lo;
        QStringList rem;
        CHECK(cleanUrl(u[0], lo, &rem) == "https://example.com/Bai-Viet?id=5" && rem == QStringList({"utm_source", "fbclid"}), "bỏ utm_/fbclid, giữ id, hạ chữ thường tên miền nhưng giữ đường dẫn");
        CHECK(cleanUrl("https://x.com/p?utm_medium=a", lo) == "https://x.com/p", "bỏ hết tham số → bỏ cả dấu ?");
        CHECK(cleanUrl("https://x.com/p?q=h%E1%BB%8Dc+t%E1%BA%ADp&utm_campaign=z", lo) == "https://x.com/p?q=h%E1%BB%8Dc+t%E1%BA%ADp", "giữ nguyên mã hoá phần tham số còn lại");
        CHECK(cleanUrl("https://x.com/p?ref=abc&source=nl", lo) == "https://x.com/p?ref=abc&source=nl", "không bỏ tham số có thể có chức năng (ref, source)");
        lo.dropFragment = true; CHECK(cleanUrl("http://a.b/c#phan", lo) == "http://a.b/c", "tuỳ chọn bỏ #fragment");
        LinkOptions off; off.stripTracking = false; CHECK(cleanUrl("https://x.com/p?utm_a=1", off) == "https://x.com/p?utm_a=1", "tắt bỏ tham số theo dõi");
        CHECK(cleanUrl("ftp://x.com/a?utm_a=1", LinkOptions()) == "ftp://x.com/a?utm_a=1" && cleanUrl("không phải link", LinkOptions()) == "không phải link", "không đụng tới thứ không phải http/https");
        auto cl = cleanLinks({"https://x.com/a?utm_source=1", "https://X.com/a", "  ", "https://x.com/b", "mailto:a@b.c", "https://x.com/a?utm_x=2"}, LinkOptions());
        CHECK(cl.urls == QStringList({"https://x.com/a", "https://x.com/b"}) && cl.duplicates == 2 && cl.cleaned >= 2 && cl.warnings.size() == 1, "làm sạch rồi bỏ trùng, giữ thứ tự, cảnh báo dòng lạ");
        const QString f = R + "/links.csv";
        auto e1 = exportLinks(f, {"https://x.com/a", "=cmd|' /C calc'!A0"});
        const QByteArray b = get(f);
        CHECK(e1.ok && b.startsWith("\xEF\xBB\xBF") && b.contains("'=cmd"), "xuất CSV UTF-8 BOM, chống chèn công thức");
        CHECK(!exportLinks(f, {"x"}).ok && get(f) == b, "xuất danh sách: từ chối ghi đè");
        CHECK(exportLinks(R + "/links.txt", {"https://x.com/a", "https://x.com/b"}).ok && get(R + "/links.txt") == "https://x.com/a\nhttps://x.com/b\n", "xuất .txt mỗi dòng một liên kết");
    }


    // ================= SẮP XẾP VÀO THƯ MỤC =================
    {
        const QString d = R + "/org";
        for (const char* n : {"Báo cáo.docx", "Bảng lương.xlsx", "ảnh 1.JPG", "a.xyz", "Bài thuyết trình.pptx", ".an.txt", "~$tam.docx"}) put(d + "/" + QString::fromUtf8(n), "x");
        put(d + "/thu-muc-con/Báo cáo.docx", "khác"); put(d + "/thu-muc-con/ghi chú.txt", "t");
        CHECK(fileCategory("DOCX") == QString::fromUtf8("Tài liệu") && fileCategory("jpeg") == QString::fromUtf8("Ảnh") && fileCategory("zzz") == "Khác", "phân loại theo đuôi (không phân biệt hoa/thường)");
        auto plan = planOrganize(d, false, OrganizeMode::ByCategory, QString());
        CHECK(plan.items.size() == 5 && plan.skipped.size() == 2, "chỉ tệp trực tiếp; tệp ẩn và ~$ bị bỏ qua có ghi lý do");
        bool ok = true;
        for (const auto& it : plan.items) ok = ok && !QFileInfo::exists(it.to);
        CHECK(ok && QFileInfo::exists(d + "/Báo cáo.docx"), "xem trước không đụng tới đĩa");
        auto r = applyOrganize(plan, J);
        CHECK(r.ok && r.done == 5 && QFileInfo::exists(d + QString::fromUtf8("/Tài liệu/Báo cáo.docx")) && QFileInfo::exists(d + QString::fromUtf8("/Ảnh/ảnh 1.JPG")) && QFileInfo::exists(d + "/Khác/a.xyz"), "chuyển vào thư mục theo loại, tên Unicode");
        CHECK(QFileInfo::exists(d + "/.an.txt") && QFileInfo::exists(d + "/thu-muc-con/Báo cáo.docx"), "không đụng tới tệp ẩn và thư mục con");
        // chạy lại: tệp đã đúng chỗ → không có gì để chuyển
        CHECK(planOrganize(d, true, OrganizeMode::ByCategory, QString()).items.size() == 2, "chạy lại với 'cả thư mục con': chỉ 2 tệp trong 'con' cần chuyển, tệp đã đúng chỗ giữ nguyên");
        // trùng tên ở đích → thêm số, không ghi đè
        auto plan2 = planOrganize(d, true, OrganizeMode::ByCategory, QString());
        bool renamed = false;
        for (const auto& it : plan2.items) if (it.to.endsWith(QString::fromUtf8("Báo cáo (2).docx"))) renamed = true;
        CHECK(renamed && plan2.renamedForConflict == 1, "trùng tên ở thư mục đích → thêm ' (2)'");
        auto r2 = applyOrganize(plan2, J);
        CHECK(r2.ok && get(d + QString::fromUtf8("/Tài liệu/Báo cáo.docx")) == "x" && get(d + QString::fromUtf8("/Tài liệu/Báo cáo (2).docx")) == QString::fromUtf8("khác").toUtf8(), "không ghi đè tệp gốc có sẵn ở đích");
        CHECK(undoJournal(r2.journalPath).ok && QFileInfo::exists(d + "/thu-muc-con/Báo cáo.docx") && !QFileInfo::exists(d + QString::fromUtf8("/Tài liệu/Báo cáo (2).docx")), "hoàn tác lần chuyển thứ hai");
        CHECK(undoJournal(r.journalPath).ok && QFileInfo::exists(d + "/Báo cáo.docx") && QFileInfo::exists(d + "/a.xyz") && QFileInfo::exists(d + QString::fromUtf8("/ảnh 1.JPG")), "hoàn tác lần chuyển đầu: tệp về chỗ cũ");
        // đích đổi sau khi xem trước → từ chối, không đổi gì
        auto plan3 = planOrganize(d, false, OrganizeMode::ByExtension, R + "/org-dich");
        put(plan3.items[0].to, "ai đó đặt vào");
        auto r3 = applyOrganize(plan3, J);
        CHECK(!r3.ok && QFileInfo::exists(plan3.items[0].from) && get(plan3.items[0].to) == QString::fromUtf8("ai đó đặt vào").toUtf8(), "đích bị chiếm sau khi xem trước → từ chối, không ghi đè");
        // tệp nguồn biến mất
        auto plan4 = planOrganize(d, false, OrganizeMode::ByYear, QString());
        QFile::remove(plan4.items[0].from);
        CHECK(!applyOrganize(plan4, J).ok, "tệp nguồn biến mất sau xem trước → từ chối");
        // theo năm/tháng
        auto plan5 = planOrganize(d, false, OrganizeMode::ByYearMonth, R + "/org-thang");
        const QString ym = QFileInfo(plan5.items[0].from).lastModified().toString("yyyy/MM");
        CHECK(plan5.items[0].to.contains("/org-thang/" + ym + "/"), "theo năm/tháng sửa");
        // thư mục đích khác ổ/thư mục
        CHECK(planOrganize(d, false, OrganizeMode::ByCategory, QString()).skipped.size() >= 2, "bỏ qua tệp ẩn khi lập lại kế hoạch");
    }

    // ================= ĐƯỜNG DẪN =================
    {
        const QString d = R + QString::fromUtf8("/Đường dẫn");
        const QString f = d + QString::fromUtf8("/Báo cáo quý 1.docx");
        put(f, "x");
        const PathForms pf = pathForms(f);
        CHECK(pf.name == QString::fromUtf8("Báo cáo quý 1.docx") && pf.quoted.startsWith('"') && pf.quoted.endsWith('"') && pf.quoted.contains(pf.name), "dạng có ngoặc kép, tên tệp");
        CHECK(pf.length == pf.full.size() && !pf.tooLong, "đếm ký tự theo UTF-16, đường dẫn ngắn không cảnh báo");
        CHECK(pf.fileUrl.startsWith("file:///") && pf.fileUrl.contains("B%C3%A1o%20c%C3%A1o"), "địa chỉ file:// mã hoá phần trăm");
        CHECK(!pf.slashes.contains('\\') && pf.extended.startsWith("\\\\?\\"), "dạng dùng / và dạng dài \\\\?\\");
#ifdef Q_OS_WIN
        CHECK(pathForms("\\\\may\\chia-se\\a.txt").extended.startsWith("\\\\?\\UNC\\"), "đường dẫn mạng → \\\\?\\UNC\\");
#endif
        // đường dẫn dài
        QString dl = R + "/dai";
        int depth = 0;
        while (dl.size() < 275 && depth < 40) dl += QString::fromUtf8("/Thư mục số %1 khá dài").arg(depth++);
        if (QDir().mkpath(dl)) {
            const QString lf = dl + "/t.txt";
            if (put(lf, "x")) {
                CHECK(pathForms(lf).tooLong, "đường dẫn ≥ 260 ký tự được cảnh báo");
                put(R + "/dai/ngan.txt", "x");
                auto lp = findLongPaths(R + "/dai", 260);
                CHECK(lp.paths.contains(lf) && !lp.paths.contains(R + "/dai/ngan.txt") && lp.scanned > 0, "dò thư mục: liệt kê đúng các đường dẫn quá dài");
                CHECK(findLongPaths(R + "/dai", 100000).paths.isEmpty(), "ngưỡng cao hơn → không có kết quả");
            } else NOTE("máy này không ghi được tệp ở đường dẫn dài — bỏ qua kiểm thử dò");
        } else NOTE("máy này không tạo được thư mục dài — bỏ qua kiểm thử dò");
    }

    // ================= ZIP / INFLATE =================
    {
        QByteArray rep; for (int i = 0; i < 4000; ++i) rep += QString("dòng %1: Tiếng Việt có dấu, lặp lại nhiều lần; ").arg(i % 50).toUtf8();
        QByteArray rnd(20000, 0); for (int i = 0; i < rnd.size(); ++i) rnd[i] = char(QRandomGenerator::global()->bounded(256));
        QByteArray shortD = "ngắn gọn";
        struct Case { QByteArray d; int level; const char* what; };
        for (const Case& c : {Case{rep, 9, "lặp lại (Huffman động)"}, Case{shortD, 9, "ngắn (Huffman cố định)"}, Case{rnd, 9, "ngẫu nhiên (không nén được)"}, Case{rep, 0, "khối lưu nguyên"}, Case{rep, 1, "mức nén 1"}}) {
            QByteArray out; QString e;
            const bool ok = inflateRaw(rawDeflate(c.d, c.level), c.d.size(), 1 << 28, &out, &e);
            CHECK(ok && out == c.d, QString("inflate đúng: %1").arg(c.what));
        }
        QByteArray out; QString e;
        const QByteArray good = rawDeflate(rep, 9);
        CHECK(!inflateRaw(good.left(good.size() / 2), rep.size(), 1 << 28, &out, &e) && out.isEmpty() && !e.isEmpty(), "dữ liệu nén bị cụt → lỗi, không trả dữ liệu dở");
        CHECK(!inflateRaw(good, rep.size() - 1, 1 << 28, &out, &e), "giải nén vượt kích thước khai báo → lỗi (chống bom nén)");
        CHECK(!inflateRaw(good, rep.size() + 5, 1 << 28, &out, &e), "kích thước khai báo lớn hơn thực tế → lỗi");
        CHECK(!inflateRaw(good, rep.size(), 1000, &out, &e), "vượt giới hạn tối đa → lỗi");
        QByteArray junk(500, 0); for (int i = 0; i < junk.size(); ++i) junk[i] = char(QRandomGenerator::global()->bounded(256));
        junk[0] = char(0xFF);
        CHECK(!inflateRaw(junk, 5000, 1 << 20, &out, &e), "dữ liệu rác → lỗi, không treo/không đổ");
        CHECK(zipCrc32("123456789") == 0xCBF43926u, "CRC32 chuẩn");

        const QString zp = R + "/thu.zip";
        QFile zf(zp); zf.open(QIODevice::WriteOnly);
        zf.write(makeZip({{QString::fromUtf8("a/Tên tệp.txt"), rep, 8, 9}, {"b.bin", rnd, 0, 0}, {"c.txt", shortD, 8, 9}})); zf.close();
        ZipReader z; QString ze;
        CHECK(z.open(zp, &ze) && z.entries().size() == 3, "mở ZIP, đọc bảng thư mục");
        QByteArray d1, d2, d3;
        CHECK(z.find(QString::fromUtf8("a/Tên tệp.txt")) && z.read(*z.find(QString::fromUtf8("a/Tên tệp.txt")), &d1, &ze) && d1 == rep, "đọc mục deflate, tên Unicode");
        CHECK(z.read(*z.find("b.bin"), &d2, &ze) && d2 == rnd, "đọc mục lưu nguyên");
        CHECK(z.read(*z.find("c.txt"), &d3, &ze) && d3 == shortD, "đọc mục ngắn");
        CHECK(!z.read(*z.find(QString::fromUtf8("a/Tên tệp.txt")), &d1, &ze, 100) && d1.isEmpty(), "mục lớn hơn giới hạn → từ chối");
        QFile zb(R + "/crc.zip"); zb.open(QIODevice::WriteOnly); zb.write(makeZip({{"x.txt", shortD, 8, 9}}, 0xDEADBEEFu, true)); zb.close();
        ZipReader zc; QByteArray dd;
        CHECK(zc.open(R + "/crc.zip", &ze) && !zc.read(zc.entries()[0], &dd, &ze) && ze.contains("CRC"), "CRC sai → báo lỗi, không đưa dữ liệu");
        put(R + "/khong-phai-zip.docx", "đây chỉ là văn bản, không phải zip");
        ZipReader zn;
        CHECK(!zn.open(R + "/khong-phai-zip.docx", &ze) && !ze.isEmpty() && !zn.open(R + "/khong-co-tep.zip", &ze), "không phải ZIP / không có tệp → lỗi rõ ràng");
        QByteArray trunc = makeZip({{"x.txt", rep, 8, 9}}); put(R + "/cut.zip", trunc.left(trunc.size() - 30));
        ZipReader zt;
        CHECK(!zt.open(R + "/cut.zip", &ze), "ZIP bị cắt cụt → lỗi");
        QByteArray bad = makeZip({{"x.txt", rep, 8, 9}});
        for (int i = 40; i < 90 && i < bad.size() - 100; ++i) bad[i] = char(bad[i] ^ 0x5A);
        put(R + "/hong.zip", bad);
        ZipReader zh; QByteArray dh;
        const bool opened = zh.open(R + "/hong.zip", &ze);
        CHECK(!opened || !zh.read(zh.entries()[0], &dh, &ze), "dữ liệu nén bị hỏng → lỗi (CRC/giải nén), không trả dữ liệu sai");
    }

    // ================= QUÉT LIÊN KẾT =================
    {
        const QString d = R + "/lk";
        { QDir().mkpath(d); QFile f(d + "/co-san.xlsx"); f.open(QIODevice::WriteOnly); f.write(makeZip({{"x.txt", "x", 0, 0}})); }
        // docx
        {
            QFile f(d + "/Hợp đồng.docx"); QDir().mkpath(d); f.open(QIODevice::WriteOnly);
            f.write(makeZip({{"[Content_Types].xml", "<Types/>", 8, 9},
                {"word/_rels/document.xml.rels", relsXml({{"rId1", "hyperlink", "https://example.com/a?id=1&x=2", "ext"}, {"rId2", "hyperlink", "tai-lieu-mat.xlsx", "ext"}, {"rId3", "hyperlink", "co-san.xlsx", "ext"},
                    {"rId4", "hyperlink", "mailto:a@b.c", "ext"}, {"rId5", "image", "media/image1.png", ""}, {"rId6", "hyperlink", "#_Toc123", ""}, {"rId7", "attachedTemplate", "file:///C:/Mau/mau-cu.dotx", "ext"}}), 8, 9}}));
        }
        // xlsx: liên kết sổ tính ngoài
        {
            QFile f(d + "/thu-muc-con/Tổng hợp.xlsx"); QDir().mkpath(d + "/thu-muc-con"); f.open(QIODevice::WriteOnly);
            f.write(makeZip({{"xl/externalLinks/_rels/externalLink1.xml.rels", relsXml({{"rId1", "externalLinkPath", "../co-san.xlsx", "ext"}, {"rId2", "externalLinkPath", "Gia.xlsx", "ext"}}), 0, 0},
                {"xl/worksheets/_rels/sheet1.xml.rels", relsXml({{"rId1", "hyperlink", "https://example.com/b", "ext"}}), 8, 9}}));
        }
        put(d + "/Trang web.url", "[InternetShortcut]\r\nURL=https://example.com/trang?utm_source=x\r\nIconIndex=0\r\n");
        put(d + "/Hỏng.url", "[InternetShortcut]\r\nIconIndex=0\r\n");
        put(d + "/dich-con.lnk", makeLnk(d + "/co-san.xlsx"));
        put(d + "/dich-mat.lnk", makeLnk(d + "/da-xoa.docx"));
        put(d + "/~$Hợp đồng.docx", "khóa tạm của Word");
        put(d + "/Hư.docx", "không phải zip");
        put(d + "/ghi-chu.txt", "bỏ qua");
        auto sr = scanLinks({d}, ScanLinksOptions(), {});
        auto find = [&](const QString& src, const QString& tgt) -> const LinkFinding* {
            for (const auto& f : sr.findings) if (f.source.endsWith(src) && f.target == tgt) return &f;
            return nullptr;
        };
        CHECK(sr.filesScanned == 8 && !sr.cancelled, QString("quét đúng 8 tệp (url×2, lnk×2, docx×2 gồm tệp hỏng, xlsx×2; bỏ ~$ và .txt) — thực tế %1").arg(sr.filesScanned));
        auto* w1 = find("Hợp đồng.docx", "https://example.com/a?id=1&x=2");
        CHECK(w1 && w1->type == TargetType::Web && w1->state == LinkState::Unchecked && w1->where == "word/document.xml" && w1->kind == QString::fromUtf8("Siêu liên kết"), "docx: liên kết web chỉ liệt kê, chưa kiểm tra mạng");
        auto* w2 = find("Hợp đồng.docx", "tai-lieu-mat.xlsx");
        CHECK(w2 && w2->state == LinkState::Broken && w2->type == TargetType::File, "docx: tệp liên kết (đường dẫn tương đối) đã mất → HỎNG");
        auto* w3 = find("Hợp đồng.docx", "co-san.xlsx");
        CHECK(w3 && w3->state == LinkState::Ok, "docx: tệp liên kết còn → Còn");
        auto* w4 = find("Hợp đồng.docx", "mailto:a@b.c");
        CHECK(w4 && w4->type == TargetType::Mail && w4->state == LinkState::Unchecked, "docx: mailto không kiểm tra");
        CHECK(!find("Hợp đồng.docx", "media/image1.png") && !find("Hợp đồng.docx", "#_Toc123"), "liên kết nội bộ (ảnh nhúng, mục lục) không bị liệt kê");
        auto* w5 = find("Hợp đồng.docx", "file:///C:/Mau/mau-cu.dotx");
        CHECK(w5 && w5->kind == QString::fromUtf8("Mẫu đính kèm") && w5->state != LinkState::Ok, "mẫu đính kèm ở ổ C: không được báo là còn");
        auto* x1 = find("Tổng hợp.xlsx", "../co-san.xlsx");
        CHECK(x1 && x1->state == LinkState::Ok && x1->where == "xl/externalLinks/externalLink1.xml" && x1->kind == QString::fromUtf8("Liên kết sổ tính ngoài"), "xlsx: sổ tính ngoài (tương đối từ thư mục của tệp) còn → Còn; mục lưu nguyên đọc được");
        auto* x2 = find("Tổng hợp.xlsx", "Gia.xlsx");
        CHECK(x2 && x2->state == LinkState::Broken, "xlsx: sổ tính ngoài đã mất → HỎNG (dạng sai công thức #REF khi mở)");
        CHECK(find("Tổng hợp.xlsx", "https://example.com/b"), "xlsx: siêu liên kết ô tính (deflate)");
        auto* u1 = find("Trang web.url", "https://example.com/trang?utm_source=x");
        CHECK(u1 && u1->type == TargetType::Web, ".url: đọc địa chỉ");
        auto* l1 = find("dich-con.lnk", d + "/co-san.xlsx");
        CHECK(l1 && l1->state == LinkState::Ok, ".lnk: đích còn → Còn");
        auto* l2 = find("dich-mat.lnk", d + "/da-xoa.docx");
        CHECK(l2 && l2->state == LinkState::Broken, ".lnk: đích đã xoá → HỎNG");
        const int expectBroken = 3 + ((w5 && w5->state == LinkState::Broken) ? 1 : 0);   // Windows: ổ C: kiểm tra được → mẫu cũ cũng hỏng
        CHECK(sr.brokenCount() == expectBroken, QString("số liên kết hỏng = %1 (docx 1, xlsx 1, lnk 1, + mẫu ổ C: chỉ trên Windows) — thực tế %2").arg(expectBroken).arg(sr.brokenCount()));
        bool hongDocx = false, hongUrl = false;
        for (const QString& s2 : sr.skipped) { if (s2.contains("Hư.docx")) hongDocx = true; if (s2.contains(QString::fromUtf8("Hỏng.url"))) hongUrl = true; }
        CHECK(hongDocx && hongUrl, "tệp hỏng được báo trong danh sách bỏ qua, các tệp khác vẫn quét tiếp");
        for (const auto& f : sr.findings) if (f.source.contains("~$")) { CHECK(false, "không quét tệp khoá tạm ~$"); break; }
        // xuất CSV
        const QString csv = R + "/lien-ket.csv";
        auto ex = exportFindingsCsv(csv, sr.findings);
        CHECK(ex.ok && get(csv).startsWith("\xEF\xBB\xBF") && ex.done == sr.findings.size(), "xuất báo cáo CSV có BOM");
        CHECK(!exportFindingsCsv(csv, sr.findings).ok, "xuất báo cáo: từ chối ghi đè");
        // huỷ giữa chừng
        auto cancelled = scanLinks({d}, ScanLinksOptions(), [](qint64, qint64) { return false; });
        CHECK(cancelled.cancelled && cancelled.filesScanned == 0, "huỷ quét ngay lập tức");
        // đường dẫn đơn lẻ, không tồn tại
        auto one = scanLinks({d + "/Hợp đồng.docx", d + "/khong-co.docx"}, ScanLinksOptions(), {});
        CHECK(one.filesScanned == 1 && one.skipped.size() == 1, "quét danh sách tệp riêng lẻ, báo tệp không tồn tại");
        CHECK(classifyTarget("HTTPS://A.B") == TargetType::Web && classifyTarget("\\\\may\\a") == TargetType::File && classifyTarget("javascript:x()") == TargetType::Other && classifyTarget("a/b.docx") == TargetType::File, "phân loại đích liên kết");
        bool chk = true;
        const QString r1 = resolveLocalTarget("file:///home/x/Tài%20liệu/a.docx", "/tmp", &chk);
#ifndef Q_OS_WIN
        CHECK(r1 == QString::fromUtf8("/home/x/Tài liệu/a.docx") && chk, "file:/// giải mã phần trăm, giữ Unicode");
        resolveLocalTarget("C:\\a\\b.docx", "/tmp", &chk);
        CHECK(!chk, "đường dẫn kiểu Windows trên hệ khác: không kiểm tra được (không báo sai là hỏng)");
#else
        (void)r1; (void)chk;
#endif
    }

    std::cout << "\nKẾT QUẢ: " << (g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT") << "  (đạt " << g_pass << ", lỗi " << g_fail << ")\n";
    return g_fail ? 1 : 0;
}
