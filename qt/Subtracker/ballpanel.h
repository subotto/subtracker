#ifndef BALLPANEL_H
#define BALLPANEL_H

#include <QFrame>

#include "treesubframe.h"

namespace Ui {
class BallPanel;
}

class BallPanel : public TreeSubFrame
{
    Q_OBJECT

public:
    explicit BallPanel(MainWindow *main, QWidget *parent = 0);
    ~BallPanel();
    void receive_frame(QSharedPointer<FrameAnalysis> frame);

private:
    Ui::BallPanel *ui;
};

#endif // BALLPANEL_H
