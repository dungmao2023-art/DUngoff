// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <cassert>   // LibreOfficeKitInit.h dùng assert() nhưng không tự include (MSVC báo lỗi)
#include <LibreOfficeKit/LibreOfficeKitInit.h>
#include <LibreOfficeKit/LibreOfficeKit.hxx>
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>

namespace dung {

// ---------------------------------------------------------------- tiện ích đường dẫn
std::string toUtf8(const fs::path& p) { return p.u8string(); }
fs::path fromUtf8(const std::string& s) { return fs::u8path(s); }

std::string toFileUrl(const fs::path& p) {
    std::string s = toUtf8(fs::absolute(p));
    std::replace(s.begin(), s.end(), '\\', '/');
    std::string out = "file://";
    if (s.empty() || s[0] != '/') out += '/';   // Windows: file:///C:/...
    static const char* hex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':';
        if (keep) out += (char)c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}

namespace {

std::string randomToken() {
    static std::mt19937_64 rng{std::random_device{}() ^
                               (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count()};
    static const char* a = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string s;
    for (int i = 0; i < 8; ++i) s += a[rng() % 36];
    return s;
}

std::string timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d-%H%M%S", &tm);
    return buf;
}

std::string lowerExt(const fs::path& p) {
    std::string e = p.extension().u8string();
    if (!e.empty() && e[0] == '.') e.erase(0, 1);
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return e;
}

bool samePath(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (fs::exists(a, ec) && fs::exists(b, ec)) return fs::equivalent(a, b, ec);
    std::string x = toUtf8(fs::weakly_canonical(a, ec)), y = toUtf8(fs::weakly_canonical(b, ec));
#ifdef _WIN32
    std::transform(x.begin(), x.end(), x.begin(), ::tolower);
    std::transform(y.begin(), y.end(), y.begin(), ::tolower);
#endif
    return x == y;
}

// Kiểm tra cấu trúc tệp đã ghi trước khi đưa vào vị trí cuối cùng.
bool looksValid(const fs::path& p, const std::string& ext, std::string& why) {
    std::error_code ec;
    auto size = fs::file_size(p, ec);
    if (ec || size == 0) { why = "tệp ghi ra rỗng"; return false; }
    std::ifstream f(p, std::ios::binary);
    std::vector<char> head(8, 0);
    f.read(head.data(), 8);
    const bool zipBased = ext == "xlsx" || ext == "docx" || ext == "pptx" || ext == "ods" ||
                          ext == "odt" || ext == "odp";
    if (zipBased) {
        if (std::memcmp(head.data(), "PK\x03\x04", 4) != 0) { why = "không phải gói ZIP hợp lệ"; return false; }
        // Tìm End Of Central Directory trong 64 KB cuối.
        std::streamoff tail = (std::streamoff)std::min<uintmax_t>(size, 65557);
        std::vector<char> buf((size_t)tail);
        f.clear();
        f.seekg(-tail, std::ios::end);
        f.read(buf.data(), tail);
        for (std::streamoff i = tail - 22; i >= 0; --i)
            if (std::memcmp(buf.data() + i, "PK\x05\x06", 4) == 0) return true;
        why = "gói ZIP bị cắt cụt (thiếu central directory)";
        return false;
    }
    if (ext == "pdf") {
        if (std::memcmp(head.data(), "%PDF-", 5) != 0) { why = "không phải PDF hợp lệ"; return false; }
    }
    return true;
}

fs::path backupPathFor(const fs::path& p) {
    fs::path b = p.parent_path() /
                 fs::u8path(p.stem().u8string() + ".dung-backup-" + timestamp() + p.extension().u8string());
    int n = 2;
    while (fs::exists(b))
        b = p.parent_path() / fs::u8path(p.stem().u8string() + ".dung-backup-" + timestamp() + "-" +
                                         std::to_string(n++) + p.extension().u8string());
    return b;
}

DocKind kindFromLok(int t) {
    switch (t) {
    case LOK_DOCTYPE_TEXT: return DocKind::Text;
    case LOK_DOCTYPE_SPREADSHEET: return DocKind::Spreadsheet;
    case LOK_DOCTYPE_PRESENTATION: return DocKind::Presentation;
    case LOK_DOCTYPE_DRAWING: return DocKind::Drawing;
    default: return DocKind::Other;
    }
}

// Mã phím VCL (com::sun::star::awt::Key) dùng để điều hướng — được phép ở chế độ chỉ đọc.
bool isNavigationKey(int charCode, int keyCode) {
    const int code = keyCode & 0x0FFF;
    const bool ctrl = (keyCode & 0x2000) != 0;
    if (ctrl && charCode == 0 && (code == 512 + 2 /*C*/ || code == 512 + 0 /*A*/)) return true; // Ctrl+C, Ctrl+A
    if (charCode != 0) return false;
    return (code >= 1024 && code <= 1031) /*mũi tên, Home/End, PgUp/PgDn*/ || code == 1282 /*Tab*/ ||
           code == 1281 /*Esc*/;
}

bool isReadOnlySafeCommand(const std::string& c) {
    static const char* allowed[] = {".uno:Copy", ".uno:GoToCell", ".uno:SelectAll", ".uno:Zoom",
                                    ".uno:ZoomPlus", ".uno:ZoomMinus", ".uno:SearchDialog",
                                    ".uno:ExecuteSearch", ".uno:JumpToTable", ".uno:FreezePanes"};
    for (auto* a : allowed)
        if (c == a) return true;
    return false;
}

// Tuỳ chọn nhập CSV: mọi cột đều kiểu Văn bản để KHÔNG ép kiểu (vd. "00123" giữ nguyên,
// "1/2" không thành ngày). Tự nhận dấu phân cách , ; hoặc tab và số cột (có xét dấu ngoặc kép).
std::string csvImportOptions(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::string text(65536, '\0');
    f.read(&text[0], (std::streamsize)text.size());
    text.resize((size_t)f.gcount());
    size_t comma = 0, semi = 0, tab = 0;
    for (char c : text) { comma += c == ','; semi += c == ';'; tab += c == '\t'; }
    char sep = ',';
    if (semi > comma && semi >= tab) sep = ';';
    else if (tab > comma && tab > semi) sep = '\t';
    int maxCols = 1, cols = 1, lines = 0;
    bool inQuote = false;
    for (char c : text) {
        if (c == '"') inQuote = !inQuote;
        else if (!inQuote && c == sep) ++cols;
        else if (!inQuote && c == '\n') {
            maxCols = std::max(maxCols, cols);
            cols = 1;
            if (++lines > 2000) break;
        }
    }
    maxCols = std::max(maxCols, cols);
    maxCols = std::min(maxCols + 8, 1024); // dư vài cột cho dòng dài hơn phần đã quét
    std::string fmt;
    for (int i = 1; i <= maxCols; ++i) fmt += (i > 1 ? "/" : "") + std::to_string(i) + "/2"; // 2 = Text
    // sep, ngoặc kép, UTF-8 (76), từ dòng 1, định dạng cột, ngôn ngữ hệ thống, ô có ngoặc là text,
    // không tự nhận số đặc biệt (ngày/giờ/khoa học)
    return std::to_string((int)(unsigned char)sep) + ",34,76,1," + fmt + ",0,true,false";
}

} // namespace

// ---------------------------------------------------------------- Engine
std::unique_ptr<Engine> Engine::create(const fs::path& programDir, const fs::path& profileDir,
                                       const fs::path& workDir, std::string* error) {
    static bool created = false;
    if (created) {
        if (error) *error = "Engine chỉ được khởi tạo một lần trong mỗi tiến trình.";
        return nullptr;
    }
    std::error_code ec;
    fs::create_directories(profileDir, ec);
    fs::create_directories(workDir, ec);
#ifndef _WIN32
    // Trên Linux/macOS, LibreOffice đổi URL -> đường dẫn theo bảng mã của locale. Với locale
    // POSIX/ASCII, tên tệp tiếng Việt sẽ không mở được -> ép LC_CTYPE sang UTF-8.
    {
        const char* vars[] = {"LC_ALL", "LC_CTYPE", "LANG"};
        std::string cur;
        for (auto* v : vars)
            if (const char* s = std::getenv(v); s && *s) { cur = s; break; }
        if (cur.find("UTF-8") == std::string::npos && cur.find("utf8") == std::string::npos) {
            unsetenv("LC_ALL");
            setenv("LC_CTYPE", "C.UTF-8", 1);
        }
    }
#endif

    const std::string prog = toUtf8(programDir);
    const std::string profile = toFileUrl(profileDir);
    LibreOfficeKit* kit = lok_init_2(prog.c_str(), profile.c_str());
    if (!kit) {
        if (error)
            *error = "Không khởi động được engine tài liệu (LibreOfficeKit) tại: " + prog +
                     ". Kiểm tra thư mục runtime 'engine/program'.";
        return nullptr;
    }
    created = true;
    std::unique_ptr<Engine> e(new Engine());
    e->m_office.reset(new lok::Office(kit));
    e->m_workDir = workDir;
    return e;
}

Engine::~Engine() = default; // LibreOffice tự dọn khi tiến trình kết thúc (không thể khởi tạo lại).

std::string Engine::versionInfo() const {
    char* v = m_office->getVersionInfo();
    std::string s = v ? v : "";
    free(v);
    return s;
}

std::unique_ptr<Document> Engine::open(const fs::path& original, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return std::unique_ptr<Document>(); };
    std::error_code ec;
    if (!fs::is_regular_file(original, ec)) return fail("Không tìm thấy tệp: " + toUtf8(original));
    const FormatInfo* info = formatForPath(original);
    if (!info) return fail("Định dạng tệp chưa được hỗ trợ: " + toUtf8(original.filename()));

