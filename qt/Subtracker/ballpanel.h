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
    void receive_settings(const FrameSettings &settings);

private slots:
    void on_ballR_valueChanged(int value);
    void on_ballG_valueChanged(int value);
    void on_ballB_valueChanged(int value);

    void on_ballStdDev_valueChanged(int value);

private:
    Ui::BallPanel *ui;
};

#endif // BALLPANEL_H
