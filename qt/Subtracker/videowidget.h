#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = 0);

protected:
    void paintEvent(QPaintEvent *event);

signals:

public slots:
};

#endif // VIDEOWIDGET_H
