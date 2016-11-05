#include "videowidget.h"

#include <QPainter>

#include <chrono>
#include <cmath>

using namespace std;
using namespace chrono;

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{

}

template< typename T >
float now_to_float(const time_point< T > &now) {
  return duration_cast< duration< float > >(now.time_since_epoch()).count();
}

void VideoWidget::paintEvent(QPaintEvent *event) {
  auto now = now_to_float(steady_clock::now());
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.drawLine(50*sin(now), 50, 100, 100);
}
