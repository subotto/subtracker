#include "beginningpanel.h"
#include "ui_beginningpanel.h"

#include "ui_mainwindow.h"
#include "mainwindow.h"

#include <QFileDialog>
#include <QtConcurrent/QtConcurrent>

#include <opencv2/imgcodecs/imgcodecs.hpp>

using namespace std;
using namespace cv;

BeginningPanel::BeginningPanel(MainWindow *main, QWidget *parent) :
    TreeSubFrame(main, parent),
    ui(new Ui::BeginningPanel)
{
    ui->setupUi(this);
    this->set_main_on_children();
    this->main->get_main_video()->set_following(this->ui->frame);
    this->load_image_filename("/home/giovanni/progetti/subotto/subtracker/data2016/ref2016.jpg", "image");
    this->load_image_filename("/home/giovanni/progetti/subotto/subtracker/data2016/ref_mask2016.png", "mask");
}

BeginningPanel::~BeginningPanel()
{
    delete ui;
}

void BeginningPanel::add_future_watcher(const QFuture< tuple< Mat, string, string > > &future) {
    auto watcher = new QFutureWatcher< tuple< Mat, string, string > >(this);
    bool res;
    res = connect(watcher, SIGNAL(finished()), this, SLOT(handle_future()));
    assert(res);
    watcher->setFuture(future);
    this->watchers.push_back(watcher);
}

bool BeginningPanel::handle_one_future(QFutureWatcher< tuple<Mat, string, string> > *future) {
    if (future->isFinished()) {
        auto image = get<0>(future->result());
        auto filename = get<1>(future->result());
        auto variant = get<2>(future->result());
        assert(variant == "image" || variant == "mask");
        if (variant == "image") {
            this->main->get_settings().ref_image = image;
            this->main->get_settings().ref_image_filename = filename;
            auto cmds = this->main->edit_commands();
            cmds.second->new_ref = true;
        } else {
            this->main->get_settings().ref_mask = image;
            this->main->get_settings().ref_mask_filename = filename;
            auto cmds = this->main->edit_commands();
            cmds.second->new_mask = true;
        }
        this->main->settings_modified();
        delete future;
        return true;
    } else {
        return false;
    }
}

void BeginningPanel::handle_future()
{
    // We don't know which watcher fired: we look through all of them and handle all those that have finished
    this->watchers.erase(remove_if(this->watchers.begin(),
                                   this->watchers.end(),
                                   [this](auto x){return this->handle_one_future(x);}),
                         this->watchers.end());
}

void BeginningPanel::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    this->ui->frame->set_current_frame(frame->frame);
    this->ui->tableFrame->set_current_frame(frame->table_frame);
    this->ui->refImage->set_current_frame(frame->ref_image);
    this->ui->refMask->set_current_frame(frame->ref_mask);
    this->ui->frameKeypoints->set_current_frame(frame->frame_matches);
}

void BeginningPanel::receive_settings(const FrameSettings &settings) {
    Q_UNUSED(settings);
    // FIXME: implement some sane default
}

tuple< Mat, string, string > load_image_thread(QString filename, string variant) {
    string cpp_filename = filename.toUtf8().constData();
    return make_tuple(imread(cpp_filename), cpp_filename, variant);
}

void BeginningPanel::async_load_image(const string &variant) {
    assert(variant == "image" || variant == "mask");
    QString title = variant == "image" ? tr("Open reference image") : tr("Open reference mask");
    QString filename = QFileDialog::getOpenFileName(this, title, "", tr("Images (*.png *.xpm *.jpg)"));
    this->load_image_filename(filename, variant);
}

void BeginningPanel::load_image_filename(QString filename, const string &variant)
{
    (variant == "image" ? this->ui->refImageFilename : this->ui->refMaskFilename)->setText(filename.toUtf8().constData());
    auto future = QtConcurrent::run(load_image_thread, filename, variant);
    this->add_future_watcher(future);
}

void BeginningPanel::on_refImageButton_clicked()
{
    async_load_image("image");
}

void BeginningPanel::on_refMaskButton_clicked()
{
    async_load_image("mask");
}
