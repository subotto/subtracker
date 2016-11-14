#ifndef MAINWINDOW_H
#define MAINWINDOW_H

class MainWindow;

#include <QMainWindow>
#include <QTimer>
#include <QMetaType>
#include <QPointer>
#include <QFrame>

#include "framereader.h"
#include "frameanalysis.h"
#include "worker.h"
#include "videowidget.h"
#include "treesubframe.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void register_sub_frame(TreeSubFrame *sub_frame);

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
    std::vector< TreeSubFrame* > sub_frames;

    void pass_string_to_label(const QString &name, const QString &value);
    void settings_modified();
    void pass_frame_to_video(VideoWidget *video, const cv::Mat &frame);
    void add_frame_to_tree(QFrame *frame, const QString &button_text);
    void add_all_frames();
};

Q_DECLARE_METATYPE(QSharedPointer< FrameAnalysis >)

#endif // MAINWINDOW_H
