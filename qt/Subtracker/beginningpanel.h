#ifndef BEGINNINGPANEL_H
#define BEGINNINGPANEL_H

#include <QFrame>
#include <QFutureWatcher>

#include <opencv2/core/core.hpp>

#include "treesubframe.h"

namespace Ui {
class BeginningPanel;
}

class BeginningPanel : public TreeSubFrame
{
    Q_OBJECT

public:
    explicit BeginningPanel(MainWindow *main, QWidget *parent = 0);
    ~BeginningPanel();
    void receive_frame(QSharedPointer<FrameAnalysis> frame);
    void receive_settings(const FrameSettings &settings);

private slots:
    void on_refImageButton_clicked();
    void on_refMaskButton_clicked();
    void handle_future();

private:
    void add_future_watcher(const QFuture<std::tuple<cv::Mat, std::string, std::string> > &future);
    bool handle_one_future(QFutureWatcher<std::tuple<cv::Mat, std::string, std::string> > *future);
    void async_load_image(const std::string &variant);
    void load_image_filename(QString filename, const std::string &variant);

    Ui::BeginningPanel *ui;
    std::vector< QFutureWatcher< std::tuple< cv::Mat, std::string, std::string > >* > watchers;
};

#endif // BEGINNINGPANEL_H
