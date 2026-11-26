// SPDX-License-Identifier: MPL-2.0
#include "PptTransition.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QSet>
#include <QXmlStreamReader>
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
    const QByteArray animXml = "<p:sld xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"><p:cSld><p:spTree><p:sp><p:nvSpPr><p:cNvPr id=\"42\" name=\"Box\"/></p:nvSpPr></p:sp></p:spTree></p:cSld><p:transition/><p:extLst/></p:sld>";
    AnimationOptions anim; anim.shapeId = 42; anim.fade = true; anim.durationMs = 750; anim.delayMs = 200;
    QByteArray animated;
    check(patchAnimationXml(animXml,anim,&animated).ok && animated.contains("spid=\"42\"") && animated.contains("dur=\"750\"") &&
        animated.contains("presetID=\"10\"") && animated.indexOf("<p:timing") > animated.indexOf("<p:transition") &&
        animated.indexOf("<p:timing") < animated.indexOf("<p:extLst"), "animation targets real shape and preserves transition order");
    check(!patchAnimationXml(animated,anim,&out).ok,"refuse overwrite existing animation");
    anim.shapeId=43;
    check(!patchAnimationXml(animXml,anim,&out).ok,"reject missing shape");
    const QByteArray masterXml="<p:sldMaster xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"><p:cSld><p:bg><p:bgRef idx=\"1001\"/></p:bg><p:spTree/></p:cSld><p:clrMap/></p:sldMaster>";
    QByteArray masterOut;
    check(patchMasterBackground(masterXml,QStringLiteral("123abc"),&masterOut).ok && masterOut.contains("val=\"123ABC\"") && !masterOut.contains("bgRef") && masterOut.contains("<p:spTree/>"),"change actual master background, retain shape tree");
    check(!patchMasterBackground(masterXml,QStringLiteral("bad"),&out).ok,"reject invalid RGB");
    QByteArray twoShapes = animXml;
    twoShapes.replace("</p:spTree>", "<p:pic><p:nvPicPr><p:cNvPr id=\"43\" name=\"Picture\"/></p:nvPicPr></p:pic></p:spTree>");
    AnimationOptions first; first.shapeId=42; first.fade=true; first.durationMs=750;
    AnimationOptions second; second.shapeId=43; second.delayMs=200;
    QByteArray sequence;
    const auto multi=patchAnimationSequenceXml(twoShapes,{second,first},&sequence);
    check(multi.ok && sequence.count("nodeType=\"clickEffect\"")==2 &&
          sequence.indexOf("spid=\"43\"")<sequence.indexOf("spid=\"42\""),"two click groups preserve user order");
    QXmlStreamReader timing(sequence); QSet<QString> ids; bool unique=true;
    while(!timing.atEnd()) { timing.readNext(); if(timing.isStartElement() && timing.name()==QLatin1String("cTn")) {
        const QString id=timing.attributes().value(QStringLiteral("id")).toString(); if(id.isEmpty()||ids.contains(id)) unique=false; ids.insert(id);
    }}
    check(!timing.hasError() && unique && ids.size()==11,"all timing nodes unique, XML well formed");
    check(!patchAnimationSequenceXml(twoShapes,{first,first},&out).ok,"reject duplicate entrance target");
    check(!patchAnimationSequenceXml(sequence,{first,second},&out).ok,"reject existing timing");
    check(!patchAnimationSequenceXml(twoShapes,{},&out).ok,"reject empty sequence");
    QByteArray threeShapes=twoShapes;
    threeShapes.replace("</p:spTree>","<p:sp><p:nvSpPr><p:cNvPr id=\"44\" name=\"Third\"/></p:nvSpPr></p:sp></p:spTree>");
    AnimationOptions with=second;with.trigger=AnimationTrigger::WithPrevious;with.fade=true;with.durationMs=500;with.delayMs=100;
    AnimationOptions after;after.shapeId=44;after.trigger=AnimationTrigger::AfterPrevious;after.delayMs=200;
    QList<AnimationTiming> plan;
    check(planAnimation({first,with,after},&plan).ok && plan.size()==3 && plan[0].click==1 &&
          plan[0].startMs==0 && plan[0].endMs==750 && plan[1].startMs==100 && plan[1].endMs==600 &&
          plan[2].startMs==800 && plan[2].endMs==801,"mixed triggers calculate previous start and finish correctly");
    QByteArray mixed;
    check(patchAnimationSequenceXml(threeShapes,{first,with,after},&mixed).ok &&
          mixed.count("nodeType=\"clickEffect\"")==1 && mixed.count("nodeType=\"withEffect\"")==1 &&
          mixed.count("nodeType=\"afterEffect\"")==1 && mixed.count("delay=\"indefinite\"")==1 &&
          mixed.contains("delay=\"800\""),"mixed triggers share a click group with correct offset");
    check(!planAnimation({with,first},&plan).ok,"first row cannot depend on absent previous animation");
    check(!patchAnimationSequenceXml(threeShapes,{after},&out).ok,"reject dependent first animation");
    // Exit presets remain distinct from entrance presets and delay the hidden state until fade finishes.
    AnimationOptions disappear; disappear.shapeId=42; disappear.exit=true; disappear.delayMs=200;
    QByteArray exitXml;
    check(patchAnimationXml(animXml,disappear,&exitXml).ok && exitXml.contains("presetClass=\"exit\"")
        && exitXml.contains("presetID=\"1\"") && exitXml.contains("val=\"hidden\"")
        && !exitXml.contains("val=\"visible\"") && !exitXml.contains("<p:animEffect"),"disappear emits hidden state only");
    AnimationOptions fadeOut=second; fadeOut.exit=true; fadeOut.fade=true; fadeOut.durationMs=750;
    fadeOut.trigger=AnimationTrigger::WithPrevious;
    check(planAnimation({disappear,fadeOut},&plan).ok && plan.size()==2 &&
        plan[1].startMs==400 && plan[1].endMs==1151,"exit timeline accounts for final visibility step");
    QByteArray exits;
    check(patchAnimationSequenceXml(twoShapes,{disappear,fadeOut},&exits).ok &&
        exits.count("presetClass=\"exit\"")==2 && exits.contains("transition=\"out\"") &&
        exits.contains("presetID=\"10\"") && exits.contains("delay=\"750\"") &&
        !exits.contains("transition=\"in\""),"fade out emits exit animation and delayed hidden state");
    QXmlStreamReader exitReader(exits);
    while(!exitReader.atEnd()) exitReader.readNext();
    check(!exitReader.hasError(),"exit timing XML is well formed");
    check(!patchAnimationSequenceXml(twoShapes,{disappear,first},&out).ok,"same target entrance and exit still refused");
    check(!patchAnimationXml(exitXml,disappear,&out).ok,"preserve existing exit timing");
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
        const auto shapes = slideShapes(src, 1, &error);
        check(!shapes.isEmpty(),"enumerate shapes in actual PPTX");
        if (!shapes.isEmpty()) {
            anim.shapeId=shapes.first().id;
            const QString animDest=temp.filePath(QStringLiteral("animation.pptx"));
            const auto animation=createAnimationCopy(src,animDest,1,anim);
            if (!animation.ok) std::cerr << animation.error.toStdString() << '\n';
            check(animation.ok && slideEntries(animDest,&error)==entries,"actual PPTX animation copy");
            check(slideShapes(animDest,1,&error).isEmpty() && !error.isEmpty(),"reject adding to existing timing");
        }
        const auto masters=masterEntries(src,&error);
        check(!masters.isEmpty(),"enumerate actual referenced masters");
        if (!masters.isEmpty()) {
            const QString masterDest=temp.filePath(QStringLiteral("master.pptx"));
            const auto patchedMaster=createMasterBackgroundCopy(src,masterDest,masters.first(),QStringLiteral("123ABC"));
            if (!patchedMaster.ok) std::cerr << patchedMaster.error.toStdString() << '\n';
            check(patchedMaster.ok && masterEntries(masterDest,&error)==masters && slideEntries(masterDest,&error)==entries,"real master copy retains relationships and slides");
            check(!createMasterBackgroundCopy(src,masterDest,masters.first(),QStringLiteral("123ABC")).ok,"refuse master overwrite");
        }
        QFile after(src); after.open(QIODevice::ReadOnly);
        check(after.readAll() == bytes, "source unchanged");
    }
    return errors ? 1 : 0;
}
