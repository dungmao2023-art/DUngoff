#include "SlideThumbs.h"
#include "SlideNavigation.h"
#include <algorithm>
#include <QIcon>
#include <QImage>
#include <QListWidgetItem>
#include <QPixmap>

void pptSetThumbnail(QListWidgetItem* item, dung::Document* doc, int part) {
    if (!item || !doc || part<0 || part>=doc->partCount()) return;
    long w = 0, h = 0;
    doc->documentSize(w, h);
    const QSize size=slideshow::thumbnailSize(w,h);
    if (!size.isValid()) return;
    const int pw=size.width(),ph=size.height();
    QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
    if(img.isNull()) return;
    img.fill(Qt::white);
    doc->paintPartTile(img.bits(), part, pw, ph, 0, 0, (int)w, (int)h);
    item->setIcon(QIcon(QPixmap::fromImage(img)));
}
