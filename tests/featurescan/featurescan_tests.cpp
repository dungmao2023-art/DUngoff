// Kiểm thử quét tính năng sẽ mất. Chỉ dùng std: chạy được trên Linux/Windows, không cần Qt hay LibreOffice.
#include "dung/FeatureScan.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace dung;

static int g_fail = 0, g_n = 0;
#define CHECK(c) do { ++g_n; if (!(c)) { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static const FeatureLoss* find(const std::vector<FeatureLoss>& v, const std::string& id) {
    for (auto& i : v) if (i.id == id) return &i;
    return nullptr;
}

static void le16(std::string& s, unsigned v) { s += char(v & 255); s += char((v >> 8) & 255); }
static void le32(std::string& s, uint32_t v) { le16(s, v & 0xFFFF); le16(s, v >> 16); }

// Zip tối thiểu (stored, không nén) chỉ để thử đọc danh mục.
static std::string makeZip(const std::vector<std::string>& names) {
    std::string out, cd;
    for (auto& n : names) {
        const uint32_t off = uint32_t(out.size());
        le32(out, 0x04034b50); le16(out, 20); le16(out, 0); le16(out, 0); le16(out, 0); le16(out, 0);
        le32(out, 0); le32(out, 0); le32(out, 0); le16(out, unsigned(n.size())); le16(out, 0); out += n;
        le32(cd, 0x02014b50); le16(cd, 20); le16(cd, 20); le16(cd, 0); le16(cd, 0); le16(cd, 0); le16(cd, 0);
        le32(cd, 0); le32(cd, 0); le32(cd, 0); le16(cd, unsigned(n.size())); le16(cd, 0); le16(cd, 0);
        le16(cd, 0); le16(cd, 0); le32(cd, 0); le32(cd, off); cd += n;
    }
    const uint32_t cdOff = uint32_t(out.size());
    out += cd;
    le32(out, 0x06054b50); le16(out, 0); le16(out, 0); le16(out, unsigned(names.size())); le16(out, unsigned(names.size()));
    le32(out, uint32_t(cd.size())); le32(out, cdOff); le16(out, 0);
    return out;
}

static fs::path tmpFile(const std::string& name, const std::string& bytes) {
    fs::path p = fs::temp_directory_path() / ("dung-fs-" + name);
    std::ofstream(p, std::ios::binary).write(bytes.data(), std::streamsize(bytes.size()));
    return p;
}

