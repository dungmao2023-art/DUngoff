#pragma once
#include "dung/Engine.h"
#include <QImage>
#include <QWidget>

// Trình chiếu toàn màn hình trong cửa sổ Qt: vẽ từng trang bằng paintPartTile,
// không đổi trang hiện tại của tài liệu và không mở cửa sổ LibreOffice.
class SlideShow : public QWidget {
    Q_OBJECT
public:
    SlideShow(dung::Document* doc, int startPart, QWidget* parent = nullptr, bool fullscreen = true);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    bool hidden(int part);
    void go(int delta);
    void render();

    dung::Document* m_doc;
    int m_part;
    int m_count;
    long m_w = 0, m_h = 0; // twip
    QImage m_img;
};
