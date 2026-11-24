// SPDX-License-Identifier: MPL-2.0
#include "PdfOps.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTemporaryDir>
#include <QPair>
#include <QProcess>
#include <QRandomGenerator>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cstdio>
#endif

namespace dungpdf {

namespace {

QString tr(const char* s) { return QString::fromUtf8(s); }

struct Run {
    bool missing = false;  // không có qpdf
    int code = -1;         // mã thoát của qpdf; -1 = không chạy được/bị lỗi; -2 = quá giờ
    QString out;
    QString err;
};

Run runQpdf(const QStringList& args) {
    Run r;
    const QString prog = findQpdf();
    if (prog.isEmpty()) {
        r.missing = true;
        return r;
    }
    QProcess p;
    p.setProgram(prog);
    p.setArguments(args);
    p.start();
    if (!p.waitForStarted(15000)) {
        r.err = p.errorString();
        return r;
    }
    if (!p.waitForFinished(10 * 60 * 1000)) {
        p.kill();
        p.waitForFinished(3000);
        r.code = -2;
        r.err = tr("quá thời gian chờ");
        return r;
    }
    r.code = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    r.out = QString::fromUtf8(p.readAllStandardOutput());
    r.err = QString::fromUtf8(p.readAllStandardError());
    return r;
}

QString firstLine(const QString& s) {
    const QString t = s.trimmed();
    const int nl = t.indexOf(QLatin1Char('\n'));
    return nl < 0 ? t : t.left(nl).trimmed();
}

bool mentionsPassword(const QString& err) { return err.contains(QLatin1String("password"), Qt::CaseInsensitive); }

// Mật khẩu đưa cho qpdf bằng --password=. Đã thử --password-file và @tệp: qpdf 12.3.2 trên Windows không
// nhận được (thoát mã 2). Hạn chế: tiến trình cùng người dùng có thể thấy mật khẩu trên dòng lệnh trong lúc qpdf chạy.
class PasswordFile {
public:
    explicit PasswordFile(const QString& password) {
        if (!password.isEmpty()) m_args << QStringLiteral("--password=") + password;
    }
    // Khi chỉ đọc thông tin thì không thêm --decrypt (qpdf bản mới từ chối tuỳ chọn biến đổi đi kèm tuỳ chọn kiểm tra).
    QStringList args(bool decrypt = true) const {
        QStringList a = m_args;
        if (decrypt && !a.isEmpty()) a << QStringLiteral("--decrypt");
        return a;
    }

private:
    QStringList m_args;
};

bool looksLikePdf(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    if (f.read(5) != "%PDF-") return false;
    const qint64 size = f.size();
    f.seek(qMax<qint64>(0, size - 2048));
    return f.readAll().contains("%%EOF");
}

bool sameFile(const QString& a, const QString& b) {
    const QFileInfo fa(a), fb(b);
    if (fa.exists() && fb.exists()) return fa.canonicalFilePath() == fb.canonicalFilePath();
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    return QDir::cleanPath(fa.absoluteFilePath()).compare(QDir::cleanPath(fb.absoluteFilePath()), cs) == 0;
}

QString randomHex() { return QString::number(QRandomGenerator::global()->generate(), 16).rightJustified(8, QLatin1Char('0')); }

QString backupPathFor(const QString& path) {
    const QFileInfo fi(path);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString base = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".dung-backup-") + stamp;
    const QString ext = fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix();
    QString cand = base + ext;
    for (int n = 1; QFileInfo::exists(cand); ++n) cand = base + QLatin1Char('-') + QString::number(n) + ext;
    return cand;
}

bool replaceFile(const QString& from, const QString& to) {
#ifdef Q_OS_WIN
    return MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(from).utf16()),
                       reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(to).utf16()),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return std::rename(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0;
#endif
}

// Chuyển lỗi của qpdf thành thông báo tiếng Việt. Trả true nếu chạy được (mã 0 hoặc 3 = có cảnh báo).
bool judge(const Run& r, Result& res) {
    if (r.missing) {
        res.error = tr("Không tìm thấy qpdf (bộ công cụ PDF). Hãy cài lại Dũng Office.");
        return false;
    }
    if (r.code == 0) return true;
    if (r.code == 3) {
        res.warnings << tr("qpdf cảnh báo: ") + firstLine(r.err);
        return true;
    }
    if (r.code == -2) {
        res.error = tr("Thao tác PDF quá thời gian chờ và đã bị dừng.");
        return false;
    }
    if (mentionsPassword(r.err)) {
        res.needPassword = true;
        res.error = tr("Tệp được bảo vệ bằng mật khẩu. Nhập mật khẩu đúng để tiếp tục.");
        return false;
    }
    res.error = tr("Không xử lý được PDF (tệp có thể hỏng): ") + firstLine(r.err);
    return false;
}

// Gom danh sách chỉ số 0-based thành chuỗi trang kiểu qpdf: "1,3,5-7".
QString rangeFromIndexes(const QList<int>& idx) {
    QStringList parts;
    int i = 0;
    while (i < idx.size()) {
        int j = i;
        while (j + 1 < idx.size() && idx[j + 1] == idx[j] + 1) ++j;
        parts << (j > i ? QStringLiteral("%1-%2").arg(idx[i] + 1).arg(idx[j] + 1) : QString::number(idx[i] + 1));
        i = j + 1;
    }
    return parts.join(QLatin1Char(','));
}

// Quy trình ghi an toàn dùng chung. `makeArgs(tmp)` dựng đối số qpdf ghi ra `tmp`.
template <class MakeArgs>
Result writeSafely(const QStringList& sources, const QString& destIn, bool allowReplace, MakeArgs makeArgs) {
    Result res;
    const QString dest = QFileInfo(destIn).absoluteFilePath();
    const QFileInfo di(dest);
    if (!QDir(di.absolutePath()).exists()) {
        res.error = tr("Thư mục đích không tồn tại: ") + QDir::toNativeSeparators(di.absolutePath());
        return res;
    }
    for (const QString& s : sources) {
        if (sameFile(s, dest)) {
            res.error = tr("Không được lưu đè lên tệp gốc. Hãy chọn một tên khác.");
            return res;
        }
    }
    const bool exists = di.exists();
    if (exists && !allowReplace) {
        res.error = tr("Tệp đích đã tồn tại. Chọn tên khác hoặc xác nhận thay thế.");
        return res;
    }

    const QString tmp = di.absolutePath() + QStringLiteral("/~dung-") + randomHex() + QLatin1Char('-') + di.fileName();
    struct Cleanup {
        QString path;
        ~Cleanup() { if (!path.isEmpty() && QFileInfo::exists(path)) QFile::remove(path); }
    } cleanup{tmp};

    if (!judge(runQpdf(makeArgs(tmp)), res)) return res;
    if (!looksLikePdf(tmp)) {
        res.error = tr("Tệp tạm ghi ra không hợp lệ, đã hủy. Tệp gốc không thay đổi.");
        return res;
    }
    const Run check = runQpdf({QStringLiteral("--check"), tmp});
    if (check.code != 0 && check.code != 3) {
        res.error = tr("Kết quả không qua bước kiểm tra, đã hủy. Tệp gốc không thay đổi.");
        return res;
    }
    if (exists) {
        const QString backup = backupPathFor(dest);
        if (!QFile::copy(dest, backup)) {
            res.error = tr("Không sao lưu được tệp cũ nên chưa thay thế. Tệp cũ giữ nguyên.");
            return res;
        }
        res.backup = backup;
    }
    if (!replaceFile(tmp, dest)) {
        res.error = tr("Không ghi được tệp đích (đang mở ở chương trình khác hoặc không đủ quyền).");
        return res;
    }
    res.ok = true;
    res.files << dest;
    return res;
}

}  // namespace

