#pragma once
#include "dung/Engine.h"
#include <QImage>
#include <QWidget>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QPointF>
#include <QElapsedTimer>
#include "SlideTransitionPlayback.h"

class QScreen;

// Trình chiếu toàn màn hình trong cửa sổ Qt: vẽ từng trang bằng paintPartTile,
// không đổi trang hiện tại của tài liệu và không mở cửa sổ LibreOffice.
class SlideShow : public QWidget {
    Q_OBJECT
public:
    SlideShow(dung::Document* doc, int startPart, QWidget* parent = nullptr, bool fullscreen = true, QScreen* targetScreen = nullptr, int intervalMs = 0, slideshow::SignatureStyle signature = slideshow::SignatureStyle::Native);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    bool hidden(int part);
    void go(int delta);
    void render();
    void restartAdvance();
    bool prepareEntrance();
    void resetEntrance();
    void stopTransition();
    void startTransition(const QImage& previous);
    QImage m_transitionFrom;
    QTimer m_transitionTimer;
    QElapsedTimer m_transitionClock;
    slideshow::TransitionPlan m_transitionPlan;
    QRect m_entranceTransitionBounds;
    QImage m_entranceTransitionOld,m_entranceTransitionNew;
    QVector<slideshow::TransitionPlan> m_entrancePlans;
    slideshow::SignatureStyle m_signature=slideshow::SignatureStyle::Native;

    dung::Document* m_doc;
    int m_part;
    int m_count;
    QVector<bool> m_visible;
    QString m_digits;
    int m_blank = 0; // 0: slide, 1: black, 2: white
    long m_w = 0, m_h = 0; // twip
    QImage m_img;
    QVector<QImage> m_entranceFrames;
    int m_entranceStep=0;
    int m_entrancePart=-1;
    QTimer m_advance;
    int m_intervalMs = 0;
    bool m_paused = false;
    bool m_laserMode = false;
    QPointF m_laserPoint{-1,-1};
};
