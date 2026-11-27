#include "dung/FeatureScan.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace dung {
namespace {

uint32_t rd32(const unsigned char* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
uint16_t rd16(const unsigned char* p) { return uint16_t(p[0] | (p[1] << 8)); }

std::string lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}
bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }
bool endsWith(const std::string& s, const char* e) {
    const size_t n = std::strlen(e);
    return s.size() >= n && s.compare(s.size() - n, n, e) == 0;
}
bool contains(const std::string& s, const char* p) { return s.find(p) != std::string::npos; }

} // namespace

bool listZipNames(const fs::path& zipFile, std::vector<std::string>& names, std::string* err) {
    names.clear();
    auto fail = [&](const char* m) { if (err) *err = m; return false; };
    std::ifstream f(zipFile, std::ios::binary);
    if (!f) return fail("không mở được tệp");
    f.seekg(0, std::ios::end);
    const long long size = static_cast<long long>(f.tellg());
    if (size < 22) return fail("tệp quá nhỏ, không phải zip");
    const long long tailLen = std::min<long long>(size, 22 + 65535);
    std::vector<unsigned char> tail(static_cast<size_t>(tailLen));
    f.seekg(size - tailLen);
    f.read(reinterpret_cast<char*>(tail.data()), tailLen);
    if (!f) return fail("không đọc được cuối tệp");
    long long eocd = -1;
    for (long long i = tailLen - 22; i >= 0; --i)
        if (rd32(&tail[static_cast<size_t>(i)]) == 0x06054b50u &&
            i+22+rd16(&tail[static_cast<size_t>(i)+20])==tailLen) { eocd = i; break; }
    if (eocd < 0) return fail("không thấy cấu trúc zip");
    const unsigned char* e = &tail[static_cast<size_t>(eocd)];
    const uint32_t count = rd16(e + 10), cdSize = rd32(e + 12), cdOff = rd32(e + 16);
    if (count == 0xFFFFu || cdSize == 0xFFFFFFFFu || cdOff == 0xFFFFFFFFu) return fail("zip64 chưa được quét");
    if (rd16(e+4)!=0 || rd16(e+6)!=0 || rd16(e+8)!=count) return fail("zip nhiều đĩa chưa hỗ trợ");
    if (count==0 || cdSize>64u*1024*1024 || static_cast<long long>(cdOff)+cdSize>size-tailLen+eocd ||
        uint64_t(count)*46>cdSize) return fail("danh mục zip không hợp lệ hoặc quá lớn");
    std::vector<unsigned char> cd(cdSize);
    f.seekg(cdOff);
    f.read(reinterpret_cast<char*>(cd.data()), cdSize);
    if (!f) return fail("không đọc được danh mục zip");
    size_t pos = 0;
    std::unordered_set<std::string> seen;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 46 > cd.size() || rd32(&cd[pos]) != 0x02014b50u) return fail("danh mục zip bị hỏng");
        const size_t nl = rd16(&cd[pos + 28]), xl = rd16(&cd[pos + 30]), cl = rd16(&cd[pos + 32]);
        if (pos + 46 + nl + xl + cl > cd.size()) return fail("danh mục zip bị hỏng");
        std::string name(reinterpret_cast<const char*>(&cd[pos+46]),nl);
        if(name.empty() || name.find('\0')!=std::string::npos || !seen.insert(lower(name)).second)
            return fail("tên mục ZIP trống, trùng hoặc không hợp lệ");
        names.push_back(std::move(name));
        pos += 46 + nl + xl + cl;
    }
    return true;
}