QString findQpdf() {
    const QString env = qEnvironmentVariable("DUNG_QPDF");
    if (!env.isEmpty() && QFileInfo::exists(env)) return env;
#ifdef Q_OS_WIN
    const QString exe = QStringLiteral("qpdf.exe");
#else
    const QString exe = QStringLiteral("qpdf");
#endif
    if (QCoreApplication::instance()) {
        const QString local = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("qpdf/") + exe);
        if (QFileInfo::exists(local)) return local;
    }
    return QStandardPaths::findExecutable(QStringLiteral("qpdf"));
}

QList<int> parsePages(const QString& spec, int total, QString* error) {
    auto fail = [&](const QString& msg) {
        if (error) *error = msg;
        return QList<int>();
    };
    QList<int> out;
    QString s = spec;
    s.remove(QLatin1Char(' '));
    const QStringList parts = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        int start = 0, end = 0;
        bool ok1 = true, ok2 = true;
        const int dash = part.indexOf(QLatin1Char('-'));
        if (dash >= 0) {
            const QString a = part.left(dash), b = part.mid(dash + 1);
            start = a.isEmpty() ? 1 : a.toInt(&ok1);
            end = b.isEmpty() ? total : b.toInt(&ok2);
        } else {
            start = end = part.toInt(&ok1);
        }
        if (!ok1 || !ok2) return fail(tr("Khoảng trang không hợp lệ: ") + part);
        if (start < 1 || end > total || start > end)
            return fail(tr("Khoảng trang không hợp lệ: ") + part + tr(" (tài liệu có %1 trang)").arg(total));
        for (int i = start; i <= end; ++i) out << (i - 1);
    }
    if (out.isEmpty()) return fail(tr("Chưa chọn trang nào."));
    return out;
}

Info info(const QString& path, const QString& password) {
    Info inf;
    const QFileInfo fi(path);
    if (!fi.exists()) {
        inf.error = tr("Không tìm thấy tệp: ") + QDir::toNativeSeparators(path);
        return inf;
    }
    inf.bytes = fi.size();
    PasswordFile pw(password);
    QStringList args = pw.args(false);
    args << QStringLiteral("--show-npages") << path;
    Result tmp;
    const Run r = runQpdf(args);
    if (!judge(r, tmp)) {
        inf.error = tmp.error;
        inf.needPassword = tmp.needPassword;
        return inf;
    }
    if (!tmp.warnings.isEmpty()) inf.warning = tmp.warnings.first();
    inf.pages = r.out.trimmed().toInt();
    QStringList enc = pw.args(false);
    enc << QStringLiteral("--is-encrypted") << path;
    inf.encrypted = runQpdf(enc).code == 0;
    inf.ok = inf.pages > 0;
    if (!inf.ok) inf.error = tr("Không đọc được số trang của PDF.");
    return inf;
}

Result merge(const QList<Input>& inputs, const QString& dest, bool allowReplace) {
    if (inputs.size() < 2) {
        Result r;
        r.error = tr("Cần ít nhất hai tệp để gộp.");
        return r;
    }
    QStringList sources;
    bool anyPassword = false;
    for (const Input& in : inputs) {
        sources << in.path;
        anyPassword = anyPassword || !in.password.isEmpty();
    }
    Result res = writeSafely(sources, dest, allowReplace, [&](const QString& tmp) {
        QStringList a{QStringLiteral("--empty")};
        if (anyPassword) a << QStringLiteral("--decrypt");
        a << QStringLiteral("--pages");
        for (const Input& in : inputs) {
            a << in.path;
            if (!in.password.isEmpty()) a << QStringLiteral("--password=") + in.password;
        }
        a << QStringLiteral("--") << tmp;
        return a;
    });
    if (res.ok && anyPassword) res.warnings << tr("Kết quả không còn mật khẩu.");
    return res;
}

Result extract(const Input& in, const QString& pagesSpec, const QString& dest, bool allowReplace) {
    Result res;
    const Info inf = info(in.path, in.password);
    if (!inf.ok) {
        res.error = inf.error;
        res.needPassword = inf.needPassword;
        return res;
    }
    QString perr;
    const QList<int> idx = parsePages(pagesSpec, inf.pages, &perr);
    if (idx.isEmpty()) {
        res.error = perr;
        return res;
    }
    PasswordFile pw(in.password);
    res = writeSafely({in.path}, dest, allowReplace, [&](const QString& tmp) {
        QStringList a = pw.args();
        a << in.path << QStringLiteral("--pages") << QStringLiteral(".") << rangeFromIndexes(idx) << QStringLiteral("--") << tmp;
        return a;
    });
    if (res.ok && !in.password.isEmpty()) res.warnings << tr("Kết quả không còn mật khẩu.");
    return res;
}

