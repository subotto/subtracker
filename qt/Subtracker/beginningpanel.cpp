#include "beginningpanel.h"
#include "ui_beginningpanel.h"

#include "ui_mainwindow.h"
#include "mainwindow.h"

BeginningPanel::BeginningPanel(MainWindow *main, QWidget *parent) :
    TreeSubFrame(main, parent),
    ui(new Ui::BeginningPanel)
{
    ui->setupUi(this);
    this->set_main_on_children();
    this->main->get_main_video()->set_following(this->ui->frame);
}

BeginningPanel::~BeginningPanel()
{
    delete ui;
}

void BeginningPanel::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    this->ui->frame->set_current_frame(frame->frame);
    this->ui->tableFrame->set_current_frame(frame->table_frame);
}

void BeginningPanel::receive_settings(const FrameSettings &settings) {

}
