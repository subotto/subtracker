#ifndef BALLPANEL_H
#define BALLPANEL_H

#include <QFrame>

namespace Ui {
class BallPanel;
}

class BallPanel : public QFrame
{
    Q_OBJECT

public:
    explicit BallPanel(QWidget *parent = 0);
    ~BallPanel();

private:
    Ui::BallPanel *ui;
};

#endif // BALLPANEL_H
