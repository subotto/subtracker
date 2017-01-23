#include "videowidget.h"

#include <QPainter>

#include <chrono>
#include <cmath>

#include "logging.h"

using namespace std;
using namespace chrono;
using namespace cv;

VideoWidget::VideoWidget(VideoWidget *following, QWidget *parent) :
    QWidget(parent), following(following), main(NULL)
{
}

VideoWidget::VideoWidget(QWidget *parent) : VideoWidget(this, parent)
{
}

VideoWidget::~VideoWidget() {
}

void VideoWidget::set_current_frame(Mat current_frame)
{
    this->current_frame = current_frame;
    this->current_image = QImage();
}

void VideoWidget::set_following(VideoWidget *following)
{
    this->following = following;
}

void VideoWidget::set_main(MainWindow *main)
{
    BOOST_LOG_NAMED_SCOPE("VideoWidget::set_main()");
    BOOST_LOG_TRIVIAL(debug) << "called " << main;
    this->main = main;
}

static inline QImage mat_to_qimage(const Mat &mat) {
    const Mat *matp = &mat;
    Mat temp;
    if (matp->cols == 1) {
        repeat(*matp, 1, 15, temp);
        matp = &temp;
    }
    const Mat &mat2 = *matp;
    if (mat2.type() == CV_8UC3) {
        return QImage(mat2.data, mat2.cols, mat2.rows, mat2.step, QImage::Format_RGB888).rgbSwapped().copy();
    } else if (mat2.type() == CV_8UC1) {
        return QImage(mat2.data, mat2.cols, mat2.rows, mat2.step, QImage::Format_Grayscale8).copy();
    } else if (mat2.type() == CV_32F || mat2.type() == CV_64F) {
        Mat temp;
        mat2.convertTo(temp, CV_8UC1, 15.0, 255.0);
        return QImage(temp.data, temp.cols, temp.rows, temp.step, QImage::Format_Grayscale8).copy();
    } else {
        throw string("Unsupported matrix type: ") + getImgType(mat2.type());
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

    QImage &frame = this->following->get_current_image();
    QSize frame_size = frame.size();
    QRect draw_rect = fit_size(frame_size, rect);
    //painter.drawImage(0, 0, frame);
    painter.drawImage(draw_rect, frame);

}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    BOOST_LOG_NAMED_SCOPE("VideoWidget::mousePressEvent()");
    BOOST_LOG_TRIVIAL(debug) << "received " << this->main;
    Q_UNUSED(event);
    if (this->main != NULL) {
        this->main->get_main_video()->set_following(this);
    }
}
