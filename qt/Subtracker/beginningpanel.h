#ifndef BEGINNINGPANEL_H
#define BEGINNINGPANEL_H

#include <QFrame>

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

private:
    Ui::BeginningPanel *ui;
};

#endif // BEGINNINGPANEL_H
