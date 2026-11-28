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
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QJsonDocument>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <cmath>
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
                   focus=signaturePlan(SignatureStyle::FocusLens);
        for(const auto& plan:{silk,prism,focus}){
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
        std::cout<<"PASS transition painter: Fade, Push, Wipe, Dissolve, Silk, Prism, Focus"<<std::endl;
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
    std::_Exit(multiOk?0:1);
}
