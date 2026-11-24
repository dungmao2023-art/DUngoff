// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "dung/Engine.h"

#include <cassert>
#include <cwchar>   // LibreOfficeKitInit.h dùng assert() nhưng không tự include (MSVC báo lỗi)
#include <LibreOfficeKit/LibreOfficeKitInit.h>
#include <LibreOfficeKit/LibreOfficeKit.hxx>
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <deque>
#include <functional>
#include <future>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#include <cstdlib>
#endif

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


// Windows giới hạn MAX_PATH = 260 ký tự cho đường dẫn thường. Đường dẫn tuyệt đối dài hơn cần tiền tố \\?\ .
// longPath() trả về đường dẫn có tiền tố khi đủ dài, ngược lại giữ nguyên; trên hệ khác luôn giữ nguyên.
fs::path longPath(const fs::path& p) {
#ifdef _WIN32
    std::error_code ec;
    fs::path a = fs::absolute(p, ec);
    if (ec) return p;
    std::wstring w = a.lexically_normal().make_preferred().wstring();
    if (w.size() < 240 || w.rfind(L"\\\\?\\", 0) == 0) return p;
    if (w.rfind(L"\\\\", 0) == 0) return fs::path(L"\\\\?\\UNC\\" + w.substr(2)); // \\máy\chia-se
    return fs::path(L"\\\\?\\" + w);
#else
    return p;
#endif
}
std::size_t pathChars(const fs::path& p) {
    std::error_code ec;
    fs::path a = fs::absolute(p, ec);
    return (ec ? p : a).native().size();
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
    if (fs::exists(longPath(a), ec) && fs::exists(longPath(b), ec)) return fs::equivalent(longPath(a), longPath(b), ec);
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
    while (fs::exists(longPath(b)))
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


// ---------------------------------------------------------------- luồng engine
// Trên Windows, bản LibreOffice chính thức dùng backend Win32 (cửa sổ ẩn thật). Nếu gọi API LOK
// từ một luồng khác "VCL Main", luồng gọi giữ SolarMutex rồi SendMessage sang cửa sổ của VCL Main,
// còn VCL Main lại chờ SolarMutex -> TREO (đã xác nhận bằng dump ngăn xếp trên CI).
// Cách giải: chế độ "unipoll" của LOK — MỘT luồng engine riêng gọi lok_init + runLoop, nên luồng
// đó chính là VCL Main; mọi lệnh gọi engine từ luồng khác được chuyển sang luồng này qua một cửa sổ
// message-only (vòng lặp của LibreOffice dispatch thông điệp của nó).
// Trên Linux, chế độ đa luồng mặc định đã chạy đúng nên giữ nguyên (gọi trực tiếp).
namespace {
#ifdef _WIN32
constexpr UINT kRunQueue = WM_APP + 0x4C4F; // "LO"
#endif

struct EngineThread {
#ifdef _WIN32
    HWND hwnd = nullptr;
    DWORD threadId = 0;
#else
    std::thread::id threadId;
    std::condition_variable cv;
    bool wake = false;
#endif
    std::mutex mutex;
    std::deque<std::function<void()>> queue;
    bool busy = false;      // đang chạy một tác vụ (tác vụ có thể tự bơm vòng lặp -> tránh lồng nhau)
    bool pending = false;

    bool onThisThread() const {
#ifdef _WIN32
        return GetCurrentThreadId() == threadId;
#else
        return std::this_thread::get_id() == threadId;
#endif
    }
    void post() {
#ifdef _WIN32
        if (hwnd) PostMessageW(hwnd, kRunQueue, 0, 0);
        else PostThreadMessageW(threadId, kRunQueue, 0, 0);
#else
        { std::lock_guard<std::mutex> g(mutex); wake = true; }
        cv.notify_all();
#endif
    }
    void drain() {
        if (busy) { pending = true; return; }
        busy = true;
        for (;;) {
            std::function<void()> job;
            {
                std::lock_guard<std::mutex> g(mutex);
                if (queue.empty()) break;
                job = std::move(queue.front());
                queue.pop_front();
            }
            job();
        }
        busy = false;
        if (pending) { pending = false; post(); }
    }
#ifdef _WIN32
    static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM w, LPARAM l);
#endif
};
EngineThread* g_engineThread = nullptr;

#ifdef _WIN32
LRESULT CALLBACK EngineThread::wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == kRunQueue && g_engineThread) { g_engineThread->drain(); return 0; }
    return DefWindowProcW(h, msg, w, l);
}
int lokPoll(void*, int) { return 0; }   // backend Win32 không dùng poll callback
void lokWake(void*) {}

