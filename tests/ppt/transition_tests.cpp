// SPDX-License-Identifier: MPL-2.0
#include "PptTransition.h"
#include "SlideNavigation.h"
#include "SimpleEntrance.h"
#include <QJsonDocument>
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
    const QList<Shape> presetShapes{{1,"Title"},{2,"Text"},{3,"Picture"}};
    QList<AnimationOptions> presetEffects;
    QList<AnimationTiming> presetTiming;
    check(makeAnimationPreset(presetShapes,AnimationPreset::AppearClickEach,&presetEffects).ok &&
          planAnimation(presetEffects,&presetTiming).ok && presetTiming.size()==3 &&
          presetTiming[0].click==1 && presetTiming[2].click==3 &&
          presetTiming[2].startMs==0 && presetTiming[2].endMs==1 &&
          !presetEffects[0].fade && !presetEffects[1].fade && !presetEffects[2].fade &&
          !presetEffects[2].exit,"preset: instant entrances without fade, one click per object");
    check(makeAnimationPreset(presetShapes,AnimationPreset::ClickEach,&presetEffects).ok &&
          planAnimation(presetEffects,&presetTiming).ok && presetTiming.size()==3 &&
          presetTiming[0].click==1 && presetTiming[2].click==3 &&
          presetEffects[2].shapeId==3 && presetEffects[2].fade,"preset: each object on its own click");
    check(makeAnimationPreset(presetShapes,AnimationPreset::Cascade,&presetEffects).ok &&
          planAnimation(presetEffects,&presetTiming).ok && presetTiming.size()==3 &&
          presetTiming[2].click==1 && presetTiming[2].startMs==1000 &&
          presetTiming[2].endMs==1500,"preset: cascade after one click");
    check(makeAnimationPreset(presetShapes,AnimationPreset::Together,&presetEffects).ok &&
          planAnimation(presetEffects,&presetTiming).ok && presetTiming.size()==3 &&
          presetTiming[2].click==1 && presetTiming[2].startMs==0 &&
          presetTiming[2].endMs==500,"preset: simultaneous after one click");
    QList<Shape> tooMany;for(unsigned i=1;i<=101;++i)tooMany.append(Shape{i,QString::number(i)});
    check(!makeAnimationPreset(tooMany,AnimationPreset::Together,&presetEffects).ok &&
          !makeAnimationPreset({{1,"A"},{1,"B"}},AnimationPreset::Cascade,&presetEffects).ok &&
          !makeAnimationPreset({},AnimationPreset::ClickEach,&presetEffects).ok &&
          !makeAnimationPreset({{0,"Invalid"}},AnimationPreset::Together,&presetEffects).ok &&
          presetEffects.size()==3 && presetEffects[2].trigger==AnimationTrigger::WithPrevious,
          "preset rejects invalid lists without altering previous effects");
    QString target;
    const auto entrance=QJsonDocument::fromJson(R"({"nodeName":"Seq","children":[{"nodeName":"Par","nodeType":"OnClick","children":[{"nodeName":"Set","attributeName":"Visibility","to":"visible","targetElement":"shape1"}]}]})").object();
    check(slideshow::findSimpleEntrance(entrance,&target)&&target=="shape1","native single on-click visibility entrance plan");
    const auto unsupported=QJsonDocument::fromJson(R"({"nodeName":"Seq","children":[{"nodeName":"Par","nodeType":"OnClick","children":[{"nodeName":"Animate","attributeName":"Opacity","to":"1","targetElement":"shape1"}]}]})").object();
    check(!slideshow::findSimpleEntrance(unsupported,&target),"complex animation not misrepresented as appear");
    const QSize wide=slideshow::thumbnailSize(16000,9000);
    const QSize tall=slideshow::thumbnailSize(100,100000);
    check(wide.width()==128 && wide.height()==72,"thumbnail preserves landscape aspect");
    check(tall.width()>=1 && tall.height()<=96,"portrait thumbnail bitmap bounded");
    check(!slideshow::thumbnailSize(0,100).isValid(),"reject invalid thumbnail dimensions");
    const QVector<bool> visible{false,true,false,true,false};
    check(slideshow::firstVisible(visible,0)==1,"show skips initial hidden slide");
    check(slideshow::firstVisible(visible,4)==3,"show from hidden last slide falls back");
    check(slideshow::firstVisible({false,false},0)==-1,"all hidden slides never shown");
    check(slideshow::jumpTo(visible,"4")==3,"number then Enter jumps to visible slide");
    check(slideshow::adjacentVisible(visible,1,1)==3&&slideshow::adjacentVisible(visible,3,-1)==1,"next and previous skip hidden slides");
    check(slideshow::adjacentVisible(visible,3,1)==-1&&slideshow::adjacentVisible(visible,1,-1)==-1,"navigation stops at visible ends");
    check(slideshow::adjacentVisible(visible,-1,1)==-1&&slideshow::adjacentVisible(visible,5,-1)==-1&&slideshow::adjacentVisible(visible,1,0)==-1,"reject invalid navigation inputs");
    check(slideshow::jumpTo(visible,"3")==-1 && slideshow::jumpTo(visible,"0")==-1 &&
          slideshow::jumpTo(visible,"6")==-1 && slideshow::jumpTo(visible,"999999999999")==-1,
          "numeric jump rejects hidden or invalid slides");
    check(slideshow::autoAdvanceDelay(visible,1,5000,false,false,false,true)==5000,"autoplay advances to next visible slide");
    check(slideshow::autoAdvanceDelay(visible,3,5000,false,false,false,true)==0,"autoplay stops at last visible slide");
    check(slideshow::autoAdvanceDelay(visible,1,5000,true,false,false,true)==0 &&
          slideshow::autoAdvanceDelay(visible,1,5000,false,true,false,true)==0 &&
          slideshow::autoAdvanceDelay(visible,1,5000,false,false,true,true)==0 &&
          slideshow::autoAdvanceDelay(visible,1,5000,false,false,false,false)==0,"autoplay pauses on blank, numeric entry, manual pause and render failure");
    check(slideshow::autoAdvanceDelay(visible,1,0,false,false,false,true)==0 &&
          slideshow::autoAdvanceDelay(visible,1,600001,false,false,false,true)==0,"autoplay interval bounded and manual mode unchanged");
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
    {
        const QByteArray blank="<p:sld xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"><p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/></p:nvGrpSpPr><p:grpSpPr/></p:spTree></p:cSld></p:sld>";
        pptdiagram::Options diagram;diagram.title="Ý tưởng & <kết quả>";diagram.items={"Một","Hai","Ba","Bốn"};
        for(int l=0;l<6;++l)for(int palette=0;palette<3;++palette){
            diagram.layout=static_cast<pptdiagram::Layout>(l);diagram.palette=palette;
            QList<pptdiagram::Node> nodes;QByteArray output;
            check(pptdiagram::plan(diagram,&nodes)&&patchDiagramXml(blank,12192000,6858000,diagram,&output).ok&&
                  output.count("<p:sp ")==nodes.size()&&output.contains("&amp;")&&output.contains("&lt;")&&
                  !patchDiagramXml(output,12192000,6858000,diagram,&out).ok,
                  "editable diagram shapes, escaping and refusal to overwrite");
            const auto edges=pptdiagram::edges(diagram,nodes);
            const int expectedEdges=l==0?3:l==1?7:l==2?4:l==3?0:l==4?3:4;
            check(edges.size()==expectedEdges&&output.count("<p:cxnSp ")==expectedEdges&&
                  output.count("<a:stCxn ")==expectedEdges&&output.count("<a:endCxn ")==expectedEdges,"attached diagram connectors");
            for(const auto& edge:edges)check(edge.from!=edge.to&&edge.from>=0&&edge.to>=0&&
                edge.from<nodes.size()&&edge.to<nodes.size()&&edge.start!=edge.end,"valid connector endpoints");
            for(const auto& node:nodes)check(QRectF(0,0,1,1).contains(node.rect),"diagram bounds");
        }
        check(!patchDiagramXml(animXml,12192000,6858000,diagram,&out).ok,"reject occupied slide");
        diagram.items.clear();check(!patchDiagramXml(blank,12192000,6858000,diagram,&out).ok,"reject empty diagram");
    }
    AnimationOptions anim; anim.shapeId = 42; anim.fade = true; anim.durationMs = 750; anim.delayMs = 200;
    QByteArray animated;
    check(patchAnimationXml(animXml,anim,&animated).ok && animated.contains("spid=\"42\"") && animated.contains("dur=\"750\"") &&
        animated.contains("presetID=\"10\"") && animated.indexOf("<p:timing") > animated.indexOf("<p:transition") &&
        animated.indexOf("<p:timing") < animated.indexOf("<p:extLst"), "animation targets real shape and preserves transition order");
    // All eight added filter families support both entrance and exit.
    const QList<QPair<AnimationFilter,QByteArray>> extraFilters{
        {AnimationFilter::Wipe,"wipe(left)"},{AnimationFilter::Blinds,"blinds(vertical)"},
        {AnimationFilter::Circle,"circle(out)"},{AnimationFilter::Diamond,"diamond(out)"},
        {AnimationFilter::Dissolve,"dissolve"},{AnimationFilter::Wheel,"wheel(4)"},
        {AnimationFilter::Checkerboard,"checkerboard(across)"},{AnimationFilter::Split,"barn(outVertical)"}
    };
    for(const auto& pair:extraFilters)for(bool exiting:{false,true}){
        AnimationOptions extra;extra.shapeId=42;extra.durationMs=650;extra.filter=pair.first;extra.exit=exiting;
        QByteArray filtered,restored;QList<AnimationTiming> filterTiming;
        check(planAnimation({extra},&filterTiming).ok&&filterTiming.first().endMs==(exiting?651:650)&&
              patchAnimationXml(animXml,extra,&filtered).ok&&
              filtered.contains(QByteArray("filter=\"")+pair.second+"\"")&&
              filtered.contains(exiting?"transition=\"out\"":"transition=\"in\"")&&
              filtered.contains(exiting?"val=\"hidden\"":"val=\"visible\"")&&
              clearAnimationXml(filtered,&restored).ok&&restored==animXml,
              "extended animation filter roundtrip");
    }
    for(const auto f:{AnimationFilter::FlyLeft,AnimationFilter::FlyRight,AnimationFilter::FlyTop,AnimationFilter::FlyBottom,AnimationFilter::Zoom}){
        for(bool exit:{false,true}){
            AnimationOptions motion;motion.shapeId=42;motion.filter=f;motion.durationMs=800;motion.exit=exit;
            QByteArray result,restored;QList<AnimationTiming> times;
            const bool zoom=f==AnimationFilter::Zoom;
            check(planAnimation({motion},&times).ok&&times.first().endMs==(exit?801:800)&&
                  patchAnimationXml(animXml,motion,&result).ok&&
                  result.contains(zoom?"presetID=\"23\"":"presetID=\"2\"")&&
                  result.count("<p:anim calcmode=")==(zoom?2:1)&&result.contains("tm=\"100000\"")&&
                  result.contains("dur=\"800\"")&&clearAnimationXml(result,&restored).ok&&restored==animXml,
                  "motion and zoom animation keyframe roundtrip");
        }
    }
    AnimationOptions invalidFilter;invalidFilter.shapeId=42;invalidFilter.filter=static_cast<AnimationFilter>(99);
    check(!patchAnimationXml(animXml,invalidFilter,&out).ok,"reject unknown animation filter");
    check(!patchAnimationXml(animated,anim,&out).ok,"refuse overwrite existing animation");
    QByteArray cleared;
    check(clearAnimationXml(animated,&cleared).ok&&cleared==animXml,"clear generated animation preserves exact slide bytes");
    check(!clearAnimationXml(animXml,&cleared).ok&&cleared==animXml,"no timing rejected without changing output");
    QByteArray emptyTiming=animXml;emptyTiming.replace("<p:extLst/>","<p:timing/><p:extLst/>");
    check(clearAnimationXml(emptyTiming,&cleared).ok&&cleared==animXml,"clear self-closing timing only");
    QByteArray duplicateTiming=emptyTiming;duplicateTiming.replace("<p:extLst/>","<p:timing/><p:extLst/>");
    check(!clearAnimationXml(duplicateTiming,&cleared).ok,"reject duplicate timing");
    QByteArray complexTiming=animXml;complexTiming.replace("<p:extLst/>","<p:timing><p:tnLst><!-- keep parsing --><p:par/></p:tnLst></p:timing><p:extLst/>");
    check(clearAnimationXml(complexTiming,&cleared).ok&&cleared==animXml,"clear nested timing while retaining extLst and transition");
    anim.shapeId=43;
    check(!patchAnimationXml(animXml,anim,&out).ok,"reject missing shape");
    const QByteArray masterXml="<p:sldMaster xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"><p:cSld><p:bg><p:bgRef idx=\"1001\"/></p:bg><p:spTree/></p:cSld><p:clrMap/></p:sldMaster>";
    QByteArray masterOut;
    check(patchMasterBackground(masterXml,QStringLiteral("123abc"),&masterOut).ok && masterOut.contains("val=\"123ABC\"") && !masterOut.contains("bgRef") && masterOut.contains("<p:spTree/>"),"change actual master background, retain shape tree");
    check(!patchMasterBackground(masterXml,QStringLiteral("bad"),&out).ok,"reject invalid RGB");
    QByteArray twoShapes = animXml;
    twoShapes.replace("</p:spTree>", "<p:pic><p:nvPicPr><p:cNvPr id=\"43\" name=\"Picture\"/></p:nvPicPr></p:pic></p:spTree>");
    // Graphic frames (tables/charts) are animated as a whole, not individual cells or series.
    QByteArray frameXml=animXml;
    frameXml.replace("</p:spTree>","<p:graphicFrame><p:nvGraphicFramePr><p:cNvPr id=\"45\" name=\"Table\"/></p:nvGraphicFramePr></p:graphicFrame></p:spTree>");
    AnimationOptions frame; frame.shapeId=45; frame.fade=true;
    QByteArray frameAnimated;
    check(patchAnimationXml(frameXml,frame,&frameAnimated).ok &&
          frameAnimated.contains("spid=\"45\"") &&
          frameAnimated.contains("<p:graphicFrame>"),"animate a top-level graphic frame as a single target");
    QByteArray frameCleared;
    check(clearAnimationXml(frameAnimated,&frameCleared).ok&&frameCleared==frameXml,
          "clearing graphic frame timing preserves the original frame exactly");
    QByteArray groupFrame=animXml;
    groupFrame.replace("</p:spTree>","<p:grpSp><p:graphicFrame><p:nvGraphicFramePr><p:cNvPr id=\"45\" name=\"Nested\"/></p:nvGraphicFramePr></p:graphicFrame></p:grpSp></p:spTree>");
    check(!patchAnimationXml(groupFrame,frame,&out).ok,"reject nested graphic frame until grouped targets are supported");
    QByteArray groupXml=animXml;
    groupXml.replace("</p:spTree>","<p:grpSp><p:nvGrpSpPr><p:cNvPr id=\"46\" name=\"Group\"/></p:nvGrpSpPr><p:grpSpPr/><p:sp><p:nvSpPr><p:cNvPr id=\"47\" name=\"Child\"/></p:nvSpPr></p:sp><p:grpSp><p:nvGrpSpPr><p:cNvPr id=\"48\" name=\"Nested\"/></p:nvGrpSpPr><p:grpSpPr/></p:grpSp></p:grpSp></p:spTree>");
    AnimationOptions group; group.shapeId=46; group.fade=true;
    QByteArray groupAnimated;
    check(patchAnimationXml(groupXml,group,&groupAnimated).ok &&
          groupAnimated.contains("spid=\"46\"") && !groupAnimated.contains("spid=\"47\"") &&
          !groupAnimated.contains("spid=\"48\""),"animate entire top-level group without targeting descendants");
    QByteArray groupCleared;
    check(clearAnimationXml(groupAnimated,&groupCleared).ok && groupCleared==groupXml,
          "clear group animation preserves original group and children");
    group.shapeId=47;
    check(!patchAnimationXml(groupXml,group,&out).ok,"reject individual grouped child");
    group.shapeId=48;
    check(!patchAnimationXml(groupXml,group,&out).ok,"reject nested group");
    QByteArray duplicateGroup=groupXml; duplicateGroup.replace("id=\"47\"","id=\"46\"");
    group.shapeId=46;
    check(!patchAnimationXml(duplicateGroup,group,&out).ok,"reject duplicate ID between group and child");
    QByteArray duplicateFrame=frameXml;
    duplicateFrame.replace("id=\"45\"","id=\"42\"");
    check(!patchAnimationXml(duplicateFrame,frame,&out).ok,"reject duplicate frame identifier");
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
    check(!patchAnimationSequenceXml(twoShapes,{disappear,first},&out).ok,"reject entrance after exit on same shape");
    AnimationOptions later=disappear; later.delayMs=0;
    QByteArray enterExit;
    check(patchAnimationSequenceXml(twoShapes,{first,later},&enterExit).ok &&
          enterExit.count("spid=\"42\"")==3 && enterExit.contains("presetClass=\"entr\"") &&
          enterExit.contains("presetClass=\"exit\"") && enterExit.indexOf("val=\"visible\"")<enterExit.indexOf("val=\"hidden\""),
          "one entrance and one exit on same shape in separate click groups");
    later.trigger=AnimationTrigger::AfterPrevious;
    check(planAnimation({first,later},&plan).ok && plan.size()==2 && plan[1].click==1 &&
          plan[1].startMs==plan[0].endMs,"same shape exit after entrance within one click");
    later.trigger=AnimationTrigger::WithPrevious;later.delayMs=100;
    check(!planAnimation({first,later},&plan).ok,"reject overlapping entrance and exit");
    later.delayMs=750;
    check(planAnimation({first,later},&plan).ok && plan[1].startMs==750,"allow exit at exact entrance end");
    later.trigger=AnimationTrigger::Click;
    check(!planAnimation({first,later,later},&plan).ok,"reject duplicate exit");
    QXmlStreamReader pairReader(enterExit);QSet<QString> pairIds;bool pairUnique=true;
    while(!pairReader.atEnd()){pairReader.readNext();if(pairReader.isStartElement()&&pairReader.name()==QLatin1String("cTn")){
        const auto id=pairReader.attributes().value(QStringLiteral("id")).toString();
        if(id.isEmpty()||pairIds.contains(id))pairUnique=false;pairIds.insert(id);
    }}
    check(!pairReader.hasError() && pairUnique,"paired timing XML well formed with unique IDs");
    check(!patchAnimationXml(exitXml,disappear,&out).ok,"preserve existing exit timing");
    if (argc > 1) {
        const QString src = QString::fromLocal8Bit(argv[1]);
        QFile before(src); before.open(QIODevice::ReadOnly); const auto bytes = before.readAll(); before.close();
        QTemporaryDir temp; const QString dst = temp.filePath(QStringLiteral("result.pptx"));
        QString error; const auto entries = slideEntries(src, &error);
        check(!entries.isEmpty(), "read actual PPTX slide order");
        o.effect = Effect::Fade;
        const auto expected=sourceFingerprint(src);
        check(expected.size()==32,"capture SHA256 of actual PPTX");
        const QString changed=temp.filePath("changed-source.pptx");
        check(QFile::copy(src,changed),"make isolated source-change fixture");
        QFile changedFile(changed);
        if(changedFile.open(QIODevice::Append)){
            check(changedFile.write("changed")==7,"modify isolated source");
            changedFile.close();
        }else check(false,"open isolated source");
        const QString stale=temp.filePath("stale-output.pptx");
        check(!createRangeCopy(changed,stale,1,1,o,expected).ok&&!QFile::exists(stale),"stale transition source creates no output");
        check(!createClearAnimationCopy(changed,stale,1,expected).ok&&!QFile::exists(stale),"stale animation clear creates no output");
        check(!createAnimationSequenceCopy(changed,stale,1,{anim},expected).ok&&!QFile::exists(stale),"stale animation selection creates no output");
        check(!createMasterBackgroundCopy(changed,stale,QStringLiteral("ppt/slideMasters/slideMaster1.xml"),QStringLiteral("123ABC"),expected).ok&&!QFile::exists(stale),"stale master selection creates no output");
        const auto patched = createCopy(src, dst, 1, o,expected);
        if (!patched.ok) std::cerr << patched.error.toStdString() << '\n';
        check(patched.ok && slideEntries(dst, &error) == entries, "actual PPTX copy retains slide order");
        check(!createCopy(src, dst, 1, o).ok && !createCopy(src, src, 1, o).ok, "refuse overwrite");
        const auto badRange=temp.filePath(QStringLiteral("invalid-range.pptx"));
        check(!createRangeCopy(src,badRange,0,1,o).ok&&!QFile::exists(badRange),"invalid first slide creates no output");
        check(!createRangeCopy(src,badRange,2,1,o).ok&&!QFile::exists(badRange),"reversed range creates no output");
        check(!createRangeCopy(src,badRange,1,int(entries.size())+1,o).ok&&!QFile::exists(badRange),"out-of-range creates no output");
        const auto rangeDest=temp.filePath(QStringLiteral("range.pptx"));
        const auto batch=createRangeCopy(src,rangeDest,1,qMin(2,int(entries.size())),o);
        if(!batch.ok)std::cerr<<batch.error.toStdString()<<'\n';
        check(batch.ok&&slideEntries(rangeDest,&error)==entries,"actual PPTX batch transition retains slide order");
        check(!createRangeCopy(src,rangeDest,1,1,o).ok,"refuse existing batch destination");
        const auto shapes = slideShapes(src, 1, &error);
        check(!shapes.isEmpty(),"enumerate shapes in actual PPTX");
        if (!shapes.isEmpty()) {
            anim.shapeId=shapes.first().id;
            const QString animDest=temp.filePath(QStringLiteral("animation.pptx"));
            const auto animation=createAnimationCopy(src,animDest,1,anim);
            if (!animation.ok) std::cerr << animation.error.toStdString() << '\n';
            check(animation.ok && slideEntries(animDest,&error)==entries,"actual PPTX animation copy");
            check(slideShapes(animDest,1,&error).isEmpty() && !error.isEmpty(),"reject adding to existing timing");
            const auto clearDest=temp.filePath("cleared-animation.pptx");
            const auto clearedCopy=createClearAnimationCopy(animDest,clearDest,1);
            check(clearedCopy.ok&&slideEntries(clearDest,&error)==entries&&!slideShapes(clearDest,1,&error).isEmpty(),"actual PPTX clear and enumerate again");
            check(!createClearAnimationCopy(animDest,clearDest,1).ok,"refuse overwriting cleared copy");
            const auto rangeClear=temp.filePath("clear-range.pptx");
            const int last=qMin(2,int(entries.size()));
            const auto animHash=sourceFingerprint(animDest);
            check(!createClearAnimationRangeCopy(animDest,rangeClear,0,last,animHash).ok&&!QFile::exists(rangeClear),"reject invalid clear range");
            check(!createClearAnimationRangeCopy(animDest,rangeClear,1,last,expected).ok&&!QFile::exists(rangeClear),"reject stale range clear");
            const auto batchClear=createClearAnimationRangeCopy(animDest,rangeClear,1,last,animHash);
            check(batchClear.ok&&slideEntries(rangeClear,&error)==entries&&!slideShapes(rangeClear,1,&error).isEmpty(),"range clear removes animation and retains slide order");
            check(sourceFingerprint(animDest)==animHash,"range clear preserves source bytes");
            check(!createClearAnimationRangeCopy(animDest,rangeClear,1,last,animHash).ok,"refuse overwrite range clear");
            const auto alreadyStatic=temp.filePath("clear-static.pptx");
            check(createClearAnimationRangeCopy(src,alreadyStatic,1,1,expected).ok,"range clear permits static slide");
            const auto recreated=temp.filePath("recreated-animation.pptx");
            check(clearedCopy.ok&&createAnimationCopy(clearDest,recreated,1,anim).ok,"create new animation after clearing");
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