Result splitEach(const Input& in, const QString& outDir) {
    Result res;
    const Info inf = info(in.path, in.password);
    if (!inf.ok) {
        res.error = inf.error;
        res.needPassword = inf.needPassword;
        return res;
    }
    const QDir dir(outDir);
    if (!dir.exists()) {
        res.error = tr("Thư mục đích không tồn tại: ") + QDir::toNativeSeparators(outDir);
        return res;
    }
    const QString stem = QFileInfo(in.path).completeBaseName();
    const int width = qMax(3, QString::number(inf.pages).size());
    QStringList finals;
    for (int i = 1; i <= inf.pages; ++i) {
        const QString name = QStringLiteral("%1-trang%2.pdf").arg(stem, QString::number(i).rightJustified(width, QLatin1Char('0')));
        const QString full = dir.filePath(name);
        if (QFileInfo::exists(full)) {
            res.error = tr("Đã có tệp trùng tên, không ghi đè: ") + name + tr(". Chọn thư mục khác.");
            return res;
        }
        finals << full;
    }

    const QString tmpName = QStringLiteral("~dung-split-") + randomHex();
    if (!dir.mkdir(tmpName)) {
        res.error = tr("Không tạo được thư mục tạm trong thư mục đích.");
        return res;
    }
    const QString tmpDir = dir.filePath(tmpName);
    struct Cleanup {
        QString path;
        ~Cleanup() { QDir(path).removeRecursively(); }
    } cleanup{tmpDir};

    PasswordFile pw(in.password);
    QStringList args = pw.args();
    args << in.path << QStringLiteral("--split-pages=1") << QDir(tmpDir).filePath(QStringLiteral("p.pdf"));
    if (!judge(runQpdf(args), res)) return res;

    QList<QPair<int, QString>> produced;
    static const QRegularExpression re(QStringLiteral("^p-(\\d+)\\.pdf$"));
    for (const QString& f : QDir(tmpDir).entryList({QStringLiteral("p-*.pdf")}, QDir::Files)) {
        const auto m = re.match(f);
        if (m.hasMatch()) produced << qMakePair(m.captured(1).toInt(), QDir(tmpDir).filePath(f));
    }
    std::sort(produced.begin(), produced.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    if (produced.size() != inf.pages) {
        res.error = tr("Số trang tách ra (%1) không khớp số trang gốc (%2), đã hủy.").arg(produced.size()).arg(inf.pages);
        return res;
    }
    for (const auto& p : produced) {
        if (!looksLikePdf(p.second)) {
            res.error = tr("Một trang tách ra không hợp lệ, đã hủy. Tệp gốc không thay đổi.");
            return res;
        }
    }
    QStringList moved;
    for (int i = 0; i < produced.size(); ++i) {
        if (!QFile::rename(produced[i].second, finals[i])) {
            for (const QString& m : moved) QFile::remove(m);  // chỉ xóa tệp do chính lần này tạo
            res.error = tr("Không ghi được tệp: ") + QFileInfo(finals[i]).fileName();
            return res;
        }
        moved << finals[i];
    }
    res.ok = true;
    res.files = finals;
    if (!in.password.isEmpty()) res.warnings << tr("Kết quả không còn mật khẩu.");
    return res;
}

Result rotate(const Input& in, const QString& pagesSpec, int degrees, const QString& dest, bool allowReplace) {
    Result res;
    if (degrees % 90 != 0) {
        res.error = tr("Chỉ xoay theo bội số của 90 độ.");
        return res;
    }
    const int deg = ((degrees % 360) + 360) % 360;
    if (deg == 0) {
        res.error = tr("Góc xoay bằng 0 độ, không có gì để làm.");
        return res;
    }
    const Info inf = info(in.path, in.password);
    if (!inf.ok) {
        res.error = inf.error;
        res.needPassword = inf.needPassword;
        return res;
    }
    QString perr;
    const QList<int> idx = parsePages(pagesSpec, inf.pages, &perr);
    if (idx.isEmpty()) {
        res.error = perr;
        return res;
    }
    PasswordFile pw(in.password);
    res = writeSafely({in.path}, dest, allowReplace, [&](const QString& tmp) {
        QStringList a = pw.args();
        a << QStringLiteral("--rotate=+%1:%2").arg(deg).arg(rangeFromIndexes(idx)) << in.path << tmp;
        return a;
    });
    if (res.ok && !in.password.isEmpty()) res.warnings << tr("Kết quả không còn mật khẩu.");
    return res;
}

Result compress(const Input& in, const QString& dest, bool allowReplace) {
    Result res;
    const Info inf = info(in.path, in.password);
    if (!inf.ok) {
        res.error = inf.error;
        res.needPassword = inf.needPassword;
        return res;
    }
    PasswordFile pw(in.password);
    res = writeSafely({in.path}, dest, allowReplace, [&](const QString& tmp) {
        QStringList a = pw.args();
        // Không nén lại ảnh: không mất chất lượng.
        a << QStringLiteral("--object-streams=generate") << QStringLiteral("--compress-streams=y")
          << QStringLiteral("--recompress-flate") << QStringLiteral("--compression-level=9") << in.path << tmp;
        return a;
    });
    if (res.ok) {
        if (QFileInfo(res.files.first()).size() >= inf.bytes)
            res.warnings << tr("Bản nén không nhỏ hơn bản gốc: tài liệu đã gọn hoặc chủ yếu là ảnh.");
        if (!in.password.isEmpty()) res.warnings << tr("Kết quả không còn mật khẩu.");
    }
    return res;
}

// ======================================================================================
// Đánh số trang, đóng dấu, ảnh -> PDF, trang trắng
// ======================================================================================
namespace {

QByteArray num(double v) { return QByteArray::number(v, 'f', 3); }
QByteArray num(qint64 v) { return QByteArray::number(v); }

// Bộ dựng PDF tối giản (bảng xref đúng). Đối tượng đánh số từ 1.
class Builder {
public:
    int reserve() { m_b.append(QByteArray()); return m_b.size(); }
    void set(int id, const QByteArray& body) { m_b[id - 1] = body; }
    int add(const QByteArray& body) { m_b.append(body); return m_b.size(); }
    qint64 bytes() const { qint64 n = 0; for (const QByteArray& b : m_b) n += b.size(); return n; }
    QByteArray finish(int root) const {
        QByteArray out = "%PDF-1.5\n%\xE2\xE3\xCF\xD3\n";
        QList<qint64> offs;
        for (int i = 0; i < m_b.size(); ++i) {
            offs << out.size();
            out += QByteArray::number(i + 1) + " 0 obj\n" + m_b[i] + "\nendobj\n";
        }
        const qint64 x = out.size();
        out += "xref\n0 " + QByteArray::number(m_b.size() + 1) + "\n0000000000 65535 f \n";
        for (qint64 o : offs) {
            char line[40];
            std::snprintf(line, sizeof line, "%010lld 00000 n \n", static_cast<long long>(o));
            out += line;
        }
        out += "trailer\n<< /Size " + QByteArray::number(m_b.size() + 1) + " /Root " + QByteArray::number(root) +
               " 0 R >>\nstartxref\n" + QByteArray::number(x) + "\n%%EOF\n";
        return out;
    }

private:
    QList<QByteArray> m_b;
};

QByteArray streamObj(const QByteArray& dictExtra, const QByteArray& data) {
    return "<< " + dictExtra + " /Length " + QByteArray::number(data.size()) + " >>\nstream\n" + data + "\nendstream";
}

// Độ rộng ký tự Helvetica (1/1000 em) cho ASCII 32..126.
const int kHelv[95] = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,  // 32..47
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,  // 48..63
    1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778, // 64..79
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,  // 80..95
    333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,  // 96..111
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584};      // 112..126

double textWidth(const QByteArray& t, double fs) {
    double w = 0;
    for (char ch : t) w += kHelv[static_cast<unsigned char>(ch) - 32];
    return w * fs / 1000.0;
}

bool isPrintableAscii(const QString& s) {
    for (QChar c : s)
        if (c.unicode() < 32 || c.unicode() > 126) return false;
    return true;
}

QByteArray pdfString(const QByteArray& t) {
    QByteArray o = "(";
    for (char ch : t) {
        if (ch == '(' || ch == ')' || ch == '\\') o += '\\';
        o += ch;
    }
    return o + ")";
}

// ---- ảnh
struct ImageObj {
    int w = 0, h = 0, comps = 0;
    QByteArray filter;  // "/DCTDecode" hoặc "/FlateDecode"
    QByteArray data;
    QByteArray alpha;   // luồng Flate của kênh alpha (xám 8 bit) nếu có
    int orient = 1;     // EXIF
};

int exifOrientation(const uchar* t, int len) {
    if (len < 16) return 1;
    const bool le = t[0] == 'I' && t[1] == 'I';
    if (!le && !(t[0] == 'M' && t[1] == 'M')) return 1;
    auto u16 = [&](int o) { return le ? (t[o] | (t[o + 1] << 8)) : ((t[o] << 8) | t[o + 1]); };
    auto u32 = [&](int o) {
        return le ? (quint32(t[o]) | quint32(t[o + 1]) << 8 | quint32(t[o + 2]) << 16 | quint32(t[o + 3]) << 24)
                  : (quint32(t[o]) << 24 | quint32(t[o + 1]) << 16 | quint32(t[o + 2]) << 8 | quint32(t[o + 3]));
    };
    const quint32 ifd = u32(4);
    if (ifd + 2 > quint32(len)) return 1;
    const int n = u16(int(ifd));
    for (int i = 0; i < n; ++i) {
        const quint32 e = ifd + 2 + quint32(i) * 12;
        if (e + 12 > quint32(len)) break;
        if (u16(int(e)) == 0x0112) {
            const int v = u16(int(e) + 8);
            return (v >= 1 && v <= 8) ? v : 1;
        }
    }
    return 1;
}

bool loadImage(const QString& path, ImageObj* im, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *err = tr("Không mở được ảnh: ") + QDir::toNativeSeparators(path);
        return false;
    }
    if (f.size() > 300LL * 1024 * 1024) {
        *err = tr("Ảnh quá lớn (trên 300 MB): ") + QFileInfo(path).fileName();
        return false;
    }
    const QByteArray d = f.readAll();
    const QString name = QFileInfo(path).fileName();
    const uchar* p = reinterpret_cast<const uchar*>(d.constData());
    const int n = d.size();
    if (n >= 4 && p[0] == 0xFF && p[1] == 0xD8) {  // JPEG: nhúng nguyên bản
        int i = 2, sof = -1, prec = 0;
        while (i + 4 <= n) {
            if (p[i] != 0xFF) { ++i; continue; }
            const int m = p[i + 1];
            if (m == 0xFF) { ++i; continue; }
            if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { i += 2; continue; }
            if (m == 0xD9 || m == 0xDA) break;
            const int len = (p[i + 2] << 8) | p[i + 3];
            if (len < 2 || i + 2 + len > n) break;
            if (m == 0xE1 && len >= 16 && std::memcmp(p + i + 4, "Exif\0\0", 6) == 0) im->orient = exifOrientation(p + i + 10, len - 8);
            if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
                sof = m;
                prec = p[i + 4];
                im->h = (p[i + 5] << 8) | p[i + 6];
                im->w = (p[i + 7] << 8) | p[i + 8];
                im->comps = p[i + 9];
                break;
            }
            i += 2 + len;
        }
        if (sof < 0 || im->w <= 0 || im->h <= 0) {
            *err = tr("Không đọc được kích thước ảnh JPEG (tệp hỏng?): ") + name;
            return false;
        }
        if (sof > 0xC2 || prec != 8 || (im->comps != 1 && im->comps != 3)) {
            *err = tr("Kiểu JPEG này chưa được hỗ trợ (CMYK, mã hóa số học hoặc không phải 8 bit): ") + name;
            return false;
        }
        im->filter = "/DCTDecode";
        im->data = d;
        return true;
    }
    if (n >= 8 && std::memcmp(p, "\x89PNG\r\n\x1a\n", 8) == 0) {
        QImage img;
        if (!img.loadFromData(d) || img.isNull()) {
            *err = tr("Không đọc được ảnh PNG (tệp hỏng?): ") + name;
            return false;
        }
        if (qint64(img.width()) * img.height() > 120LL * 1000 * 1000) {
            *err = tr("Ảnh quá lớn (trên 120 triệu điểm ảnh): ") + name;
            return false;
        }
        const bool alpha = img.hasAlphaChannel();
        const QImage c = img.convertToFormat(alpha ? QImage::Format_ARGB32 : QImage::Format_RGB888);
        const int w = c.width(), h = c.height();
        QByteArray rgb(w * h * 3, 0), a;
        if (alpha) a.resize(w * h);
        for (int y = 0; y < h; ++y) {
            if (alpha) {
                const QRgb* s = reinterpret_cast<const QRgb*>(c.constScanLine(y));
                for (int x = 0; x < w; ++x) {
                    rgb[(y * w + x) * 3 + 0] = char(qRed(s[x]));
                    rgb[(y * w + x) * 3 + 1] = char(qGreen(s[x]));
                    rgb[(y * w + x) * 3 + 2] = char(qBlue(s[x]));
                    a[y * w + x] = char(qAlpha(s[x]));
                }
            } else {
                std::memcpy(rgb.data() + qsizetype(y) * w * 3, c.constScanLine(y), size_t(w) * 3);
            }
        }
        im->w = w;
        im->h = h;
        im->comps = 3;
        im->filter = "/FlateDecode";
        im->data = qCompress(rgb, 6).mid(4);  // bỏ 4 byte độ dài của qCompress, còn lại là luồng zlib
        if (alpha) im->alpha = qCompress(a, 6).mid(4);
        return true;
    }
    *err = tr("Chỉ nhận ảnh JPEG hoặc PNG (đã kiểm tra theo nội dung tệp): ") + name;
    return false;
}