int main(int argc, char** argv) {
    const fs::path fx = argc > 1 ? fs::path(argv[1]) : fs::path("tests/fixtures/features");

    // --- Phân loại theo tên ---
    {
        auto v = classifyEntries({"word/document.xml", "word/fonts/font1.odttf", "word/fonts/font2.odttf"});
        CHECK(v.size() == 1);
        auto* f = find(v, "font-nhung");
        CHECK(f && f->count == 2 && f->certain);
        CHECK(classifyEntries({"WORD/Fonts/A.ODTTF"}).size() == 1);  // không phân biệt hoa thường
        CHECK(find(classifyEntries({"ppt/fonts/font1.fntdata"}), "font-nhung") != nullptr);
        CHECK(classifyEntries({"word/fontTable.xml", "xl/worksheets/sheet1.xml"}).empty());  // fontTable không phải font nhúng
        CHECK(find(classifyEntries({"xl/vbaProject.bin"}), "macro") != nullptr);
        CHECK(find(classifyEntries({"word/vbaProject.bin"}), "macro") != nullptr);
        {
            auto b = classifyEntries({"xl/vbaProject.bin", "xl/ctrlProps/ctrlProp1.xml", "xl/ctrlProps/ctrlProp2.xml"});
            CHECK(find(b, "nut-bieu-mau") && find(b, "nut-bieu-mau")->count == 2);
            CHECK(find(classifyEntries({"xl/ctrlProps/ctrlProps2.xml"}), "nut-bieu-mau") != nullptr);  // tên LibreOffice ghi
            CHECK(find(classifyEntries({"xl/vbaProject.bin"}), "nut-bieu-mau") == nullptr);
            const std::string d = describeFeatureLoss(b);
            CHECK(d.find("KHÔNG chạy macro") != std::string::npos);
            CHECK(d.find("tệp gốc") != std::string::npos);
        }
        CHECK(find(classifyEntries({"ppt/diagrams/data1.xml", "ppt/diagrams/layout1.xml"}), "smartart")->count == 1);
        CHECK(find(classifyEntries({"xl/slicers/slicer1.xml", "xl/slicerCaches/slicerCache1.xml"}), "slicer")->count == 2);
        CHECK(find(classifyEntries({"xl/timelines/timeline1.xml"}), "timeline") != nullptr);
        CHECK(find(classifyEntries({"xl/model/item.data"}), "mo-hinh-du-lieu") != nullptr);
        CHECK(find(classifyEntries({"word/activeX/activeX1.xml"}), "activex")->certain == false);
        CHECK(classifyEntries({"ppt/slideMasters/slideMaster1.xml"}).empty());  // một master: không cảnh báo
        CHECK(classifyEntries({"ppt/slideMasters/slideMaster1.xml", "ppt/slideLayouts/slideLayout1.xml"}).empty());  // 1 bố cục: không báo
        {
            auto l = classifyEntries({"ppt/slideMasters/slideMaster1.xml", "ppt/slideLayouts/slideLayout1.xml",
                                      "ppt/slideLayouts/slideLayout2.xml", "ppt/slideLayouts/_rels/slideLayout1.xml.rels"});
            CHECK(l.size() == 1 && l[0].id == "master-tach" && l[0].count == 2 && !l[0].certain);
            CHECK(!l[0].blocking);  // chỉ báo ngắn, không bắt xác nhận
            CHECK(find(classifyEntries({"xl/vbaProject.bin"}), "macro")->blocking);
            CHECK(describeFeatureLoss(l).find("trang cái") != std::string::npos);
            // Nhiều master: dùng mục slide-master cũ, không thêm mục thứ hai
            auto k = classifyEntries({"ppt/slideMasters/slideMaster1.xml", "ppt/slideMasters/slideMaster2.xml",
                                      "ppt/slideLayouts/slideLayout1.xml", "ppt/slideLayouts/slideLayout2.xml"});
            CHECK(k.size() == 1 && k[0].id == "slide-master");
            // Không có pptx (docx/xlsx) thì không bao giờ báo
            CHECK(classifyEntries({"word/document.xml", "xl/worksheets/sheet1.xml"}).empty());
        }
        auto m = classifyEntries({"ppt/slideMasters/slideMaster1.xml", "ppt/slideMasters/slideMaster2.xml"});
        CHECK(m.size() == 1 && m[0].id == "slide-master" && !m[0].certain);
        CHECK(classifyEntries({}).empty());
    }

    // --- Mô tả tiếng Việt ---
    {
        auto v = classifyEntries({"word/fonts/font1.odttf", "xl/vbaProject.bin"});
        const std::string d = describeFeatureLoss(v);
        CHECK(d.find("Phông chữ nhúng") != std::string::npos);
        CHECK(d.find("Macro VBA") != std::string::npos);
        CHECK(describeFeatureLoss({}).empty());
    }

    // --- Zip tổng hợp ---
    {
        auto p = tmpFile("ok.zip", makeZip({"[Content_Types].xml", "word/fonts/font1.odttf", "xl/vbaProject.bin"}));
        auto r = scanFeatureLoss(p);
        CHECK(r.scanned && r.items.size() == 2 && find(r.items, "font-nhung") && find(r.items, "macro"));
        fs::remove(p);
        auto p2 = tmpFile("clean.zip", makeZip({"[Content_Types].xml", "word/document.xml"}));
        auto r2 = scanFeatureLoss(p2);
        CHECK(r2.scanned && r2.items.empty());
        fs::remove(p2);
    }

    // --- Đầu vào xấu: không kết luận, không sập ---
    {
        const std::string good = makeZip({"word/fonts/font1.odttf"});
        auto bad1 = tmpFile("notzip.bin", std::string(5000, 'x'));
        auto r = scanFeatureLoss(bad1);
        CHECK(!r.scanned && !r.note.empty() && r.items.empty());
        fs::remove(bad1);
        auto bad2 = tmpFile("tiny.bin", "PK");
        CHECK(!scanFeatureLoss(bad2).scanned);
        fs::remove(bad2);
        auto bad3 = tmpFile("empty.bin", "");
        CHECK(!scanFeatureLoss(bad3).scanned);
        fs::remove(bad3);
        // Cắt cụt: mất EOCD
        auto bad4 = tmpFile("trunc.zip", good.substr(0, good.size() - 10));
        CHECK(!scanFeatureLoss(bad4).scanned);
        fs::remove(bad4);
        // Danh mục trỏ ra ngoài tệp
        std::string c = good;
        c[c.size() - 6] = char(0x7F);  // cdOff byte cao
        auto bad5 = tmpFile("badoff.zip", c);
        CHECK(!scanFeatureLoss(bad5).scanned);
        fs::remove(bad5);
        CHECK(!scanFeatureLoss(fs::temp_directory_path() / "dung-fs-khong-ton-tai.zip").scanned);
    }

    // --- Tệp mẫu thật (tests/fixtures/features) ---
    {
        auto d = scanFeatureLoss(fx / "features.docx");
        CHECK(d.scanned);
        auto* f = find(d.items, "font-nhung");
        CHECK(f && f->count >= 1 && f->certain);
        auto x = scanFeatureLoss(fx / "features.xlsx");
        CHECK(x.scanned && x.items.empty());
        auto pv = scanFeatureLoss(fx / "features-pivot.xlsx");
        CHECK(pv.scanned && pv.items.empty());
        auto pp = scanFeatureLoss(fx / "features.pptx");
        // features.pptx: 1 master + 11 bố cục -> báo "có thể" (đúng với số đo trong docs/PPTX_LIMITS.md)
        CHECK(pp.scanned && pp.items.size() == 1 && pp.items[0].id == "master-tach" && pp.items[0].count == 11);
    }

    std::printf("%d/%d kiểm tra đạt\n", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
