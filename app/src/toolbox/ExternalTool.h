// SPDX-License-Identifier: MPL-2.0
// Tool ngoài: một thư mục có tool.json trong thư mục dữ liệu người dùng (…/DungOffice/tools/<tên>/).
// Dự án KHÔNG đóng gói Python hay AutoHotkey: tool gọi chương trình do NGƯỜI DÙNG tự cài (python, AutoHotkey64.exe...).
// Chỉ cần QtCore để kiểm thử được không cần giao diện.
//
// An toàn:
//  - Không có shell: lệnh chạy bằng QProcess với danh sách đối số riêng, giá trị nhập không thể "chèn lệnh".
//  - Lệnh chỉ là tên chương trình trần (tìm trong PATH) hoặc đường dẫn TƯƠNG ĐỐI nằm trong thư mục của tool.
//    Đường dẫn tuyệt đối, "..", ổ đĩa, liên kết tượng trưng thoát thư mục đều bị từ chối.
//  - Không cho gọi trực tiếp cmd/powershell/sh/bash/wscript/mshta..., không cho .bat/.cmd/.lnk.
//  - Lần đầu mỗi tool phải được người dùng xác nhận; xác nhận gắn với dấu vân tay (SHA-256) của TOÀN BỘ
//    thư mục tool, nên sửa script/tool.json là phải xác nhận lại. Chưa xác nhận thì prepare() từ chối.
#pragma once
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QStringDecoder>

class QProcess;
class QTimer;

namespace toolbox {

struct FieldSpec {
    QString id;
    QString label;
    QString type = QStringLiteral("text"); // text | multiline | number | file | folder | choice
    QString hint;
    QString def;
    bool required = false;
    QStringList choices;
    bool hasMin = false, hasMax = false;
    double min = 0, max = 0;
    int maxLen = 4000;
};

struct ToolSpec {
    QString id, name, description, category, version;
    QString command;      // tên trần hoặc đường dẫn tương đối trong thư mục tool (có '/')
    QStringList args;     // mẫu đối số, có thể chứa {mã_trường} và {tooldir}
    QString workingDir = QStringLiteral(".");
    int timeoutSec = 60;
    QList<FieldSpec> fields;
    QString dir;          // thư mục tool (đường dẫn chuẩn), do loadToolDir điền
    QString fingerprint;  // SHA-256 hex của toàn thư mục, do loadToolDir điền
};

struct ParseResult {
    bool ok = false;
    QString error; // tiếng Việt, nói rõ lỗi nằm ở đâu
    ToolSpec spec;
};

// Kiểm tra cú pháp + quy tắc an toàn của tool.json. toolDir chỉ dùng để gắn vào spec.dir (có thể rỗng khi thử).
ParseResult parseManifest(const QByteArray& json, const QString& toolDir = QString());
// Đường dẫn tương đối hợp lệ trong thư mục tool? (không tuyệt đối, không "..", không ổ đĩa/dấu '\')
bool isSafeRelativePath(const QString& rel, QString* why = nullptr);

// Đọc <root>/<folder>/tool.json, kiểm tra thư mục nằm trong root, không có liên kết tượng trưng,
// lệnh tương đối có thật và nằm trong thư mục tool; tính dấu vân tay.
ParseResult loadToolDir(const QString& root, const QString& folder);

struct ScanResult {
    QList<ToolSpec> tools;
    QStringList errors; // "<thư mục>: <lỗi>" — để người dùng biết tool nào bị bỏ qua và vì sao
};
ScanResult scan(const QString& root);
QString toolsRoot(); // <thư mục dữ liệu>/tools (tự tạo)

// Xác nhận lần đầu
bool isConfirmed(const ToolSpec& s);
void setConfirmed(const ToolSpec& s);
void revokeConfirmation(const QString& id);

struct Command {
    QString program; // đường dẫn tuyệt đối của chương trình
    QStringList args;
    QString workDir;
    int timeoutSec = 60;
};
struct Prepared {
    bool ok = false;
    QString error;
    Command cmd;
};
// Kiểm tra: đã xác nhận, thư mục tool chưa đổi kể từ lúc quét, giá trị nhập hợp lệ; rồi ghép đối số.
// Trường tuỳ chọn để trống mà đối số chỉ gồm đúng {trường đó} thì đối số bị bỏ.
Prepared prepare(const ToolSpec& s, const QMap<QString, QString>& values);
// Mô tả lệnh để hiện trong hộp thoại xác nhận (chưa thay giá trị).
QString describeCommand(const ToolSpec& s);

// Chạy một lệnh đã chuẩn bị: ghi log, hết giờ tự dừng, có thể dừng tay.
class ToolRunner : public QObject {
    Q_OBJECT
public:
    explicit ToolRunner(QObject* parent = nullptr);
    ~ToolRunner() override;
    void setToolId(const QString& id) { m_id = id; }
    bool start(const Command& cmd, QString* error);
    void stop();
    bool running() const;
    static QString logPath();

signals:
    void output(const QString& text, bool isError);
    void finished(int exitCode, bool timedOut, bool stopped, bool crashed);

private:
    void onFinished(int code, int status);
    void logLine(const QString& s) const;
    QProcess* m_proc = nullptr;
    QTimer* m_timeout = nullptr;
    QTimer* m_kill = nullptr;
    QString m_id;
    bool m_timedOut = false, m_stopped = false, m_truncated = false;
    qint64 m_bytes = 0;
    qint64 m_startMs = 0;
    QStringDecoder m_decOut{QStringConverter::Utf8}; // giải mã UTF-8 theo luồng (không cắt đôi ký tự giữa hai lần đọc)
    QStringDecoder m_decErr{QStringConverter::Utf8};
};

} // namespace toolbox