// Thêm đối tượng ảnh (và SMask nếu có) vào bộ dựng; trả id đối tượng ảnh.
int addImageObject(Builder& b, const ImageObj& im) {
    QByteArray extra;
    if (!im.alpha.isEmpty()) {
        const int sm = b.add(streamObj("/Type /XObject /Subtype /Image /Width " + QByteArray::number(im.w) + " /Height " +
                                           QByteArray::number(im.h) + " /ColorSpace /DeviceGray /BitsPerComponent 8 /Filter /FlateDecode",
                                       im.alpha));
        extra = " /SMask " + QByteArray::number(sm) + " 0 R";
    }
    return b.add(streamObj("/Type /XObject /Subtype /Image /Width " + QByteArray::number(im.w) + " /Height " + QByteArray::number(im.h) +
                               " /ColorSpace " + QByteArray(im.comps == 1 ? "/DeviceGray" : "/DeviceRGB") + " /BitsPerComponent 8 /Filter " + im.filter + extra,
                           im.data));
}

// ---- hình học trang (qua qpdf --json)
struct PageGeom {
    double w = 612, h = 792;  // kích thước nhìn thấy (đã tính xoay)
    bool defaulted = false;
};

QJsonObject dictOf(const QJsonObject& objs, const QString& ref) {
    const QJsonObject o = objs.value(QStringLiteral("obj:") + ref).toObject();
    if (o.contains(QStringLiteral("value"))) return o.value(QStringLiteral("value")).toObject();
    return o.value(QStringLiteral("stream")).toObject().value(QStringLiteral("dict")).toObject();
}

