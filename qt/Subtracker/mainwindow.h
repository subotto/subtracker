#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QMetaType>
#include <QPointer>
#include <QFrame>
#include <QLabel>

class MainWindow;

#include "framereader.h"
#include "frameanalysis.h"
#include "worker.h"
#include "videowidget.h"
#include "treesubframe.h"
#include "videowidget.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    //friend class BallPanel;
    //friend class FoosmenPanel;
    //friend class BeginningPanel;

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void register_sub_frame(TreeSubFrame *sub_frame);
    VideoWidget *get_main_video();
    void settings_modified();
    FrameSettings &get_settings();

private slots:
    void on_actionStart_triggered();
    void on_actionStop_triggered();
    void update_mem();

public slots:
    void receive_frame();
    void when_worker_finished();

private:
    Ui::MainWindow *ui;

    FrameCycle *frame_cycle;
    QTimer timer, mem_timer;
    QPointer< Worker > worker;
    FrameSettings settings;
    std::vector< TreeSubFrame* > sub_frames;
    size_t current_rss, peak_rss;

    void pass_string_to_label(QLabel *label, const QString &value);
    void pass_frame_to_video(VideoWidget *video, const cv::Mat &frame);
    void add_frame_to_tree(QFrame *frame, const QString &button_text);
    void add_all_frames();
};

Q_DECLARE_METATYPE(QSharedPointer< FrameAnalysis >)

#endif // MAINWINDOW_H