// Hook WH_GETMESSAGE gắn vào luồng "VCL Main" của LibreOffice: khi vòng lặp của LibreOffice lấy
// thông điệp kRunQueue (thread message), hook chạy các tác vụ đang chờ NGAY TRÊN luồng VCL Main.
LRESULT CALLBACK getMsgHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == PM_REMOVE && g_engineThread) {
        MSG* m = reinterpret_cast<MSG*>(lParam);
        if (m->message == kRunQueue && m->hwnd == nullptr) {
            m->message = WM_NULL;
            g_engineThread->drain();
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

struct FindCtx { DWORD pid; HWND found; };
BOOL CALLBACK findSalWnd(HWND h, LPARAM lp) {
    auto* c = reinterpret_cast<FindCtx*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 63);
    if (pid == c->pid && std::wcsncmp(cls, L"SAL", 3) == 0) { c->found = h; return FALSE; }
    return TRUE;
}
// Tìm luồng VCL Main: luồng sở hữu cửa sổ ẩn SALCOMWND / SAL* của LibreOffice trong tiến trình này.
DWORD findVclMainThread() {
    for (int attempt = 0; attempt < 50; ++attempt) {
        HWND h = FindWindowExW(HWND_MESSAGE, nullptr, L"SALCOMWND", nullptr);
        if (!h) {
            FindCtx c{GetCurrentProcessId(), nullptr};
            EnumWindows(&findSalWnd, reinterpret_cast<LPARAM>(&c));
            h = c.found;
        }
        if (h) return GetWindowThreadProcessId(h, nullptr);
        Sleep(100);
    }
    return 0;
}
#else
// Linux (headless svp, unipoll): LibreOffice gọi poll khi rảnh -> chờ tác vụ tối đa timeoutUs rồi chạy.
int lokPoll(void* data, int timeoutUs) {
    auto* et = static_cast<EngineThread*>(data);
    {
        std::unique_lock<std::mutex> lk(et->mutex);
        if (et->queue.empty() && !et->wake && timeoutUs != 0)
            et->cv.wait_for(lk, std::chrono::microseconds(timeoutUs < 0 ? 100000 : timeoutUs),
                            [et] { return et->wake || !et->queue.empty(); });
        et->wake = false;
    }
    et->drain();
    return 0;
}
void lokWake(void* data) {
    auto* et = static_cast<EngineThread*>(data);
    { std::lock_guard<std::mutex> g(et->mutex); et->wake = true; }
    et->cv.notify_all();
}
#endif
} // namespace

template <class F> static auto onEngine(F&& f) -> decltype(f()) {
    using R = decltype(f());
    if (!g_engineThread || g_engineThread->onThisThread()) return f();
    std::promise<void> done;
    auto fut = done.get_future();
    std::optional<std::conditional_t<std::is_void_v<R>, int, R>> result;
    {
        std::lock_guard<std::mutex> g(g_engineThread->mutex);
        g_engineThread->queue.emplace_back([&] {
            if constexpr (std::is_void_v<R>) f();
            else result.emplace(f());
            done.set_value();
        });
    }
    g_engineThread->post();
    fut.wait();
    if constexpr (!std::is_void_v<R>) return std::move(*result);
}