QJsonValue inheritedKey(const QJsonObject& objs, QJsonObject dict, const QString& key) {
    for (int depth = 0; depth < 64; ++depth) {
        if (dict.contains(key)) return dict.value(key);
        const QString parent = dict.value(QStringLiteral("/Parent")).toString();
        if (parent.isEmpty()) break;
        dict = dictOf(objs, parent);
    }
    return QJsonValue();
}

QJsonValue resolveRef(const QJsonObject& objs, const QJsonValue& v) {
    if (v.isString() && v.toString().endsWith(QLatin1String(" R"))) {
        const QJsonObject o = objs.value(QStringLiteral("obj:") + v.toString()).toObject();
        return o.value(QStringLiteral("value"));
    }
    return v;
}

bool boxOf(const QJsonObject& objs, const QJsonObject& page, const QString& key, double r[4]) {
    const QJsonValue v = resolveRef(objs, inheritedKey(objs, page, key));
    if (!v.isArray()) return false;
    const QJsonArray a = v.toArray();
    if (a.size() != 4) return false;
    for (int i = 0; i < 4; ++i) {
        const QJsonValue e = resolveRef(objs, a.at(i));
        if (!e.isDouble()) return false;
        r[i] = e.toDouble();
    }
    return true;
}

bool runJson(const Input& in, const QStringList& extra, QJsonDocument* doc, Result* res) {
    PasswordFile pw(in.password);
    QStringList a = pw.args(false);
    a << QStringLiteral("--json=2") << extra << in.path;
    const Run r = runQpdf(a);
    if (!judge(r, *res)) return false;
    QJsonParseError pe;
    *doc = QJsonDocument::fromJson(r.out.toUtf8(), &pe);
    if (doc->isNull() || !doc->isObject()) {
        res->error = tr("Không đọc được cấu trúc trang của PDF.");
        return false;
    }
    return true;
}

bool pageGeometry(const Input& in, QList<PageGeom>* out, Result* res) {
    QJsonDocument doc;
    if (!runJson(in, {QStringLiteral("--json-key=pages"), QStringLiteral("--json-key=qpdf")}, &doc, res)) return false;
    const QJsonArray pages = doc.object().value(QStringLiteral("pages")).toArray();
    const QJsonArray q = doc.object().value(QStringLiteral("qpdf")).toArray();
    const QJsonObject objs = q.size() > 1 ? q.at(1).toObject() : QJsonObject();
    for (const QJsonValue& pv : pages) {
        const QString ref = pv.toObject().value(QStringLiteral("object")).toString();
        const QJsonObject dict = dictOf(objs, ref);
        PageGeom g;
        double b[4];
        // qpdf đặt lớp phủ theo TrimBox, nếu không có thì CropBox, rồi MediaBox.
        if (boxOf(objs, dict, QStringLiteral("/TrimBox"), b) || boxOf(objs, dict, QStringLiteral("/CropBox"), b) ||
            boxOf(objs, dict, QStringLiteral("/MediaBox"), b)) {
            g.w = std::fabs(b[2] - b[0]);
            g.h = std::fabs(b[3] - b[1]);
        } else {
            g.defaulted = true;
        }
        const QJsonValue rv = resolveRef(objs, inheritedKey(objs, dict, QStringLiteral("/Rotate")));
        const int rot = ((rv.toInt(0) % 360) + 360) % 360;
        if (rot == 90 || rot == 270) std::swap(g.w, g.h);
        if (g.w < 1 || g.h < 1) { g.w = 612; g.h = 792; g.defaulted = true; }
        out->append(g);
    }
    if (out->isEmpty()) {
        res->error = tr("Không đọc được danh sách trang của PDF.");
        return false;
    }
    return true;
}

// Vị trí góc dưới-trái (x, y) của khối rộng bw cao bh theo `pos` (không dùng cho Diagonal).
void placeBox(Position pos, double W, double H, double bw, double bh, double* x, double* y) {
    const double m = 28;
    switch (pos) {
        case Position::TopLeft: *x = m; *y = H - m - bh; break;
        case Position::TopCenter: *x = (W - bw) / 2; *y = H - m - bh; break;
        case Position::TopRight: *x = W - m - bw; *y = H - m - bh; break;
        case Position::BottomLeft: *x = m; *y = m; break;
        case Position::BottomCenter: *x = (W - bw) / 2; *y = m; break;
        case Position::BottomRight: *x = W - m - bw; *y = m; break;
        default: *x = (W - bw) / 2; *y = (H - bh) / 2; break;
    }
}

struct OverlayPage {
    double w, h;
    QByteArray content;
};

// Dựng tệp lớp phủ: mỗi trang có đúng kích thước nhìn thấy của trang gốc.
QByteArray buildOverlay(const QList<OverlayPage>& pages, double opacity, const ImageObj* img) {
    Builder b;
    const int cat = b.reserve(), pagesObj = b.reserve();
    const int font = b.add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
    const int gs = b.add("<< /Type /ExtGState /ca " + num(opacity) + " /CA " + num(opacity) + " >>");
    int imgId = 0;
    if (img) imgId = addImageObject(b, *img);
    QList<int> kids;
    for (const OverlayPage& p : pages) {
        const int c = b.add(streamObj(QByteArray(), p.content.isEmpty() ? QByteArray(" ") : p.content));
        QByteArray res = "<< /Font << /F1 " + QByteArray::number(font) + " 0 R >> /ExtGState << /GS1 " + QByteArray::number(gs) + " 0 R >>";
        if (img) res += " /XObject << /Im1 " + QByteArray::number(imgId) + " 0 R >>";
        res += " >>";
        kids << b.add("<< /Type /Page /Parent " + QByteArray::number(pagesObj) + " 0 R /MediaBox [0 0 " + num(p.w) + " " + num(p.h) +
                      "] /Resources " + res + " /Contents " + QByteArray::number(c) + " 0 R >>");
    }
    QByteArray kidList;
    for (int k : kids) kidList += QByteArray::number(k) + " 0 R ";
    b.set(pagesObj, "<< /Type /Pages /Count " + QByteArray::number(kids.size()) + " /Kids [ " + kidList + "] >>");
    b.set(cat, "<< /Type /Catalog /Pages " + QByteArray::number(pagesObj) + " 0 R >>");
    return b.finish(cat);
}