    // 1) Sao chép tệp gốc sang bản làm việc tạm. Tệp gốc chỉ được ĐỌC đúng một lần ở đây.
    fs::path dir = m_workDir / ("doc-" + std::to_string(++m_counter) + "-" + randomToken());
    fs::create_directories(dir, ec);
    if (ec) return fail("Không tạo được thư mục làm việc tạm: " + ec.message());
    fs::path working = dir / original.filename();
    fs::copy_file(original, working, fs::copy_options::overwrite_existing, ec);
    if (ec) return fail("Không đọc được tệp (có thể đang bị khoá): " + ec.message());
    // Đảm bảo bản tạm ghi được (tệp gốc có thể mang thuộc tính chỉ đọc).
    fs::permissions(working, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add, ec);

    // 2) Nạp bản tạm vào engine.
    // "Batch=true": chế độ im lặng — engine KHÔNG được hiện bất kỳ hộp thoại cũ nào của
    // LibreOffice (nếu hiện sẽ treo vì không có giao diện LibreOffice).
    std::string options = "Batch=true";
    if (info->ext == "csv") options += "," + csvImportOptions(working);
    lok::Document* d = m_office->documentLoad(toFileUrl(working).c_str(), options.c_str());
    if (!d) {
        char* err = m_office->getError();
        std::string m = std::string("Engine không mở được tệp. ") + (err ? err : "");
        free(err);
        fs::remove_all(dir, ec);
        return fail(m);
    }
    std::unique_ptr<Document> doc(new Document());
    doc->m_engine = this;
    doc->m_doc.reset(d);
    doc->m_original = original;
    doc->m_working = working;
    doc->m_workDir = dir;
    doc->m_kind = kindFromLok(d->getDocumentType());
    if (info->fidelity != Fidelity::Native) doc->m_readOnly = true;
    if (!info->warning.empty()) doc->m_warnings.push_back(info->warning);
    d->registerCallback(&Document::lokCallback, doc.get());
    d->initializeForRendering(nullptr);
    return doc;
}