// ---------------------------------------------------------------- Bảng thay thế phông
// Các phông hiện đại của Microsoft 365 (Aptos...) không có trong engine. Nếu không có bảng thay thế,
// engine rơi về phông hệ thống (vd PMingLiU) và chữ tiếng Việt có dấu bị vỡ khi xuất PDF/in.
// Ta ghi vào hồ sơ engine một bảng thay thế sang phông có sẵn trong engine và đủ chữ Việt (Carlito, Liberation Sans).
// SubstituteFont chỉ ghi MỘT phông (dạng "A;B" làm engine bỏ qua bảng — đã đo trên CI Windows, biến thể v1/v2).
// "Always=true": CI Windows vòng 3 cho thấy Always=false KHÔNG áp dụng (phủ chữ 46,8%), Always=true áp dụng (100%).
// Hệ quả: máy có Aptos thật cũng sẽ hiển thị/in bằng Carlito (số đo gần tương đương). Cần người dùng cân nhắc.
// Chỉ thêm, không ghi đè mục khác của hồ sơ; chạy lặp lại không thêm trùng.
static void seedFontSubstitution(const fs::path& profileDir) {
    static const char* kMarker = "dung-aptos-0";
    const char* fromFonts[] = {"Aptos", "Aptos Display", "Aptos Narrow", "Aptos SemiBold", "Aptos Light", "Aptos Black"};
    std::string items =
        "<item oor:path=\"/org.openoffice.Office.Common/Font/Substitution\"><prop oor:name=\"Replacement\" oor:op=\"fuse\"><value>true</value></prop></item>\n";
    int i = 0;
    for (const char* f : fromFonts) {
        items += std::string("<item oor:path=\"/org.openoffice.Office.Common/Font/Substitution/FontPairs\"><node oor:name=\"dung-aptos-") +
                 std::to_string(i++) + "\" oor:op=\"replace\"><prop oor:name=\"Always\" oor:op=\"fuse\"><value>true</value></prop>"
                 "<prop oor:name=\"OnScreenOnly\" oor:op=\"fuse\"><value>false</value></prop>"
                 "<prop oor:name=\"ReplaceFont\" oor:op=\"fuse\"><value>" + f + "</value></prop>"
                 "<prop oor:name=\"SubstituteFont\" oor:op=\"fuse\"><value>Carlito</value></prop></node></item>\n";
    }
    std::error_code ec;
    const fs::path dir = profileDir / "user";
    fs::create_directories(dir, ec);
    const fs::path file = dir / "registrymodifications.xcu";
    std::string content;
    if (fs::exists(file, ec)) {
        std::ifstream in(file, std::ios::binary);
        content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (content.find(kMarker) != std::string::npos) return; // đã có
        const size_t pos = content.rfind("</oor:items>");
        if (pos == std::string::npos) return; // tệp lạ: không đụng vào
        content.insert(pos, items);
    } else {
        content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<oor:items xmlns:oor=\"http://openoffice.org/2001/registry\" "
                  "xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n" +
                  items + "</oor:items>\n";
    }
    const fs::path tmp = file.string() + ".dung-tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << content;
    }
    fs::rename(tmp, file, ec);
}

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
    seedFontSubstitution(profileDir);
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

#ifdef _WIN32
    // LibreOfficeKitInit.h nạp mergedlo.dll bằng LoadLibraryW(đường dẫn đầy đủ). Windows KHÔNG tìm DLL
    // phụ thuộc trong thư mục của DLL đó, nên phải thêm thư mục program vào đường tìm DLL và PATH.
    {
        const std::wstring wprog = fs::absolute(programDir).make_preferred().wstring();
        SetDllDirectoryW(wprog.c_str());
        const wchar_t* oldPath = _wgetenv(L"PATH");
        std::wstring newPath = wprog + L";" + (oldPath ? oldPath : L"");
        _wputenv_s(L"PATH", newPath.c_str());
    }
    const std::string prog = toUtf8(fs::absolute(programDir).make_preferred());
#else
    const std::string prog = toUtf8(programDir);
#endif
    const std::string profile = toFileUrl(profileDir);
#ifdef _WIN32
    const char* modeEnv = std::getenv("DUNG_ENGINE_MODE");
    const bool useEngineThread = modeEnv && std::string(modeEnv) == "unipoll";
    // Engine tự build với --enable-headless (có tệp đánh dấu): backend svp không có cửa sổ Win32
    // -> chạy chế độ đa luồng như trên Linux, gọi trực tiếp, không cần hook.
    const bool headlessEngine = fs::exists(fs::absolute(programDir).parent_path() / "dung-engine-headless.txt");
    if (headlessEngine) _putenv_s("SAL_USE_VCLPLUGIN", "svp");
#else
    const char* up = std::getenv("DUNG_LOK_UNIPOLL");
    const bool useEngineThread = up && std::string(up) == "1";
