// Dũng Office — lớp engine: bọc LibreOfficeKit (LOK) để dùng các module xử lý tài liệu của
// LibreOffice (Calc/Writer/Impress/Draw core, filter OOXML/ODF/PDF) mà KHÔNG dùng giao diện
// (frame, menu, toolbar, cửa sổ) của LibreOffice. Giao diện hoàn toàn do Dũng Office vẽ.
// SPDX-License-Identifier: MPL-2.0
#pragma once

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
    void paintPartTile(uint8_t* buffer, int part, int pxW, int pxH, int twipX, int twipY, int twipW, int twipH);
    // Chế độ hiển thị của trang chiếu: 0 = trang chiếu, 1 = trang ghi chú (chỉ Impress).
    void setPartMode(int mode);
    // Thông tin trang dạng JSON của engine (vd. {"visible":"1","mode":0,...}); rỗng nếu engine không hỗ trợ.
    std::string partInfo(int part);
    // Băm nội dung trang (đổi khi nội dung trang đổi); rỗng nếu engine không hỗ trợ.
    std::string partHash(int part);
    // Báo engine vùng đang hiển thị và tỉ lệ zoom (giúp Calc tính đúng con trỏ, tiêu đề hàng/cột).
    void setClientView(int pxTileW, int pxTileH, int twipTileW, int twipTileH,
                       int visX, int visY, int visW, int visH);

    // --- Nhập liệu (toạ độ chuột theo twip) ---
    void postKey(int type, int charCode, int keyCode);
    void postMouse(int type, int twipX, int twipY, int count, int buttons, int modifier);
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
    // Macro VBA / điều khiển biểu mẫu (nút...) phát hiện trong gói OOXML khi mở (xlsm). Dùng để cảnh báo rõ
    // trước khi "Lưu thành" định dạng không giữ được chúng.
    bool hasMacro() const { return m_hasMacro; }
    int formControlCount() const { return m_formControls; }
    // CHỈ DÙNG CHO KIỂM KÊ (tools/smoketest): ghi thẳng bằng engine, BỎ QUA chính sách định dạng, để đo xem engine
    // có làm hỏng macro/nút không. Ứng dụng KHÔNG được gọi hàm này. Không bao giờ ghi vào tệp gốc/bản làm việc.
    bool saveRawForDiagnostics(const fs::path& target, const std::string& format);

private:
    Document() = default;
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
    fs::path m_original;
    fs::path m_working;
    fs::path m_workDir;   // thư mục tạm riêng của tài liệu này
    std::string m_newName;
    bool m_hasMacro = false;
    int m_formControls = 0;
    std::vector<std::string> m_warnings;
    std::vector<fs::path> m_sessionOutputs; // tệp do chính phiên này ghi ra (Ctrl+S lặp lại không cần sao lưu)

    std::mutex m_mutex;
    std::condition_variable m_cv;
    EventSink m_sink;
    std::vector<std::pair<int, std::string>> m_last; // payload gần nhất theo loại
    std::vector<std::pair<int, std::string>> m_queue; // sự kiện chưa được waitFor xử lý

    friend class Engine;
};

} // namespace dung
