// SPDX-License-Identifier: MPL-2.0
// Exercise the actual SlideShow widget with a real PPTX and native Impress renderer.
#include "SlideShow.h"
#include "PptTransition.h"
#include <QApplication>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QJsonDocument>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <cmath>
#include <QThread>
#include <QElapsedTimer>
#include "SimpleEntrance.h"
#include <QPainter>
#include "SlideTransitionPlayback.h"

namespace {
std::filesystem::path pathOf(const QString& path){
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}
void key(QWidget* widget,int code){
    QKeyEvent ev(QEvent::KeyPress,code,Qt::NoModifier);
    QCoreApplication::sendEvent(widget,&ev);
    QApplication::processEvents();
}
int bluePixels(const QImage& img){
    int count=0;
    for(int y=0;y<img.height();++y)for(int x=0;x<img.width();++x){
        const auto c=img.pixelColor(x,y);
        if(c.blue()>120&&c.blue()>c.red()+55&&c.blue()>c.green()+35)++count;
    }
    return count;
}
int redPixels(const QImage& img){
    int count=0;
    for(int y=0;y<img.height();++y)for(int x=0;x<img.width();++x){
        const auto c=img.pixelColor(x,y);
        if(c.red()>120&&c.red()>c.blue()+55&&c.red()>c.green()+35)++count;
    }
    return count;
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    const auto args=app.arguments();
    if(args.size()!=3){std::cerr<<"usage: dung-pptplaybacktest PROGRAM PPTX\n";return 2;}
    // Exercise the same painter used by the real SlideShow. Endpoints,
    // intermediate frames and direction must differ from a static cut.
    {
        using namespace slideshow;
        QImage oldFrame(128,128,QImage::Format_ARGB32_Premultiplied),newFrame=oldFrame;
        oldFrame.fill(Qt::red);newFrame.fill(Qt::blue);
        auto draw=[&](const TransitionPlan& plan,qreal t){
            QImage img(128,128,QImage::Format_ARGB32_Premultiplied);img.fill(Qt::black);
            QPainter p(&img);paintTransition(p,QRectF(0,0,128,128),oldFrame,newFrame,plan,t);p.end();return img;
        };
        const auto fade=transitionPlan(QJsonObject{{"transitionType","Fade"},{"transitionSubtype","CrossFade"},{"transitionDuration",500}});
        const auto push=transitionPlan(QJsonObject{{"transitionType","PushWipe"},{"transitionSubtype","FromRight"},{"transitionDuration",500}});
        const auto wipe=transitionPlan(QJsonObject{{"transitionType","BarWipe"},{"transitionSubtype","LeftToRight"},{"transitionDirection",true},{"transitionDuration",500}});
        const auto dissolve=transitionPlan(QJsonObject{{"transitionType","Dissolve"},{"transitionDuration",500}});
        for(const auto& plan:{fade,push,wipe,dissolve}){
            if(plan.kind==TransitionKind::Cut||draw(plan,0)!=oldFrame||draw(plan,1)!=newFrame||
               draw(plan,.5)==oldFrame||draw(plan,.5)==newFrame){
                std::cerr<<"FAIL transition endpoints/intermediate pixels"<<std::endl;return 1;
            }
        }
        const auto pushMid=draw(push,.5),wipeMid=draw(wipe,.5);
        if(pushMid.pixelColor(8,64)!=Qt::red||pushMid.pixelColor(120,64)!=Qt::blue||
           wipeMid.pixelColor(8,64)!=Qt::blue||wipeMid.pixelColor(120,64)!=Qt::red||
           draw(dissolve,.5)!=draw(dissolve,.5)||
           transitionPlan(QJsonObject{{"transitionType","Zoom"},{"transitionDuration",500}}).kind!=TransitionKind::Cut){
            std::cerr<<"FAIL direction or unsupported fallback"<<std::endl;return 1;
        }
        const auto silk=signaturePlan(SignatureStyle::SilkWave),prism=signaturePlan(SignatureStyle::PrismTiles),
                   focus=signaturePlan(SignatureStyle::FocusLens),ink=signaturePlan(SignatureStyle::InkBloom),
                   hologram=signaturePlan(SignatureStyle::HologramScan);
        for(const auto& plan:{silk,prism,focus,ink,hologram}){
            if(plan.kind==TransitionKind::Cut||plan.durationMs<=0||
               draw(plan,0)!=oldFrame||draw(plan,1)!=newFrame||
               draw(plan,.5)==oldFrame||draw(plan,.5)==newFrame||
               draw(plan,.35)==draw(plan,.65)){
                std::cerr<<"FAIL signature transition endpoints/motion"<<std::endl;return 1;
            }
        }
        if(draw(silk,.5)==draw(prism,.5)||draw(silk,.5)==draw(focus,.5)||
           draw(prism,.5)==draw(focus,.5)||signaturePlan(SignatureStyle::Native).kind!=TransitionKind::Cut){
            std::cerr<<"FAIL distinct signature effects"<<std::endl;return 1;
        }
        if(draw(ink,.4)==draw(hologram,.4)||draw(ink,.4)==draw(focus,.4)||
           draw(hologram,.4)==draw(silk,.4)){
            std::cerr<<"FAIL distinct ink and hologram effects"<<std::endl;return 1;
        }
        const QList<QPair<QString,QString>> extraMasks{
            {"BlindWipe","Vertical"},{"EllipseWipe","Horizontal"},{"IrisWipe","Diamond"},
            {"PineWheelWipe","4Blade"},{"CheckerBoardWipe","Across"},{"BarnDoorWipe","Vertical"}};
        for(const auto& mask:extraMasks){
            const auto plan=transitionPlan(QJsonObject{{"transitionType",mask.first},{"transitionSubtype",mask.second},
                {"transitionDirection",true},{"transitionDuration",500}});
            if(plan.kind==TransitionKind::Cut||draw(plan,0)!=oldFrame||draw(plan,1)!=newFrame||
               draw(plan,.4)==oldFrame||draw(plan,.4)==newFrame||draw(plan,.3)==draw(plan,.6)){
                std::cerr<<"FAIL extended transition mask "<<mask.first.toStdString()<<std::endl;return 1;
            }
        }
        std::cout<<"PASS extended masks and existing transition painter"<<std::endl;
    }
    {
        const auto tree=QJsonDocument::fromJson(R"({"nodeName":"Par","nodeType":"OnClick","children":[{"nodeName":"Set","attributeName":"Visibility","to":"visible","targetElement":"shape","begin":"0s"},{"nodeName":"TransitionFilter","targetElement":"shape","transitionType":"Fade","transitionSubType":"CrossFade","dur":"0.75s","begin":"0s"}]})").object();
        QVector<slideshow::ClickEntrance> plans;
        if(!slideshow::findEntrancePlans(tree,&plans)||plans.size()!=1||
           plans[0].plan.kind!=slideshow::TransitionKind::Fade||plans[0].plan.durationMs!=750){
            std::cerr<<"FAIL entrance filter metadata"<<std::endl;return 1;
        }
        auto invalid=tree;auto children=invalid.value("children").toArray();
        auto leaf=children[1].toObject();leaf.insert("targetElement","wrong");children[1]=leaf;invalid.insert("children",children);
        if(slideshow::findEntrancePlans(invalid,&plans)){
            std::cerr<<"FAIL mismatched animation target accepted"<<std::endl;return 1;
        }
        QImage first(64,64,QImage::Format_ARGB32_Premultiplied);first.fill(Qt::white);
        QImage second=first.copy();second.setPixelColor(3,4,Qt::blue);second.setPixelColor(18,22,Qt::red);
        if(slideshow::changedBounds(first,second)!=QRect(QPoint(3,4),QPoint(18,22))){
            std::cerr<<"FAIL object animation bounds"<<std::endl;return 1;
        }
    }
    {
        using namespace slideshow;
        const auto fly=QJsonDocument::fromJson(R"({"nodeName":"Par","nodeType":"OnClick","children":[{"nodeName":"Set","attributeName":"Visibility","to":"visible","targetElement":"shape"},{"nodeName":"Animate","attributeName":"X","targetElement":"shape","dur":"0.8s","values":"0-width/2;x","keyTimes":"0;1"}]})").object();
        const auto zoom=QJsonDocument::fromJson(R"({"nodeName":"Par","nodeType":"OnClick","children":[{"nodeName":"Set","attributeName":"Visibility","to":"visible","targetElement":"shape"},{"nodeName":"Animate","attributeName":"Width","targetElement":"shape","dur":"0.8s","values":"0;width"},{"nodeName":"Animate","attributeName":"Height","targetElement":"shape","dur":"0.8s","values":"0;height"}]})").object();
        QVector<ClickEntrance> plans;
        if(!findEntrancePlans(fly,&plans)||plans[0].plan.kind!=TransitionKind::Fly||plans[0].plan.dx!=-1||
           !findEntrancePlans(zoom,&plans)||plans[0].plan.kind!=TransitionKind::Zoom){
            std::cerr<<"FAIL motion metadata"<<std::endl;return 1;
        }
        QImage layer(100,100,QImage::Format_ARGB32_Premultiplied);layer.fill(Qt::transparent);
        {QPainter p(&layer);p.fillRect(QRect(60,40,20,20),Qt::blue);}
        auto frame=[&](TransitionPlan plan,qreal f){
            QImage result(100,100,QImage::Format_ARGB32_Premultiplied);result.fill(Qt::white);
            QPainter p(&result);paintMotionLayer(p,QRectF(0,0,100,100),layer,QRect(60,40,20,20),plan,f);
            p.fillRect(QRect(65,45,4,4),Qt::red);p.end();return result;
        };
        const auto f=frame({TransitionKind::Fly,800,-1,0},.5);
        const auto z=frame({TransitionKind::Zoom,800,0,0},.5);
        if(f.pixelColor(30,50)!=Qt::blue||f.pixelColor(70,50)!=Qt::white||
           z.pixelColor(70,50)!=Qt::blue||z.pixelColor(61,41)!=Qt::white||
           z.pixelColor(66,46)!=Qt::red||frame({TransitionKind::Fly,800,-1,0},1)!=frame({TransitionKind::Zoom,800,0,0},1)){
            std::cerr<<"FAIL isolated motion layer and overlay order"<<std::endl;return 1;
        }
        std::cout<<"PASS isolated Fly/Zoom renderer and metadata"<<std::endl;
    }
    QTemporaryDir temp;
    if(!temp.isValid())return 2;
    QString error;
    const auto shapes=ppttransition::slideShapes(args[2],1,&error);
    if(shapes.size()!=2){std::cerr<<"Expected two fixture shapes: "<<error.toStdString()<<std::endl;return 1;}
    ppttransition::AnimationOptions options;
    options.shapeId=shapes.first().id;options.fade=false;options.durationMs=100;
    const auto animated=temp.filePath("animated.pptx");
    const auto made=ppttransition::createAnimationCopy(args[2],animated,1,options);
    if(!made.ok){std::cerr<<"Cannot create animated fixture: "<<made.error.toStdString()<<std::endl;return 1;}
    std::string err;
    auto engine=dung::Engine::create(pathOf(args[1]),pathOf(temp.filePath("profile")),pathOf(temp.filePath("work")),&err);
    if(!engine){std::cerr<<err<<std::endl;return 1;}
    // The diagram must be a real editable PPTX rendered by the engine,
    // not just well-formed XML or an image preview.
    {
        const auto source=QFileInfo(args[2]).absolutePath()+"/ppt-diagram-input.pptx";
        const auto fingerprint=ppttransition::sourceFingerprint(source);
        if(fingerprint.isEmpty()){std::cerr<<"FAIL diagram fixture missing"<<std::endl;return 1;}
        auto original=engine->open(pathOf(source),&err);
        if(!original||original->partCount()!=2){std::cerr<<"FAIL open diagram fixture"<<std::endl;return 1;}
        auto render=[](dung::Document* doc,int part){
            long w=0,h=0;doc->documentSize(w,h);
            if(w<=0||h<=0)return QImage{};
            QImage frame(640,480,QImage::Format_ARGB32_Premultiplied);frame.fill(Qt::white);
            if(!doc->paintPartTile(frame.bits(),part,640,480,0,0,int(w),int(h)))return QImage{};
            return frame;
        };
        const auto blank=render(original.get(),0),neighbour=render(original.get(),1);
        if(blank.isNull()||neighbour.isNull()||blank==neighbour){std::cerr<<"FAIL diagram baseline"<<std::endl;return 1;}
        for(const auto layout:{pptdiagram::Layout::Process,pptdiagram::Layout::MindMap}){
            pptdiagram::Options o;o.title=QStringLiteral("Sơ đồ thử nghiệm");
            o.items={QStringLiteral("Ý tưởng"),QStringLiteral("Chuẩn bị"),QStringLiteral("Thực hiện"),QStringLiteral("Kết quả")};o.layout=layout;
            const auto path=temp.filePath(QStringLiteral("diagram-%1.pptx").arg(int(layout)));
            const auto made=ppttransition::createDiagramCopy(source,path,1,o,fingerprint);
            if(!made.ok){std::cerr<<"FAIL diagram export "<<made.error.toStdString()<<std::endl;return 1;}
            QString error;QList<pptdiagram::Node> planned;pptdiagram::plan(o,&planned);
            const auto shapes=ppttransition::slideShapes(path,1,&error);
            // Native shape inventory includes the attached connectors where supported.
            if(shapes.size()<planned.size()){
                std::cerr<<"FAIL diagram editable shape inventory "<<error.toStdString()<<std::endl;return 1;
            }
            auto generated=engine->open(pathOf(path),&err);
            if(!generated||generated->partCount()!=2){std::cerr<<"FAIL open exported diagram "<<err<<std::endl;return 1;}
            const auto changed=render(generated.get(),0),other=render(generated.get(),1);
            if(changed.isNull()||changed==blank||bluePixels(changed)<500||other!=neighbour||
               ppttransition::sourceFingerprint(source)!=fingerprint){
                std::cerr<<"FAIL diagram rendered pixels or unchanged neighbour"<<std::endl;return 1;
            }
            std::cout<<"PASS native diagram render and editable shapes "<<int(layout)<<std::endl;
        }
    }
    auto doc=engine->open(pathOf(animated),&err);
    if(!doc){std::cerr<<err<<std::endl;return 1;}
    doc->setClientView(512,512,7680,7680,0,0,7680,7680);
    doc->waitInputIdle(350,4000);
    std::cout<<"Native animation metadata: "<<doc->presentationInfo()<<std::endl;
    const bool supported=doc->supportsSlideLayers();
    std::cout<<"Native layer ABI: "<<supported<<std::endl;
    const bool requireNative=qEnvironmentVariableIsSet("DUNG_REQUIRE_NATIVE_ENTRANCE");
    if(requireNative&&!supported){std::cerr<<"FAIL required native layer ABI missing"<<std::endl;return 1;}
    qputenv("DUNG_PPT_LAYER_TRACE","1");
    const int originalPart=doc->currentPart();
    auto* show=new SlideShow(doc.get(),0,nullptr,false);
    QApplication::processEvents();
    const auto before=show->grab().toImage();
    std::cout<<"Before click: "<<before.width()<<"x"<<before.height()<<" blue="<<bluePixels(before)<<" red="<<redPixels(before)<<std::endl;
    QPointer<SlideShow> safe(show);
    // Laser is a transient overlay, never a document operation or an animation click.
    key(show,Qt::Key_L);
    const QPointF center=QRectF(show->rect()).center();
    const QPointF global=show->mapToGlobal(center.toPoint());
    QMouseEvent move(QEvent::MouseMove,center,global,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(show,&move);
    const auto withLaser=show->grab().toImage();
    QMouseEvent click(QEvent::MouseButtonPress,center,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QCoreApplication::sendEvent(show,&click);
    const bool stillShowing=safe&&safe->isVisible();
    if(!stillShowing){std::cerr<<"FAIL laser click closed show"<<std::endl;std::_Exit(1);}
    const auto afterLaserClick=show->grab().toImage();
    key(show,Qt::Key_L);
    const bool laserOk=!before.isNull()&&withLaser.size()==before.size()&&withLaser!=before&&
        withLaser==afterLaserClick&&show->grab().toImage()==before&&doc->currentPart()==originalPart;
    std::cout<<(laserOk?"PASS":"FAIL")<<" laser overlay visible, click does not advance, toggle restores original pixels"<<std::endl;
    if(!laserOk)std::_Exit(1);
    key(show,Qt::Key_Right);
    // Older system engines without the layer ABI must still render an intact
    // static show and navigate normally, never claim animation support.
    if(!supported){
        const bool ok=!before.isNull()&&bluePixels(before)>500&&redPixels(before)>500&&
            (!safe||!safe->isVisible())&&doc->currentPart()==originalPart;
        std::cout<<(ok?"PASS":"FAIL")<<" static fallback on engine without native layer ABI"<<std::endl;
        std::_Exit(ok?0:1);
    }
    // A static fallback would close a one-slide show instead of showing an effect.
    if(!safe||!safe->isVisible()){std::cerr<<"FAIL click advanced instead of playing effect"<<std::endl;std::_Exit(1);}
    const auto after=show->grab().toImage();
    key(show,Qt::Key_Left);
    const auto undone=show->grab().toImage();
    const int b=bluePixels(before),a=bluePixels(after),r0=redPixels(before),r1=redPixels(after);
    std::cout<<"Native pixels blue before/after "<<b<<"/"<<a<<", red "<<r0<<"/"<<r1<<std::endl;
    const bool ok=!before.isNull()&&before.size()==after.size()&&before!=after&&before==undone&&
        a>b+500&&r0>500&&r1>500&&std::abs(r0-r1)<100&&doc->currentPart()==originalPart;
    std::cout<<(ok?"PASS":"FAIL")<<" actual entrance click/back preserves neighbour and editor slide"<<std::endl;
    delete show;
    if(!ok)std::_Exit(1);
    const auto sequence=temp.filePath("sequence.pptx");
    auto second=options;second.shapeId=shapes[1].id;
    const auto seq=ppttransition::createAnimationSequenceCopy(args[2],sequence,1,{options,second});
    if(!seq.ok){std::cerr<<seq.error.toStdString()<<std::endl;std::_Exit(1);}
    auto multi=engine->open(pathOf(sequence),&err);
    if(!multi){std::cerr<<err<<std::endl;std::_Exit(1);}
    multi->setClientView(512,512,7680,7680,0,0,7680,7680);
    multi->waitInputIdle(350,4000);
    std::cout<<"Multi metadata: "<<multi->presentationInfo()<<std::endl;
    const int originalMulti=multi->currentPart();
    auto* playback=new SlideShow(multi.get(),0,nullptr,false);
    QPointer<SlideShow> live(playback);
    QApplication::processEvents();
    const auto zero=playback->grab().toImage();
    key(playback,Qt::Key_Right);
    if(!live||!live->isVisible())std::_Exit(1);
    const auto one=playback->grab().toImage();
    key(playback,Qt::Key_Right);
    if(!live||!live->isVisible())std::_Exit(1);
    const auto two=playback->grab().toImage();
    key(playback,Qt::Key_Left);
    const auto backOne=playback->grab().toImage();
    key(playback,Qt::Key_Left);
    const auto backZero=playback->grab().toImage();
    const bool multiOk=bluePixels(zero)<100&&redPixels(zero)<100&&
        bluePixels(one)>500&&redPixels(one)<100&&
        bluePixels(two)>500&&redPixels(two)>500&&one==backOne&&zero==backZero&&
        multi->currentPart()==originalMulti;
    std::cout<<(multiOk?"PASS":"FAIL")<<" two native click entrances and backward steps"<<std::endl;
    delete playback;
    if(!multiOk)std::_Exit(1);
    // Real PPTX with one timed entrance; verify an actual intermediate frame
    // and return to the hidden state. This is deliberately not a static XML test.
    auto fadeOptions=options;fadeOptions.fade=true;fadeOptions.durationMs=1000;
    const auto fadeFile=temp.filePath("fade-entrance.pptx");
    if(!ppttransition::createAnimationCopy(args[2],fadeFile,1,fadeOptions).ok)std::_Exit(1);
    auto fadeDoc=engine->open(pathOf(fadeFile),&err);
    if(!fadeDoc)std::_Exit(1);
    fadeDoc->setClientView(512,512,7680,7680,0,0,7680,7680);
    fadeDoc->waitInputIdle(350,4000);
    std::cout<<"Fade metadata: "<<fadeDoc->presentationInfo()<<std::endl;
    auto* fadeShow=new SlideShow(fadeDoc.get(),0,nullptr,false);
    QPointer<SlideShow> liveFade(fadeShow);
    QApplication::processEvents();
    const auto hidden=fadeShow->grab().toImage();
    key(fadeShow,Qt::Key_Right);
    if(!liveFade||!liveFade->isVisible())std::_Exit(1);
    auto pump=[](int ms){QElapsedTimer elapsed;elapsed.start();while(elapsed.elapsed()<ms){QApplication::processEvents();QThread::msleep(10);}};
    pump(300);const auto middle=fadeShow->grab().toImage();
    pump(900);const auto finished=fadeShow->grab().toImage();
    key(fadeShow,Qt::Key_Left);
    const bool fadeOk=hidden!=middle&&middle!=finished&&bluePixels(finished)>bluePixels(hidden)+500&&
                      fadeShow->grab().toImage()==hidden;
    std::cout<<(fadeOk?"PASS":"FAIL")<<" native timed fade entrance with intermediate pixels"<<std::endl;
    delete fadeShow;
    if(!fadeOk)std::_Exit(1);
    // Exercise real exported OOXML motion with native layer metadata, not just
    // the synthetic renderer. A static fallback on a one-slide fixture closes
    // on next, so it cannot be mistaken for successful animation playback.
    for(const auto filter:{ppttransition::AnimationFilter::FlyLeft,ppttransition::AnimationFilter::Zoom}){
        auto motionOptions=options;motionOptions.filter=filter;motionOptions.durationMs=1200;
        const bool zoom=filter==ppttransition::AnimationFilter::Zoom;
        const auto file=temp.filePath(zoom?"zoom-entrance.pptx":"fly-entrance.pptx");
        const auto made=ppttransition::createAnimationCopy(args[2],file,1,motionOptions);
        if(!made.ok){std::cerr<<"FAIL motion export "<<made.error.toStdString()<<std::endl;std::_Exit(1);}
        auto motionDoc=engine->open(pathOf(file),&err);
        if(!motionDoc){std::cerr<<"FAIL motion open "<<err<<std::endl;std::_Exit(1);}
        motionDoc->setClientView(512,512,7680,7680,0,0,7680,7680);
        motionDoc->waitInputIdle(350,4000);
        std::cout<<(zoom?"Zoom":"Fly")<<" metadata: "<<motionDoc->presentationInfo()<<std::endl;
        const auto originalPart=motionDoc->currentPart();
        auto* motionShow=new SlideShow(motionDoc.get(),0,nullptr,false);
        QPointer<SlideShow> liveMotion(motionShow);
        QApplication::processEvents();
        const auto initial=motionShow->grab().toImage();
        key(motionShow,Qt::Key_Right);
        if(!liveMotion||!liveMotion->isVisible()){
            std::cerr<<"FAIL native motion treated as static slide"<<std::endl;std::_Exit(1);
        }
        pump(350);const auto moving=motionShow->grab().toImage();
        pump(1000);const auto final=motionShow->grab().toImage();
        key(motionShow,Qt::Key_Left);
        const auto reversed=motionShow->grab().toImage();
        const bool motionOk=initial!=moving&&moving!=final&&bluePixels(final)>bluePixels(initial)+500&&
            initial==reversed&&redPixels(initial)>500&&
            std::abs(redPixels(initial)-redPixels(final))<100&&motionDoc->currentPart()==originalPart;
        std::cout<<(motionOk?"PASS":"FAIL")<<" actual "<<(zoom?"Zoom":"Fly")<<" entrance frames and reverse"<<std::endl;
        delete motionShow;
        if(!motionOk)std::_Exit(1);
    }
    std::_Exit(0);
}
