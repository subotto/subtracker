#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "logging.h"
#include "categorybutton.h"

#include "ballpanel.h"
#include "foosmenpanel.h"
#include "beginningpanel.h"

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
    qRegisterMetaType< QSharedPointer< FrameAnalysis > >();
    this->add_all_frames();
}

void MainWindow::add_all_frames() {
    this->add_frame_to_tree(new BeginningPanel(this), "Table detection panel");
    this->add_frame_to_tree(new BallPanel(this), "Ball panel");
    this->add_frame_to_tree(new FoosmenPanel(this), "Foosmen panel");

    for (auto &sub_frame : this->sub_frames) {
        sub_frame->receive_settings(this->settings);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
    //delete this->worker;
}

void MainWindow::register_sub_frame(TreeSubFrame *sub_frame)
{
    this->sub_frames.push_back(sub_frame);
    connect(&this->timer, SIGNAL(timeout()), sub_frame, SLOT(update()));
}

VideoWidget *MainWindow::get_main_video()
{
    return this->ui->mainVideo;
}

// Heavily copied from http://www.fancyaddress.com/blog/qt-2/create-something-like-the-widget-box-as-in-the-qt-designer/
void MainWindow::add_frame_to_tree(QFrame *frame, const QString &button_text) {
    QTreeWidget *tree = this->ui->tree;
    QTreeWidgetItem *category = new QTreeWidgetItem();
    tree->addTopLevelItem(category);
    tree->setItemWidget(category, 0, new CategoryButton(button_text, tree, category));

    frame->setParent(tree);
    QTreeWidgetItem *container = new QTreeWidgetItem();
    container->setDisabled(true);
    category->addChild(container);
    tree->setItemWidget(container, 0, frame);
}

void MainWindow::on_actionStart_triggered()
{
    bool res;
    this->ui->statusBar->showMessage("Start!", 1000);
    this->worker = new Worker(this->settings);
    res = connect(this->worker, SIGNAL(frame_produced(QSharedPointer<FrameAnalysis>)), this, SLOT(receive_frame(QSharedPointer<FrameAnalysis>)), Qt::QueuedConnection);
    assert(res);
    res = connect(this->worker, SIGNAL(finished()), this, SLOT(when_worker_finished()), Qt::QueuedConnection);
    assert(res);
    this->worker->start();
    this->timer.start();
}

void MainWindow::settings_modified() {
    if (this->worker != NULL) {
        this->worker->set_settings(this->settings);
    }
}

FrameSettings &MainWindow::get_settings()
{
    return this->settings;
}

void MainWindow::on_actionStop_triggered()
{
    this->ui->statusBar->showMessage("Stop!", 1000);
    this->worker->stop();
    this->timer.stop();
}

void MainWindow::pass_frame_to_video(VideoWidget *video, const Mat &frame) {
    //auto video = *this->findChildren< VideoWidget* >(name).begin();
    video->set_current_frame(frame);
}

void MainWindow::pass_string_to_label(QLabel *label, const QString &value) {
    //auto label = *this->findChildren< QLabel* >(name).begin();
    label->setText(value);
}

static inline string time_point_to_string(const time_point< system_clock > &now) {
    ostringstream stream;
    time_t time = duration_cast< seconds >(now.time_since_epoch()).count();
    double fractional = duration_cast< duration< double > >(now.time_since_epoch()).count() - time;
    assert(0.0 <= fractional && fractional < 1.0);
    struct tm tm;
    localtime_r(&time, &tm);
    stream << put_time(&tm, "%x %T") << "." << fixed << setprecision(0) << fractional * 1000;
    return stream.str();
}

static inline string duration_to_string(const system_clock::duration &dur) {
    ostringstream stream;
    double value = duration_cast< duration< double, milli > >(dur).count();
    stream << fixed << setprecision(2) << value << " ms";
    return stream.str();
}

void MainWindow::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    BOOST_LOG_NAMED_SCOPE("when frame produced");
    //BOOST_LOG_TRIVIAL(debug) << "Received frame";
    auto now = system_clock::now();
    this->pass_string_to_label(this->ui->currentTime, time_point_to_string(now).c_str());
    this->pass_string_to_label(this->ui->frameTime, time_point_to_string(frame->time).c_str());
    this->pass_string_to_label(this->ui->totalFrameDelay, duration_to_string(now - frame->time).c_str());
    this->pass_string_to_label(this->ui->acquisitionTime, time_point_to_string(frame->acquisitionTime).c_str());
    this->pass_string_to_label(this->ui->localFrameDelay, duration_to_string(now - frame->acquisitionTime).c_str());
    this->pass_string_to_label(this->ui->phase1Time, duration_to_string(frame->phase1_time()).c_str());
    this->pass_string_to_label(this->ui->phase2Time, duration_to_string(frame->phase2_time()).c_str());
    this->pass_string_to_label(this->ui->phase3Time, duration_to_string(frame->phase3_time()).c_str());
    this->pass_string_to_label(this->ui->totalProcessingTime, duration_to_string(frame->total_processing_time()).c_str());

    for (auto &sub_frame : this->sub_frames) {
        sub_frame->receive_frame(frame);
    }
}

void MainWindow::when_worker_finished() {
    this->worker->wait();
    delete this->worker;
    this->ui->statusBar->showMessage("Worker exited", 1000);
    BOOST_LOG_TRIVIAL(debug) << "Worker exited";
}
