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

static void set_color_slider(QSlider *slider, int value) {
    slider->setMinimum(0);
    slider->setMaximum(255);
    slider->setValue(static_cast<int>(round(value * 255)));
}

void BallPanel::receive_settings(const FrameSettings &settings)
{
    set_color_slider(this->ui->ballR, settings.objects_colors[2][2]);
    set_color_slider(this->ui->ballG, settings.objects_colors[2][1]);
    set_color_slider(this->ui->ballB, settings.objects_colors[2][0]);
}

void BallPanel::on_ballR_valueChanged(int value)
{
    this->main->settings.objects_colors[2][2] = static_cast<float>(value) / 255.0f;
    this->main->settings_modified();
}

void BallPanel::on_ballG_valueChanged(int value)
{
    this->main->settings.objects_colors[2][1] = static_cast<float>(value) / 255.0f;
    this->main->settings_modified();
}

void BallPanel::on_ballB_valueChanged(int value)
{
    this->main->settings.objects_colors[2][0] = static_cast<float>(value) / 255.0f;
    this->main->settings_modified();
}