// Chạy `qpdf in --overlay ov -- tmp` an toàn.
Result applyOverlay(const Input& in, const QByteArray& overlay, const QString& dest, bool allowReplace) {
    Result res;
    QTemporaryDir td;
    if (!td.isValid()) {
        res.error = tr("Không tạo được thư mục tạm.");
        return res;
    }
    const QString ov = td.filePath(QStringLiteral("overlay.pdf"));
    QFile f(ov);
    if (!f.open(QIODevice::WriteOnly) || f.write(overlay) != overlay.size()) {
        res.error = tr("Không ghi được tệp tạm cho lớp phủ.");
        return res;
    }
    f.close();
    PasswordFile pw(in.password);
    res = writeSafely({in.path}, dest, allowReplace, [&](const QString& tmp) {
        QStringList a = pw.args();
        a << in.path << QStringLiteral("--overlay") << ov << QStringLiteral("--") << tmp;
        return a;
    });
    if (res.ok && !in.password.isEmpty()) res.warnings << tr("Kết quả không còn mật khẩu.");
    return res;
}

}  // namespace

Result addPageNumbers(const Input& in, const NumberOptions& opt, const QString& dest, bool allowReplace) {
    Result res;
    if (opt.format.isEmpty() || opt.format.size() > 200 || !isPrintableAscii(opt.format)) {
        res.error = tr("Mẫu số trang chỉ nhận chữ không dấu (ASCII), tối đa 200 ký tự. "
                       "Chữ có dấu tiếng Việt chưa hỗ trợ khi đánh số trang.");
        return res;
    }
    if (opt.fontSize < 4 || opt.fontSize > 72 || opt.gray < 0 || opt.gray > 1) {
        res.error = tr("Cỡ chữ phải từ 4 đến 72 và độ xám từ 0 đến 1.");
        return res;
    }
    if (opt.pos == Position::Diagonal) {
        res.error = tr("Số trang không đặt theo đường chéo; hãy chọn một trong sáu vị trí góc/giữa.");
        return res;
    }
    const Info inf = info(in.path, in.password);
    if (!inf.ok) {
        res.error = inf.error;
        res.needPassword = inf.needPassword;
        return res;
    }
    QString perr;
    const QList<int> idx = parsePages(opt.pages, inf.pages, &perr);
    if (idx.isEmpty()) {
        res.error = perr;
        return res;
    }
    QList<PageGeom> geo;
    if (!pageGeometry(in, &geo, &res)) return res;
    if (geo.size() != inf.pages) {
        res.error = tr("Số trang đọc được không khớp, đã hủy.");
        return res;
    }
    QHash<int, int> order;  // chỉ số trang -> thứ tự trong phần được chọn
    for (int i = 0; i < idx.size(); ++i) order.insert(idx[i], i);
    const int lastNumber = opt.start + idx.size() - 1;
    QList<OverlayPage> ops;
    QStringList defaulted;
    for (int i = 0; i < geo.size(); ++i) {
        OverlayPage op{geo[i].w, geo[i].h, QByteArray()};
        if (geo[i].defaulted) defaulted << QString::number(i + 1);
        if (order.contains(i)) {
            QString t = opt.format;
            t.replace(QLatin1String("{n}"), QString::number(opt.start + order.value(i)));
            t.replace(QLatin1String("{total}"), QString::number(lastNumber));
            const QByteArray text = t.toLatin1();
            const double tw = textWidth(text, opt.fontSize);
            double x, y;
            placeBox(opt.pos, op.w, op.h, tw, opt.fontSize, &x, &y);
            op.content = num(opt.gray) + " g BT /F1 " + num(opt.fontSize) + " Tf " + num(x) + " " + num(y) + " Td " + pdfString(text) + " Tj ET";
        }
        ops.append(op);
    }
    res = applyOverlay(in, buildOverlay(ops, 1.0, nullptr), dest, allowReplace);
    if (res.ok && !defaulted.isEmpty())
        res.warnings << tr("Không đọc được cỡ giấy của trang %1; đã dùng cỡ Letter nên vị trí số có thể lệch.").arg(defaulted.join(QLatin1Char(',')));
    return res;
}

Result stamp(const Input& in, const StampOptions& opt, const QString& dest, bool allowReplace) {
    Result res;
    const bool useImage = opt.text.isEmpty();
    if (useImage && opt.imagePath.isEmpty()) {
        res.error = tr("Chưa có chữ hoặc ảnh để đóng dấu.");
        return res;
    }
    if (!useImage && (opt.text.size() > 200 || !isPrintableAscii(opt.text))) {
        res.error = tr("Chữ đóng dấu chỉ nhận chữ không dấu (ASCII), tối đa 200 ký tự. "
                       "Với chữ có dấu tiếng Việt hãy đóng dấu bằng ảnh.");
        return res;
    }
    if (opt.opacity <= 0 || opt.opacity > 1 || opt.gray < 0 || opt.gray > 1 || opt.fontSize < 4 || opt.fontSize > 400 ||
        opt.widthPercent < 1 || opt.widthPercent > 100) {
        res.error = tr("Thông số đóng dấu không hợp lệ (độ đậm 0–1, độ xám 0–1, cỡ chữ 4–400, bề rộng ảnh 1–100%).");
        return res;
    }
    ImageObj im;
    if (useImage) {
        QString e;
        if (!loadImage(opt.imagePath, &im, &e)) {
            res.error = e;
            return res;
        }
        if (sameFile(opt.imagePath, dest)) {
            res.error = tr("Không được lưu đè lên ảnh dùng để đóng dấu.");
            return res;
        }
    }
    const Info inf = info(in.path, in.password);
    if (!inf.ok) {
        res.error = inf.error;
        res.needPassword = inf.needPassword;
        return res;
    }
    QString perr;
    const QList<int> idx = parsePages(opt.pages, inf.pages, &perr);
    if (idx.isEmpty()) {
        res.error = perr;
        return res;
    }
    QList<PageGeom> geo;
    if (!pageGeometry(in, &geo, &res)) return res;
    if (geo.size() != inf.pages) {
        res.error = tr("Số trang đọc được không khớp, đã hủy.");
        return res;
    }
    QSet<int> sel;
    for (int i : idx) sel.insert(i);
    const double kPi = 3.14159265358979323846;
    const double c45 = std::cos(kPi / 4), s45 = std::sin(kPi / 4);
    QList<OverlayPage> ops;
    for (int i = 0; i < geo.size(); ++i) {
        OverlayPage op{geo[i].w, geo[i].h, QByteArray()};
        if (sel.contains(i)) {
            if (!useImage) {
                const QByteArray text = opt.text.toLatin1();
                const double tw = textWidth(text, opt.fontSize);
                const QByteArray color = num(opt.gray) + " g /GS1 gs ";
                if (opt.pos == Position::Diagonal) {
                    op.content = "q " + color + num(c45) + " " + num(s45) + " " + num(-s45) + " " + num(c45) + " " + num(op.w / 2) + " " + num(op.h / 2) +
                                 " cm BT /F1 " + num(opt.fontSize) + " Tf " + num(-tw / 2) + " " + num(-opt.fontSize * 0.35) + " Td " + pdfString(text) + " Tj ET Q";
                } else {
                    double x, y;
                    placeBox(opt.pos, op.w, op.h, tw, opt.fontSize, &x, &y);
                    op.content = "q " + color + "BT /F1 " + num(opt.fontSize) + " Tf " + num(x) + " " + num(y) + " Td " + pdfString(text) + " Tj ET Q";
                }
            } else {
                double w = op.w * opt.widthPercent / 100.0;
                double h = w * im.h / im.w;
                if (h > op.h * 0.9) { h = op.h * 0.9; w = h * im.w / im.h; }
                if (opt.pos == Position::Diagonal) {
                    const double a = w * c45, b = w * s45, c = -h * s45, d = h * c45;
                    const double e = op.w / 2 - (a + c) / 2, f = op.h / 2 - (b + d) / 2;
                    op.content = "q /GS1 gs " + num(a) + " " + num(b) + " " + num(c) + " " + num(d) + " " + num(e) + " " + num(f) + " cm /Im1 Do Q";
                } else {
                    double x, y;
                    placeBox(opt.pos, op.w, op.h, w, h, &x, &y);
                    op.content = "q /GS1 gs " + num(w) + " 0 0 " + num(h) + " " + num(x) + " " + num(y) + " cm /Im1 Do Q";
                }
            }
        }
        ops.append(op);
    }
    res = applyOverlay(in, buildOverlay(ops, opt.opacity, useImage ? &im : nullptr), dest, allowReplace);
    if (res.ok && useImage && im.orient != 1) res.warnings << tr("Ảnh có hướng xoay EXIF; hướng này không được áp dụng khi đóng dấu.");
    return res;
}

