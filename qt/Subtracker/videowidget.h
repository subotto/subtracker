#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include "framereader.h"

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
  explicit VideoWidget(QWidget *parent = 0);
  ~VideoWidget();

protected:
    void paintEvent(QPaintEvent *event);

signals:

public slots:

private:
    FrameCycle *f;
};

#endif // VIDEOWIDGET_H