#endif
    LibreOfficeKit* kit = nullptr;
    if (!useEngineThread) {
        kit = lok_init_2(prog.c_str(), profile.c_str());
#ifdef _WIN32
        if (kit && !headlessEngine) {
            // Chế độ đa luồng (LibreOffice tự chạy luồng VCL Main). Mọi lệnh gọi LOK sẽ được chạy
            // trên luồng VCL Main qua hook — tránh deadlock SolarMutex/SendMessage giữa hai luồng.
            const DWORD vclThread = findVclMainThread();
            if (!vclThread) {
                if (error) *error = "Không tìm thấy luồng VCL Main của engine.";
                return nullptr;
            }
            auto* et = new EngineThread();
            et->threadId = vclThread;
            if (!SetWindowsHookExW(WH_GETMESSAGE, &getMsgHook, nullptr, vclThread)) {
                if (error) *error = "Không gắn được hook vào luồng engine (lỗi " + std::to_string(GetLastError()) + ").";
                return nullptr;
            }
            g_engineThread = et;
            std::fprintf(stderr, "[dung-engine] chế độ đa luồng + hook, VCL Main = luồng %lu\n", (unsigned long)vclThread);
            std::fflush(stderr);
        }
#endif
    } else {
        std::promise<LibreOfficeKit*> ready;
        auto readyFut = ready.get_future();
        std::thread([prog, profile, &ready] {
            auto* et = new EngineThread();
#ifdef _WIN32
            et->threadId = GetCurrentThreadId();
            WNDCLASSW wc{};
            wc.lpfnWndProc = &EngineThread::wndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"DungOfficeEngineQueue";
            RegisterClassW(&wc);
            et->hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
            _putenv_s("SAL_LOK_OPTIONS", "unipoll");
#else
            et->threadId = std::this_thread::get_id();
            setenv("SAL_LOK_OPTIONS", "unipoll", 1);
#endif
            LibreOfficeKit* k = lok_init_2(prog.c_str(), profile.c_str());
            if (!k) { ready.set_value(nullptr); return; }
            g_engineThread = et;
            ready.set_value(k);
            // Vòng lặp chính của LibreOffice chạy trên luồng này cho đến khi tiến trình kết thúc.
            // Gọi thẳng qua bảng hàm C (không tạo thêm lok::Office để tránh destroy() hai lần).
            const auto t0 = std::chrono::steady_clock::now();
            // DUNG_ENGINE_RUNLOOP=0: bỏ qua runLoop (không chạy Desktop::Main), chỉ tự bơm thông điệp.
            const char* rl = std::getenv("DUNG_ENGINE_RUNLOOP");
#ifdef _WIN32
            const bool doRunLoop = rl && std::string(rl) == "1";   // Windows: mặc định KHÔNG (Desktop::Main thoát ngay)
#else
            const bool doRunLoop = !(rl && std::string(rl) == "0");
#endif
            if (doRunLoop) k->pClass->runLoop(k, &lokPoll, &lokWake, et);
            const long long ms = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - t0).count();
            std::fprintf(stderr, "[dung-engine] runLoop=%d, trả về sau %lld ms; luồng engine tự bơm thông điệp\n", doRunLoop ? 1 : 0, ms);
            std::fflush(stderr);
#ifdef _WIN32
            // VCL Main vẫn là luồng này (InitVCL đã chạy ở đây), cửa sổ ẩn và bộ hẹn giờ của LibreOffice
            // đều thuộc luồng này -> tự bơm thông điệp là đủ để engine tiếp tục xử lý.
            MSG m;
            while (GetMessageW(&m, nullptr, 0, 0) > 0) {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
#else
            for (;;) lokPoll(et, 100000);
#endif
        }).detach();
        kit = readyFut.get();
    }
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
    char* v = onEngine([&] { return m_office->getVersionInfo(); });
    std::string s = v ? v : "";
    free(v);
    return s;
}