std::unique_ptr<Document> Engine::createNew(DocKind kind, std::string* error) {
    const char* factory = nullptr;
    const char* name = nullptr;
    switch (kind) {
    case DocKind::Spreadsheet: factory = "private:factory/scalc"; name = "Sổ làm việc mới"; break;
    case DocKind::Text: factory = "private:factory/swriter"; name = "Tài liệu mới"; break;
    case DocKind::Presentation: factory = "private:factory/simpress"; name = "Bản trình chiếu mới"; break;
    default:
        if (error) *error = "Không thể tạo mới loại tài liệu này.";
        return nullptr;
    }
    lok::Document* d = m_office->documentLoad(factory, "Batch=true");
    if (!d) {
        char* err = m_office->getError();
        if (error) *error = std::string("Engine không tạo được tài liệu mới. ") + (err ? err : "");
        free(err);
        return nullptr;
    }
    std::unique_ptr<Document> doc(new Document());
    doc->m_engine = this;
    doc->m_doc.reset(d);
    doc->m_kind = kindFromLok(d->getDocumentType());
    doc->m_newName = name;
    std::error_code ec;
    doc->m_workDir = m_workDir / ("new-" + std::to_string(++m_counter) + "-" + randomToken());
    fs::create_directories(doc->m_workDir, ec);
    d->registerCallback(&Document::lokCallback, doc.get());
    d->initializeForRendering(nullptr);
    return doc;
}

