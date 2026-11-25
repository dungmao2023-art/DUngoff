// SPDX-License-Identifier: MPL-2.0
// Kiểm tra chính tả tiếng Việt theo âm tiết. Toàn bộ bảng vần được SINH bằng luật (nguyên âm × âm cuối) ở dưới,
// không sao chép bảng từ hay từ điển nào. Xem giới hạn trong SyllableCheck.h.
#include "SyllableCheck.h"

#include <QChar>
#include <QHash>
#include <QSet>
#include <algorithm>

namespace vntext::spell {
namespace {

// ---------------------------------------------------------------- bảng vần (sinh bằng luật)
enum class Kind { Normal, Glide, QuOnly, YEmpty };

struct Rime {
    QString text;      // vần không dấu thanh (có dấu mũ/móc/trăng), viết đúng chính tả
    int main = 0;      // vị trí nguyên âm chính (nơi đặt dấu thanh theo kiểu mới)
    int alt = -1;      // vị trí chấp nhận được khác (hoà/hòa, thuỷ/thủy); -1: không có
    bool stop = false; // âm cuối c, ch, p, t: chỉ thanh sắc/nặng
    bool front = false;// bắt đầu bằng nguyên âm trước (e ê i y ia iê yê): chi phối c/k, g/gh, ng/ngh
    bool rare = false; // hiếm / thường gặp ở từ mượn, tên riêng
    Kind kind = Kind::Normal;
};

QVector<Rime> g_rimes;
QHash<QString, QVector<int>> g_index;

bool isStopCoda(const QString &c) { return c == QLatin1String("c") || c == QLatin1String("ch") || c == QLatin1String("p") || c == QLatin1String("t"); }

bool isFrontText(const QString &t) {
    if (t.isEmpty()) return false;
    const QChar f = t.at(0);
    return f == QChar(QLatin1Char('e')) || f == QChar(0xEA) || f == QChar(QLatin1Char('i')) || f == QChar(QLatin1Char('y'));
}

void addRime(const QString &text, int main, int alt, bool stop, bool rare, Kind kind) {
    Rime r;
    r.text = text; r.main = main; r.alt = alt; r.stop = stop; r.rare = rare; r.kind = kind;
    r.front = (kind == Kind::Glide) ? false : isFrontText(text);
    g_index[text].push_back(g_rimes.size());
    g_rimes.push_back(r);
}

QStringList L(const char *s) { return QString::fromUtf8(s).split(QLatin1Char(' '), Qt::SkipEmptyParts); }

// Âm cuối: "-" nghĩa là không có âm cuối.
void addNucleus(const QString &nuc, int main, const QStringList &codas, const QStringList &rareCodas = {}) {
    for (int pass = 0; pass < 2; ++pass)
        for (const QString &c : (pass ? rareCodas : codas))
            addRime(nuc + (c == QLatin1String("-") ? QString() : c), main, -1, isStopCoda(c), pass == 1, Kind::Normal);
}

// mainRel: vị trí nguyên âm chính TRONG nguyên âm (không tính âm đệm)
void addGlide(const QString &glide, const QString &nuc, int mainRel, const QStringList &codas, const QStringList &rareCodas = {}, bool openAlt = false) {
    for (int pass = 0; pass < 2; ++pass)
        for (const QString &c : (pass ? rareCodas : codas)) {
            const bool open = (c == QLatin1String("-"));
            const int m = glide.size() + mainRel;
            addRime(glide + nuc + (open ? QString() : c), m, (open && openAlt) ? 0 : -1, isStopCoda(c), pass == 1, Kind::Glide);
        }
}

void buildRimes() {
    if (!g_rimes.isEmpty()) return;
    const QString a = QString::fromUtf8("a"), ab = QString::fromUtf8("ă"), ac = QString::fromUtf8("â");
    const QString e = QString::fromUtf8("e"), ec = QString::fromUtf8("ê"), i = QString::fromUtf8("i");
    const QString o = QString::fromUtf8("o"), oc = QString::fromUtf8("ô"), oh = QString::fromUtf8("ơ");
    const QString u = QString::fromUtf8("u"), uh = QString::fromUtf8("ư");
    // --- nguyên âm đơn
    addNucleus(a, 0, L("- i y o u m n ng nh c ch p t"));
    addNucleus(ab, 0, L("m n ng c p t"), L("k"));
    addNucleus(ac, 0, L("y u m n ng c p t"));
    addNucleus(e, 0, L("- o m n ng c ch p t"));
    addNucleus(ec, 0, L("- u m n nh ch p t"));
    addNucleus(i, 0, L("- u m n nh ch p t"));
    addNucleus(QString::fromUtf8("y"), 0, L("-"));
    addNucleus(o, 0, L("- i m n ng c p t"));
    addNucleus(oc, 0, L("- i m n ng c p t"));
    addNucleus(oh, 0, L("- i m n p t"));
    addNucleus(u, 0, L("- i m n ng c p t"), L("k"));
    addNucleus(uh, 0, L("- i u ng c t m"), L("n"));
    // --- nguyên âm đôi
    addNucleus(QString::fromUtf8("ia"), 0, L("-"));
    addNucleus(QString::fromUtf8("iê"), 1, L("c m n ng p t u"));
    addNucleus(QString::fromUtf8("ua"), 0, L("-"));
    addNucleus(QString::fromUtf8("uô"), 1, L("c i m n ng t"));
    addNucleus(QString::fromUtf8("ưa"), 0, L("-"));
    addNucleus(QString::fromUtf8("ươ"), 1, L("c i m n ng p t u"));
    // yê: chỉ khi không có phụ âm đầu (yêu, yên, yếm, yết)
    for (const QString &c : L("m n t u")) addRime(QString::fromUtf8("yê") + c, 1, -1, isStopCoda(c), false, Kind::YEmpty);
    addRime(QString::fromUtf8("yêng"), 1, -1, false, true, Kind::YEmpty);
    // --- có âm đệm (o/u + nguyên âm)
    addGlide(QString::fromUtf8("o"), QString::fromUtf8("a"), 0, L("- i y c ch m n ng nh t"), L("p o"), true);
    addGlide(QString::fromUtf8("o"), QString::fromUtf8("ă"), 0, L("c m n ng t"));
    addGlide(QString::fromUtf8("o"), QString::fromUtf8("e"), 0, L("- o m n t"), L("ng"), true);
    addGlide(QString::fromUtf8("u"), QString::fromUtf8("ê"), 0, L("- nh ch u"), L("n"));
    addGlide(QString::fromUtf8("u"), QString::fromUtf8("y"), 0, L("- t n nh ch u"), L("p"), true);
    addGlide(QString::fromUtf8("u"), QString::fromUtf8("yê"), 1, L("n t"));
    addGlide(QString::fromUtf8("u"), QString::fromUtf8("ya"), 0, L("-"));
    addGlide(QString::fromUtf8("u"), QString::fromUtf8("â"), 0, L("n t ng y"));
    addGlide(QString::fromUtf8("u"), QString::fromUtf8("ơ"), 0, L("-"));
    addGlide(QString::fromUtf8("o"), QString::fromUtf8("o"), 0, {}, L("ng c"));
    for (Rime &r : g_rimes) if (r.text == QLatin1String("oong") || r.text == QLatin1String("ooc")) { r.main = 0; r.alt = 1; }

    // --- vần chỉ đi sau "qu": quy, quỳnh, quýt, quyên, quyết... (bỏ chữ u đầu của vần uy*/uya)
    const int n = g_rimes.size();
    for (int k = 0; k < n; ++k) {
        const Rime g = g_rimes[k];
        if (g.kind == Kind::Glide && g.text.startsWith(QLatin1Char('u')) && g.text.size() > 1 && g.text.at(1) == QLatin1Char('y'))
            addRime(g.text.mid(1), g.main - 1, -1, g.stop, g.rare, Kind::QuOnly);
    }
    for (Rime &r : g_rimes) if (r.kind == Kind::QuOnly) r.front = true;
}

// ---------------------------------------------------------------- phụ âm đầu
const QStringList &onsetList() {
    static const QStringList l = []{
        QStringList x = {QStringLiteral("ngh"), QStringLiteral("ch"), QStringLiteral("gh"), QStringLiteral("gi"), QStringLiteral("kh"),
                         QStringLiteral("ng"), QStringLiteral("nh"), QStringLiteral("ph"), QStringLiteral("qu"), QStringLiteral("th"), QStringLiteral("tr")};
        for (const char *s : {"b", "c", "d", "g", "h", "k", "l", "m", "n", "p", "q", "r", "s", "t", "v", "x"}) x << QString::fromLatin1(s);
        x << QString::fromUtf8("đ");
        x << QString();   // không có phụ âm đầu
        return x;
    }();
    return l;
}

enum class Ok { Ok, Rare, Bad };
struct OnsetVerdict { Ok ok = Ok::Ok; QString why; };

OnsetVerdict bad(const char *why) { return {Ok::Bad, QString::fromUtf8(why)}; }

OnsetVerdict onsetCheck(const QString &on, const Rime &r) {
    const QString &t = r.text;
    const bool glide = r.kind == Kind::Glide;
    auto rarity = [&](OnsetVerdict v) { if (v.ok == Ok::Ok && r.rare) v.ok = Ok::Rare; return v; };
    if (r.kind == Kind::QuOnly) return on == QLatin1String("qu") ? rarity({}) : bad("vần này chỉ đi sau “qu” (sau phụ âm khác phải có âm đệm “u”)");
    if (r.kind == Kind::YEmpty) return on.isEmpty() ? rarity({}) : bad("sau phụ âm đầu phải viết “iê”, không viết “yê”");
    if (on.isEmpty()) {
        if (t.startsWith(QString::fromUtf8("iê"))) return bad("không có phụ âm đầu thì viết “yê”, không viết “iê”");
        return rarity({});
    }
    if (on == QLatin1String("q")) return bad("chữ “q” luôn đi với “u” (qu)");
    if (on == QLatin1String("qu")) {
        if (glide) return bad("sau “qu” không thêm âm đệm o/u");
        if (t == QString::fromUtf8("ôc")) return {};
        const QChar f = t.at(0);
        if (f == QChar(QLatin1Char('a')) || f == QChar(0x103) || f == QChar(0xE2) || f == QChar(QLatin1Char('e')) || f == QChar(0xEA) || f == QChar(0x1A1)) return rarity({});
        if (f == QChar(QLatin1Char('i')) && !t.startsWith(QLatin1String("ia")) && !t.startsWith(QString::fromUtf8("iê")))
            return (t == QLatin1String("i") || t == QLatin1String("it") || t == QLatin1String("in") || t == QLatin1String("inh")) ? OnsetVerdict{Ok::Rare, {}} : bad("sau “qu” không đi với vần này");
        return bad("sau “qu” không đi với vần này");
    }
    const bool front = r.front;
    if (on == QLatin1String("k")) {
        if (glide || !front) return bad("“k” chỉ đứng trước e, ê, i, y, ia, iê (trước a, o, u, ô, ơ, ư, ă, â dùng “c”)");
        return rarity({});
    }
    if (on == QLatin1String("c")) {
        if (front) return bad("“c” không đứng trước e, ê, i, y, ia, iê (dùng “k”)");
        if (glide) return {Ok::Rare, {}};
        return rarity({});
    }
    if (on == QLatin1String("g")) {
        if (glide) return {Ok::Rare, {}};
        if (front) {
            // “gi” + vần: chữ i của “gi” dùng chung với i của vần (gì, gìn, giêng, giếng, giết, gia)
            if (t.at(0) == QChar(QLatin1Char('i'))) {
                if (t.startsWith(QLatin1String("ia")) || t.startsWith(QString::fromUtf8("iê"))) return rarity({});
                return (t == QLatin1String("i") || t == QLatin1String("in")) ? rarity({}) : OnsetVerdict{Ok::Rare, {}};
            }
            return bad("“g” không đứng trước e, ê, y (dùng “gh”)");
        }
        return rarity({});
    }
    if (on == QLatin1String("gh")) {
        if (glide || !front) return bad("“gh” chỉ đứng trước e, ê, i, iê (dùng “g”)");
        if (t.startsWith(QLatin1String("ia")) || t.at(0) == QChar(QLatin1Char('y'))) return {Ok::Rare, {}};
        return rarity({});
    }
    if (on == QLatin1String("ng")) {
        if (front) return bad("“ng” không đứng trước e, ê, i, y, ia, iê (dùng “ngh”)");
        return rarity({});
    }
    if (on == QLatin1String("ngh")) {
        if (glide || !front) return bad("“ngh” chỉ đứng trước e, ê, i, ia, iê (dùng “ng”)");
        if (t.at(0) == QChar(QLatin1Char('y'))) return {Ok::Rare, {}};
        return rarity({});
    }
    if (on == QLatin1String("gi")) {
        if (glide) return bad("“gi” không đi với vần có âm đệm");
        if (t.at(0) == QChar(QLatin1Char('i')) || t.at(0) == QChar(QLatin1Char('y'))) return bad("“gi” đã có sẵn chữ i; không viết thêm i/y");
        return rarity({});
    }
    if (on == QLatin1String("p")) return {Ok::Rare, {}};
    if (glide && (on == QLatin1String("b") || on == QLatin1String("m") || on == QLatin1String("n") || on == QLatin1String("r") || on == QLatin1String("v")))
        return {Ok::Rare, {}};
    return rarity({});
}

// ---------------------------------------------------------------- phân tích
struct Parsed {
    Verdict v = Verdict::Invalid;
    QString onset;
    int rimeIdx = -1;
    QString reason;
};

// base: chữ thường, đã ghép dựng sẵn (ă â ê ô ơ ư đ), KHÔNG dấu thanh.
QVector<Parsed> parseAll(const QString &base, QString *whyFail) {
    buildRimes();
    QVector<Parsed> out;
    QString orthoWhy;
    for (const QString &on : onsetList()) {
        if (!base.startsWith(on)) continue;
        const QString rest = base.mid(on.size());
        if (rest.isEmpty()) continue;
        const auto it = g_index.constFind(rest);
        if (it == g_index.constEnd()) continue;
        for (int idx : it.value()) {
            const OnsetVerdict ov = onsetCheck(on, g_rimes[idx]);
            if (ov.ok == Ok::Bad) { if (orthoWhy.isEmpty()) orthoWhy = ov.why; continue; }
            Parsed p;
            p.v = ov.ok == Ok::Ok ? Verdict::Valid : Verdict::Unusual;
            p.onset = on; p.rimeIdx = idx;
            out.push_back(p);
        }
    }
    // Valid trước Unusual
    std::stable_sort(out.begin(), out.end(), [](const Parsed &x, const Parsed &y) { return (x.v == Verdict::Valid) && (y.v != Verdict::Valid); });
    if (out.isEmpty()) {
        if (!orthoWhy.isEmpty()) *whyFail = orthoWhy;
        else *whyFail = QString::fromUtf8("không có vần nào như vậy trong tiếng Việt");
    }
    return out;
}

bool isVowelLetter(QChar c) {
    static const QString v = QString::fromUtf8("aăâeêioôơuưy");
    return v.contains(c);
}

struct Cluster { QChar letter; int tone = 0; };

struct Split {
    bool ok = false;
    QString whyNot;
    QString base;      // không dấu thanh
    int toneIdx = -1;
    int tone = 0;
    bool multiTone = false;
    bool vnMarks = false;   // có dấu/mũ/móc/đ đặc trưng tiếng Việt
};

int toneOfMark(ushort m) {
    switch (m) { case 0x0300: return 1; case 0x0309: return 2; case 0x0303: return 3; case 0x0301: return 4; case 0x0323: return 5; }
    return 0;
}

Split splitSyllable(const QString &syllable) {
    Split s;
    const QString lower = syllable.toLower().normalized(QString::NormalizationForm_D);
    QVector<QString> bases;           // chữ cái cơ sở + dấu mũ/móc/trăng (chưa ghép)
    int toneCount = 0, toneIdx = -1, toneVal = 0;
    for (QChar ch : lower) {
        if (ch.category() == QChar::Mark_NonSpacing) {
            if (bases.isEmpty()) { s.whyNot = QString::fromUtf8("dấu đứng một mình"); return s; }
            const int tn = toneOfMark(ch.unicode());
            if (tn) {
                if (!isVowelLetter(bases.last().normalized(QString::NormalizationForm_C).at(0))) { s.whyNot = QString::fromUtf8("dấu thanh trên phụ âm: không phải chữ Việt"); return s; }
                ++toneCount; toneIdx = bases.size() - 1; toneVal = tn;
            } else if (ch.unicode() == 0x0302 || ch.unicode() == 0x0306 || ch.unicode() == 0x031B) {
                bases.last().append(ch);
                s.vnMarks = true;
            } else { s.whyNot = QString::fromUtf8("ký tự có dấu không thuộc chữ Việt"); return s; }
        } else if (ch.isLetter()) {
            bases.push_back(QString(ch));
        } else { s.whyNot = QString::fromUtf8("không phải chữ cái"); return s; }
    }
    if (bases.isEmpty()) { s.whyNot = QString::fromUtf8("rỗng"); return s; }
    static const QString alphabet = QString::fromUtf8("abcdđeghiklmnopqrstuvxyăâêôơư");
    for (const QString &b : bases) {
        const QString c = b.normalized(QString::NormalizationForm_C);
        if (c.size() != 1 || !alphabet.contains(c.at(0))) { s.whyNot = QString::fromUtf8("chữ không thuộc bảng chữ cái tiếng Việt"); return s; }
        s.base += c;
        if (c.at(0) == QChar(0x111)) s.vnMarks = true;
    }
    s.toneIdx = toneIdx; s.tone = toneVal;
    s.multiTone = toneCount > 1;
    if (toneCount) s.vnMarks = true;
    s.ok = true;
    return s;
}

QString composeTone(const QString &base, int idx, int tone) {
    if (tone == 0 || idx < 0 || idx >= base.size()) return base;
    static const ushort marks[] = {0, 0x0300, 0x0309, 0x0303, 0x0301, 0x0323};
    QString out;
    for (int i = 0; i < base.size(); ++i) {
        if (i == idx) out += (QString(base.at(i)) + QChar(marks[tone])).normalized(QString::NormalizationForm_C);
        else out += base.at(i);
    }
    return out;
}

// Dạng chuẩn (dấu thanh ở nguyên âm chính) nếu hợp lệ, ngược lại chuỗi rỗng.
QString renderValid(const QString &base, int tone, bool allowUnusual, Verdict *vOut = nullptr) {
    QString why;
    const auto ps = parseAll(base, &why);
    for (const Parsed &p : ps) {
        if (p.v == Verdict::Unusual && !allowUnusual) continue;
        const Rime &r = g_rimes[p.rimeIdx];
        if (r.stop && tone != 0 && tone != 4 && tone != 5) continue;
        if (r.stop && tone == 0) continue;
        if (vOut) *vOut = p.v;
        return composeTone(base, tone ? p.onset.size() + r.main : -1, tone);
    }
    return QString();
}

QString applyCase(const QString &orig, const QString &sug) {
    if (orig.isEmpty() || sug.isEmpty()) return sug;
    int up = 0, letters = 0;
    for (QChar c : orig) if (c.isLetter()) { ++letters; if (c.isUpper()) ++up; }
    if (letters >= 2 && up == letters) return sug.toUpper();
    if (orig.at(0).isUpper()) return sug.at(0).toUpper() + sug.mid(1);
    return sug;
}

QStringList suggest(const Split &s, const QString &original) {
    // 1) biến thể phụ âm đầu (c↔k, g↔gh, ng↔ngh, q→qu)
    QStringList bases{s.base};
    struct Sw { const char *from, *to; };
    static const Sw sw[] = {{"ngh", "ng"}, {"ng", "ngh"}, {"gh", "g"}, {"g", "gh"}, {"k", "c"}, {"c", "k"}, {"q", "qu"}};
    for (const Sw &x : sw) {
        const QString f = QString::fromLatin1(x.from);
        if (s.base.startsWith(f)) {
            if (f == QLatin1String("c") && s.base.startsWith(QLatin1String("ch"))) continue;
            if (f == QLatin1String("g") && s.base.startsWith(QLatin1String("gh"))) continue;
            if (f == QLatin1String("g") && s.base.startsWith(QLatin1String("gi"))) continue;
            if (f == QLatin1String("ng") && s.base.startsWith(QLatin1String("ngh"))) continue;
            if (f == QLatin1String("k") && s.base.startsWith(QLatin1String("kh"))) continue;
            if (f == QLatin1String("q") && s.base.startsWith(QLatin1String("qu"))) continue;
            bases << QString::fromLatin1(x.to) + s.base.mid(f.size());
        }
    }
    // 2) thêm dấu mũ/móc/trăng còn thiếu hoặc nhầm (a↔ă↔â, e↔ê, o↔ô↔ơ, u↔ư, d↔đ, i↔y)
    static const QHash<QChar, QString> alt = {
        {QChar(QLatin1Char('a')), QString::fromUtf8("ăâ")}, {QChar(0x103), QString::fromUtf8("aâ")}, {QChar(0xE2), QString::fromUtf8("aă")},
        {QChar(QLatin1Char('e')), QString::fromUtf8("ê")},  {QChar(0xEA), QString::fromUtf8("e")},
        {QChar(QLatin1Char('o')), QString::fromUtf8("ôơ")}, {QChar(0xF4), QString::fromUtf8("oơ")}, {QChar(0x1A1), QString::fromUtf8("oô")},
        {QChar(QLatin1Char('u')), QString::fromUtf8("ư")},  {QChar(0x1B0), QString::fromUtf8("u")},
        {QChar(QLatin1Char('d')), QString::fromUtf8("đ")},  {QChar(0x111), QString::fromUtf8("d")},
        {QChar(QLatin1Char('i')), QString::fromUtf8("y")},  {QChar(QLatin1Char('y')), QString::fromUtf8("i")}};
    auto oneLetter = [&](const QStringList &in) {
        QStringList out;
        for (const QString &b : in)
            for (int i = 0; i < b.size(); ++i) {
                const auto it = alt.constFind(b.at(i));
                if (it == alt.constEnd()) continue;
                for (QChar r : it.value()) { QString c = b; c[i] = r; out << c; }
            }
        return out;
    };
    const QStringList step1 = oneLetter(bases);
    QStringList cands = bases + step1;
    if (!s.vnMarks) cands += oneLetter(step1);   // chữ không dấu: có thể thiếu hai dấu (vd. "nguoi" → "người")
    QStringList out;
    QSet<QString> seen;
    const QString origLower = original.toLower().normalized(QString::NormalizationForm_C);
    auto push = [&](const QString &base, int tone) {
        const QString r = renderValid(base, tone, false);
        if (r.isEmpty() || seen.contains(r) || r == origLower) return;
        seen.insert(r); out << applyCase(original, r);
    };
    for (const QString &c : cands) {
        if (out.size() >= 5) break;
        if (renderValid(c, s.tone, false).isEmpty()) { push(c, 4); push(c, 5); }   // thanh không hợp với vần p,t,c,ch -> thử sắc/nặng
        else push(c, s.tone);
    }
    return out.mid(0, 5);
}

SyllableResult analyzeCore(const QString &syllable) {
    SyllableResult res;
    const Split s = splitSyllable(syllable);
    if (!s.ok) { res.verdict = Verdict::NotChecked; res.reason = s.whyNot; return res; }
    bool hasVowel = false;
    for (QChar c : s.base) if (isVowelLetter(c)) { hasVowel = true; break; }
    if (!hasVowel) { res.verdict = Verdict::NotChecked; res.reason = QString::fromUtf8("không có nguyên âm (viết tắt, ký hiệu?)"); return res; }
    res.tone = s.tone;
    const Certainty structural = s.vnMarks ? Certainty::High : Certainty::Medium;

    if (s.multiTone) {
        res.verdict = Verdict::Invalid; res.certainty = Certainty::High;
        res.reason = QString::fromUtf8("một âm tiết chỉ có một dấu thanh");
        return res;
    }
    QString why;
    const QVector<Parsed> ps = parseAll(s.base, &why);
    if (ps.isEmpty()) {
        res.verdict = Verdict::Invalid; res.certainty = structural;
        res.reason = s.base.size() > 7 ? QString::fromUtf8("quá dài cho một âm tiết (có thể thiếu dấu cách)") : why;
        if (s.base.size() <= 7) res.suggestions = suggest(s, syllable);
        return res;
    }
    // chọn phép tách hợp với dấu thanh
    const Parsed *chosen = nullptr;
    QString toneWhy; Certainty toneCert = Certainty::High; const Parsed *first = &ps.first();
    for (const Parsed &p : ps) {
        const Rime &r = g_rimes[p.rimeIdx];
        if (s.tone) {
            if (r.stop && s.tone != 4 && s.tone != 5) {
                if (toneWhy.isEmpty()) toneWhy = QString::fromUtf8("vần kết thúc bằng c, ch, p, t chỉ mang thanh sắc hoặc nặng");
                continue;
            }
            const int on = p.onset.size();
            const bool onMain = s.toneIdx == on + r.main || (r.alt >= 0 && s.toneIdx == on + r.alt);
            if (!onMain) {
                if (toneWhy.isEmpty()) {
                    toneWhy = QString::fromUtf8("dấu thanh đặt sai chữ (phải đặt ở nguyên âm chính)");
                    if (s.toneIdx >= 0 && s.toneIdx < on && !isVowelLetter(s.base.at(s.toneIdx))) toneCert = Certainty::Medium;
                }
                continue;
            }
        } else if (r.stop) {
            // vần p,t,c,ch không dấu thanh (thanh ngang) không tồn tại
            if (toneWhy.isEmpty()) toneWhy = QString::fromUtf8("vần kết thúc bằng c, ch, p, t phải có dấu sắc hoặc nặng");
            toneCert = s.vnMarks ? Certainty::High : Certainty::Medium;
            continue;
        }
        chosen = &p; break;
    }
    if (!chosen) {
        res.verdict = Verdict::Invalid; res.certainty = toneCert; res.reason = toneWhy;
        res.onset = first->onset; res.rime = g_rimes[first->rimeIdx].text;
        const QStringList sg = suggest(s, syllable);
        res.suggestions = sg;
        return res;
    }
    res.onset = chosen->onset; res.rime = g_rimes[chosen->rimeIdx].text;
    res.verdict = chosen->v;
    res.certainty = chosen->v == Verdict::Valid ? Certainty::High : Certainty::Low;
    if (chosen->v == Verdict::Unusual) res.reason = QString::fromUtf8("tổ hợp hiếm, thường gặp ở từ mượn hoặc tên riêng");
    return res;
}

// Viết tắt kiểu UBND, TPHCM, CSDL: toàn chữ HOA, không có dấu đặc trưng tiếng Việt -> không kiểm.
SyllableResult analyze(const QString &syllable) {
    SyllableResult res = analyzeCore(syllable);
    if (res.verdict == Verdict::Invalid && res.certainty != Certainty::High) {
        int letters = 0, upper = 0;
        for (QChar c : syllable) if (c.isLetter()) { ++letters; if (c.isUpper()) ++upper; }
        if (letters >= 2 && letters == upper) {
            SyllableResult n;
            n.verdict = Verdict::NotChecked;
            n.reason = QString::fromUtf8("toàn chữ hoa, không dấu: coi là viết tắt");
            return n;
        }
    }
    return res;
}

} // namespace

// ---------------------------------------------------------------- API
SyllableResult checkSyllable(const QString &syllable) { return analyze(syllable); }

bool isValidSyllable(const QString &syllable) {
    const auto r = analyze(syllable);
    return r.verdict == Verdict::Valid || r.verdict == Verdict::Unusual;
}

QVector<TextIssue> checkText(const QString &text, Certainty minCertainty) {
    QVector<TextIssue> out;
    int i = 0;
    const int n = text.size();
    auto inWord = [&](QChar c) { return c.isLetter() || c.category() == QChar::Mark_NonSpacing; };
    while (i < n) {
        if (!inWord(text.at(i))) { ++i; continue; }
        int j = i;
        while (j < n && inWord(text.at(j))) ++j;
        const QString tok = text.mid(i, j - i);
        const SyllableResult r = analyze(tok);
        if (r.verdict == Verdict::Invalid && int(r.certainty) >= int(minCertainty)) {
            TextIssue t; t.start = i; t.length = j - i; t.text = tok; t.result = r;
            out.push_back(t);
        }
        i = j;
    }
    return out;
}

QStringList generateToneless() {
    buildRimes();
    QSet<QString> set;
    for (const QString &on : onsetList())
        for (const Rime &r : g_rimes)
            if (onsetCheck(on, r).ok != Ok::Bad) set.insert(on + r.text);
    QStringList l = set.values();
    l.sort();
    return l;
}

QStringList generateAllSyllables() {
    buildRimes();
    QSet<QString> set;
    for (const QString &on : onsetList())
        for (const Rime &r : g_rimes) {
            if (onsetCheck(on, r).ok == Ok::Bad) continue;
            const QString base = on + r.text;
            const int mi = on.size() + r.main;
            for (int t = 0; t <= 5; ++t) {
                if (r.stop && t != 4 && t != 5) continue;
                set.insert(composeTone(base, t ? mi : -1, t));
            }
        }
    QStringList l = set.values();
    l.sort();
    return l;
}

QString describeIssue(const QString &text, const TextIssue &issue, int radius) {
    const int from = qMax(0, issue.start - radius);
    const int to = qMin(text.size(), issue.start + issue.length + radius);
    QString ctx = text.mid(from, to - from).simplified();
    QString out = QStringLiteral("\u2026") + ctx + QStringLiteral("\u2026  \u2192  \u00ab") + issue.text + QStringLiteral("\u00bb");
    if (!issue.result.reason.isEmpty()) out += QStringLiteral(" \u2014 ") + issue.result.reason;
    if (!issue.result.suggestions.isEmpty())
        out += QStringLiteral(" (g\u1ee3i \u00fd: ") + issue.result.suggestions.join(QStringLiteral(", ")) + QLatin1Char(')');
    return out;
}

} // namespace vntext::spell
