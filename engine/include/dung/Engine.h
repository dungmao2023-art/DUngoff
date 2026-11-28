// Dũng Office — lớp engine: bọc LibreOfficeKit (LOK) để dùng các module xử lý tài liệu của
// LibreOffice (Calc/Writer/Impress/Draw core, filter OOXML/ODF/PDF) mà KHÔNG dùng giao diện
// (frame, menu, toolbar, cửa sổ) của LibreOffice. Giao diện hoàn toàn do Dũng Office vẽ.
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "dung/FeatureScan.h"
#include "dung/FormatPolicy.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lok { class Office; class Document; }

namespace dung {

namespace fs = std::filesystem;

// Đường dẫn <-> UTF-8 (hỗ trợ tên tệp tiếng Việt trên Windows).
std::string toUtf8(const fs::path& p);
fs::path fromUtf8(const std::string& s);
// Đường dẫn hệ thống -> URL file:/// đã mã hoá phần trăm (định dạng LibreOffice yêu cầu).
std::string toFileUrl(const fs::path& p);
// Đường dẫn dài trên Windows: thêm tiền tố \\?\ khi đường dẫn tuyệt đối từ 240 ký tự trở lên (hệ khác: giữ nguyên).
fs::path longPath(const fs::path& p);
// Publish complete staged file without replacing a competing destination.
// On POSIX the caller removes the staged sibling after success.
bool publishNewFile(const fs::path& sibling, const fs::path& dest, std::error_code& ec);

// Sự kiện từ engine (đến từ luồng của LibreOffice, KHÔNG phải luồng giao diện).
struct EngineEvent {
    int type = 0;          // giá trị LibreOfficeKitCallbackType (LOK_CALLBACK_*)
    std::string payload;   // nội dung (thường là chuỗi hoặc JSON)
};
using EventSink = std::function<void(const EngineEvent&)>;

struct SaveOptions {
    // Ghi đè chính tệp gốc. Mặc định KHÔNG. Nếu bật, một bản sao lưu của tệp gốc được tạo
    // trước (tên.dung-backup-YYYYMMDD-HHMMSS.ext) rồi mới ghi.
    bool allowOverwriteOriginal = false;
    // Cho phép thay thế một tệp KHÁC đã tồn tại ở đích (người dùng đã xác nhận). Tệp cũ
    // cũng được sao lưu trước khi thay.
    bool allowReplaceExisting = false;
    // Cho phép ghi ra định dạng có mất mát (xuất PDF...). Mặc định KHÔNG.
    bool allowLossyExport = false;
    // Người dùng đã xem danh sách tính năng sẽ KHÔNG được giữ (font nhúng, macro, SmartArt...) và đồng ý.
    // Mặc định KHÔNG: nếu tệp gốc có tính năng sẽ mất, saveAs từ chối và nêu chính xác từng mục.
    bool allowFeatureLoss = false;
};

struct SaveResult {
    bool ok = false;
    fs::path written;      // tệp đã ghi
    fs::path backup;       // bản sao lưu đã tạo (nếu có)
    std::string message;   // thông báo tiếng Việt cho người dùng
};

class Document;

class Engine {
public:
    // programDir: thư mục "program" của runtime LibreOffice (chứa sofficeapp.dll / libmergedlo.so).
    // profileDir: hồ sơ người dùng riêng của Dũng Office (không đụng hồ sơ LibreOffice của máy).
    // workDir: nơi chứa bản làm việc tạm của tài liệu đang mở.
    // Chỉ được tạo MỘT lần trong một tiến trình (giới hạn của LibreOffice).
    static std::unique_ptr<Engine> create(const fs::path& programDir, const fs::path& profileDir,
                                          const fs::path& workDir, std::string* error);

    // Thư mục chứa từ điển Hunspell của người dùng (thành phần cài riêng, vd. <app>\dictionaries\vi chứa vi-DauCu.dic/.aff).
    // Gọi TRƯỚC Engine::create. Mỗi cặp <tên>.dic + <tên>.aff trong thư mục được đăng ký cho locale vi-VN vào hồ sơ
    // engine (registrymodifications.xcu). Thư mục không có/không còn từ điển -> mục đăng ký cũ được gỡ. Không đụng tệp từ điển.
    static void setViDictionaryDir(const fs::path& dir);
    // Nội dung khối xcu sẽ ghi cho thư mục từ điển (rỗng nếu không có cặp .dic/.aff). Dùng cho kiểm thử.
    static std::string viDictionaryRegistryXml(const fs::path& dir);
    ~Engine();

    // Mở tài liệu. Engine KHÔNG bao giờ mở trực tiếp tệp gốc: tệp gốc được sao chép sang
    // bản làm việc tạm, và chỉ bản tạm được nạp. Nhờ đó tệp gốc không thể bị khoá/ghi đè.
    std::unique_ptr<Document> open(const fs::path& original, std::string* error);

