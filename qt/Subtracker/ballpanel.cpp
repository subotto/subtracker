#include "ballpanel.h"
#include "ui_ballpanel.h"

BallPanel::BallPanel(MainWindow *main, QWidget *parent) :
    TreeSubFrame(main, parent),
    ui(new Ui::BallPanel)
{
    ui->setupUi(this);
    this->set_main_on_children();
}

BallPanel::~BallPanel()
{
    delete ui;
}

void BallPanel::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    this->ui->ballLL->set_current_frame(frame->viz_objects_ll[2]);
}

static void set_color_slider(QSlider *slider, float value) {
    slider->setMinimum(0);
    slider->setMaximum(255);
    slider->setValue(static_cast<int>(roundf(value * 255.0f)));
}

void BallPanel::receive_settings(const FrameSettings &settings)
{
    set_color_slider(this->ui->ballR, settings.objects_colors[2][2]);
    set_color_slider(this->ui->ballG, settings.objects_colors[2][1]);
    set_color_slider(this->ui->ballB, settings.objects_colors[2][0]);
    set_color_slider(this->ui->ballStdDev, settings.objects_color_stddev[2]);
}

void BallPanel::on_ballR_valueChanged(int value)
{
    this->main->get_settings().objects_colors[2][2] = static_cast<float>(value) / 255.0f;
    this->main->settings_modified();
}

void BallPanel::on_ballG_valueChanged(int value)
{
    this->main->get_settings().objects_colors[2][1] = static_cast<float>(value) / 255.0f;
    this->main->settings_modified();
}

void BallPanel::on_ballB_valueChanged(int value)
{
    this->main->get_settings().objects_colors[2][0] = static_cast<float>(value) / 255.0f;
    this->main->settings_modified();
}

void BallPanel::on_ballStdDev_valueChanged(int value)
{
    this->main->get_settings().objects_color_stddev[2] = static_cast<float>(value) / 255.0f;
    this->main->settings_modified();
}