std::vector<FeatureLoss> classifyEntries(const std::vector<std::string>& rawNames) {
    int fonts = 0, macro = 0, formctl = 0, activex = 0, smartart = 0, slicers = 0, timelines = 0, model = 0, ink = 0, masters = 0, layouts = 0;
    int connections=0, signatures=0, external=0;
    int embedded=0, customXml=0, threadedComments=0, richData=0, vbaSignatures=0, customRibbon=0;
    for (const auto& raw : rawNames) {
        const std::string n = lower(raw);
        if ((startsWith(n, "word/fonts/") && endsWith(n, ".odttf")) ||
            (startsWith(n, "ppt/fonts/") && endsWith(n, ".fntdata")))
            ++fonts;
        else if (endsWith(n, "/vbaproject.bin"))
            ++macro;
        else if (endsWith(n,"/vbaprojectsignature.bin") ||
                 endsWith(n,"/vbaprojectsignatureagile.bin") ||
                 endsWith(n,"/vbaprojectsignaturev3.bin"))
            ++vbaSignatures;
        else if ((startsWith(n,"word/embeddings/") || startsWith(n,"xl/embeddings/") ||
                  startsWith(n,"ppt/embeddings/")) && !endsWith(n,"/") && !contains(n,"/_rels/"))
            ++embedded;
        else if (startsWith(n,"customxml/item") && endsWith(n,".xml") && !startsWith(n,"customxml/itemprops"))
            ++customXml;
        else if (startsWith(n,"xl/threadedcomments/threadedcomment") && endsWith(n,".xml"))
            ++threadedComments;
        else if ((startsWith(n,"xl/richdata/") && !endsWith(n,"/") && !contains(n,"/_rels/")) ||
                 n=="xl/metadata.xml")
            ++richData;
        else if (n=="customui/customui.xml" || n=="customui/customui14.xml")
            ++customRibbon;
        else if (contains(n, "/ctrlprops/ctrlprop") && endsWith(n, ".xml"))
            ++formctl;
        else if (contains(n, "/activex/") && !endsWith(n, "/"))
            ++activex;
        else if (contains(n, "/diagrams/data") && endsWith(n, ".xml"))
            ++smartart;
        else if (startsWith(n, "xl/slicers/slicer") || startsWith(n, "xl/slicercaches/slicercache"))
            ++slicers;
        else if (startsWith(n, "xl/timelines/timeline") || startsWith(n, "xl/timelinecaches/timelinecache"))
            ++timelines;
        else if (startsWith(n, "xl/model/") && !endsWith(n, "/"))
            ++model;
        else if (n=="xl/connections.xml" || (startsWith(n,"xl/querytables/querytable") && endsWith(n,".xml")))
            ++connections;
        else if (startsWith(n,"_xmlsignatures/") && endsWith(n,".xml"))
            ++signatures;
        else if (startsWith(n,"xl/externallinks/externallink") && endsWith(n,".xml"))
            ++external;
        else if (contains(n, "/ink/") && endsWith(n, ".xml"))
            ++ink;
        else if (startsWith(n, "ppt/slidemasters/slidemaster") && endsWith(n, ".xml"))
            ++masters;
        else if (startsWith(n, "ppt/slidelayouts/slidelayout") && endsWith(n, ".xml"))
            ++layouts;
    }
    std::vector<FeatureLoss> v;
    auto add = [&](const char* id, const char* title, const char* effect, int c, bool certain, bool blocking = true) {
        if (c > 0) v.push_back({id, title, effect, c, certain, blocking});
    };
    add("font-nhung", "Phông chữ nhúng trong tệp",
        "Bản lưu KHÔNG còn phông nhúng. Máy nào không cài phông này sẽ hiển thị bằng phông thay thế, "
        "có thể lệch chữ và ngắt trang.", fonts, true);
    // Kiểm chứng trên engine thật (tests/excel/verify_formats.py): lưu .xlsm -> .xlsx KHÔNG còn xl/vbaProject.bin
    // (macro mất), các nút biểu mẫu vẫn còn hiển thị nhưng không có macro; lưu .xlsm -> .xlsm chỉ ghi lại một
    // dự án VBA khác byte với bản gốc (không bảo đảm), nên .xlsm luôn mở chỉ đọc.
    add("macro", "Macro VBA",
        "Dũng Office KHÔNG chạy macro. Bản lưu mới sẽ MẤT macro (KHÔNG chứa macro, chỉ có dữ liệu và định dạng). "
        "Macro vẫn còn nguyên trong tệp gốc vì tệp gốc không bị thay đổi; hãy giữ tệp gốc nếu cần dùng macro.", macro, true);
    add("nut-bieu-mau", "Nút bấm / điều khiển biểu mẫu",
        "Các nút vẫn hiển thị trong bản lưu mới nhưng bấm vào sẽ không làm gì vì macro gắn với nút không được giữ.", formctl, true);
    add("activex", "Điều khiển ActiveX",
        "Điều khiển ActiveX có thể không được giữ hoặc không còn hoạt động.", activex, false);
    add("smartart", "SmartArt",
        "SmartArt sẽ trở thành hình vẽ thường, không còn chỉnh sửa như SmartArt.", smartart, true);
    add("slicer", "Slicer (bộ cắt lát) của bảng/Pivot",
        "Slicer không được giữ; bộ lọc tương ứng có thể không còn.", slicers, true);
    add("timeline", "Timeline (dòng thời gian) của Pivot",
        "Timeline không được giữ.", timelines, true);
    add("mo-hinh-du-lieu", "Mô hình dữ liệu / Power Query",
        "Mô hình dữ liệu và truy vấn Power Query không được giữ.", model, true);
    add("ket-noi-du-lieu", "Kết nối dữ liệu / QueryTable",
        "Chưa xác nhận khả năng giữ hoặc làm mới kết nối, kể cả Power Query. Có thể mất truy vấn hoặc thông tin kết nối khi lưu. Giữ tệp gốc, chỉ lưu bản sao.",connections,false);
    add("lien-ket-ngoai", "Liên kết sổ tính bên ngoài",
        "Tham chiếu và dữ liệu liên kết ngoài có thể thay đổi khi lưu; chưa xác nhận refresh. Giữ bản gốc.",external,false);
    add("chu-ky-ooxml", "Chữ ký số tài liệu",
        "Sửa nội dung có thể làm mất hiệu lực chữ ký. Không dùng bản xuất làm tài liệu đã ký; giữ nguyên bản gốc.",signatures,false);
    // These features are detected by part name only. Do not imply that an
    // actual engine roundtrip has demonstrated loss or that their contents are safe.
    add("doi-tuong-nhung","Đối tượng / tệp nhúng",
        "Đối tượng nhúng có thể bị thay đổi hoặc không mở được trong bản lưu mới. Chưa kiểm chứng bảo toàn nội dung nhúng; không thay thế bản gốc.",embedded,false);
    add("xml-tuy-bien","Dữ liệu XML tùy biến",
        "Có thể chứa dữ liệu biểu mẫu hoặc liên kết nội dung. Chưa kiểm chứng khả năng giữ nguyên khi lưu; giữ bản gốc.",customXml,false);
    add("binh-luan-luong","Bình luận theo luồng Excel",
        "Chưa kiểm chứng bảo toàn luồng trả lời, người tham gia và nội dung khi lưu. Kiểm tra lại bản sao.",threadedComments,false);
    add("du-lieu-nang-cao","Metadata / kiểu dữ liệu nâng cao Excel",
        "Có thể chứa kiểu dữ liệu phong phú hoặc metadata công thức. Chưa xác nhận bảo toàn đầy đủ khi lưu hay làm mới.",richData,false);
    add("chu-ky-vba","Chữ ký dự án VBA",
        "Không xác minh chữ ký VBA và không chạy macro. Bản lưu mới không được coi là dự án VBA đã ký; giữ nguyên tệp nguồn.",vbaSignatures,false);
    add("ribbon-tuy-bien","Ribbon Office tùy biến",
        "Chưa kiểm chứng bảo toàn menu và nút tùy biến; các nút có thể phụ thuộc macro không được chạy.",customRibbon,false);
    add("muc-ve-tay", "Nét vẽ tay (ink)",
        "Nét vẽ tay có thể bị mất hoặc đổi thành hình.", ink, false);
    if (masters >= 2)
        add("slide-master", "Nhiều slide master tuỳ biến",
            "Slide master có thể bị gộp hoặc dồn xuống từng bố cục; nên so lại giao diện slide sau khi lưu.", masters, false);
    // Một master nhiều bố cục: đã đo (docs/PPTX_LIMITS.md) LibreOffice ghi lại thành mỗi bố cục một master.
    // Đếm theo tên thành phần (rẻ, không đọc rels) nên chỉ là "có thể": bố cục không dùng có thể bị bỏ.
    else if (masters == 1 && layouts >= 2)
        add("master-tach", "Bố cục trang cái (slide master)",
            "Lưu PPTX: các bố cục trang cái có thể bị tách thành nhiều trang cái; hiển thị không đổi nhưng sửa trang cái trong PowerPoint sẽ phải sửa nhiều chỗ.",
            layouts, false, /*blocking=*/false);
    return v;
}

FeatureScanResult scanFeatureLoss(const fs::path& zipFile) {
    FeatureScanResult r;
    std::vector<std::string> names;
    std::string err;
    if (!listZipNames(zipFile, names, &err)) { r.note = err; return r; }
    r.scanned = true;
    r.items = classifyEntries(names);
    return r;
}

std::string describeFeatureLoss(const std::vector<FeatureLoss>& items) {
    std::string out;
    for (const auto& it : items) {
        out += "• " + it.title + " (" + std::to_string(it.count) + (it.certain ? "): " : ", có thể): ") + it.effect + "\n";
    }
    return out;
}

} // namespace dung
