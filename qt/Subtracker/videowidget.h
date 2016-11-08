#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>

#include <opencv2/core/core.hpp>

#include "framereader.h"

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = 0);
    ~VideoWidget();
    void set_current_frame(cv::Mat current_frame);

protected:
    void paintEvent(QPaintEvent *event);

private:
    FrameCycle *f;
    cv::Mat current_frame;
};

#endif // VIDEOWIDGET_H
