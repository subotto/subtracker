#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "logging.h"

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
}

MainWindow::~MainWindow()
{
    delete ui;
    //delete this->worker;
}

void MainWindow::on_actionStart_triggered()
{
    this->ui->statusBar->showMessage("Start!", 1000);
    this->worker = new Worker();
    bool res = connect(this->worker, SIGNAL(frame_produced(QSharedPointer<FrameAnalysis>)), this, SLOT(when_frame_produced(QSharedPointer<FrameAnalysis>)), Qt::QueuedConnection);
    assert(res);
    connect(this->worker, SIGNAL(frame_produced(QSharedPointer<FrameAnalysis>)), qApp, SLOT(aboutQt()));
    this->worker->start();
    this->timer.start();
}

void MainWindow::on_actionStop_triggered()
{
    this->ui->statusBar->showMessage("Stop!", 1000);
    //delete this->worker;
    this->timer.stop();
}

void MainWindow::when_frame_produced(QSharedPointer<FrameAnalysis> frame)
{
    BOOST_LOG_NAMED_SCOPE("when frame produced");
    BOOST_LOG_TRIVIAL(debug) << "Received frame";
    auto video = *this->findChildren< VideoWidget* >("mainVideo").begin();
    video->set_current_frame(frame->test_phase3);
}
