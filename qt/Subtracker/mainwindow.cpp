#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "logging.h"

#include <iomanip>

using namespace std;
using namespace chrono;
using namespace cv;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    frame_cycle(NULL),
    timer(this)
{
    ui->setupUi(this);
    this->timer.setInterval(50);
    connect(&this->timer, SIGNAL(timeout()), this, SLOT(update()));
    //this->timer.start();
    qRegisterMetaType< QSharedPointer< FrameAnalysis > >();
}

MainWindow::~MainWindow()
{
    delete ui;
    //delete this->worker;
}

void MainWindow::on_actionStart_triggered()
{
    bool res;
    this->ui->statusBar->showMessage("Start!", 1000);
    this->worker = new Worker();
    res = connect(this->worker, SIGNAL(frame_produced(QSharedPointer<FrameAnalysis>)), this, SLOT(receive_frame(QSharedPointer<FrameAnalysis>)), Qt::QueuedConnection);
    assert(res);
    res = connect(this->worker, SIGNAL(finished()), this, SLOT(when_worker_finished()), Qt::QueuedConnection);
    assert(res);
    this->worker->start();
    this->timer.start();
}

void MainWindow::on_actionStop_triggered()
{
    this->ui->statusBar->showMessage("Stop!", 1000);
    this->worker->stop();
    this->timer.stop();
}

void MainWindow::pass_frame_to_video(const QString &name, const Mat &frame) {
    auto video = *this->findChildren< VideoWidget* >(name).begin();
    video->set_current_frame(frame);
}

void MainWindow::pass_string_to_label(const QString &name, const QString &value) {
    auto label = *this->findChildren< QLabel* >(name).begin();
    label->setText(value);
}

static inline string time_point_to_string(const time_point< system_clock > &now) {
    return "";
}

static inline string duration_to_string(const system_clock::duration &dur) {
    double value = duration_cast< duration< double > >(dur).count();
    ostringstream stream;
    stream << fixed << setprecision(10) << value;
    return stream.str();
}

void MainWindow::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    BOOST_LOG_NAMED_SCOPE("when frame produced");
    //BOOST_LOG_TRIVIAL(debug) << "Received frame";
    this->pass_frame_to_video("mainVideo", frame->test_phase3);
    this->pass_string_to_label("processingTime", duration_to_string(frame->end_time - frame->begin_time).c_str());
}

void MainWindow::when_worker_finished() {
    this->worker->wait();
    delete this->worker;
    this->ui->statusBar->showMessage("Worker exited", 1000);
    BOOST_LOG_TRIVIAL(debug) << "Worker exited";
}
