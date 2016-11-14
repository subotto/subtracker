#include "ballpanel.h"
#include "ui_ballpanel.h"

BallPanel::BallPanel(MainWindow *main, QWidget *parent) :
    TreeSubFrame(main, parent),
    ui(new Ui::BallPanel)
{
    ui->setupUi(this);
}

BallPanel::~BallPanel()
{
    delete ui;
}

void BallPanel::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    this->ui->ballLL->set_current_frame(frame->viz_objects_ll[2]);
}
