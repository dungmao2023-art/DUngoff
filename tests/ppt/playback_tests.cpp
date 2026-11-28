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
    std::_Exit(ok?0:1);
}
