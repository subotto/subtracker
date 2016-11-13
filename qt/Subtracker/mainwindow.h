#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QMetaType>
#include <QPointer>

#include "framereader.h"
#include "frameanalysis.h"
#include "worker.h"
#include "videowidget.h"

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
    void receive_frame(QSharedPointer< FrameAnalysis > frame);
    void when_worker_finished();

private:
    Ui::MainWindow *ui;

    FrameCycle *frame_cycle;
    QTimer timer;
    QPointer< Worker > worker;
    FrameSettings settings;

    void pass_string_to_label(const QString &name, const QString &value);
    void settings_modified();
    void pass_frame_to_video(VideoWidget *video, const cv::Mat &frame);
};

Q_DECLARE_METATYPE(QSharedPointer< FrameAnalysis >)

#endif // MAINWINDOW_H
