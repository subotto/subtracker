#include "debugpanel.h"
#include "ui_debugpanel.h"

DebugPanel::DebugPanel(MainWindow *main, QWidget *parent) :
    TreeSubFrame(main, parent),
    ui(new Ui::DebugPanel)
{
    ui->setupUi(this);
}

DebugPanel::~DebugPanel()
{
    delete ui;
}

void DebugPanel::receive_frame(QSharedPointer<FrameAnalysis> frame)
{
    for (size_t i = this->videos.size(); i < frame->debug.size(); i++) {
        auto video = QSharedPointer< VideoWidget >(new VideoWidget(this));
        video->show();
        video->set_main(this->main);
        video->setMinimumHeight(200);
        this->videos.emplace_back(video);
    }
    for (size_t i = 0; i < frame->debug.size(); i++) {
        this->videos[i]->set_current_frame(frame->debug[i]);
    }
}

void DebugPanel::receive_settings(const FrameSettings &settings)
{
    Q_UNUSED(settings);
}