    // Tạo tài liệu trống mới ("Sổ làm việc mới", "Tài liệu mới", ...).
    std::unique_ptr<Document> createNew(DocKind kind, std::string* error);

    std::string versionInfo() const;
    const fs::path& workDir() const { return m_workDir; }

private:
    Engine() = default;
    std::unique_ptr<lok::Office> m_office;
    fs::path m_workDir;
    int m_counter = 0;
    friend class Document;
};

class Document {
public:
    ~Document();

    DocKind kind() const { return m_kind; }
    bool readOnly() const { return m_readOnly; }
    bool isNew() const { return m_original.empty(); }
    const fs::path& originalPath() const { return m_original; }
    const fs::path& workingCopy() const { return m_working; }
    const std::vector<std::string>& warnings() const { return m_warnings; }
    // Tính năng của tệp gốc mà bản lưu bằng engine sẽ không giữ (quét lúc mở; rỗng nếu tệp mới/không quét được).
    const std::vector<FeatureLoss>& featureLoss() const { return m_featureLoss; }
    std::string displayName() const;

    // --- Trang / sheet / slide ---
    int partCount();
    int currentPart();
    void setPart(int part);
    std::string partName(int part);

    // Kích thước tài liệu (đơn vị twip, 1/1440 inch) của phần hiện tại.
    void documentSize(long& widthTwips, long& heightTwips);

    // Vẽ một vùng tài liệu vào bộ đệm BGRA 32-bit (premultiplied), pxW x pxH điểm ảnh.
    void paintTile(uint8_t* buffer, int pxW, int pxH, int twipX, int twipY, int twipW, int twipH);
    // Vẽ MỘT TRANG/SLIDE bất kỳ (không đổi trang hiện tại) vào bộ đệm BGRA premultiplied.
    // Dùng cho hình thu nhỏ và trình chiếu. mode: 0 = bình thường.
    // False: invalid input or optional LOK ABI unavailable; buffer untouched.
    bool paintPartTile(uint8_t* buffer, int part, int pxW, int pxH, int twipX, int twipY, int twipW, int twipH);
    // Chế độ hiển thị của trang chiếu: 0 = trang chiếu, 1 = trang ghi chú (chỉ Impress).
    void setPartMode(int mode);
    // Thông tin trang dạng JSON của engine (vd. {"visible":"1","mode":0,...}); rỗng nếu engine không hỗ trợ.
    std::string partInfo(int part);
    // Băm nội dung trang (đổi khi nội dung trang đổi); rỗng nếu engine không hỗ trợ.
    std::string partHash(int part);
    // Native Impress presentation and isolated slide layers, bounded bitmap memory.
    struct SlideLayer { std::string json; std::vector<uint8_t> pixels; };
    struct SlideLayers { int width=0,height=0; bool rgba=false; std::vector<SlideLayer> layers; };
    bool supportsSlideLayers();
    std::string presentationInfo();
    bool renderSlideLayers(int part,const std::string& hash,int width,int height,SlideLayers& result);
    // Báo engine vùng đang hiển thị và tỉ lệ zoom (giúp Calc tính đúng con trỏ, tiêu đề hàng/cột).
    void setClientView(int pxTileW, int pxTileH, int twipTileW, int twipTileH,
                       int visX, int visY, int visW, int visH);

    // --- Nhập liệu (toạ độ chuột theo twip) ---
    void postKey(int type, int charCode, int keyCode);
    void postMouse(int type, int twipX, int twipY, int count, int buttons, int modifier);
    // Chữ đang soạn của bộ gõ (IME) qua LOK_EXT_TEXTINPUT: type = 0 (đặt chữ đang soạn), 1 (POS), 2 (END, chốt).
    void postExtTextInput(int type, const std::string& utf8Text);
    // Ngôn ngữ giao diện của view hiện tại (vd. "vi").
    void setViewLanguage(const std::string& lang);
    void typeText(const std::u16string& text);   // gõ chuỗi ký tự (tiện cho kiểm thử/công thức)

    // --- Lệnh (UNO command, vd. ".uno:Bold") ---
    void command(const std::string& unoCommand, const std::string& jsonArgs = std::string(),
                 bool notifyWhenFinished = false);
    std::string commandValues(const std::string& query);
    // Dựng chuỗi đối số JSON cho lệnh UNO kiểu chuỗi: {"Name":{"type":"string","value":"..."}}
    static std::string stringArg(const std::string& name, const std::string& value);

