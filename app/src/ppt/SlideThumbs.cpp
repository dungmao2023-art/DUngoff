#include "SlideThumbs.h"
#include <algorithm>
#include <QIcon>
#include <QImage>
#include <QListWidgetItem>
#include <QPixmap>

void pptSetThumbnail(QListWidgetItem* item, dung::Document* doc, int part) {
    long w = 0, h = 0;
    doc->documentSize(w, h);
    if (!item || w <= 0 || h <= 0) return;
    const int pw = 128, ph = std::max(1, int(128.0 * h / w));
    QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    doc->paintPartTile(img.bits(), part, pw, ph, 0, 0, (int)w, (int)h);
    item->setIcon(QIcon(QPixmap::fromImage(img)));
}
