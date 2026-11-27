// SPDX-License-Identifier: MPL-2.0
// Kiểm thử thư viện mẫu: cài gói mới phải THAY HẲN gói cũ cùng mã, không để lại mẫu cũ, không đụng tệp ngoài.
#include "templates/TemplateLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, what) do { const bool ok_ = (c); std::printf("[%s]  %s\n", ok_ ? "ĐẠT" : "LỖI", what); if (!ok_) ++g_fail; } while (0)

static void write(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(data);
}

// Tạo gói giả: manifest + danh mục CSV (có BOM, một trường đặt trong ngoặc kép có dấu phẩy) + các tệp mẫu
static QString makePack(const QString& base, const QString& folder, const QString& version, const QStringList& files) {
    const QString root = base + QLatin1Char('/') + folder;
    write(root + QStringLiteral("/manifest.json"),
          QByteArray("{\"id\":\"thu-vien-bieu-mau\",\"name\":\"Thu vien thu nghiem\",\"version\":\"") + version.toUtf8() + "\"}");
    QByteArray csv("\xEF\xBB\xBF" "Duong_dan,Tinh_trang,Nguon_goc,URL,Dinh_dang\r\n");
    for (const QString& f : files) {
        write(root + QLatin1Char('/') + f, "noi dung " + f.toUtf8());
        csv += f.toUtf8() + ",\"Mẫu, tham khảo\",Thử,,DOCX\r\n";
    }
    write(root + QStringLiteral("/00_HUONG_DAN_VA_DANH_MUC/DANH_MUC_TAP_TIN.csv"), csv);
    write(root + QStringLiteral("/00_HUONG_DAN_VA_DANH_MUC/NGUON_THAM_KHAO_CHINH_THUC.csv"),
          "Nhom,Van_ban_nguon,URL,Huong_dan\r\nVan thu,\"ND 30/2020\",https://example.org/,Tra cuu\r\n");
    return root;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("DungOfficeTest"));
    QStandardPaths::setTestModeEnabled(true);
    QDir(tpl::installedRoot()).removeRecursively(); // sạch trước khi thử
    QTemporaryDir tmp;
    const QString sub = QStringLiteral("01_Doanh_nghiep/");

    const QString v1 = makePack(tmp.path(), QStringLiteral("THU_VIEN_2025"), QStringLiteral("2025.1"),
                                {sub + QStringLiteral("001_Mẫu cũ A.docx"), sub + QStringLiteral("002_Mẫu bị thay thế.docx")});
    CHECK(tpl::installed().valid == false, "ban đầu chưa có gói");
    auto r1 = tpl::installFromDir(v1);
    CHECK(r1.ok && !r1.replaced && r1.info.version == QStringLiteral("2025.1"), "cài gói v2025.1");
    CHECK(tpl::entries(tpl::installed()).size() == 2, "danh mục đọc đúng 2 mẫu (CSV có BOM, ngoặc kép)");
    CHECK(tpl::entries(tpl::installed()).first().status == QStringLiteral("Mẫu, tham khảo"), "trường CSV có dấu phẩy đọc đúng");
    CHECK(tpl::sources(tpl::installed()).size() == 1, "đọc danh sách nguồn chính thức");

    const QString v2 = makePack(tmp.path(), QStringLiteral("THU_VIEN_2026"), QStringLiteral("2026.1"),
                                {sub + QStringLiteral("001_Mẫu cũ A.docx"), sub + QStringLiteral("003_Mẫu mới.docx")});
    auto r2 = tpl::installFromDir(v2);
    CHECK(r2.ok && r2.replaced && r2.info.version == QStringLiteral("2026.1"), "cài gói v2026.1 thay gói cũ");
    const QString inst = tpl::installed().root;
    CHECK(!QFile::exists(inst + QLatin1Char('/') + sub + QStringLiteral("002_Mẫu bị thay thế.docx")), "mẫu bị thay thế đã biến mất");
    CHECK(QFile::exists(inst + QLatin1Char('/') + sub + QStringLiteral("003_Mẫu mới.docx")), "mẫu mới có mặt");
    CHECK(tpl::entries(tpl::installed()).size() == 2, "danh mục chỉ còn 2 mẫu của gói mới");
    QDir top(tpl::installedRoot());
    CHECK(top.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden).size() == 1, "không còn thư mục tạm/bản cũ");

    auto r3 = tpl::installFromDir(v1);
    CHECK(!r3.ok && r3.needConfirm, "cài gói cũ hơn bị chặn, hỏi xác nhận");
    CHECK(tpl::installed().version == QStringLiteral("2026.1"), "gói đang cài vẫn là v2026.1 sau khi từ chối bản cũ");
    auto r4 = tpl::installFromDir(v1, true);
    CHECK(r4.ok && tpl::installed().version == QStringLiteral("2025.1"), "đồng ý hạ cấp thì thay được");
    // bản làm việc: không ghi đè, mẫu gốc không đổi
    const QString src = tpl::installed().root + QLatin1Char('/') + sub + QStringLiteral("001_Mẫu cũ A.docx");
    const QString dst = tmp.path() + QStringLiteral("/ban-lam-viec.docx");
    QString err;
    CHECK(tpl::makeWorkingCopy(src, dst, &err) && QFile::exists(dst), "tạo bản làm việc từ mẫu");
    CHECK(!tpl::makeWorkingCopy(src, dst, &err), "không ghi đè bản làm việc đã có");
    // thư mục không phải gói
    QTemporaryDir empty;
    CHECK(!tpl::installFromDir(empty.path()).ok, "thư mục không phải gói bị từ chối");
    CHECK(tpl::installed().version == QStringLiteral("2025.1"), "gói hiện tại giữ nguyên khi cài thất bại");
    CHECK(tpl::compareVersion("2026.10", "2026.9") > 0, "so sánh phiên bản theo số, không theo chuỗi");
    const auto invalid=makePack(tmp.path(),QStringLiteral("invalid"),QStringLiteral("2027.1"),{sub+QStringLiteral("Mẫu.docx")});
    write(invalid+QStringLiteral("/manifest.json"),"{\"id\":\"../escape\",\"version\":\"2027.1\"}");
    CHECK(!tpl::readInfo(invalid).valid&&!tpl::installFromDir(invalid).ok,"mã gói thoát thư mục bị từ chối");
    CHECK(!tpl::installed(QStringLiteral("../escape")).valid,"không đọc gói ngoài thư mục mẫu");
    write(invalid+QStringLiteral("/manifest.json"),"{\"id\":\"thu-vien-bieu-mau\",\"version\":\"2027.1\"}");
    write(invalid+QStringLiteral("/00_HUONG_DAN_VA_DANH_MUC/DANH_MUC_TAP_TIN.csv"),"Duong_dan,Tinh_trang,Nguon_goc,URL,Dinh_dang\n../ban-lam-viec.docx,,,,DOCX\n");
    CHECK(!tpl::installFromDir(invalid).ok,"đường dẫn danh mục thoát thư mục bị từ chối");
    CHECK(tpl::installed().version=="2025.1"&&QFile::exists(dst),"gói đang cài và tài liệu làm việc giữ nguyên");
    CHECK(!tpl::installFromDir(tpl::installed().root).ok,"không tự cài đè từ gói đang cài");
    // Gói thật (tệp .zip do CI tạo từ thư mục templates/): giải nén bằng công cụ của hệ điều hành rồi cài.
    const QString zip = qEnvironmentVariable("DUNG_TEST_ZIP");
    if (!zip.isEmpty()) {
        auto rz = tpl::installFromZip(zip, true);
        std::printf("       %s\n", rz.message.toUtf8().constData());
        CHECK(rz.ok, "cài gói mẫu thật từ tệp .zip");
        CHECK(tpl::compareVersion(tpl::installed().version, QStringLiteral("2026.2")) >= 0, "gói thật phiên bản 2026.2 trở lên");
        CHECK(tpl::sources(tpl::installed()).size() >= 35, "kho nguồn luật mở rộng");
        int excelCount = 0;
        for (const auto& e : tpl::entries(tpl::installed())) if (e.format == QStringLiteral("XLSX")) ++excelCount;
        CHECK(excelCount >= 12, "có ít nhất 12 mẫu Excel");
        CHECK(tpl::entries(tpl::installed()).size() >= 150, "gói thật liệt kê từ 150 mẫu trở lên (đủ tệp, tên tiếng Việt)");
    }
    std::printf("\nĐã chạy xong, %d lỗi\n", g_fail);
    return g_fail ? 1 : 0;
}