    // --- Bộ nhớ tạm (clipboard) ---
    // Ở chế độ nhúng, engine dùng clipboard nội bộ; giao diện chuyển qua lại với clipboard hệ thống.
    std::string selectionAs(const std::string& mimeType);   // vd. "text/plain;charset=utf-8", "text/html"
    bool paste(const std::string& mimeType, const std::string& data);
    // Đợi engine xử lý xong phím/lệnh vừa gửi (phím và lệnh UNO được engine xếp hàng xử lý sau, còn dán thì chạy ngay).
    // Đợi cho tới khi đã yên lặng quietMs tính từ lần gửi phím/lệnh cuối VÀ từ sự kiện thật cuối cùng của engine
    // (không tính sự kiện giả "SYNTH"); tối đa maxMs. Không đợi nếu không có gì mới được gửi gần đây.
    void waitInputIdle(int quietMs = 200, int maxMs = 3000);
    // Nonblocking observation for UI-only deferred work; not a write barrier.
    bool inputIdleFor(int quietMs = 100) const;

    // --- Sự kiện ---
    void setEventSink(EventSink sink);
    void clearEvents();   // bỏ các sự kiện đang chờ trong hàng đợi waitFor
    // Chờ đến khi có sự kiện `type` thoả `pred` (hoặc hết thời gian). Dùng cho kiểm thử/đồng bộ.
    bool waitFor(int type, const std::function<bool(const std::string&)>& pred, int timeoutMs);
    // Lấy payload gần nhất của một loại sự kiện (vd. LOK_CALLBACK_CELL_ADDRESS).
    std::string lastPayload(int type);

    bool modified() const { return m_modified; }

    // --- Lưu an toàn ---
    // Gợi ý đường dẫn "Lưu thành" mặc định: cùng thư mục với tệp gốc, tên mới,
    // không trùng tệp đã có. Không bao giờ trả về chính tệp gốc.
    fs::path suggestSaveAsPath(const std::string& ext = std::string()) const;
    SaveResult saveAs(const fs::path& target, const SaveOptions& opts = SaveOptions());

    // --- Bản phục hồi (tự lưu) ---
    // Ghi một bản sao của trạng thái hiện tại vào `dest` (định dạng theo đuôi tệp) để khôi phục sau sự cố.
    // KHÔNG đổi cờ "đã sửa", KHÔNG coi là lần lưu của người dùng, KHÔNG đụng tệp gốc. Ghi ra tệp tạm rồi đổi tên.
    bool saveRecoveryCopy(const fs::path& dest, std::string* error = nullptr);
    // Đuôi tệp dùng cho bản phục hồi của tài liệu này (docx/xlsx/pptx theo loại).
    std::string recoveryExt() const;
    // Tài liệu này được mở từ một bản phục hồi: nhớ tệp gốc và tên hiển thị thật; đánh dấu "chưa lưu".
    void setRecoveredOrigin(const fs::path& originalPath, const std::string& displayName);
    bool isRecovered() const { return !m_recoveredName.empty(); }
    void markModified() { m_modified = true; }

private:
    Document();
    static void lokCallback(int type, const char* payload, void* data);
    void onEvent(int type, const char* payload);

    Engine* m_engine = nullptr;
    std::unique_ptr<lok::Document> m_doc;
    DocKind m_kind = DocKind::Other;
    bool m_readOnly = false;
    // Engine Windows (headless) không tự báo LOK_CALLBACK_INVALIDATE_TILES sau khi gõ/ra lệnh.
    // Sau mỗi thao tác có thể làm đổi tài liệu, phát một sự kiện làm mới có payload "SYNTH"
    // để giao diện vẽ lại (xem docs/WINDOWS_ENGINE.md).
    void emitSynthetic();
    std::atomic<bool> m_modified{false};
    std::atomic<long long> m_lastInputMs{0};   // lần gửi phím/lệnh/chuột gần nhất (steady_clock, ms)
    std::atomic<long long> m_lastEventMs{0};   // sự kiện thật gần nhất từ engine
    fs::path m_original;
    fs::path m_working;
    fs::path m_workDir;   // thư mục tạm riêng của tài liệu này
    std::string m_newName;
    fs::path m_recoveredOrigin;   // tệp gốc của bản phục hồi (có thể rỗng nếu là tài liệu mới)
    std::string m_recoveredName;  // tên hiển thị thật của bản phục hồi
    std::vector<std::string> m_warnings;
    std::vector<FeatureLoss> m_featureLoss;
    struct SessionOutput {
        fs::path path;
        uintmax_t size;
        fs::file_time_type modified;
    };
    std::vector<SessionOutput> m_sessionOutputs; // chỉ coi là bản của phiên nếu metadata vẫn khớp

    std::mutex m_mutex;
    std::condition_variable m_cv;
    EventSink m_sink;
    std::vector<std::pair<int, std::string>> m_last; // payload gần nhất theo loại
    std::vector<std::pair<int, std::string>> m_queue; // sự kiện chưa được waitFor xử lý

    friend class Engine;
};

// Số tài liệu engine đang mở (dùng để quyết định có được thoát cứng an toàn hay không).
int liveDocumentCount();

} // namespace dung