std::unique_ptr<Document> Engine::open(const fs::path& original, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return std::unique_ptr<Document>(); };
    std::error_code ec;
    if (!fs::is_regular_file(longPath(original), ec)) {
        const size_t n = pathChars(original);
        return fail("Không tìm thấy tệp: " + toUtf8(original) +
                    (n >= 240 ? " (đường dẫn dài " + std::to_string(n) + " ký tự; nếu tệp có thật, hãy chuyển tệp tới thư mục có đường dẫn ngắn hơn)" : std::string()));
    }
    const FormatInfo* info = formatForPath(original);
    if (!info) return fail("Định dạng tệp chưa được hỗ trợ: " + toUtf8(original.filename()));

    // 1) Sao chép tệp gốc sang bản làm việc tạm. Tệp gốc chỉ được ĐỌC đúng một lần ở đây.
    //    Bản làm việc nằm ở thư mục tạm NGẮN; nếu tên tệp gốc quá dài thì rút ngắn tên (giữ đuôi) để tổng đường dẫn < 240 ký tự,
    //    vì engine không mở được đường dẫn từ 260 ký tự trở lên.
    fs::path dir = m_workDir / ("doc-" + std::to_string(++m_counter) + "-" + randomToken());
    fs::create_directories(longPath(dir), ec);
    if (ec) return fail("Không tạo được thư mục làm việc tạm: " + ec.message());
    fs::path working = dir / original.filename();
    if (pathChars(working) > 230) {
        const std::string e = original.extension().u8string();
        const size_t room = pathChars(dir) + 1 + e.size() < 230 ? 230 - (pathChars(dir) + 1 + e.size()) : 8;
        std::string stem = original.stem().u8string();
        // cắt theo ký tự UTF-8 hoàn chỉnh
        size_t cut = std::min(stem.size(), room);
        while (cut > 0 && (static_cast<unsigned char>(stem[cut]) & 0xC0) == 0x80 && cut < stem.size()) --cut;
        working = dir / fs::u8path(stem.substr(0, cut) + e);
    }
    fs::copy_file(longPath(original), longPath(working), fs::copy_options::overwrite_existing, ec);
    if (ec) return fail("Không đọc được tệp (có thể đang bị khoá): " + ec.message());
    // Đảm bảo bản tạm ghi được (tệp gốc có thể mang thuộc tính chỉ đọc).
    fs::permissions(longPath(working), fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add, ec);

    // 2) Nạp bản tạm vào engine.
    // "Batch=true": chế độ im lặng — engine KHÔNG được hiện bất kỳ hộp thoại cũ nào của
    // LibreOffice (nếu hiện sẽ treo vì không có giao diện LibreOffice).
    std::string options = "Batch=true";
    if (info->ext == "csv") options += "," + csvImportOptions(working);
    const std::string url = toFileUrl(working);
    lok::Document* d = onEngine([&] { return m_office->documentLoad(url.c_str(), options.c_str()); });
    if (!d) {
        char* err = onEngine([&] { return m_office->getError(); });
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
    if (info->fidelity != Fidelity::Native) doc->m_readOnly = true;
    if (!info->warning.empty()) doc->m_warnings.push_back(info->warning);
    // Quét tệp GỐC (không phải bản tạm) để biết trước tính năng nào sẽ không được giữ khi lưu.
    if (info->ext == "docx" || info->ext == "xlsx" || info->ext == "pptx" || info->ext == "xlsm" ||
        info->ext == "docm" || info->ext == "pptm" || info->ext == "dotx" || info->ext == "xltx" || info->ext == "potx") {
        const FeatureScanResult sc = scanFeatureLoss(longPath(original));
        if (sc.scanned) {
            doc->m_featureLoss = sc.items;
            for (const auto& it : sc.items)
                doc->m_warnings.push_back("Khi lưu bản sao sẽ mất: " + it.title + (it.certain ? "" : " (có thể)") + ". " + it.effect);
        }
    }
    Document* raw = doc.get();
    onEngine([&] {
        raw->m_kind = kindFromLok(d->getDocumentType());
        d->registerCallback(&Document::lokCallback, raw);
        d->initializeForRendering(nullptr);
    });
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
    lok::Document* d = onEngine([&] { return m_office->documentLoad(factory, "Batch=true"); });
    if (!d) {
        char* err = onEngine([&] { return m_office->getError(); });
        if (error) *error = std::string("Engine không tạo được tài liệu mới. ") + (err ? err : "");
        free(err);
        return nullptr;
    }
    std::unique_ptr<Document> doc(new Document());
    doc->m_engine = this;
    doc->m_doc.reset(d);
    doc->m_kind = onEngine([&] { return kindFromLok(d->getDocumentType()); });
    doc->m_newName = name;
    std::error_code ec;
    doc->m_workDir = m_workDir / ("new-" + std::to_string(++m_counter) + "-" + randomToken());
    fs::create_directories(doc->m_workDir, ec);
    Document* raw = doc.get();
    onEngine([&] {
        d->registerCallback(&Document::lokCallback, raw);
        d->initializeForRendering(nullptr);
    });
    return doc;
}

