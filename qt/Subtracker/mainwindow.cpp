#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    frame_cycle(NULL),
    timer(this)
{
    ui->setupUi(this);
    this->timer.setInterval(50);
    connect(&this->timer, SIGNAL(timeout()), this, SLOT(update()));
    this->timer.start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionStart_triggered()
{
  this->ui->statusBar->showMessage("Start!", 1000);
  this->timer.start();
}

void MainWindow::on_actionStop_triggered()
{
  this->ui->statusBar->showMessage("Stop!", 1000);
  this->timer.stop();
}