// ---------------------------------------------------------------- Document
Document::~Document() {
    if (m_doc) {
        m_doc->registerCallback(nullptr, nullptr);
        m_doc.reset(); // gọi destroy() của LOK
    }
    std::error_code ec;
    if (!m_workDir.empty()) fs::remove_all(m_workDir, ec); // chỉ xoá thư mục TẠM của chính mình
}

std::string Document::displayName() const {
    return isNew() ? m_newName : toUtf8(m_original.filename());
}

int Document::partCount() { return m_doc->getParts(); }
int Document::currentPart() { return m_doc->getPart(); }
void Document::setPart(int part) { m_doc->setPart(part); }
std::string Document::partName(int part) {
    char* s = m_doc->getPartName(part);
    std::string r = s ? s : "";
    free(s);
    return r;
}

void Document::documentSize(long& w, long& h) { m_doc->getDocumentSize(&w, &h); }

void Document::paintTile(uint8_t* buffer, int pxW, int pxH, int twipX, int twipY, int twipW, int twipH) {
    m_doc->paintTile(buffer, pxW, pxH, twipX, twipY, twipW, twipH);
}

void Document::setClientView(int pxTileW, int pxTileH, int twipTileW, int twipTileH, int visX,
                             int visY, int visW, int visH) {
    m_doc->setClientZoom(pxTileW, pxTileH, twipTileW, twipTileH);
    m_doc->setClientVisibleArea(visX, visY, visW, visH);
}

void Document::postKey(int type, int charCode, int keyCode) {
    if (m_readOnly && !isNavigationKey(charCode, keyCode)) return; // chặn chỉnh sửa ở chế độ chỉ đọc
    m_doc->postKeyEvent(type, charCode, keyCode);
}

void Document::postMouse(int type, int x, int y, int count, int buttons, int modifier) {
    if (m_readOnly && count >= 2) count = 1; // không cho nhấp đúp để vào chế độ sửa ô
    m_doc->postMouseEvent(type, x, y, count, buttons, modifier);
}

void Document::typeText(const std::u16string& text) {
    for (char16_t ch : text) {
        int keyCode = 0;
        if (ch == u'\n') keyCode = 1280; // RETURN
        else if (ch == u'\t') keyCode = 1282; // TAB
        postKey(LOK_KEYEVENT_KEYINPUT, keyCode ? 0 : (int)ch, keyCode);
        postKey(LOK_KEYEVENT_KEYUP, keyCode ? 0 : (int)ch, keyCode);
    }
}

void Document::command(const std::string& cmd, const std::string& args, bool notify) {
    if (m_readOnly && !isReadOnlySafeCommand(cmd)) return;
    m_doc->postUnoCommand(cmd.c_str(), args.empty() ? nullptr : args.c_str(), notify);
}

std::string Document::commandValues(const std::string& q) {
    char* s = m_doc->getCommandValues(q.c_str());
    std::string r = s ? s : "";
    free(s);
    return r;
}

std::string Document::stringArg(const std::string& name, const std::string& value) {
    std::string v;
    for (unsigned char c : value) {
        switch (c) {
        case '"': v += "\\\""; break;
        case '\\': v += "\\\\"; break;
        case '\n': v += "\\n"; break;
        case '\r': v += "\\r"; break;
        case '\t': v += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); v += b; }
            else v += (char)c;
        }
    }
    return "{\"" + name + "\":{\"type\":\"string\",\"value\":\"" + v + "\"}}";
}

std::string Document::selectionAs(const std::string& mime) {
    char* used = nullptr;
    char* s = m_doc->getTextSelection(mime.c_str(), &used);
    std::string r = s ? s : "";
    free(s);
    free(used);
    return r;
}

bool Document::paste(const std::string& mime, const std::string& data) {
    if (m_readOnly) return false;
    return m_doc->paste(mime.c_str(), data.c_str(), data.size());
}

