#include "videowidget.h"

#include <QPainter>

#include <chrono>
#include <cmath>

#include <opencv2/core/core.hpp>

using namespace std;
using namespace chrono;
using namespace cv;

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
  this->f = new JPEGReader("test.gjpeg", true, true);
  this->f->start();
}

VideoWidget::~VideoWidget() {
  delete this->f;
}

template< typename T >
static inline float now_to_float(const time_point< T > &now) {
  return duration_cast< duration< float > >(now.time_since_epoch()).count();
}

static inline QImage mat_to_qimage(const Mat &mat) {
    assert(mat.depth() == CV_8U);
    return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
}

void VideoWidget::paintEvent(QPaintEvent *event) {
  // Ack unused parameter
  (void) event;

  auto now = now_to_float(steady_clock::now());
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  //painter.drawLine(50*sin(now), 50, 100, 100);

  auto frame = f->get_last();
  painter.drawImage(0, 0, mat_to_qimage(frame.data));

}
