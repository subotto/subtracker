#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

class VideoWidget;

#include <QWidget>

#include <opencv2/core/core.hpp>

#include "framereader.h"
#include "mainwindow.h"

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = 0);
    VideoWidget(VideoWidget *following, QWidget *parent = 0);
    ~VideoWidget();
    void set_current_frame(cv::Mat current_frame);
    void set_following(VideoWidget *following);
    void set_main(MainWindow *main);

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);

private:
    FrameCycle *f;
    cv::Mat current_frame;
    QImage current_image;
    QImage &get_current_image();
    VideoWidget *following;
    MainWindow *main;
};

#endif // VIDEOWIDGET_H
