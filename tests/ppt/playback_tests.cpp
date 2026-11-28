// SPDX-License-Identifier: MPL-2.0
// Exercise the actual SlideShow widget with a real PPTX and native Impress renderer.
#include "SlideShow.h"
#include "PptTransition.h"
#include <QApplication>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QKeyEvent>
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
    options.shapeId=shapes.first().id;options.fade=false;options.durationMs=1;
    const auto animated=temp.filePath("animated.pptx");
    const auto made=ppttransition::createAnimationCopy(args[2],animated,1,options);
    if(!made.ok){std::cerr<<"Cannot create animated fixture: "<<made.error.toStdString()<<std::endl;return 1;}
    std::string err;
    auto engine=dung::Engine::create(pathOf(args[1]),pathOf(temp.filePath("profile")),pathOf(temp.filePath("work")),&err);
    if(!engine){std::cerr<<err<<std::endl;return 1;}
    auto doc=engine->open(pathOf(animated),&err);
    if(!doc){std::cerr<<err<<std::endl;return 1;}
    std::cout<<"Native animation metadata: "<<doc->presentationInfo()<<std::endl;
    const int originalPart=doc->currentPart();
    auto* show=new SlideShow(doc.get(),0,nullptr,false);
    QApplication::processEvents();
    const auto before=show->grab().toImage();
    QPointer<SlideShow> safe(show);
    key(show,Qt::Key_Right);
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
    doc.reset();engine.reset();
    return ok?0:1;
}
