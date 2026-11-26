// SPDX-License-Identifier: MPL-2.0
#include "PptTransition.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
using namespace ppttransition;
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int errors = 0;
    auto check = [&](bool ok, const char* msg) { std::cout << (ok ? "PASS " : "FAIL ") << msg << '\n'; if (!ok) ++errors; };
    const QByteArray xml = "<p:sld xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"><p:cSld/><p:clrMapOvr/><p:timing/><p:extLst/></p:sld>";
    QByteArray out;
    Options o; o.autoAdvanceMs = 3000;
    auto result = patchXml(xml, o, &out);
    check(result.ok && out.contains("advTm=\"3000\"") && out.contains("<p:fade/>") && out.indexOf("<p:transition") < out.indexOf("<p:timing"), "insert transition before timing");
    QByteArray replaced; o.effect = Effect::PushLeft;
    check(patchXml(out, o, &replaced).ok && replaced.count("<p:transition") == 1 && replaced.contains("<p:push dir=\"l\"/>") && !replaced.contains("<p:fade/>"), "replace simple transition");
    o.effect = Effect::None; o.autoAdvanceMs = 0;
    QByteArray removed;
    check(patchXml(replaced, o, &removed).ok && removed == xml, "remove transition preserves original XML");
    o.onClick = false;
    check(!patchXml(xml, o, &out).ok, "reject no advance method");
    o.onClick = true;
    check(!patchXml("<not-slide/>", o, &out).ok, "reject bad XML");
    if (argc > 1) {
        const QString src = QString::fromLocal8Bit(argv[1]);
        QFile before(src); before.open(QIODevice::ReadOnly); const auto bytes = before.readAll(); before.close();
        QTemporaryDir temp; const QString dst = temp.filePath(QStringLiteral("result.pptx"));
        QString error; const auto entries = slideEntries(src, &error);
        check(!entries.isEmpty(), "read actual PPTX slide order");
        o.effect = Effect::Fade;
        const auto patched = createCopy(src, dst, 1, o);
        if (!patched.ok) std::cerr << patched.error.toStdString() << '\n';
        check(patched.ok && slideEntries(dst, &error) == entries, "actual PPTX copy retains slide order");
        check(!createCopy(src, dst, 1, o).ok && !createCopy(src, src, 1, o).ok, "refuse overwrite");
        QFile after(src); after.open(QIODevice::ReadOnly);
        check(after.readAll() == bytes, "source unchanged");
    }
    return errors ? 1 : 0;
}
