#include "ballpanel.h"
#include "ui_ballpanel.h"

BallPanel::BallPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::BallPanel)
{
    ui->setupUi(this);
}

BallPanel::~BallPanel()
{
    delete ui;
}