void Document::setEventSink(EventSink sink) {
    std::vector<std::pair<int, std::string>> replay;
    {
        std::lock_guard<std::mutex> g(m_mutex);
        m_sink = sink;
        if (sink) replay = m_last;
    }
    // Sự kiện phát ra trước khi giao diện đăng ký (con trỏ ô, nội dung ô, trạng thái nút...)
    // được phát lại để giao diện không bị trống lúc mới mở. Bỏ qua sự kiện vẽ lại.
    for (const auto& e : replay)
        if (e.first != LOK_CALLBACK_INVALIDATE_TILES) sink(EngineEvent{e.first, e.second});
}

void Document::clearEvents() {
    std::lock_guard<std::mutex> g(m_mutex);
    m_queue.clear();
}

void Document::lokCallback(int type, const char* payload, void* data) {
    static_cast<Document*>(data)->onEvent(type, payload);
}

void Document::onEvent(int type, const char* payload) {
    EngineEvent ev{type, payload ? payload : ""};
    EventSink sink;
    {
        std::lock_guard<std::mutex> g(m_mutex);
        if (type == LOK_CALLBACK_STATE_CHANGED) {
            if (ev.payload.rfind(".uno:ModifiedStatus=", 0) == 0)
                m_modified = ev.payload.find("true") != std::string::npos;
        }
        bool found = false;
        const auto key = [&](const std::string& pl) { return pl.substr(0, pl.find('=')); };
        for (auto& p : m_last)
            if (p.first == type && (type != LOK_CALLBACK_STATE_CHANGED || key(p.second) == key(ev.payload))) {
                p.second = ev.payload;
                found = true;
                break;
            }
        if (!found) m_last.emplace_back(type, ev.payload);
        if (m_queue.size() > 4000) m_queue.erase(m_queue.begin(), m_queue.begin() + 2000);
        m_queue.emplace_back(type, ev.payload);
        sink = m_sink;
    }
    m_cv.notify_all();
    if (sink) sink(ev);
}

bool Document::waitFor(int type, const std::function<bool(const std::string&)>& pred, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::unique_lock<std::mutex> lk(m_mutex);
    for (;;) {
        for (size_t i = 0; i < m_queue.size(); ++i) {
            if (m_queue[i].first == type && (!pred || pred(m_queue[i].second))) {
                m_queue.erase(m_queue.begin(), m_queue.begin() + (long)i + 1);
                return true;
            }
        }
        m_queue.clear();
        if (m_cv.wait_until(lk, deadline) == std::cv_status::timeout && m_queue.empty()) return false;
    }
}

std::string Document::lastPayload(int type) {
    std::lock_guard<std::mutex> g(m_mutex);
    for (auto& p : m_last)
        if (p.first == type) return p.second;
    return {};
}

fs::path Document::suggestSaveAsPath(const std::string& wantedExt) const {
    std::string ext = wantedExt;
    if (ext.empty()) {
        if (!isNew()) {
            const FormatInfo* f = formatForPath(m_original);
            if (f && f->fidelity == Fidelity::Native) ext = f->ext;
        }
        if (ext.empty()) {
            auto natives = nativeSaveFormats(m_kind);
            ext = natives.empty() ? "pdf" : natives.front()->ext; // xlsx / docx / pptx ưu tiên đầu
        }
    }
    fs::path dir;
    std::string stem;
    if (isNew()) {
        const char* home = std::getenv(
#ifdef _WIN32
            "USERPROFILE"
#else
            "HOME"
#endif
        );
        dir = home ? fs::u8path(home) / "Documents" : fs::current_path();
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) dir = home ? fs::u8path(home) : fs::current_path();
        stem = m_newName;
    } else {
        dir = m_original.parent_path();
        stem = m_original.stem().u8string() + " - Dũng Office";
    }
    fs::path p = dir / fs::u8path(stem + "." + ext);
    for (int n = 2; fs::exists(p) || (!isNew() && samePath(p, m_original)); ++n)
        p = dir / fs::u8path(stem + " (" + std::to_string(n) + ")." + ext);
    return p;
}