// ---------------------------------------------------------------- Document
Document::~Document() {
    if (m_doc) {
        onEngine([&] {
            m_doc->registerCallback(nullptr, nullptr);
            m_doc.reset(); // gọi destroy() của LOK
        });
    }
    std::error_code ec;
    if (!m_workDir.empty()) fs::remove_all(m_workDir, ec); // chỉ xoá thư mục TẠM của chính mình
}

std::string Document::displayName() const {
    if (!m_recoveredName.empty()) return m_recoveredName;
    return isNew() ? m_newName : toUtf8(m_original.filename());
}

void Document::setRecoveredOrigin(const fs::path& originalPath, const std::string& displayName) {
    m_recoveredOrigin = originalPath;
    m_recoveredName = displayName.empty() ? std::string("Tài liệu khôi phục") : displayName;
    m_modified = true; // bản khôi phục luôn coi là chưa lưu
}

std::string Document::recoveryExt() const {
    switch (m_kind) {
    case DocKind::Spreadsheet: return "xlsx";
    case DocKind::Presentation: return "pptx";
    default: return "docx";
    }
}

bool Document::saveRecoveryCopy(const fs::path& dest, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return false; };
    if (m_workDir.empty()) return fail("Chưa có thư mục làm việc tạm.");
    const std::string ext = lowerExt(dest);
    if (!formatForExt(ext) || formatForExt(ext)->kind != m_kind) return fail("Đuôi tệp phục hồi không phù hợp: ." + ext);
    std::error_code ec;
    const bool wasModified = m_modified;
    fs::path stage = m_workDir / fs::u8path("~rec-" + randomToken() + "." + ext);
    const std::string url = toFileUrl(stage);
    const bool ok = onEngine([&] { return m_doc->saveAs(url.c_str(), ext.c_str(), nullptr); });
    // Một số bản engine báo "đã lưu" sau khi ghi bản sao; đây không phải lần lưu của người dùng nên khôi phục cờ.
    if (wasModified) m_modified = true;
    if (!ok) { fs::remove(stage, ec); return fail("Engine không ghi được bản phục hồi."); }
    std::string why;
    if (!looksValid(stage, ext, why)) { fs::remove(stage, ec); return fail("Bản phục hồi không hợp lệ (" + why + ")."); }
    fs::create_directories(longPath(dest.parent_path()), ec);
    fs::path sibling = dest.parent_path() / fs::u8path("~rec-" + randomToken() + ".tmp");
    fs::copy_file(longPath(stage), longPath(sibling), fs::copy_options::none, ec);
    fs::remove(stage, ec);
    if (ec) return fail("Không ghi được bản phục hồi: " + ec.message());
    fs::rename(longPath(sibling), longPath(dest), ec);
    if (ec) {
        const std::string em = ec.message();
        fs::remove(longPath(sibling), ec);
        return fail("Không đặt được bản phục hồi: " + em);
    }
    return true;
}

int Document::partCount() { return onEngine([&] { return m_doc->getParts(); }); }
int Document::currentPart() { return onEngine([&] { return m_doc->getPart(); }); }
void Document::setPart(int part) { onEngine([&] { m_doc->setPart(part); }); }
std::string Document::partName(int part) {
    char* s = onEngine([&] { return m_doc->getPartName(part); });
    std::string r = s ? s : "";
    free(s);
    return r;
}

void Document::documentSize(long& w, long& h) { onEngine([&] { m_doc->getDocumentSize(&w, &h); }); }

void Document::paintTile(uint8_t* buffer, int pxW, int pxH, int twipX, int twipY, int twipW, int twipH) {
    onEngine([&] { m_doc->paintTile(buffer, pxW, pxH, twipX, twipY, twipW, twipH); });
}

void Document::paintPartTile(uint8_t* buffer, int part, int pxW, int pxH, int twipX, int twipY, int twipW, int twipH) {
    onEngine([&] { m_doc->paintPartTile(buffer, part, 0, pxW, pxH, twipX, twipY, twipW, twipH); });
}

void Document::setPartMode(int mode) { onEngine([&] { m_doc->setPartMode(mode); }); }