Result imagesToPdf(const QStringList& images, const ImageOptions& opt, const QString& dest, bool allowReplace) {
    Result res;
    if (images.isEmpty()) {
        res.error = tr("Chưa chọn ảnh nào.");
        return res;
    }
    if (images.size() > 500) {
        res.error = tr("Tối đa 500 ảnh mỗi lần.");
        return res;
    }
    if (opt.dpi < 30 || opt.dpi > 1200) {
        res.error = tr("Độ phân giải phải từ 30 đến 1200 dpi.");
        return res;
    }
    Builder b;
    const int cat = b.reserve(), pagesObj = b.reserve();
    QList<int> kids;
    for (const QString& path : images) {
        ImageObj im;
        QString e;
        if (!loadImage(path, &im, &e)) {
            res.error = e;
            return res;
        }
        // Hướng EXIF: 3, 6, 8 áp dụng bằng /Rotate của trang; các kiểu lật gương chỉ cảnh báo.
        int rot = 0;
        if (im.orient == 3) rot = 180;
        else if (im.orient == 6) rot = 90;
        else if (im.orient == 8) rot = 270;
        else if (im.orient != 1)
            res.warnings << tr("Ảnh \"%1\" có hướng lật gương trong EXIF; không áp dụng, ảnh có thể bị lật.").arg(QFileInfo(path).fileName());
        double PW, PH, x = 0, y = 0, w, h;
        if (opt.fitA4) {
            const double aw = 595.276, ah = 841.89;
            const bool landscapeShown = (rot == 90 || rot == 270) ? (im.w > im.h) == false : (im.w > im.h);
            const double dispW = landscapeShown ? ah : aw, dispH = landscapeShown ? aw : ah;
            PW = (rot == 90 || rot == 270) ? dispH : dispW;
            PH = (rot == 90 || rot == 270) ? dispW : dispH;
            const double s = std::min(PW / im.w, PH / im.h);
            w = im.w * s;
            h = im.h * s;
            x = (PW - w) / 2;
            y = (PH - h) / 2;
        } else {
            PW = w = im.w * 72.0 / opt.dpi;
            PH = h = im.h * 72.0 / opt.dpi;
        }
        const int imgId = addImageObject(b, im);
        const int c = b.add(streamObj(QByteArray(), "q " + num(w) + " 0 0 " + num(h) + " " + num(x) + " " + num(y) + " cm /Im1 Do Q"));
        kids << b.add("<< /Type /Page /Parent " + QByteArray::number(pagesObj) + " 0 R /MediaBox [0 0 " + num(PW) + " " + num(PH) + "] /Rotate " +
                      QByteArray::number(rot) + " /Resources << /XObject << /Im1 " + QByteArray::number(imgId) + " 0 R >> >> /Contents " +
                      QByteArray::number(c) + " 0 R >>");
        if (b.bytes() > 1500LL * 1024 * 1024) {
            res.error = tr("Tổng dung lượng ảnh quá lớn (trên 1,5 GB); hãy chia thành nhiều lần.");
            return res;
        }
    }
    QByteArray kidList;
    for (int k : kids) kidList += QByteArray::number(k) + " 0 R ";
    b.set(pagesObj, "<< /Type /Pages /Count " + QByteArray::number(kids.size()) + " /Kids [ " + kidList + "] >>");
    b.set(cat, "<< /Type /Catalog /Pages " + QByteArray::number(pagesObj) + " 0 R >>");
    const QByteArray pdf = b.finish(cat);

    QTemporaryDir td;
    if (!td.isValid()) {
        res.error = tr("Không tạo được thư mục tạm.");
        return res;
    }
    const QString scratch = td.filePath(QStringLiteral("raw.pdf"));
    QFile f(scratch);
    if (!f.open(QIODevice::WriteOnly) || f.write(pdf) != pdf.size()) {
        res.error = tr("Không ghi được tệp tạm.");
        return res;
    }
    f.close();
    const QStringList warnKeep = res.warnings;
    res = writeSafely(images, dest, allowReplace, [&](const QString& tmp) {
        // qpdf đọc lại bản dựng, kiểm tra cấu trúc rồi ghi ra tệp tạm chuẩn.
        return QStringList{scratch, tmp};
    });
    if (res.ok) res.warnings = warnKeep + res.warnings;
    return res;
}

