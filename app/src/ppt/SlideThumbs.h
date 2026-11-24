#pragma once
#include "dung/Engine.h"
class QListWidgetItem;

// Gắn ảnh thu nhỏ của trang chiếu (vẽ bằng paintPartTile, không đổi trang hiện tại).
void pptSetThumbnail(QListWidgetItem* item, dung::Document* doc, int part);
