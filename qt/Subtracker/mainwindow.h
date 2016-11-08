#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "framereader.h"
#include "frameanalysis.h"
#include "worker.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionStart_triggered();
    void on_actionStop_triggered();

public slots:
    void when_frame_produced(QSharedPointer< FrameAnalysis > frame);

private:
    Ui::MainWindow *ui;

    FrameCycle *frame_cycle;
    QTimer timer;
    Worker *worker;
};

#endif // MAINWINDOW_H
