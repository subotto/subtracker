#include "videowidget.h"

#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{

}

void VideoWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawLine(50, 50, 100, 100);
}