std::string Document::partInfo(int part) {
    char* s = onEngine([&]() -> char* {
        LibreOfficeKitDocument* d = m_doc->get();
        return d->pClass->getPartInfo ? d->pClass->getPartInfo(d, part) : nullptr;
    });
    std::string r = s ? s : "";
    free(s);
    return r;
}

std::string Document::partHash(int part) {
    char* s = onEngine([&] { return m_doc->getPartHash(part); });
    std::string r = s ? s : "";
    free(s);
    return r;
}

void Document::setClientView(int pxTileW, int pxTileH, int twipTileW, int twipTileH, int visX,
                             int visY, int visW, int visH) {
    onEngine([&] {
        m_doc->setClientZoom(pxTileW, pxTileH, twipTileW, twipTileH);
        m_doc->setClientVisibleArea(visX, visY, visW, visH);
    });
}

namespace {
long long steadyMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}

void Document::postKey(int type, int charCode, int keyCode) {
    if (m_readOnly && !isNavigationKey(charCode, keyCode)) return; // chặn chỉnh sửa ở chế độ chỉ đọc
    m_lastInputMs = steadyMs();
    onEngine([&] { m_doc->postKeyEvent(type, charCode, keyCode); });
    if (type == LOK_KEYEVENT_KEYINPUT) emitSynthetic();
}

void Document::postMouse(int type, int x, int y, int count, int buttons, int modifier) {
    if (m_readOnly && count >= 2) count = 1; // không cho nhấp đúp để vào chế độ sửa ô
    onEngine([&] { m_doc->postMouseEvent(type, x, y, count, buttons, modifier); });
    if (type != LOK_MOUSEEVENT_MOUSEMOVE) emitSynthetic();
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
    m_lastInputMs = steadyMs();
    onEngine([&] { m_doc->postUnoCommand(cmd.c_str(), args.empty() ? nullptr : args.c_str(), notify); });
    emitSynthetic();
}

std::string Document::commandValues(const std::string& q) {
    char* s = onEngine([&] { return m_doc->getCommandValues(q.c_str()); });
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
    char* s = onEngine([&] { return m_doc->getTextSelection(mime.c_str(), &used); });
    std::string r = s ? s : "";
    free(s);
    free(used);
    return r;
}

bool Document::paste(const std::string& mime, const std::string& data) {
    if (m_readOnly) return false;
    waitInputIdle(); // phím vừa gõ có thể còn nằm trong hàng đợi engine: đợi xử lý xong rồi mới dán để không lẫn thứ tự
    const bool ok = onEngine([&] { return m_doc->paste(mime.c_str(), data.c_str(), data.size()); });
    emitSynthetic();
    return ok;
}