SaveResult Document::saveAs(const fs::path& target, const SaveOptions& opts) {
    SaveResult r;
    const std::string ext = lowerExt(target);
    const FormatInfo* info = formatForExt(ext);
    const bool isPdf = ext == "pdf";
    std::error_code ec;

    // --- Kiểm tra quy tắc trước khi ghi bất cứ thứ gì ---
    if (!info && !isPdf) { r.message = "Không ghi được định dạng ." + ext; return r; }
    if (isPdf) {
        if (!opts.allowLossyExport) {
            r.message = m_kind == DocKind::Drawing
                ? "PDF được mở để xem. Tạo lại tệp PDF có thể thay đổi bố cục so với bản gốc. Cần xác nhận xuất."
                : "Xuất PDF là bản in (mất công thức/khả năng chỉnh sửa). Cần xác nhận xuất.";
            return r;
        }
    } else {
        if (!info || info->kind != m_kind) {
            r.message = "Định dạng ." + ext + " không phù hợp với loại tài liệu " + kindName(m_kind) + ".";
            return r;
        }
        if (info->fidelity != Fidelity::Native) {
            const auto natives = nativeSaveFormats(m_kind);
            r.message = "Dũng Office chưa ghi được định dạng ." + ext + " mà bảo đảm không mất dữ liệu." +
                        (natives.empty() ? std::string(" Có thể dùng \"Xuất PDF\".")
                                         : " Hãy chọn định dạng ." + natives.front()->ext + ".");
            return r;
        }
        if (m_readOnly && !opts.allowLossyExport) {
            r.message = "Tài liệu đang ở chế độ chỉ đọc. Lưu sang định dạng mới có thể làm mất một phần "
                        "nội dung của tệp gốc (vd. macro). Cần xác nhận trước khi lưu.";
            return r;
        }
    }
    const bool targetIsOriginal = !isNew() && samePath(target, m_original);
    if (targetIsOriginal && !opts.allowOverwriteOriginal) {
        r.message = "Để bảo vệ dữ liệu, Dũng Office không ghi đè tệp gốc. Hãy dùng \"Lưu thành\" với tên mới.";
        return r;
    }
    const bool exists = fs::exists(target, ec);
    bool ownOutput = false; // tệp do chính phiên này ghi ra trước đó -> ghi lại không cần hỏi/sao lưu
    for (const auto& o : m_sessionOutputs)
        if (samePath(o, target)) ownOutput = true;
    if (exists && !targetIsOriginal && !ownOutput && !opts.allowReplaceExisting) {
        r.message = "Tệp đích đã tồn tại. Cần xác nhận thay thế.";
        return r;
    }
    if (!fs::is_directory(target.parent_path(), ec)) {
        r.message = "Thư mục đích không tồn tại: " + toUtf8(target.parent_path());
        return r;
    }

    // --- Ghi ra tệp tạm cùng thư mục, kiểm tra, rồi mới thay thế nguyên tử ---
    fs::path tmp = target.parent_path() /
                   fs::u8path("~dung-" + randomToken() + "-" + target.stem().u8string() + "." + ext);
    bool ok = m_doc->saveAs(toFileUrl(tmp).c_str(), ext.c_str(), nullptr);
    if (!ok) {
        fs::remove(tmp, ec);
        r.message = "Engine không ghi được tệp.";
        return r;
    }
    std::string why;
    if (!looksValid(tmp, ext, why)) {
        fs::remove(tmp, ec);
        r.message = "Tệp ghi ra không hợp lệ (" + why + "). Tệp đích chưa bị thay đổi.";
        return r;
    }
    if (exists && !ownOutput) {
        r.backup = backupPathFor(target);
        fs::copy_file(target, r.backup, fs::copy_options::none, ec);
        if (ec) {
            fs::remove(tmp, ec);
            r.message = "Không tạo được bản sao lưu trước khi thay thế. Đã huỷ để bảo vệ dữ liệu.";
            r.backup.clear();
            return r;
        }
    }
    fs::rename(tmp, target, ec); // thay thế nguyên tử trong cùng ổ đĩa
    if (ec) {
        fs::remove(tmp, ec);
        r.message = "Không đưa được tệp vào vị trí đích: " + ec.message();
        return r;
    }
    r.ok = true;
    r.written = target;
    r.message = "Đã lưu: " + toUtf8(target.filename());
    if (!isPdf) m_modified = false;
    if (!ownOutput) m_sessionOutputs.push_back(target);
    return r;
}

} // namespace dung
