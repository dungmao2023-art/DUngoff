// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "PdfOps.h"
namespace pdfedit {
bool available();
dungpdf::Result replaceObject(const QString& source,int page,const QString& oldText,const QString& newText,const QString& dest,
                             const QByteArray& expectedSource = {});
}
