#include "beginningpanel.h"
#include "ui_beginningpanel.h"

BeginningPanel::BeginningPanel(MainWindow *main, QWidget *parent) :
    TreeSubFrame(main, parent),
    ui(new Ui::BeginningPanel)
{
    ui->setupUi(this);
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
