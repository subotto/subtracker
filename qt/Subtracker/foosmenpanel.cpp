#include "foosmenpanel.h"
#include "ui_foosmenpanel.h"

FoosmenPanel::FoosmenPanel(MainWindow *main, QWidget *parent) :
    TreeSubFrame(main, parent),
    ui(new Ui::FoosmenPanel)
{
    ui->setupUi(this);
}

FoosmenPanel::~FoosmenPanel()
{
    delete ui;
}

void FoosmenPanel::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    this->ui->redFoosmenLL->set_current_frame(frame->viz_objects_ll[0]);
    this->ui->blueFoosmenLL->set_current_frame(frame->viz_objects_ll[1]);
}

void FoosmenPanel::receive_settings(const FrameSettings &settings) {

}
