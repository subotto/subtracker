#include "videowidget.h"

#include <QPainter>

#include <chrono>
#include <cmath>

using namespace std;
using namespace chrono;
using namespace cv;

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
}

VideoWidget::~VideoWidget() {
}

void VideoWidget::set_current_frame(Mat current_frame)
{
    this->current_frame = current_frame;
    this->current_image = QImage();
}

static inline QImage mat_to_qimage(const Mat &mat) {
    if (mat.type() == CV_8UC3) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
    } else if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
    } else {
        assert("Unsupported matrix type" == 0);
    }
}

QImage &VideoWidget::get_current_image() {
    if (this->current_image.isNull()) {
        this->current_image = mat_to_qimage(this->current_frame);
    }
    return this->current_image;
}

template< typename T >
static inline float now_to_float(const time_point< T > &now) {
  return duration_cast< duration< float > >(now.time_since_epoch()).count();
}

static inline QRect fit_size(const QSize &size, const QRect &rect) {
    QSize rsize = rect.size();
    double wratio = ((double) rsize.width()) / ((double) size.width());
    double hratio = ((double) rsize.height()) / ((double) size.height());
    double asp_ratio = ((double) size.width()) / ((double) size.height());
    if (wratio > hratio) {
        int h = rect.height();
        int w = (int) floor(h * asp_ratio);
        int px = rect.x() + (rect.width() - w) / 2;
        int py = rect.y();
        return QRect(px, py, w, h);
    } else {
        int w = rect.width();
        int h = (int) floor(w / asp_ratio);
        int px = rect.x();
        int py = rect.y() + (rect.height() - h) / 2;
        return QRect(px, py, w, h);
    }
}

void VideoWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QRect rect = this->rect();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(0, 0, 0));
    painter.fillRect(rect, QColor(0, 0, 0));

    QImage &frame = this->get_current_image();
    QSize frame_size = frame.size();
    QRect draw_rect = fit_size(frame_size, rect);
    //painter.drawImage(0, 0, frame);
    painter.drawImage(draw_rect, frame);

}