namespace {

// Trong luồng nội dung có lệnh vẽ (chữ, đường, ảnh, tô) hay không. Bỏ chuỗi, chú thích và tên.
bool hasPaintingOps(const QByteArray& c) {
    static const QSet<QByteArray> paint = {"S", "s", "f", "F", "f*", "B", "B*", "b", "b*", "sh", "Do", "Tj", "TJ", "'", "\"", "BI"};
    const int n = c.size();
    int i = 0;
    auto isWs = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\0'; };
    auto isDelim = [](char ch) { return std::strchr("()<>[]{}/%", ch) != nullptr; };
    while (i < n) {
        const char ch = c[i];
        if (isWs(ch)) { ++i; continue; }
        if (ch == '%') { while (i < n && c[i] != '\n' && c[i] != '\r') ++i; continue; }
        if (ch == '(') {
            int depth = 1;
            ++i;
            while (i < n && depth > 0) {
                if (c[i] == '\\') ++i;
                else if (c[i] == '(') ++depth;
                else if (c[i] == ')') --depth;
                ++i;
            }
            continue;
        }
        if (ch == '<') {
            if (i + 1 < n && c[i + 1] == '<') { i += 2; continue; }
            while (i < n && c[i] != '>') ++i;
            ++i;
            continue;
        }
        if (ch == '>' || ch == '[' || ch == ']' || ch == '{' || ch == '}') { ++i; continue; }
        if (ch == '/') {
            ++i;
            while (i < n && !isWs(c[i]) && !isDelim(c[i])) ++i;
            continue;
        }
        int j = i;
        while (j < n && !isWs(c[j]) && !isDelim(c[j])) ++j;
        if (j == i) { ++i; continue; }
        if (paint.contains(c.mid(i, j - i))) return true;
        i = j;
    }
    return false;
}

int objNumber(const QString& ref) { return ref.section(QLatin1Char(' '), 0, 0).toInt(); }

}  // namespace

BlankReport findBlankPages(const Input& in) {
    BlankReport br;
    br.notes << tr("Chỉ phát hiện trang không có nội dung. Trang trông trắng nhưng có ảnh quét hoặc ô tô nền trắng "
                   "KHÔNG được phát hiện.");
    const Info inf = info(in.path, in.password);
    if (!inf.ok) {
        br.error = inf.error;
        br.needPassword = inf.needPassword;
        return br;
    }
    br.pages = inf.pages;
    Result tmp;
    QJsonDocument doc;
    if (!runJson(in, {QStringLiteral("--json-key=pages"), QStringLiteral("--json-key=qpdf")}, &doc, &tmp)) {
        br.error = tmp.error;
        br.needPassword = tmp.needPassword;
        return br;
    }
    const QJsonArray pages = doc.object().value(QStringLiteral("pages")).toArray();
    const QJsonArray q = doc.object().value(QStringLiteral("qpdf")).toArray();
    const QJsonObject objs = q.size() > 1 ? q.at(1).toObject() : QJsonObject();
    if (pages.size() != inf.pages) {
        br.error = tr("Số trang đọc được không khớp, đã hủy.");
        return br;
    }
    QList<QList<int>> contents;
    QList<bool> annots;
    QSet<int> need;
    for (const QJsonValue& pv : pages) {
        const QJsonObject po = pv.toObject();
        QList<int> ids;
        for (const QJsonValue& c : po.value(QStringLiteral("contents")).toArray()) {
            const int id = objNumber(c.toString());
            if (id > 0) { ids << id; need.insert(id); }
        }
        contents << ids;
        const QJsonObject dict = dictOf(objs, po.value(QStringLiteral("object")).toString());
        const QJsonValue an = resolveRef(objs, dict.value(QStringLiteral("/Annots")));
        annots << (an.isArray() && !an.toArray().isEmpty());
    }
    // Lấy dữ liệu luồng (đã giải nén) theo từng lô để dòng lệnh không quá dài.
    QHash<int, QByteArray> data;
    QSet<int> failed;
    const QList<int> ids = QList<int>(need.begin(), need.end());
    for (int off = 0; off < ids.size(); off += 100) {
        QStringList extra{QStringLiteral("--json-key=qpdf"), QStringLiteral("--json-stream-data=inline")};
        for (int k = off; k < qMin(off + 100, int(ids.size())); ++k) extra << QStringLiteral("--json-object=%1").arg(ids[k]);
        QJsonDocument d2;
        Result t2;
        if (!runJson(in, extra, &d2, &t2)) {
            br.error = t2.error;
            br.needPassword = t2.needPassword;
            return br;
        }
        const QJsonArray q2 = d2.object().value(QStringLiteral("qpdf")).toArray();
        const QJsonObject o2 = q2.size() > 1 ? q2.at(1).toObject() : QJsonObject();
        for (auto it = o2.begin(); it != o2.end(); ++it) {
            if (!it.key().startsWith(QLatin1String("obj:"))) continue;
            const int id = objNumber(it.key().mid(4));
            const QJsonObject st = it.value().toObject().value(QStringLiteral("stream")).toObject();
            if (st.contains(QStringLiteral("data"))) data.insert(id, QByteArray::fromBase64(st.value(QStringLiteral("data")).toString().toLatin1()));
            else failed.insert(id);
        }
    }
    for (int i = 0; i < contents.size(); ++i) {
        if (annots[i]) continue;
        QByteArray all;
        bool unknown = false;
        for (int id : contents[i]) {
            if (!data.contains(id) || failed.contains(id)) { unknown = true; break; }
            all += data.value(id);
            all += '\n';
        }
        if (unknown) {
            br.notes << tr("Trang %1: không đọc được nội dung nên không kết luận.").arg(i + 1);
            continue;
        }
        if (!hasPaintingOps(all)) br.blank << i;
    }
    br.ok = true;
    return br;
}

Result removeBlankPages(const Input& in, const QString& dest, bool allowReplace) {
    Result res;
    const BlankReport br = findBlankPages(in);
    if (!br.ok) {
        res.error = br.error;
        res.needPassword = br.needPassword;
        return res;
    }
    if (br.blank.isEmpty()) {
        res.error = tr("Không tìm thấy trang trắng (không có nội dung) nào để bỏ.");
        return res;
    }
    if (br.blank.size() >= br.pages) {
        res.error = tr("Mọi trang đều không có nội dung; không bỏ để tránh tạo tệp rỗng.");
        return res;
    }
    QList<int> keep;
    const QSet<int> bl(br.blank.begin(), br.blank.end());
    for (int i = 0; i < br.pages; ++i)
        if (!bl.contains(i)) keep << i;
    PasswordFile pw(in.password);
    res = writeSafely({in.path}, dest, allowReplace, [&](const QString& tmp) {
        QStringList a = pw.args();
        a << in.path << QStringLiteral("--pages") << QStringLiteral(".") << rangeFromIndexes(keep) << QStringLiteral("--") << tmp;
        return a;
    });
    if (res.ok) {
        res.warnings << tr("Đã bỏ %1 trang không có nội dung.").arg(br.blank.size());
        res.warnings << br.notes.first();
        if (!in.password.isEmpty()) res.warnings << tr("Kết quả không còn mật khẩu.");
    }
    return res;
}

QString suggestOutputPath(const QString& sourcePath, const QString& label) {
    const QFileInfo fi(sourcePath);
    const QString name = label.isEmpty() ? QStringLiteral("Dũng Office") : label;
    const QString base = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(" - ") + name;
    QString cand = base + QStringLiteral(".pdf");
    for (int n = 2; QFileInfo::exists(cand); ++n) cand = base + QStringLiteral(" (%1).pdf").arg(n);
    return cand;
}

}  // namespace dungpdf