void Document::waitInputIdle(int quietMs, int maxMs) {
    const long long start = steadyMs();
    for (;;) {
        const long long now = steadyMs();
        const long long last = std::max(m_lastInputMs.load(), m_lastEventMs.load());
        if (last == 0 || now - last >= quietMs || now - start >= maxMs) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void Document::emitSynthetic() { onEvent(LOK_CALLBACK_INVALIDATE_TILES, "SYNTH"); }

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
    if (!(type == LOK_CALLBACK_INVALIDATE_TILES && ev.payload == "SYNTH")) m_lastEventMs = steadyMs();
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
    } else if (!m_recoveredName.empty()) {
        // Bản khôi phục: gợi ý lưu cạnh tệp gốc cũ (nếu còn thư mục), không phải trong thư mục phục hồi.
        std::error_code ec;
        if (!m_recoveredOrigin.empty() && fs::is_directory(longPath(m_recoveredOrigin.parent_path()), ec)) {
            dir = m_recoveredOrigin.parent_path();
            stem = m_recoveredOrigin.stem().u8string() + " - Dũng Office";
        } else {
            const char* home = std::getenv(
#ifdef _WIN32
                "USERPROFILE"
#else
                "HOME"
#endif
            );
            dir = home ? fs::u8path(home) / "Documents" : fs::current_path();
            if (!fs::is_directory(dir, ec)) dir = home ? fs::u8path(home) : fs::current_path();
            stem = fs::u8path(m_recoveredName).stem().u8string() + " - khôi phục";
        }
    } else {
        dir = m_original.parent_path();
        stem = m_original.stem().u8string() + " - Dũng Office";
    }
    fs::path p = dir / fs::u8path(stem + "." + ext);
    for (int n = 2; fs::exists(longPath(p)) || (!isNew() && samePath(p, m_original)); ++n)
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
    if (!isPdf && !m_featureLoss.empty() && !opts.allowFeatureLoss) {
        r.message = "Bản lưu sẽ KHÔNG giữ các thành phần sau của tệp gốc:\n" + describeFeatureLoss(m_featureLoss) +
                    "Cần xác nhận trước khi lưu. Tệp gốc sẽ không bị thay đổi.";
        return r;
    }
    const bool targetIsOriginal = !isNew() && samePath(target, m_original);
    if (targetIsOriginal && !opts.allowOverwriteOriginal) {
        r.message = "Để bảo vệ dữ liệu, Dũng Office không ghi đè tệp gốc. Hãy dùng \"Lưu thành\" với tên mới.";
        return r;
    }
    const bool exists = fs::exists(longPath(target), ec);
    bool ownOutput = false; // tệp do chính phiên này ghi ra trước đó -> ghi lại không cần hỏi/sao lưu
    for (const auto& o : m_sessionOutputs)
        if (samePath(o, target)) ownOutput = true;
    if (exists && !targetIsOriginal && !ownOutput && !opts.allowReplaceExisting) {
        r.message = "Tệp đích đã tồn tại. Cần xác nhận thay thế.";
        return r;
    }
    if (!fs::is_directory(longPath(target.parent_path()), ec)) {
        r.message = "Thư mục đích không tồn tại: " + toUtf8(target.parent_path());
        return r;
    }
    const size_t targetChars = pathChars(target);
    const std::string longNote = targetChars >= 240
        ? " (đường dẫn đích dài " + std::to_string(targetChars) + " ký tự; hãy lưu vào thư mục có đường dẫn ngắn hơn)" : std::string();

    // --- Ghi ra tệp tạm, kiểm tra, rồi mới thay thế nguyên tử ---
    // Engine ghi vào thư mục tạm NGẮN (engine không ghi được đường dẫn từ 260 ký tự), sau đó chép sang tệp tạm
    // cùng thư mục với đích (dùng tiền tố \\?\ khi đường dẫn dài) và đổi tên để thay thế đích.
    fs::path stage = m_workDir / fs::u8path("~luu-" + randomToken() + "." + ext);
    const std::string stageUrl = toFileUrl(stage);
    bool ok = onEngine([&] { return m_doc->saveAs(stageUrl.c_str(), ext.c_str(), nullptr); });
    if (!ok) {
        fs::remove(stage, ec);
        r.message = "Engine không ghi được tệp.";
        return r;
    }
    std::string why;
    if (!looksValid(stage, ext, why)) {
        fs::remove(stage, ec);
        r.message = "Tệp ghi ra không hợp lệ (" + why + "). Tệp đích chưa bị thay đổi.";
        return r;
    }
    if (exists && !ownOutput) {
        r.backup = backupPathFor(target);
        fs::copy_file(longPath(target), longPath(r.backup), fs::copy_options::none, ec);
        if (ec) {
            fs::remove(stage, ec);
            r.message = "Không tạo được bản sao lưu trước khi thay thế. Đã huỷ để bảo vệ dữ liệu." + longNote;
            r.backup.clear();
            return r;
        }
    }
    fs::path sibling = target.parent_path() / fs::u8path("~dung-" + randomToken() + "." + ext);
    fs::copy_file(longPath(stage), longPath(sibling), fs::copy_options::none, ec);
    if (ec) {
        fs::remove(longPath(sibling), ec);
        fs::remove(stage, ec);
        r.message = "Không ghi được vào thư mục đích: " + ec.message() + "." + longNote + " Tệp đích chưa bị thay đổi.";
        return r;
    }
    fs::rename(longPath(sibling), longPath(target), ec); // thay thế nguyên tử trong cùng thư mục
    if (ec) {
        const std::string em = ec.message();
        fs::remove(longPath(sibling), ec);
        fs::remove(stage, ec);
        r.message = "Không đưa được tệp vào vị trí đích: " + em + "." + longNote;
        return r;
    }
    fs::remove(stage, ec);
    r.ok = true;
    r.written = target;
    r.message = "Đã lưu: " + toUtf8(target.filename());
    if (!isPdf) m_modified = false;
    if (!ownOutput) m_sessionOutputs.push_back(target);
    return r;
}

} // namespace dung
