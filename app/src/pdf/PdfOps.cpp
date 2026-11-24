// SPDX-License-Identifier: MPL-2.0
#include "PdfOps.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPair>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <algorithm>

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

// Mật khẩu đưa cho qpdf qua tệp tạm (không hiện trên dòng lệnh); tệp bị xóa khi đối tượng hủy.
class PasswordFile {
public:
    explicit PasswordFile(const QString& password) {
        if (password.isEmpty()) return;
        if (m_file.open()) {
            // Tệp tham số của qpdf (@tệp): mỗi dòng một tham số. qpdf 12 trên Windows không nhận --password-file.
            m_file.write("--password=" + password.toUtf8() + "\n");
            m_file.close();
            m_args << QStringLiteral("@") + m_file.fileName();
        }
    }
    // Khi chỉ đọc thông tin thì không thêm --decrypt (qpdf bản mới từ chối tuỳ chọn biến đổi đi kèm tuỳ chọn kiểm tra).
    QStringList args(bool decrypt = true) const {
        QStringList a = m_args;
        if (decrypt && !a.isEmpty()) a << QStringLiteral("--decrypt");
        return a;
    }

private:
    QTemporaryFile m_file;
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

QString suggestOutputPath(const QString& sourcePath, const QString& label) {
    const QFileInfo fi(sourcePath);
    const QString name = label.isEmpty() ? QStringLiteral("Dũng Office") : label;
    const QString base = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(" - ") + name;
    QString cand = base + QStringLiteral(".pdf");
    for (int n = 2; QFileInfo::exists(cand); ++n) cand = base + QStringLiteral(" (%1).pdf").arg(n);
    return cand;
}

}  // namespace dungpdf
