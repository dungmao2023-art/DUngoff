// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "PdfOps.h"
#include <QPointF>
#include <optional>
namespace pdfedit {
bool available();
// Native image object on the current page. Center is normalized from top-left;
// widthPercent is a percentage of page width, aspect ratio is preserved.
dungpdf::Result insertImage(const QString& source,int page,const QString& imagePath,QPointF center,
                           double widthPercent,const QByteArray& expectedSource,const QString& dest);
// Select one unambiguous top-level image by a point, then move/resize it.
// widthPercent=0 retains the existing size.
dungpdf::Result moveImage(const QString& source,int page,QPointF selected,QPointF center,
                          double widthPercent,const QByteArray& expectedSource,const QString& dest);
// Replace one selected image with a new independent image object, fitting
// the old bounding box without distorting aspect ratio.
dungpdf::Result replaceImage(const QString& source,int page,QPointF selected,const QString& imagePath,
                             const QByteArray& expectedSource,const QString& dest);
// Remove one unambiguous top-level image, retaining the other page objects.
dungpdf::Result removeImage(const QString& source,int page,QPointF selected,
                            const QByteArray& expectedSource,const QString& dest);
dungpdf::Result replaceObject(const QString& source,int page,const QString& oldText,const QString& newText,const QString& dest,
                             const QByteArray& expectedSource = {}, std::optional<QPointF> selected = std::nullopt);
}
